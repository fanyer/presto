#!/usr/bin/perl -w
# 
# File name:          merge.pl
# Contents:           Merges a PO file with the translation template.
#
# Copyright (C) 2003-2011 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

require v5.6.0;

# External code and variables
use Getopt::Std;
use vars qw/$opt_r $opt_v $opt_q/;

# File names
$template = 'en/opera.pot';

# Handle error messages on Win32
if ($^O eq 'MSWin32')
{
	$SIG{'__DIE__'} = \&diehandler;
}

# Print banner
$VERSION = &findversion;
print "Opera translation merging tool $VERSION\n";
print "Copyright 2003-2011 Opera Software ASA\n\n";

# Read command line
$Getopt::Std::STANDARD_HELP_VERSION = 1;
unless (getopts('r:vq'))
{
	HELP_MESSAGE();
}

my $verbose = 0;
$verbose ++ if defined $opt_v;
$verbose -- if defined $opt_q;

# Run in interactive mode if no parameters are given
my $input;
if ($#ARGV == -1 && $verbose >= 0)
{
	my $ok = 0;
	while (!$ok)
	{
		print "Please enter PO file to merge: ";
		$input = <STDIN>;
		chomp $input;
		if (-f $input)
		{
			$ok = 1;
		}
		elsif (-f "$input.po")
		{
			$input .= ".po";
			$ok = 1;
		}
		elsif (-f "$input/$input.po")
		{
			$input = "$input/$input.po";
			$ok = 1
		}
		else
		{
			print "That file ($input) seems not to exist.\n";
			print "Please try again.\n";
		}
		if ($input !~ /\.po$/)
		{
			print "The file name must end in \".po\".\n";
			print "Please try again.\n";
			$ok = 0;
		}
	}
}
else
{
	die "No input files given\n" if $#ARGV == -1;
	$input = shift;
	if (! -f $input)
	{
		$input = "$input.po" if -f "$input.po";
		$input = "$input/$input.po" if -f "$input/$input.po";
	}	
}

# Check for BOM mark in PO file
open INPUT, '<', $input
	or die "Cannot read $input: $!\n";
read INPUT, $preamble, 3;
if ($preamble eq "\xEF\xBB\xBF")
{
	print "Stripping UTF-8 BOM from $input\n"
		if $verbose >= 0;

	# Read the rest of the file (sans BOM)
	my @input = <INPUT>;
	close INPUT;

	# Re-write the file in-place
	open REWRITE, '>', $input
		or die "Cannot overwrite $input: $!\n";
	print REWRITE @input;
	close REWRITE;

	open INPUT, '<', $input
		or die "Cannot read rewritten $input: $!\n";
}
elsif (substr($preamble,0,2) eq "\xFE\xFF" ||
       substr($preamble,0,2) eq "\xFF\xFE" ||
       substr($preamble,0,1) eq "\x00" ||
       substr($preamble,1,1) eq "\x00")
{
	die "$input is UTF-16\n";
}

# Check if the translation uses the old-style num::string format in msgid
my $findctxt = 0;
FINDCTXT: while (my $line = <INPUT>)
{
	if ($line =~ /^msgctxt/)
	{
		# Already in new style, skip conversion
		$findctxt = 1;
		close INPUT;
		last FINDCTXT;
	}
	elsif ($line =~ m'^(msgid )?"-?[0-9]+::')
	{
		# Old-style, rewrite input using msgctxt, making a backup copy
		# first.
		$findctxt = 2;
		close INPUT;

		# Rename to a backup file
		my $backup = $input . '.bak';
		print "Converting $input to use msgctxt. Backing up original to $backup\n";
		rename $input, $backup
			or die "Cannot rename $input to $backup: $!\n";
		open OLD, '<', $backup
			or die "Cannot read back renamed $backup: $!\n";
		open NEW, '>', $input
			or die "Cannot rewrite $input: $!\n";
		my $ctxt = '';
		while (my $poline = <OLD>)
		{
			$ctxt = '' if $poline =~ /^$/;
			if ($poline =~ /#: ([SDM].*):/)
			{
				$ctxt = $1;
				print NEW $poline;
			}
			elsif ($poline =~ /^(.*)msgid ".*?::(.*)"/)
			{
				# Add msgctxt
				print NEW qq'$1msgctxt "$ctxt"\n' if $ctxt ne '';
				$ctxt = '';
				print NEW qq'$1msgid "$2"\n';
			}
			elsif ($poline =~ /^(.*)msgid ""/)
			{
				# Add msgctxt
				print NEW qq'$1msgctxt "$ctxt"\n' if $ctxt ne '';
				$ctxt = '';
				# Find the next line and possibly strip id from it
				print NEW $poline;
				my $poline = <OLD>;
				if ($poline =~ /(.*)".*?::(.*)"/)
				{
					print NEW qq'$1"$2"\n';
				}
				else
				{
					print NEW $poline;
				}
			}
			elsif ($poline =~ /^# This is an Opera translation file in \.po format\. Each msgid starts with a/)
			{
				print NEW "# This is an Opera translation file in .po format.\n";
			}
			elsif ($poline =~ /# unique identifier number\. This number should \*NOT\* be translated - eg\./ ||
			       $poline =~ /# "12345::Foo" should be translated to "Bar", not "12345::Bar"\./)
			{
				# Skip this line.
			}
			else
			{
				print NEW $poline;
			}
		}
		close OLD;
		close NEW;

		last FINDCTXT;
	}
}

if (0 == $findctxt)
{
	print "Warning: Cannot determine format of PO file, continuing anyway...\n";
}

# Retrieve specific opera.pot file if requested
if (defined $opt_r)
{
	print "Retrieving en/opera.pot revision $opt_r\n"
		if $verbose > 0;

	# Sanity check
	die '"HOME" environment variable not configured!' unless defined $ENV{'HOME'};

	# Run CVS or Git to get a file
	$template = 'tmp.opera.' . $$ . '.pot';
	unlink $template if -e $template;
	open TMP, '>' . $template
		or die "Cannot create $template: $!";
	if (-d 'CVS')
	{
		open CVS, qq'cvs up "-r$opt_r" -p en/opera.pot|'
			or die "Cannot run cvs command: $!\n";
	}
	else
	{
		die "-r: Cannot use CVS revisions with Git: $opt_r\n"
			if $opt_r =~ /^1\./;
		open CVS, qq'git show $opt_r:en/opera.pot|'
			or die "Cannot run git command: $!\n";
	}
	while (<CVS>)
	{
		print TMP;
	}
	close CVS
		or die $! ? "Error running cvs: $!\n"
		          : "Exit status $? from cvs\n";
	close TMP;

	print "Merging into revision $opt_r of en/opera.pot\n"
		if $verbose >= 0;
}
else
{
	print "Merging into current en/opera.pot\n"
		if $verbose >= 0;
}

# Run msgmerge
open MERGE, qq'msgmerge --previous --no-wrap "$input" "$template"|'
	or die "Cannot run msgmerge: $!\n";
@newfile = <MERGE>; # Slurp entire file
close MERGE
	or die $! ? "Error running msgmerge: $!\n"
	          : "Exit status $? from msgmerge\n";

# Load dbversion from POT file
if (open POT, '<', $template)
{
	my $dbversion = 0;
	READ: while (<POT>)
	{
		if (/Project-Id-Version: Opera, dbversion ([0-9]+)\\/)
		{
			$dbversion = $1;
			last READ;
		}
	}
	close POT;

	if ($dbversion)
	{
		# Update dbversion in created PO file
		MODIFY: foreach (@newfile)
		{
			if (/Project-Id-Version/)
			{
				$_ = qq'"Project-Id-Version: Opera, dbversion $dbversion\\n"\n';
				last MODIFY;
			}
		}
	}
}

# Remove old PO file
unlink $input or die "Cannot delete $input: $!\n";

# Rewrite PO file
print "Rewriting $input\n" if $verbose >= 0;
open NEWPO, ">$input"
	or die "Cannot create new $input: $!\n";
print NEWPO @newfile; # Now that was simple, huh?
close NEWPO;

# Remove temporary POT file
if (defined $opt_r)
{
	unlink $template or die "Cannot delete $template: $!\n";
}

print "Done.\n" if $verbose >= 0;
if ($^O eq 'MSWin32' && $verbose >= 0)
{
	print "Press <Enter> to exit:";
	my $notused = <STDIN>;
}

sub diehandler
{
	print shift;
	print "Press <Enter> to exit:";
	my $notused = <STDIN>;
}

sub HELP_MESSAGE
{
	print "Usage: $0 [-r rev] [-v|-q] po-file\n";
	print "Updates a translation using the opera.pot translation template.\n\n";
	print "  -r rev    Use this CVS/Git revision of opera.pot instead of current.\n";
	print "  -v | -q    Be verbose or be quiet\n";
	print "  po-file   Name of PO file to update.\n";
	exit 1;
}

# Find self version
# Needed for --version
sub findversion
{
	my $path = $0;
	$path =~ s/merge\.pl//;
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
