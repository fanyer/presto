#!/usr/bin/perl -w
# 
# File name:          makepot.pl
# Contents:           Creates initial POT file to send to translators from
#                     the english.db file
#
# Copyright (C) 2002-2011 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

require v5.6.0;

# External code and variables
use Getopt::Std;
use POSIX;
use vars qw/$opt_r $opt_i $opt_b $opt_e/;

# Parameter setup
my $compatibility_level = 9;

# File names
my ($input, @makelng);
if (-e '../strings/english.db')
{
	$input  = '../strings/english.db';
	@makelng=('../strings/scripts/makelang.pl', '-S', '../strings', '-d', 'en');
}
elsif (-e '../../../strings/data/strings/english.db')
{
	$input  = '../../../strings/data/strings/english.db';
	@makelng=('../../../strings/data/strings/scripts/makelang.pl', '-S',
	          '../../../strings/data/strings', '-d', 'en');
}
else
{
	die 'Unable to locate your checkout of the "strings" module.';
}

# Scopes to include (-i)
my %include_scopes;

# Handle error messages on Win32
if ($^O eq 'MSWin32')
{
	$SIG{'__DIE__'} = \&diehandler;
}

# Print banner
$VERSION = &findversion;
print "Opera translation template tool $VERSION\n";
print "Copyright 2002-2011 Opera Software ASA\n\n";

# Read command line
$Getopt::Std::STANDARD_HELP_VERSION = 1;
unless (getopts('r:i:b:e'))
{
	HELP_MESSAGE();
}

# Sanity check
die "Cannot combine -i and -b" if defined $opt_i && defined $opt_b;

# Build a command line
my @cmdline = ('perl', @makelng, '-c', $compatibility_level);
push @cmdline, (defined $opt_e) ? '-e' : '-P';
if (defined $opt_b)
{
	push @cmdline, ('-b', $opt_b)
}
elsif (defined $opt_i)
{
	push @cmdline, ('-i', $opt_i)
}
push @cmdline, ('-r', $opt_r) if defined $opt_r;

print 'Calling: "', join(' ', @cmdline), "\"...\n";
system(@cmdline) == 0 or die "Failed to run $makelng[0]\n";

exit 0;

sub diehandler
{
	print shift;
	print "Press <Enter> to exit:";
	my $notused = <STDIN>;
}

sub HELP_MESSAGE
{
	print "Usage: $0 [-r rev] [-d] [-b <file>] [-i <file>]\n";
	print "Creates an opera.pot translation template from the string database.\n\n";
	print "  -e         Create an en.po file with translations instead.\n";
	print "  -r rev     Use this CVS/Git revision of english.db instead of current.\n";
	print "  -b <file>  Include only strings included in <file> (build.strings).\n";
	print "  -i <file>  Include only strings for the scopes defined in <file>.\n";
	print "             File format is one scope on each line.\n\n";
	exit 1;
}

# Find self version
# Needed for --version
sub findversion
{
	my $path = $0;
	$path =~ s/makepot\.pl//;
	$path .= '/module.about';
	if (open ABOUT, '<', $path)
	{
		while (my $line = <ABOUT>)
		{
			if ($line =~ /version: ([^\n]+)/)
			{
				return $1;
			}
		}
	}

	return 'n/a';
}
