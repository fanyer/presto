#!/usr/bin/perl -w
# -*- Mode: perl; tab-width: 4; indent-tabs-mode: t; -*-
#
# File name:          multiupdate.pl
# Contents:           Automates update of multiple translations.
#
# Copyright 2005-2011 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

# Handle error
$SIG{'__DIE__'} = \&diehandler;

# External code and variables
require v5.6.0;
use Getopt::Std;
use vars qw/$opt_i $opt_I $opt_b $opt_B $opt_r/;

# Print banner
$VERSION = &findversion;
print "Opera translation multi-updater $VERSION\n";
print "Copyright 2000-2011 Opera Software ASA\n\n";

# Check environment
my $makelng = '';
if (-e '../strings/english.db')
{
	$makelng = '../strings/scripts/makelang.pl -S ../strings';
}
elsif (-e '../../../strings/data/strings/english.db')
{
	$makelng = '../../../strings/data/strings/scripts/makelang.pl -S ../../../strings/data/strings';
}

die 'Cannot locate makelang.pl' if $makelng eq '';

# Check parameters
$VERSION = '#Revision#';
$VERSION =~ s/.Revision: (.*) ./$1/;
$Getopt::Std::STANDARD_HELP_VERSION = 1;
unless (getopts('IBi:b:r:'))
{
	HELP_MESSAGE()
}

# Parameter check-up
die "Need list of languages"         if $#ARGV == -1;
die "Cannot combine -i and -b"       if defined $opt_i && defined $opt_b;
die "Cannot combine -i and -I"       if defined $opt_i && defined $opt_I;
die "Cannot combine -i and -B"       if defined $opt_i && defined $opt_B;

die "Cannot combine -b and -I"       if defined $opt_b && defined $opt_I;
die "Cannot combine -b and -B"       if defined $opt_b && defined $opt_B;

die "Cannot combine -I and -B"       if defined $opt_I && defined $opt_B;

# Generate POT file


# Update languages
foreach my $language (@ARGV)
{

	print "Generating en/opera.pot...\n";
	my $cmdline = 'perl makepot.pl';
	$cmdline .= " -i$opt_i" if defined $opt_i;
	$cmdline .= " -b$opt_b" if defined $opt_b;
	$cmdline .= " -i$language/scopes.txt" if defined $opt_I;
	$cmdline .= " -b$language/default.strings" if defined $opt_B;
	$cmdline .= " -r$opt_r" if defined $opt_r;
	print "  $cmdline\n";
	my $rc = system $cmdline;
	die "Failed running makepot.pl: $!\n$language update failed!\n" if ($rc >> 8) != 0;

	# Retrieve the dbversion number
	open POT, '<', 'en/opera.pot' or die "Cannot read generated opera.pot: $!\n";
	my $dbversion = -1;
	while (<POT>)
	{
		if (/Opera, dbversion ([0-9]+)/)
		{
			$dbversion = $1, last;
		}
	}
	close POT;
	die "No dbversion found in opera.pot\n" if $dbversion == -1;

	print "Processing: $language...\n";
	my $origfile = "$language/$language.po";
	my $tempfile = "$language/$language.bak";
	#print "Copying $origfile to $tempfile\n";
	&copyfile($origfile, $tempfile);
	
	# Create temporary directory
	my $tmpdir = "updated_files";
	mkdir $tmpdir;

	# Merge to the opera.pot file
	print "- merging\n";
	print "  perl merge.pl -q $language\n";
	$rc = system "perl merge.pl -q $language";
	die "Failed running merge: $!\n$language update failed!\n"  if ($rc >> 8) != 0;

	# Copy updated po file to temp directory
	my $tmppofile = "${tmpdir}/${language}_db${dbversion}.po";
	&outgoing("${language}/${language}.po", $tmppofile);

	&copyfile ($tempfile, $origfile);
	unlink ($tempfile);
}

sub copyfile
{
	my ($src, $dest) = @_;
	open SRC, '<', $src or die "Cannot read $src: $!\n";
	open DST, '>', $dest or die "Cannot write $dest: $!\n";
	while (my $line = <SRC>)
	{
		print DST $line or die "Copy error: $!\n";
	}
	close SRC;
	close DST;
}

sub outgoing
{
	#Even if there is only one difference between the normal copy and
	#copy for outgoing files to agencies, I'll separate these totally now
	#as it is likely that we will need to do some other changes to the outgoing
	#files in the future, so it is easier to expand this section as needed
	my ($src, $dest) = @_;
	open SRC, '<', $src or die "Cannot read $src: $!\n";
	open DST, '>', $dest or die "Cannot write $dest: $!\n";
	
	my $obsoletes = 0;
	
	while (my $line = <SRC>)
	{
		if ($line =~ m/^#~/) {
			$obsoletes = 1;
		} elsif (!$obsoletes) { 
		print DST $line or die "Copy error: $!\n";
		}
	}
	close SRC;
	close DST;
}

sub diehandler
{
	print "Fatal error: ", shift;

	# Wait for exit on MSWIN
	if ($^O eq 'MSWin32')
	{
		print "Press <Enter> to exit:";
		my $notused = <STDIN>;
	}

	exit 1;
}

sub HELP_MESSAGE
{
	print "Usage: multiupdate.pl [-i file] [-b file] [-r rev] language ...\n";
	print "Update several translations.\n\n";
	print "Options for makepot.pl:\n";
	print "  -b file   Use a file with list of strings to include (optional).\n";
	print "  -i file   Use file with list of scopes to include (optional).\n";
	print "  -B		   Use language/default.strings for each language (optional).\n";
	print "  -I		   Use language/scopes.txt for each language (optional).\n";
	print "  NOTE: You can not combine -i -b -I or -B with each other, only select one.\n";
	print "  -r rev    CVS revision of english.db to use (optional).\n\n";
	print "You also need to list the languages you want to update, separated by space.\n";
	exit 1;
}

# Find self version
# Needed for --version
sub findversion
{
	my $path = $0;
	$path =~ s/multiupdate\.pl//;
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
