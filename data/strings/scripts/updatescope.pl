#!/usr/bin/perl -w
# -*- Mode: perl; tab-width: 4; indent-tabs-mode: t; -*-
#
# File name:          updatescope.pl
# Contents:           Reads a string list file (build.strings) and updates
#                     the corresponding scope in the english.db to contains
#                     the strings (and only the strings) listed there.
#
# Copyright © 2007 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

use strict;

# Check command line
if ($#ARGV != 1)
{
	print "Usage: $0 stringlistfile scope\n";
	print "Updates a scope from a string list.\n\n";
	print "  stringlistfile   The build.strings file to read.\n";
	print "  scope            The scope to update.\n";
	exit 1;
}

# Get command line parameters
my ($stringfile, $scope) = @ARGV;

# Set up directories
my $subdir = '';
$subdir = 'strings/' if -e 'strings/scripts/updatescope.pl';
$subdir = 'data/strings/' if -e 'data/strings/scripts/updatescope.pl';
$subdir = '../' if -e './updatescope.pl';
my $englishdb = $subdir . 'english.db';
die "Cannot locate $englishdb\n" unless -e $englishdb;

# Read string list
my %stringlist;
my $count = 0;
open STRINGS, "<$stringfile"
	or die "Cannot open file $stringfile: $!\n";
while (<STRINGS>)
{
	next if /^$/;
	next if /^#/;
	chomp;
	s/^\s+//;
	s/\s+$//;
	die "Duplicate entry in $stringfile: $_" if defined $stringlist{$_};
	$stringlist{$_} = 1;
	++ $count;
}
close STRINGS;
print "$count strings read from $stringfile\n";

# Read and update database
my ($deleted, $added) = (0, 0);
open DB, "<$englishdb"
	or die "Cannot open file $englishdb: $!\n";
open DBNEW, ">${englishdb}.new"
	or die "Cannot create file ${englishdb}.new: $!\n";
while (my $dbline = <DB>)
{
	# Look for scope definitions, ignore everything else
	if ($dbline =~ /^([A-Za-z0-9_]+)\.scope="(.*)"/)
	{
		# Parse entry
		my $id = $1;
		my @scopes = split(/,/, $2);

		# Check if string was listed
		if (defined $stringlist{$id})
		{
			# Check that the scope is in @scopes
			if (!grep { $_ eq $scope } @scopes)
			{
				# No, add it
				push @scopes, $scope;
				++ $added;
			}
		}
		else
		{
			# Remove the scope from @scopes, if listed.
			my $oldcount = $#scopes;
			@scopes = grep { $_ ne $scope } @scopes;
			$deleted += $oldcount != $#scopes;
		}

		# Re-assemble db line
		$dbline = $id . '.scope="' . join(',', @scopes) . qq'"\n';
	}

	# Copy data over to the new file
	print DBNEW $dbline;
}
close DB;
close DBNEW;

print $added + $deleted, " db entries modified";
print " (" if $added + $deleted;
print "$added added" if $added;
print ", " if $added && $deleted;
print "$deleted deleted" if $deleted;
print ")" if $added + $deleted;
print "\n";

# Replace database if there was any changes
if ($added + $deleted)
{
	unlink $englishdb
		or die "Cannot remove old $englishdb: $!\n";
	rename "${englishdb}.new", $englishdb
		or die "Cannot move ${englishdb}.new to replace $englishdb: $!\n";
	print "$englishdb updated\n";
}
else
{
	unlink "${englishdb}.new"
		or die "Cannot remove ${englishdb}.new: $!\n";
	print "$englishdb not updated\n";
}
