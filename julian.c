/* Unless otherwise specified, all functions use a Gregorian calendar with the
 * Reformation taking place on 1582-10-05/15. */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>  /* strchr */
#include <ctype.h>  /* isdigit, digittoint */
#include <errno.h>
#include <limits.h>  /* INT_MAX */
#include <time.h>
#include <unistd.h>

const char version[] =
 "julian -- julian date converter, v.1.0\n"
 "Copyright (C) 2014 John T. Wodder II <jwodder@sdf.lonestar.org>\n"
 "julian is distributed under the terms of the MIT License.\n"
 "See <https://github.com/jwodder/julian> for the latest version.\n";

#define JD_MIN  (365-INT_MAX)
/* This limit on Julian dates is so that julian2julian's behavior of calling
 * `julian2julian(365 - jdays, year, yday)` when `jdays` is negative will never
 * result in another extremely negative `jdays` value, which would lead to an
 * infinite loop. */

#define JS_PRECISION  6

#define MIN       60
#define HOUR      (60 * MIN)
#define HALF_DAY  (12 * HOUR)
#define DAY       (24 * HOUR)

#define GREG_REFORM  2299161             /* noon on 1582-10-15 */
#define YDAY_REFORM  277                 /* zero-indexed yday for Oct. 05 */
#define START1583    (GREG_REFORM + 78)  /* noon on 1583-01-01 */
#define START1600    2305448             /* noon on 1600-01-01 */
#define UK_REFORM    2361222             /* noon on 1752-09-14 */

struct yds {
 int year;  /* 0 == 1 BC */
 int days;  /* days from the start of the year; 0 == Jan 01 */
 int secs;  /* seconds after midnight; negative means ignore the field */
};

/* These are the calendar dates corresponding to the minimum & maximum allowed
 * Julian dates when int is 32 bits: */
struct yds min_date = {.year = -5884201, .days =  75, .secs = HALF_DAY};
struct yds max_date = {.year =  5874898, .days = 153, .secs = DAY-1};

const int months[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void usage(void);
bool parseInt(int* i, char* str, char** endp, char* fullarg);

struct yds now(void);

bool isLeap(int year);
int  daysSince1582(int year);
int  yearLength(int year);
bool beforeGregorian(struct yds when);
int  cmpYDS(struct yds when, struct yds then);

bool breakDays(int days, int leap, int* month, int* mday);
bool unbreakDays(int year, int month, int mday, struct yds* when);
bool breakSeconds(int secs, int* hour, int* min, int* sec);

void       toJulianDate(struct yds when, int* jdays, int* jsecs);
struct yds fromJulianDate(int jdays, int jsecs);
void       julian2julian(int jdays, int* year, int* yday);

void printStyled(struct yds when, int jdays, int jsecs, int oldStyle);
void printYDS(struct yds when, bool jul);
void printJulian(int jdays, int jsecs, int places);
void printOldStyle(int jdays, int jsecs);

bool printYday = false, intsecs = false;
char* argv0 = NULL;

int main(int argc, char** argv) {
 int ch;
 bool verbose = false, errored = false, endGetoptNow = false;
 int oldStyle = 0;
 argv0 = argv[0];
 while (!endGetoptNow && (ch = getopt(argc, argv, "jOosvV0123456789")) != -1) {
  switch (ch) {
   case 'j': printYday = true; break;
   case 'o': oldStyle = 1; break;
   case 'O': oldStyle = 2; break;
   case 's': intsecs = true; break;
   case 'v': verbose = true; break;
   case 'V': fputs(version, stdout); return 0;

   /* This allows negative Julian dates and YYYY-MM-DD dates with negative
    * years to be used without getopt mistaking them for switches: */
   case '0': case '1': case '2': case '3': case '4':
   case '5': case '6': case '7': case '8': case '9':
    if (optind > 1 && strchr(argv[optind-1], ch) != NULL) {
     /* If the "previous" argument contains the digit, then since no options
      * take any arguments, it must be the case that the "previous" argument is
      * actually the current argument and that there are no more characters
      * after the digit, causing getopt to have already incremented optind to
      * point to the next argument.  Thus, in order for julian to see this
      * argument, optind must be decremented. */
     optind--;
    }
    if (argv[optind][1] != ch) {
     /* The user appended a number to the end of a normal option bundle. */
     fprintf(stderr, "%s: %s: invalid options\n", argv0, argv[optind]);
     usage();
     return 2;
    }
    endGetoptNow = true;
    break;

   default: usage(); return 2;
  }
 }
 argc -= optind;
 argv += optind;

 if (argc == 0) {
  struct yds nowest = now();
  int jd, js;
  toJulianDate(nowest, &jd, &js);
  if (verbose) {
   printStyled(nowest, jd, js, oldStyle);
   printf(" = ");
  }
  printJulian(jd, js, JS_PRECISION);
  putchar('\n');

 } else {
  for (int i=0; i<argc; i++) {
   char *endp, *endp2;
   int leading;
   if (!parseInt(&leading, argv[i], &endp, argv[i])) {
    errored = true;
    continue;
   }

   if (*endp == '-') {
    /* Input is a calendar date; convert it to a Julian date. */
    /* It seems that strptime can't handle years before 1900, hence the need to
     * parse the leading integer first. */
    int year = leading;
    struct tm tm_when;
    struct yds when;
    /* Should the calls to strptime care about trailing characters in str? */
    if ((endp2 = strptime(endp, "-%m-%d", &tm_when)) != NULL) {
     int month = tm_when.tm_mon + 1;
     int mday  = tm_when.tm_mday;
     if (!unbreakDays(year, month, mday, &when)) {
      fprintf(stderr, "%s: %.4d-%02d-%02d: invalid date\n",
	      argv0, year, month, mday);
      errored = true;
      continue;
     }
    } else if ((endp2 = strptime(endp, "-%j", &tm_when)) != NULL) {
     when.year = year;
     when.days = tm_when.tm_yday;
     when.secs = -1;
     if (when.days < 0 || when.days >= yearLength(year)) {
      fprintf(stderr, "%s: yday value %d out of rage for year %d\n",
	      argv0, when.days, year);
      errored = true;
      continue;
     }
    } else {
     fprintf(stderr, "%s: %s: invalid argument\n", argv0, argv[i]);
     errored = true;
     continue;
    }
    if (strptime(endp2, "T %H:%M:%S", &tm_when)) {
     when.secs = tm_when.tm_hour * HOUR + tm_when.tm_min * MIN + tm_when.tm_sec;
    }
    if (cmpYDS(when, min_date) < 0 || cmpYDS(when, max_date) > 0) {
     fprintf(stderr, "%s: %s: value outside of allowed range\n",argv0,argv[i]);
     errored = true;
     continue;
    }
    int jd, js;
    toJulianDate(when, &jd, &js);
    if (verbose) {
     printStyled(when, jd, js, oldStyle);
     printf(" = ");
    }
    printJulian(jd, js, JS_PRECISION);
    putchar('\n');

   } else if (*endp == '.') {
    /* Input is a Julian date with fractional part; convert it to a calendar
     * date. */
    int jdays = leading, jsecs = 0;
    int coef = DAY/10, accum = 0, denom = 1;
    for (endp++; isdigit(*endp); endp++) {
     accum += coef * digittoint(*endp);
     jsecs += accum / denom;
     accum %= denom;
     if (coef % 10) {accum *= 10; denom *= 10; }
     else coef /= 10;
    }
    if (*endp != '\0') {
     fprintf(stderr, "%s: %s: invalid argument\n", argv0, argv[i]);
     errored = true;
     continue;
    }
    if (accum * 2 >= denom) jsecs++;
    if (jdays == INT_MAX && jsecs >= HALF_DAY) {
     fprintf(stderr, "%s: %s: value outside of allowed range\n",argv0,argv[i]);
     errored = true;
     continue;
    }
    if (verbose) {
     printJulian(jdays, jsecs, JS_PRECISION);
     printf(" = ");
    }
    struct yds when = fromJulianDate(jdays, jsecs);
    printStyled(when, jdays, jsecs, oldStyle);
    putchar('\n');

   } else if (*endp == ':') {
    /* Input is a Julian date with seconds; convert it to a calendar date. */
    int jdays = leading, jsecs;
    if (!parseInt(&jsecs, endp+1, &endp2, argv[i])) {
     errored = true;
     continue;
    }
    int jdays0 = jdays, jsecs0 = jsecs;
    jdays += jsecs / DAY;
    jsecs %= DAY;
    if (jsecs < 0) {jdays--; jsecs += DAY; }
    if ((jsecs0 > 0 ? jdays < jdays0 : jdays > jdays0)
        || jdays < JD_MIN || (jdays == INT_MAX && jsecs >= HALF_DAY)) {
     fprintf(stderr, "%s: %s: value outside of allowed range\n",argv0,argv[i]);
     errored = true;
     continue;
    }
    if (verbose) {
     printJulian(jdays, jsecs, JS_PRECISION);
     printf(" = ");
    }
    struct yds when = fromJulianDate(jdays, jsecs);
    printStyled(when, jdays, jsecs, oldStyle);
    putchar('\n');

   } else if (*endp == '\0') {
    /* Input is an integral Julian date; convert it to a calendar date. */
    int jdays = leading, jsecs = -1;
    if (verbose) {
     printJulian(jdays, jsecs, JS_PRECISION);
     printf(" = ");
    }
    struct yds when = fromJulianDate(jdays, jsecs);
    printStyled(when, jdays, jsecs, oldStyle);
    putchar('\n');

   } else {
    fprintf(stderr, "%s: %s: invalid argument\n", argv0, argv[i]);
    errored = true;
   }
  }
 }
 return errored ? 2 : 0;
}

void usage(void) {
 fprintf(stderr, "Usage: %s [-O | -o] [-jv] [date ...]\n", argv0);
}

bool parseInt(int* i, char* str, char** endp, char* fullarg) {
 errno = 0;
 long parsed = strtol(str, endp, 10);
 if (*endp == str) {
  fprintf(stderr, "%s: %s: invalid argument\n", argv0, fullarg);
  return false;
 } else if (errno == ERANGE || parsed < JD_MIN || parsed > INT_MAX) {
  /* The `365-INT_MAX` limit is so that julian2julian's behavior of calling
   * `julian2julian(365 - jdays, year, yday)` when `jdays` is negative will
   * never result in another extremely negative `jdays` value, which would lead
   * to an infinite loop. */
  fprintf(stderr, "%s: %s: value outside of allowed range\n", argv0, fullarg);
  return false;
 } else {
  *i = (int) parsed;
  return true;
 }
}

struct yds now(void) {
 time_t now0 = time(NULL);
 struct tm* nower = gmtime(&now0);
 return (struct yds) {
  .year = nower->tm_year + 1900,
  .days = nower->tm_yday,
  .secs = nower->tm_hour * HOUR + nower->tm_min * MIN + nower->tm_sec,
 };
}

bool isLeap(int year) {
 if (year <= 1582) return year % 4 == 0;
 else return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int daysSince1582(int year) {
 /* Returns the number of days in the years [1583..year-1] combined */
 /* (`year` must be at least 1583.) */
 return (year - 1583) * 365 + (year - 1581)/4
      - (year - 1501)/100 + (year - 1201)/400;
}

int yearLength(int year) {
 if (year == 1582) return 355;
 else return 365 + isLeap(year);
}

bool beforeGregorian(struct yds when) {
 return when.year < 1582 || (when.year == 1582 && when.days < YDAY_REFORM);
}

int cmpYDS(struct yds when, struct yds then) {
 if (when.year < then.year) return -1;
 if (when.year > then.year) return  1;
 if (when.days < then.days) return -1;
 if (when.days > then.days) return  1;
 if (when.secs < then.secs) return -1;
 if (when.secs > then.secs) return  1;
 return 0;
}

bool breakDays(int days, int leap, int* month, int* mday) {
 /* `month` and `mday` will start at 1. */
 /* `leap` is 0 for non-leap years, 1 for leap years, and -1 for 1582. */
 if (days < 0) return false;
 for (int i=0; i<12; i++) {
  int length = months[i];
  if (leap == 1 && i == 1) length++;
  else if (leap == -1 && i == 9) {
   length = 21;
   if (days < length) {
    *month = 10;
    *mday = days + (days < 4 ? 1 : 11);
    return true;
   }
  }
  if (days < length) {
   *month = i+1;
   *mday = days+1;
   return true;
  } else {days -= length; }
 }
 return false;
}

bool unbreakDays(int year, int month, int mday, struct yds* when) {
 /* `month` and `mday` must start at 1. */
 if (month < 1 || month > 12) return false;
 if (mday  < 1 || (mday > months[month-1]
		   && !(isLeap(year) && month == 2 && mday == 29)))
  return false;
 month--;
 int yday = 0;
 for (int i=0; i<month; i++) {
  yday += months[i];
  if (i==1 && isLeap(year)) yday++;
 }
 yday += mday - 1;
 if (year == 1582 && (month>9 || (month==9 && mday>=15))) yday -= 10;
 /* If someone enters a date that was skipped by the Gregorian Reformation,
  * just assume it's Old Style. */
 when->year = year;
 when->days = yday;
 when->secs = -1;
 return true;
}

bool breakSeconds(int secs, int* hour, int* min, int* sec) {
 if (secs < 0) return false;
 else {
  /* TODO: Check for too-large values? */
  *hour = secs / HOUR;
  secs %= HOUR;
  *min = secs / MIN;
  *sec = secs % MIN;
  return true;
 }
}

void toJulianDate(struct yds when, int* jdays, int* jsecs) {
 if (when.year < -4712) {
  int revYear = -4712 - when.year;
  *jdays = when.days - (revYear * 365 + revYear / 4);
 } else if (beforeGregorian(when)) {
  *jdays = (when.year + 4712) * 365 + (when.year + 4712 + 3)/4 + when.days;
  /* Note that -1/4 == 0. */
 } else if (when.year == 1582) {
  *jdays = GREG_REFORM + (when.days - YDAY_REFORM);
 } else {
  *jdays = START1583 + daysSince1582(when.year) + when.days;
 }
 if (jsecs != NULL) {
  if (when.secs < 0) {*jsecs = -1; }
  else if (when.secs < HALF_DAY) {(*jdays)--; *jsecs = when.secs + HALF_DAY; }
  else {*jsecs = when.secs - HALF_DAY; }
 }
}

struct yds fromJulianDate(int jdays, int jsecs) {
 int days = jdays;
 int secs = jsecs >= 0 ? jsecs + HALF_DAY : -1;
 if (secs >= DAY) {secs -= DAY; days++; }
 if (days < START1600) {
  if (GREG_REFORM <= days) days += 10;
  int year, yday;
  julian2julian(days, &year, &yday);
  if (GREG_REFORM <= days && days-10 < START1583) yday -= 10;
  return (struct yds) {.year = year, .days = yday, .secs = secs};
 } else {
  days -= START1600;
  int year = 1600 + (days / 146097) * 400;
  days %= 146097;
  /* Add a "virtual leap day" to the end of each non-Gregorian centennial year
   * so that `days` can then be handled as in the Julian calendar: */
  if (days > 365) days += (days - 366)/36524;
  year += (days / 1461) * 4;
  days %= 1461;
  if (days > 365) days += (days - 366)/365;
  year += days/366;
  days %= 366;
  return (struct yds) {.year = year, .days = days, .secs = secs};
 }
}

void julian2julian(int jdays, int* year, int* yday) {
 /* Convert a Julian date to a year & yday in the Julian calendar */
 if (jdays < 0) {
  julian2julian(365 - jdays, year, yday);
  *year = -4712 - (*year + 4712);
  *yday = yearLength(*year) - 1 - *yday;
 } else {
  *year = (jdays / 1461) * 4;
  *yday = jdays % 1461;
  /* Add a "virtual leap day" to the end of each common year so that `*yday`
   * can be divided & modded by 366 evenly: */
  if (*yday > 365) *yday += (*yday - 366)/365;
  *year += *yday/366;
  *yday %= 366;
  *year -= 4712;
 }
}

void printStyled(struct yds when, int jdays, int jsecs, int oldStyle) {
 printYDS(when, false);
 if (oldStyle && GREG_REFORM <= jdays && (jdays < UK_REFORM || oldStyle > 1)) {
  printf(" [");
  printOldStyle(jdays, jsecs);
  putchar(']');
 }
}

void printYDS(struct yds when, bool jul) {
 /* `jul` is true iff `when` should be treated as a date in the Julian calendar
  * rather than reformed Gregorian. */
 if (printYday) {
  printf("%.4d-%03d", when.year, when.days+1);
 } else {
  int month, mday;
  int leap = jul ? when.year % 4 == 0
		 : when.year == 1582 ? -1 : isLeap(when.year);
  breakDays(when.days, leap, &month, &mday);
  /* TODO: Check return value! */
  printf("%.4d-%02d-%02d", when.year, month, mday);
 }
 if (when.secs >= 0) {
  int hour, min, sec;
  breakSeconds(when.secs, &hour, &min, &sec);
  /* TODO: Check return value! */
  printf("T%02d:%02d:%02dZ", hour, min, sec);
 }
}

void printJulian(int jdays, int jsecs, int places) {
 printf("%d", jdays);
 if (jsecs >= 0) {
  if (intsecs) {printf(":%05d", jsecs); }
  else if (places > 0) {
   putchar('.');
   for (int i=0; i<places; i++) {
    jsecs *= 10;
    int dig = jsecs / DAY;
    jsecs %= DAY;
    /* TODO: What should happen when the last digit is a 9 that should be
     * rounded up? */
    if (i == places-1 && jsecs * 2 >= DAY && dig < 9) dig++;
    printf("%d", dig);
   }
  }
 }
}

void printOldStyle(int jdays, int jsecs) {
 /* TODO: Should this be merged into `printStyled`? */
 int secs = jsecs >= 0 ? jsecs + HALF_DAY : -1;
 if (secs >= DAY) {secs -= DAY; jdays++; }
 int year, yday;
 julian2julian(jdays, &year, &yday);
 printf("O.S. ");
 printYDS((struct yds) {.year = year, .days = yday, .secs = secs}, true);
}
