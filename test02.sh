#!/bin/bash
# Tests whether converting seconds from :sssss to .DDDDDD and back again
# preserves the value for all seconds in [0,86400)
#
# This program produces two files: test02.in, which contains all values from
# $base_JD:00000 through $base_JD:86399, and test02.out, which contains the
# respective values of test02.in after converting to .DDDDDD format and back.

program=./julian
base_JD=2451545

( for ((i=0; i<86400; i++)); do printf '%d:%05d\n' $base_JD $i; done ) > test02.in

xargs "$program" -v < test02.in | sed 's/ = .*//' | xargs "$program" -vs \
 | sed 's/ = .*//' > test02.out

diff test02.in test02.out
