#!/usr/bin/perl
# -*- tab-width: 4; fill-column: 80 -*-

use strict;
use warnings;
use diagnostics;
use FindBin;
use lib "$FindBin::Bin/src", "$FindBin::Bin/../src", "$FindBin::Bin/../../../ubs/common", "$FindBin::Bin/../../../ubs/common/package";
use Test;

use ErrorHandler;

BEGIN { plan tests => 8 }

{
	my $eh = new ErrorHandler;
	ok($eh->ok());
	ok($eh->error('Testing error handler, please ignore this'), undef);
	ok(!$eh->ok());
	ok($eh->error('Testing error handler, please ignore this'), undef);
	ok(!$eh->ok());
	$eh->reset();
	ok($eh->ok());
	ok($eh->error('Testing error handler, please ignore this'), undef);
	ok(!$eh->ok());
}
