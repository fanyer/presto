#!/usr/bin/perl -w
#
# File name:          updatedb.pl
# Contents:           Simple script to add locale strings to the database
#
# Copyright © 2002-2011 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

require v5.6.0;

# Handle error
$SIG{'__DIE__'} = \&diehandler;

# Global variables
my @date = gmtime;

# Greet user
chomp($rev = `git show-ref -s --abbrev HEAD`);
print "Opera string database update tool $rev\n";
print "Copyright 2002-2011 Opera Software ASA\n\n";

my $highest_id_found = -1;

my $srcdir = '';
$srcdir = '.' if -e 'english.db';
$srcdir = '..' if -e '../english.db';
die "Unable to find english.db\n" if $srcdir eq '';

if ($#ARGV >= 0 && $ARGV[0] eq 'core-1')
{
	# Load current database to find first available ID number
	open DB, '<:utf8', "${srcdir}/english.db" or die "Cannot read ${srcdir}/english.db: $!\n";

	print "Reading database...\n";
	$highest_id_found = -1; # Highest number (total)
	while (<DB>)
	{
		if (/^[^.]*\s*=\s*([0-9]+)$/)
		{
			if ($1 > $highest_id_found && $1 < 100000)
			{
				$highest_id_found = $1;
			}
		}
	}
	close DB;

	# Print this information
	print 'Highest current id found in database: ', $highest_id_found, "\n";
}
else
{
	print "The added strings will NOT be assigned numbers for core-1 compatibility.\n";
	print "To do so, please re-run this script with the command line\n";
	print "  $0 core-1\n";
}

$current_id = $highest_id_found  + 1;
@newdialog = ();
@newstrings = ();
@newmenu = ();
$somethingtoadd = 0;
INPUT: while (1)
{
	# Get string section
	print "\nWhat section should the string be added to?\n";
	print "* D = Dialog title and components\n";
	print "* S = General strings\n";
	print "* M = Menu strings\n";
	print "Enter X to exit\n";
	my $ok = 0;
	my $section;
	while (!$ok)
	{
		print "> ";
		$section = uc(<STDIN>);
		chomp $section;
		last INPUT if $section eq 'X';
		$ok = 1 if $section eq 'D' or $section eq 'S' or $section eq 'M';
	}

	# Get enum name
	$ok = 0;
	my $enum;
	while (!$ok)
	{
		print "\nEnter enum name of new string (without ", $section,
		      "_ prefix):\n> ";
		$enum = uc(<STDIN>);
		chomp $enum;
		$enum =~ tr/a-z /A-Z_/;
		$ok = 1 if $enum =~ /^[A-Z0-9_]+$/;
		print qq'"$enum" does not look like a valid enumeration name to me.\n'
			unless $ok;
	}

	# Get string caption
	print "\nEnter the caption of this string:\n> ";
	my $caption = <STDIN>;
	chomp $caption;

	# Replace "..." with unicode ellipsis
	$caption =~ s/\.{3,}/\x{2026}/g;

	# Get string comment
	print "\nEnter a comment for this string (Enter to leave blank, not recommended):\n> ";
	my $comment = <STDIN>;
	chomp $comment;

	# Get string scope
	print qq'\nEnter a comma separated scope parameter for this string (e.g "mac,windows").\n';
	print "See documentation for details on the exact format.\n> ";
	my $scope = <STDIN>;
	chomp $scope;

	# Build strings for addition to database
	my $completename = $section . '_' . $enum;
	print "\n", $completename, " = ", $current_id ? $current_id : -1, "\n";

	$somethingtoadd = 1;
	my $dbstring = '';
	$dbstring .= $completename . '=' . ($current_id ? $current_id : -1) . "\n";
	$dbstring .= $completename . '.caption="' . $caption . qq'"\n';
	$dbstring .= $completename . '.scope="' . $scope . qq'"\n';
	$dbstring .= $completename . '.description="' . $comment . qq'"\n'
		if $comment ne '';

	push @newdialog,  $dbstring if $section eq 'D';
	push @newstrings, $dbstring if $section eq 'S';
	push @newmenu,    $dbstring if $section eq 'M';

	$current_id ++ if $current_id;
}

# Write database if changes are made
if ($somethingtoadd)
{
	print "Updating database...\n";

	open OLD, '<:utf8', "${srcdir}/english.db"
		or die "Cannot read ${srcdir}/english.db: $!\n";
	open NEW, '>:utf8', "${srcdir}/english.db.new"
		or die "Cannot create english.db.new: $!\n";

	my $insection = 0;
	my $donesections = 0;

	while (<OLD>)
	{
		if (/^\[/)
		{
			# Locate section starts
			$insection = 1 if /Dialog title/;
			$insection = 2 if /General strings/;
			$insection = 3 if /Menu strings/;

			# Copy header
			print NEW;
		}
		elsif ($insection && /^(\.end)?$/)
		{
			# End of section, add any pending strings
			if (1 == $insection)
			{
				foreach $newdlgstring (@newdialog)
				{
					print NEW $newdlgstring;
				}
				++ $donesections;
			}
			elsif (2 == $insection)
			{
				foreach $newstring (@newstrings)
				{
					print NEW $newstring;
				}
				++ $donesections;
			}
			elsif (3 == $insection)
			{
				foreach $newmenustring (@newmenu)
				{
					print NEW $newmenustring;
				}
				++ $donesections;
			}

			# We're not in Kansas anymore
			$insection = 0;
			print NEW;
		}
		elsif (/^; Copyright/)
		{
			# Always update copyright line
			print NEW "; Copyright © 1995-", $date[5] + 1900,
			          " Opera Software ASA. All rights reserved.\n";
		}
		else
		{
			# Copy anything else verbatim
			print NEW;
		}
	}
	close OLD;
	close NEW;

	die "Unable to find end of section $insection\n" if $insection;
	die "Unable to insert all sections (missing newline between sections?)\n"
		if 3 != $donesections;
	
	unlink "${srcdir}/english.db" or die "Cannot delete old english.db: $!\n";
	rename "${srcdir}/english.db.new", "${srcdir}/english.db"
		or die "Cannot rename new english.db: $!\n";

	print "\nUpdate finished successfully.\n";
	print "Remember to run makelang.pl to check your changes.\n";
	print "Commit the changes to your work branch and send a patch request to the module\n";
	print "owner if you want these strings to be part of an official release.\n";
}
else
{
	print "No changes made, not updating database...\n";
}

if ($^O eq 'MSWin32')
{
	print "Press <Enter> to exit:";
	my $notused = <STDIN>;
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

	exit 0;
}

