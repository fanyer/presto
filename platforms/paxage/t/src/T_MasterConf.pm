# -*- tab-width: 4; fill-column: 80 -*-

package T_MasterConf;

use strict;
use warnings;
use Time::localtime;

sub new()
{
	my ($class, $operadir) = @_;
	return {
		starttime => localtime,
		source => $operadir,
		buildroot => [],
		outdir => "$operadir/packages",
		develdir => "$operadir/run",
		temp_root => "$operadir/packages/temp",
		strip_command => 'strip',
		strip => 1,
		os => 'Linux',
		arch => 'x86_64',
		version => '10.50',
		build => '9999',
	};
}

1;
