#!/usr/bin/perl
# -*- tab-width: 4; fill-column: 80 -*-

use strict;
use warnings;
use diagnostics;
use FindBin;
use lib "$FindBin::Bin/src", "$FindBin::Bin/../src", "$FindBin::Bin/../../../ubs/common", "$FindBin::Bin/../../../ubs/common/package";
use Test;
use POSIX qw(strftime);

use MacroProcessor;
use PackageConfig;
use T_MasterConf;

BEGIN { plan tests => 55 }

my $operadir = $FindBin::Bin;
$operadir =~ s|/[^/]+/[^/]+/[^/]+/?$||;

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'deb';
	$conf->{product} = 'opera';
	$conf->prepare();
	my $mp = new MacroProcessor($conf);

	ok($mp->isDefined('i386'));
	ok(!$mp->isDefined('test_macro'));

	$conf->{eh}->reset();
	ok($mp->interpolate('@@{test_macro}', 'macro.t:(expected)', 0), '@@{test_macro}');
	ok(!$conf->{eh}->ok());

	$mp->define('test_macro', '');
	ok($mp->isDefined('test_macro'));
	$conf->{eh}->reset();
	ok($mp->interpolate('@@{test_macro}', 'macro.t:(unexpected)', 0), '');
	ok($conf->{eh}->ok());

	$mp->define('test_macro', '');
	ok($mp->isDefined('test_macro'));
	$conf->{eh}->reset();
	ok($mp->interpolate('@@{test_macro}', 'macro.t:(unexpected)', 0), '');
	ok($conf->{eh}->ok());

	$mp->define('test_macro', undef);
	ok(!$mp->isDefined('test_macro'));
	$conf->{eh}->reset();
	ok($mp->interpolate('@@{test_macro}', 'macro.t:(expected)', 0), '@@{test_macro}');
	ok(!$conf->{eh}->ok());

	$conf->{eh}->reset();
	ok($mp->interpolate('@@{SUFFIX}', 'macro.t:(unexpected)', 0), '');
	ok($mp->interpolate('@@{USUFFIX}', 'macro.t:(unexpected)', 0), '');
	ok($mp->interpolate('@@{_SUFFIX}', 'macro.t:(unexpected)', 0), '');
	ok($mp->interpolate('@@{PREFIX}', 'macro.t:(unexpected)', 0), '/usr');
	ok($mp->interpolate('@@{TIME:%s}', 'macro.t:(unexpected)', 0), strftime('%s', @{$conf->{starttime}}));
	ok($mp->interpolate('@@{CONF:package_type}', 'macro.t:(unexpected)', 0), 'deb');
	ok($mp->interpolate('@@{ABS:RESOURCES}', 'macro.t:(unexpected)', 0), '/usr/share/opera');
	ok($mp->interpolate('@@{REL:RESOURCES}', 'macro.t:(unexpected)', 0), 'share/opera');
	ok($conf->{eh}->ok());

	$conf->{eh}->reset();
	ok($mp->interpolate('@@{CONF:nonexistent}', 'macro.t:(expected)', 0), '@@{CONF:nonexistent}');
	ok(!$conf->{eh}->ok());

	$conf->{eh}->reset();
	ok($mp->interpolate('@@{ABS:NONEXISTENT}', 'macro.t:(expected)', 0), '@@{ABS:NONEXISTENT}');
	ok(!$conf->{eh}->ok());

	$conf->{eh}->reset();
	ok($mp->interpolate('@@{REL:NONEXISTENT}', 'macro.t:(expected)', 0), '@@{REL:NONEXISTENT}');
	ok(!$conf->{eh}->ok());

	ok(!$mp->needFurtherProcessing());
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'i386';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'deb';
	$conf->{product} = 'opera-next';
	$conf->prepare();
	my $mp = new MacroProcessor($conf);

	ok($mp->interpolate('@@{SUFFIX}', 'macro.t:(unexpected)', 0), '-next');
	ok($mp->interpolate('@@{USUFFIX}', 'macro.t:(unexpected)', 0), '-NEXT');
	ok($mp->interpolate('@@{_SUFFIX}', 'macro.t:(unexpected)', 0), ' Next');
	ok($mp->interpolate('@@{ABS:RESOURCES}', 'macro.t:(unexpected)', 0), '/usr/share/opera-next');
	ok($mp->interpolate('@@{REL:RESOURCES}', 'macro.t:(unexpected)', 0), 'share/opera-next');

	ok(!$mp->needFurtherProcessing());
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'FreeBSD';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera';
	$conf->prepare();
	my $mp = new MacroProcessor($conf);

	ok($mp->isDefined('x86_64'));

	$conf->{eh}->reset();
	ok($mp->interpolate('@@{SUFFIX}', 'macro.t:(unexpected)', 1), '@@{SUFFIX}');
	ok($mp->interpolate('@@{USUFFIX}', 'macro.t:(unexpected)', 1), '@@{USUFFIX}');
	ok($mp->interpolate('@@{_SUFFIX}', 'macro.t:(unexpected)', 1), '@@{_SUFFIX}');
	ok($mp->interpolate('@@{PREFIX}', 'macro.t:(unexpected)', 1), '@@{PREFIX}');
	ok($mp->interpolate('@@{TIME:%s}', 'macro.t:(unexpected)', 1), strftime('%s', @{$conf->{starttime}}));
	ok($mp->interpolate('@@{CONF:package_type}', 'macro.t:(unexpected)', 1), 'tar');
	ok($mp->interpolate('@@{ABS:RESOURCES}', 'macro.t:(unexpected)', 1), '@@{PREFIX}/share/opera@@{SUFFIX}');
	ok($mp->interpolate('@@{REL:RESOURCES}', 'macro.t:(unexpected)', 1), 'share/opera@@{SUFFIX}');
	ok($conf->{eh}->ok());

	ok($mp->needFurtherProcessing());
}

{
	my $master_conf = T_MasterConf->new($operadir);
	$master_conf->{os} = 'Linux';
	$master_conf->{arch} = 'x86_64';
	my $conf = new PackageConfig($master_conf);
	$conf->{package_type} = 'tar';
	$conf->{product} = 'opera';
	$conf->prepare();

	my $testdir = "$conf->{temp}/test_macro";
	File::Path::mkpath($testdir);
	foreach my $input (<$FindBin::Bin/data/*.in>) {
		my $mp = new MacroProcessor($conf);
		my $output = $input;
		$output =~ s/\.in$/.out/;
		my $temp_out = $output;
		$temp_out =~ s|^.*/([^/]+)$|$testdir/$1|;

		$conf->{eh}->reset();
		ok($mp->process($input, $temp_out, 1));
		ok($conf->{eh}->ok());

		my $diff = `diff -u '$output' '$temp_out'`;
		print STDERR $diff unless $? == 0;
		ok($?, 0);
	}
}
