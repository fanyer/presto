#!/usr/bin/perl
# -*- tab-width: 4; fill-column: 80 -*-

use strict;
use warnings;
use diagnostics;
use FindBin;
use lib "$FindBin::Bin/src", "$FindBin::Bin/../src", "$FindBin::Bin/../../../ubs/common", "$FindBin::Bin/../../../ubs/common/package";
use Test;

use FileCache;

sub putfile($$)
{
	my ($filename, $text) = @_;
	open OUT, '>', $filename;
	print OUT $text;
	close OUT;
}

sub getfile($)
{
	my $filename = shift;
	open IN, '<', $filename;
	my $text = join '', <IN>;
	close IN;
	return $text;
}

BEGIN { plan tests => 4 }

my $operadir = $FindBin::Bin;
$operadir =~ s|/[^/]+/[^/]+/[^/]+/?$||;
my $tempdir = "$operadir/packages/temp";

{
	my $cache = new FileCache("$tempdir/cache.t");
	ok($cache->get('somekey'), undef);
	putfile("$tempdir/test_file1", 'foo');
	$cache->put('somekey', "$tempdir/test_file1");
	ok(getfile($cache->get('somekey')), 'foo');
	putfile("$tempdir/test_file2", 'bar');
	$cache->put('somekey', "$tempdir/test_file2");
	ok(getfile($cache->get('somekey')), 'bar');
	ok(getfile("$tempdir/test_file1"), 'foo'); # original file is unmodified
	# cleanup
	File::Path::rmtree("$tempdir/cache.t");
	unlink "$tempdir/test_file1", "$tempdir/test_file2";
}
