#!/usr/bin/perl
# -*- tab-width: 4; fill-column: 80 -*-

use strict;
use warnings;
use diagnostics;
use FindBin;
use lib "$FindBin::Bin/src", "$FindBin::Bin/../src", "$FindBin::Bin/../../../ubs/common", "$FindBin::Bin/../../../ubs/common/package";
use Test;

use Scheduler;

BEGIN { plan tests => 9 }

{
	my $scheduler = new Scheduler;
	ok($scheduler->taskCount(), 0);
	ok($scheduler->done());
	ok($scheduler->filesProduced(), 0);
}

{
	my $scheduler = new Scheduler;
	$scheduler->scheduleCommand('sleep 2', 'output-1');
	$scheduler->scheduleCommand('sleep 3', 'output-2');
	ok($scheduler->taskCount(), 2);
	my $time1 = time;
	ok($scheduler->done());
	my $time2 = time;
	ok($time2 - $time1 < 5); # The tasks ran in parallel
	ok($scheduler->filesProduced(), 2);
	ok(grep $_ eq 'output-1', $scheduler->filesProduced());
	ok(grep $_ eq 'output-2', $scheduler->filesProduced());
}
