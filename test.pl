#!/usr/bin/perl -w
use strict;

sub test($$);

my $program = "./julian";

my($success, $failure) = (0, 0);
while (<DATA>) {
 next if /^\s*#/ || /^\s*$/;
 chomp;
 my($julian, $cal) = split /\t+/;
 test($julian, $cal);
 test($cal, $julian);
}
print $success + $failure, " tests: $success successes, $failure failures\n";

sub test($$) {
 my($in, $goodOut) = @_;
 my $outOut = `$program -q -- $in`;
 chomp $outOut;
 if ($goodOut eq $outOut) { $success++ }
 else {
  print <<EOT;
FAILURE
Input: $in
Expected output: $goodOut
Received output: $outOut

EOT
  $failure++;
 }
}

__DATA__

0		-4712-01-01
366		-4711-01-01
1719656		-0004-02-29
2159358		1200-01-01
2195883		1300-01-01
2232408		1400-01-01

2268932		1499-12-31

2268933		1500-01-01
2268993		1500-03-01
2269115		1500-07-01
2269146		1500-08-01
2269177		1500-09-01
2269204		1500-09-28
2269205		1500-09-29
2269207		1500-10-01
2269298		1500-12-31

2269299		1501-01-01
2269663		1501-12-31
2298796		1581-10-05
2298883		1581-12-31
2298884		1582-01-01
2299159		1582-10-03
2299160		1582-10-04

# These also need to be checked for accuracy of O.S. dates:
2299161		1582-10-15
2299162		1582-10-16
2299238		1582-12-31
2299239		1583-01-01
2305448		1600-01-01
2341973		1700-01-01

2378497		1800-01-01
2415021		1900-01-01
2451545		2000-01-01
2451605		2000-03-01
2451910		2000-12-31
2451911		2001-01-01
2453066		2004-03-01
2456746		2014-03-29
