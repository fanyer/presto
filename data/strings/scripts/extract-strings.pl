#!/usr/bin/perl -w
#
# File name:          extract-strings.pl
# Contents:           Extract strings from Opera sources, for initial creation
#                     of module.strings files.
#
# Copyright © 2005-2006 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

require v5.6.0;

# Read source code
my %data;
&recurse('modules');
&recurse('adjunct');
&recurse('platforms');

# Output files
&output;

sub recurse
{
	# Recurse down a subdirectory
	my $subdir = shift;
	my @subdirs;

	foreach my $component (glob("$subdir/*"))
	{
		if (-d $component && $component !~ /CVS$/)
		{
			push @subdirs, $component;
		}
	}

	if ($subdir =~ m"/")
	{
		my $target = join('/', (split(m'/', $subdir))[0,1]);
		&extract($subdir, "$target/module.strings");
	}

	foreach my $more (@subdirs)
	{
		&recurse($more);
	}
}

sub extract
{
	# Retrieve all strings from *.cpp and *.h files in the given directory
	my ($dir, $target) = @_;
	return if $dir =~ / /;

	print "Processing $dir, outputting to $target...\n";
	my @files;

	foreach my $src (glob("$dir/*.cpp"), glob("$dir/*.h"))
	{
#		print "  Reading $src\n";

		open SRC, '<', $src or die "Cannot read $src: $!\n";
		while (<SRC>)
		{
			while (/Str\s*::\s*([SMD]I?_[A-Za-z0-9_]+)/g)
			{
				$data{$target}->{$1} = 1;
			}
		}
		close SRC;
	}
}

sub output
{
	# Dump module.strings files
	print "Creating output...\n";

	FILE: foreach my $file (keys %data)
	{
		print "  $file";
		if (-e $file)
		{
			print " - skipping (already exists)\n";
			next FILE;
		}
		open STR, '>', $file or die "Cannot create $file: $!\n";
		print STR "// Strings referenced in this Opera module.\n";
		print STR "// Comment lines like these are ignored.\n";
		print STR "// Preprocessor directives are allowed.\n";
		foreach my $string (sort keys %{$data{$file}})
		{
			print STR $string, "\n";
		}
		close STR;
		print ".\n";
	}
}
