#!/usr/bin/perl
# -*- tab-width: 4; fill-column: 80 -*-

use strict;
use warnings;
use diagnostics;
use FindBin;
use lib "$FindBin::Bin/src", "$FindBin::Bin/../src", "$FindBin::Bin/../../../ubs/common", "$FindBin::Bin/../../../ubs/common/package";
use Test;

use PackageConfig;
use T_MasterConf;

BEGIN { plan tests => 430 }

my $operadir = $FindBin::Bin;
$operadir =~ s|/[^/]+/[^/]+/[^/]+/?$||;

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'devel';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'devel', @{$conf->{targets}});
	ok(grep $_ eq 'devel:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'debug', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'devel';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'devel', @{$conf->{targets}});
	ok(grep $_ eq 'devel:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'debug', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'devel';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'devel', @{$conf->{targets}});
	ok(grep $_ eq 'devel:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'debug', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'devel';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'devel', @{$conf->{targets}});
	ok(grep $_ eq 'devel:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'debug', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'devel';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'devel', @{$conf->{targets}});
	ok(grep $_ eq 'devel:opera', @{$conf->{targets}});
	ok(grep $_ eq 'FreeBSD', @{$conf->{targets}});
	ok(grep $_ eq 'debug', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'devel';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'devel', @{$conf->{targets}});
	ok(grep $_ eq 'devel:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'FreeBSD', @{$conf->{targets}});
	ok(grep $_ eq 'debug', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'devel';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'devel', @{$conf->{targets}});
	ok(grep $_ eq 'devel:opera', @{$conf->{targets}});
	ok(grep $_ eq 'FreeBSD', @{$conf->{targets}});
	ok(grep $_ eq 'debug', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'devel';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'devel', @{$conf->{targets}});
	ok(grep $_ eq 'devel:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'FreeBSD', @{$conf->{targets}});
	ok(grep $_ eq 'debug', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-10.50-9999.x86_64.linux');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-next-10.50-9999.x86_64.linux');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-10.50-9999.i386.linux');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-next-10.50-9999.i386.linux');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-10.50-9999.x86_64.linux');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-next-10.50-9999.x86_64.linux');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera', @{$conf->{targets}});
	ok(grep $_ eq 'FreeBSD', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(!grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-10.50-9999.i386.freebsd');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'FreeBSD', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(!grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-next-10.50-9999.i386.freebsd');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera', @{$conf->{targets}});
	ok(grep $_ eq 'FreeBSD', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(!grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-10.50-9999.amd64.freebsd');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'tar', @{$conf->{targets}});
	ok(grep $_ eq 'tar:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'FreeBSD', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok(!grep $_ eq 'gz', @{$conf->{compression}});
	ok(grep $_ eq 'bz2', @{$conf->{compression}});
	ok(!grep $_ eq 'Z', @{$conf->{compression}});
	ok($conf->{prefix}, '/usr/local');
	ok($conf->{stem}, 'opera-next-10.50-9999.amd64.freebsd');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'deb';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'deb', @{$conf->{targets}});
	ok(grep $_ eq 'deb:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{prefix}, '/usr');
	ok($conf->{stem}, 'opera_10.50.9999_i386');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'deb';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'deb', @{$conf->{targets}});
	ok(grep $_ eq 'deb:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{prefix}, '/usr');
	ok($conf->{stem}, 'opera-next_10.50.9999_i386');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'deb';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'deb', @{$conf->{targets}});
	ok(grep $_ eq 'deb:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{prefix}, '/usr');
	ok($conf->{stem}, 'opera_10.50.9999_amd64');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'deb';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'deb', @{$conf->{targets}});
	ok(grep $_ eq 'deb:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{prefix}, '/usr');
	ok($conf->{stem}, 'opera-next_10.50.9999_amd64');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'rpm';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'rpm', @{$conf->{targets}});
	ok(grep $_ eq 'rpm:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{prefix}, '/usr');
	ok($conf->{stem}, 'opera-10.50-9999.i386');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'rpm';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'rpm', @{$conf->{targets}});
	ok(grep $_ eq 'rpm:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{prefix}, '/usr');
	ok($conf->{stem}, 'opera-next-10.50-9999.i386');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'rpm';
	$conf->{product} = 'opera';
	$conf->prepare();
	ok(grep $_ eq 'opera', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'rpm', @{$conf->{targets}});
	ok(grep $_ eq 'rpm:opera', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{prefix}, '/usr');
	ok($conf->{stem}, 'opera-10.50-9999.x86_64');
	ok($conf->{suffix}, '');
	ok($conf->{_suffix}, '');
	ok($conf->{usuffix}, '');
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'rpm';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	ok(grep $_ eq 'opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'unix', @{$conf->{targets}});
	ok(grep $_ eq 'x11', @{$conf->{targets}});
	ok(!grep /^qt/, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{os}, @{$conf->{targets}});
	ok(grep $_ eq $master_conf->{arch}, @{$conf->{targets}});
	ok(grep $_ eq 'rpm', @{$conf->{targets}});
	ok(grep $_ eq 'rpm:opera-next', @{$conf->{targets}});
	ok(grep $_ eq 'Linux', @{$conf->{targets}});
	ok(grep $_ eq 'release', @{$conf->{targets}});
	ok(grep $_ eq 'alien', @{$conf->{targets}}); # feature_scanner works
	ok($conf->{prefix}, '/usr');
	ok($conf->{stem}, 'opera-next-10.50-9999.x86_64');
	ok($conf->{suffix}, '-next');
	ok($conf->{_suffix}, ' Next');
	ok($conf->{usuffix}, '-NEXT');
}
