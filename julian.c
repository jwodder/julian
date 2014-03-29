/* TODO:
 - Add a switch for setting the precision of Julian date output
 - Reconsider the " ± 0.5" thing
 - Add an option for setting the date of the Reformation (affecting input or
   just output?)
*/

/* Unless otherwise specified, all functions use a Gregorian calendar with the
 * Reformation taking place on 1582-10-05/15. */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>

#define JS_PRECISION  5

#define MIN       60
#define HOUR      (60 * MIN)
#define HALF_DAY  (12 * HOUR)
#define DAY       (24 * HOUR)

#define GREG_REFORM  2299161             /* noon on 1582-10-15 */
#define YDAY_REFORM  277                 /* zero-indexed yday for Oct. 05 */
#define START1583    (GREG_REFORM + 78)  /* noon on 1583-01-01 */
#define UK_REFORM    2361222             /* noon on 1752-09-14 */

struct yds {
 int year;  /* 0 == 1 BC */
 int days;  /* days from the start of the year; 0 == Jan 01 */
 int secs;  /* seconds after midnight; negative means ignore the field */
};

const int months[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

struct yds now(void);

bool isLeap(int year);
int daysSince1582(int year);
int yearLength(int year);
bool beforeGregorian(struct yds when);

bool breakDays(int year, int days, int* month, int* mday);
struct yds unbreakDays(int year, int month, int mday);
bool breakSeconds(int secs, int* hour, int* min, int* sec);

void toJulianDate(struct yds when, int* jdays, int* jsecs);
struct yds fromJulianDate(int jdays, int jsecs);
void julian2julian(int jdays, int* year, int* ydays);

void printYDS(struct yds when);
void printJulian(int jdays, int jsecs, int places);
void printOldStyle(int jdays, int jsecs);

int main(int argc, char** argv) {
 int ch;
 bool quiet = false;
 int oldStyle = 0;
 while ((ch = getopt(argc, argv, "qoO")) != -1) {
  switch (ch) {
   case 'q': quiet = true; break;
   case 'o': oldStyle = 1; break;
   case 'O': oldStyle = 2; break;
   default:
    fprintf(stderr, "Usage: %s [-oOq] [date ...]\n", argv[0]);
    return 2;
  }
 }
 argc -= optind;
 argv += optind;
 if (argc == 0) {
  struct yds nowest = now();
  if (!quiet) {
   printYDS(nowest);
   printf(" = ");
  }
  int jd, js;
  toJulianDate(nowest, &jd, &js);
  printJulian(jd, js, JS_PRECISION);
  putchar('\n');
 } else {
  for (int i=0; i<argc; i++) {
   char* endp;
   int leading = (int) strtol(argv[i], &endp, 10);
   if (endp == argv[i]) {
    fprintf(stderr, "%s: %s: invalid argument\n", argv[0], argv[i]);
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
    if (strptime(endp, "-%m-%d T %H:%M:%S", &tm_when)) {
     when = unbreakDays(year, tm_when.tm_mon+1, tm_when.tm_mday);
     when.secs = tm_when.tm_hour * HOUR + tm_when.tm_min * MIN + tm_when.tm_sec;
    } else if (strptime(endp, "-%m-%d", &tm_when)) {
     when = unbreakDays(year, tm_when.tm_mon+1, tm_when.tm_mday);
    } else {
     fprintf(stderr, "%s: %s: invalid argument\n", argv[0], argv[i]);
     continue;
    }
    if (!quiet) {
     printYDS(when);
     printf(" = ");
    }
    int jd, js;
    toJulianDate(when, &jd, &js);
    printJulian(jd, js, JS_PRECISION);
    putchar('\n');

   } else if (*endp == '\0' || *endp == '.') {
    /* Input is a Julian date; convert it to a calendar date. */
    int jdays = leading;
    int jsecs = -1;
    if (*endp == '.') {
     /* TODO: Improve this part. */
     endp++;
     char* endp2;
     jsecs = (int) strtol(endp, &endp2, 10);
     if (endp2 == endp || *endp2 != '\0') {
      fprintf(stderr, "%s: %s: invalid argument\n", argv[0], argv[i]);
      continue;
     }
     int digits = endp2 - endp;
     if (digits > 4) {
      /* Letting jsecs go over 4 digits will likely lead to overflow when
       * multiplying by DAY. */
      for (int j=0; j < digits-4; j++) jsecs /= 10;
      digits = 4;
     }
     jsecs *= DAY;
     for (int j=0; j < digits; j++) jsecs /= 10;
    }
    if (!quiet) {
     printJulian(jdays, jsecs, JS_PRECISION);
     printf(" = ");
    }
    struct yds when = fromJulianDate(jdays, jsecs);
    printYDS(when);
    if (oldStyle && GREG_REFORM <= jdays
		 && (jdays < UK_REFORM || oldStyle > 1)) {
     printf(" [");
     printOldStyle(jdays, jsecs);
     putchar(']');
    }
    putchar('\n');

   } else {fprintf(stderr, "%s: %s: invalid argument\n", argv[0], argv[i]); }
  }
 }
 return 0;
}

struct yds now(void) {
 time_t now0 = time(NULL);
 struct tm* nower = gmtime(&now0);
 struct yds nowest = {
  .year = nower->tm_year + 1900,
  .days = nower->tm_yday,
  .secs = nower->tm_hour * HOUR + nower->tm_min * MIN + nower->tm_sec,
 };
 return nowest;
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

bool breakDays(int year, int days, int* month, int* mday) {
 /* `month` and `mday` will start at 1. */
 if (days < 0) return false;
 for (int i=0; i<12; i++) {
  int length = months[i];
  if (i == 1 && isLeap(year)) length++;
  if (year == 1582 && i == 9) {
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

struct yds unbreakDays(int year, int month, int mday) {
 /* `month` and `mday` must start at 1. */
 /* TODO: Should this do anything about invalid dates (e.g., XXXX-04-32)? */
 /* TODO: Handle months over 12. */
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
 return (struct yds) {.year = year, .days = yday, .secs = -1};
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
 if (beforeGregorian(when)) {
  *jdays = (when.year + 4712) * 365 + (when.year + 4711)/4 + 1 + when.days;
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
 if (secs > DAY) {secs -= DAY; days++; }
 if (days >= GREG_REFORM) {
  if (days < START1583) {
   return (struct yds) {.year = 1582, .days = days - GREG_REFORM + YDAY_REFORM,
			.secs = secs};
  } else {
   days -= START1583;
   int year = days / 365 + 1583;
   days -= daysSince1582(year);
   while (days < 0) days += yearLength(--year);
   return (struct yds) {.year = year, .days = days, .secs = secs};
  }
 } else {
  int year, ydays;
  julian2julian(days, &year, &ydays);
  return (struct yds) {.year = year, .days = ydays, .secs = secs};
 }
}

void julian2julian(int jdays, int* year, int* ydays) {
 /* Convert a Julian date to a year & yday in the Julian calendar */
 *year = (jdays / 1461) * 4;
 *ydays = jdays % 1461;
 if (*ydays >= 366) {
  *ydays -= 366;
  *year += 1 + *ydays/365;
  *ydays %= 365;
 }
 *year -= 4712;
}

void printYDS(struct yds when) {
 int month, mday;
 breakDays(when.year, when.days, &month, &mday);
 /* TODO: Check return value! */
 printf("%.4d-%02d-%02d", when.year, month, mday);
 if (when.secs >= 0) {
  int hour, min, sec;
  breakSeconds(when.secs, &hour, &min, &sec);
  /* TODO: Check return value! */
  printf("T%02d:%02d:%02dZ", hour, min, sec);
 }
}

void printJulian(int jdays, int jsecs, int places) {
 printf("%d", jdays);
 if (places > 0) {
  if (jsecs >= 0) {
   putchar('.');
   for (int i=0; i<places; i++) {
    jsecs *= 10;
    int dig = jsecs / DAY;
    jsecs %= DAY;
    if (i == places-1 && jsecs * 2 >= DAY) dig++;
    printf("%d", dig);
   }
  } else {printf(" ± 0.5"); }
 }
}

void printOldStyle(int jdays, int jsecs) {
 int secs = jsecs >= 0 ? jsecs + HALF_DAY : -1;
 if (secs > DAY) {secs -= DAY; jdays++; }
 int year, ydays;
 julian2julian(jdays, &year, &ydays);
 int month=0, mday=0;
 for (int i=0; i<12; i++) {
  int length = months[i];
  if (i == 1 && year % 4 == 0) length++;
  if (ydays < length) {
   month = i+1;
   mday = ydays+1;
   break;
  } else {ydays -= length; }
 }
 printf("O.S. %.4d-%02d-%02d", year, month, mday);
 if (secs >= 0) {
  int hour, min, sec;
  breakSeconds(secs, &hour, &min, &sec);
  /* TODO: Check return value! */
  printf("T%02d:%02d:%02dZ", hour, min, sec);
 }
}
