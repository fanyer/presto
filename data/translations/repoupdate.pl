#!/usr/bin/perl -w
# -*- Mode: perl; tab-width: 4; indent-tabs-mode: t; -*-
#
# File name:          repoupdate.pl
# Contents:           Git-SVN-Git Express Line
#
# Copyright 2005-2011 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.
#
# Written in 2007 by Teemu Granqvist
# Hacked in 2011 by Peter Krefting
#
# Bits and pieces stolen from other scripts
#

my $cvspath = ""; # Git path
my $svnpath = "";
my $cvsdir = "";
my $svndir = "";
my $path_to_list = "";
my $scopes_or_strings = 0;
my $scopelist = "";

if (!open (PREFS, "<locflow.conf"))
{
	print STDERR "Unable to open locflow.conf\n";
	print STDERR "Please edit locflow.conf.template and save it as locflow.conf in the\n";
	print STDERR "data/translations/ directory.\n";
	exit 1;
}

while (my $prefline = <PREFS>)
{
	if ($prefline =~ m/^cvsdir/) {
		($cvsdir) = $prefline =~ m/^.*=(.*?)\/*?\s/;
	} elsif ($prefline =~ m/^svndir/) {
		($svndir) = $prefline =~ m/^.*=(.*?)\/*?\s/;
	}
}

close (PREFS);

if ($ARGV[2])
{


	if ($ARGV[0] =~ m/^-i$/)
	{
		$scopes_or_strings = 1;
	} elsif ($ARGV[0] =~ m/^-b$/) {
		$scopes_or_strings = 2;
	} elsif ($ARGV[0] =~ m/^-ia$/) {
		$scopes_or_strings = 3;
	} elsif ($ARGV[0] =~ m/^-ba$/) {
		$scopes_or_strings = 4;
	} else {
		die "Invalid parameter $ARGV[1], please select either -i, -b, -ia or -ba\n";
	}

	if ($ARGV[1] =~ m/./)
	{
		$scopelist = $ARGV[1]
	} else {
		die "No scope or string list supplied\n";
	}

	if ($ARGV[2] =~ m/./)
	{
		$path_to_list = $ARGV[2];
	} else {
		die "No language list supplied\n";
	}

} else {
	die "Usage: repoupdate.pl -i|b|a scopes|strings lanugagelist\n";
}

#print "DEBUG:\n$cvsdir\n$svndir\n";

my @all = ();

# Read the list of languages to process
open (LIST, '<', $path_to_list) or die "Cannot open $path_to_list: $!\n";

while (my $line = <LIST>)
{
	if (($line !~ m/^#/) && ($line =~ m/\s/))
	{
		chomp($line);
		if ($line !~ m/^#/)
		{
		push (@all, $line);
		}
	}
}

close (LIST);

# Run update for all the languages in the list
foreach my $po (@all) {
	if ($po =~ m/en\.po$/)
	{
		&makeenglish($po);
	}
	elsif ($po =~ m/\.po$/)
	{
		&copymachine($po);
	} else {
		die "File $po is not a po-file\n";
	}
}

# Copy a file between directories, with fix-ups
sub copyfile
{
	my ($src, $dest) = @_;

	open SRC, '<', $src or die "Cannot read $src: $!\n";
	open DST, '>', $dest or die "Cannot write $dest: $!\n";

	my $linenum = 1;
	while (my $line = <SRC>)
	{

		#Clean up and fix stuff we know goes wrong and can safely fix

		#UTF-8 BOM trips several scripts, so removing it as it is unneccessary
		if (($linenum == 1) && ($line =~ m/^\xEF\xBB\xBF/))
		{
			$line =~ s/^\xEF\xBB\xBF//;
		}

		#Windows and Mac newlines trip some checks, so we convert them to Unix newlines
		$line =~ s/\r\n/\n/;
		$line =~ s/\r/\n/;

		#Sometimes String IDs (reference comments) are duplicated as translator comments, so removing
		#translator comments that look like String IDs
		if ($line !~ m/^# [A-Z|_]+:[0-9]+/)
		{
			print DST $line or &copyerror;
		}
	}

	close SRC;
	close DST;
}


sub copymachine
{
	my ($filepath) = @_;
	#print "DEBUG: $filepath\n";
	my ($langcode, $filename) = $filepath =~ m/(.+)\/(.+)\..+$/;
	print "DEBUG: $langcode, $filename\n";

	# Set up file names
	my $cvspath = "$cvsdir/$langcode/$langcode.po";
	my $svnpath = "$svndir/translations/$langcode/locale/$filename.po";
	my $cvsfinalpath = "$cvsdir/$langcode/$filename.po";
	my $temp_copy_path = "$cvsdir/$langcode/$filename.tmp";
	my $cmdline = "";

	# Copy from Subversion to Git
	print "Copying $svnpath to $cvspath\n";

	&copyfile($svnpath, $cvspath);
	&copyfile($svnpath, $temp_copy_path);

	# Update and copy from Git to Subversion
	# First generate a template (POT) matching the language's scope list
	if ($scopes_or_strings == 1) {
		$cmdline = "perl makepot.pl -i $scopelist";
		print "***\nDEBUG: $cmdline\n***\n";
	} elsif ($scopes_or_strings == 2) {
		$cmdline = "perl makepot.pl -b $scopelist";
		print "***\nDEBUG: $cmdline\n***\n";
	} elsif ($scopes_or_strings == 3) {
		my $scopelist = $langcode . "/$scopelist";
		$cmdline = "perl makepot.pl -i $scopelist";
		print "***\nDEBUG: $cmdline\n***\n";
	} elsif ($scopes_or_strings == 4) {
		my $scopelist = $langcode . "/$scopelist";
		$cmdline = "perl makepot.pl -b $scopelist";
		print "***\nDEBUG: $cmdline\n***\n";
	} else {
		print "Invalid switch selected. Should've been caught already but wasn't, beat up the dev\n";
	}

	#print "DEBUG: $scopelist\n";

	#print $cmdline;
	my $rc = system $cmdline;
	if (($rc >> 8) != 0)
	{
		die "***Fatal Error!***\n$! - $langcode, language not updated\n";
	} else {
		# Check the generated template file for sanity
		open POT, '<', 'en/opera.pot';
		my $dbversion = -1;
		while (<POT>)
		{
			if (/Opera, dbversion ([0-9]+)/)
			{
				$dbversion = 1;
			}
		}
		close POT;
		if ($dbversion > -1)
		{
			# Now update the PO file in Git
			$rc = system "perl merge.pl $langcode";
			if (($rc >> 8) != 0)
			{

				die "***Fatal Error!***\n$! - $langcode, language not updated!\n";
			} else {
				# Copy the updated file from Git to Subversion
				&copyfile($cvspath, $svnpath);
				&copyfile($temp_copy_path, $cvsfinalpath);
				unlink($temp_copy_path);
			}
		} else {
			die "***Fatal Error!\nProblem with .pot file, $langcode not updated!\n";

		}
	}
}

# Create a (fully populated) en.po file from the English database
sub makeenglish
{
	my ($filepath) = @_;
	my $rc = system 'perl makepot.pl -e';
	if (($rc >> 8) != 0)
	{
		die "***Fatal Error!***\n$! - en, language not updated\n";
	}
}
