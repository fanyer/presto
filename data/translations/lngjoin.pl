#!/usr/bin/perl -w
# -*- Mode: perl; tab-width: 4; indent-tabs-mode: t; -*-
#
# File name:          lngjoin.pl
# Contents:           Joins several language files into one, using the platform
#                     section support (FEATURE_EXTENDED_LANGUAGE_FILE)
#
# Copyright © 2006 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

# Handle error
$SIG{'__DIE__'} = \&diehandler;

# External code and variables
require v5.6.0;

# Check command line
if ($#ARGV < 2)
{
	print "Usage: $0 output.lng file1.lng file2.lng [file3.lng]\n";
	print "Joins language files for different platforms.\n\n";
	print "  output.lng    Name of output file.\n";
	print "  file[n].lng   File to join.\n";
	print "\n";
	print "All the language files thare are to be joined must be translations\n";
	print "into the same language, must use the same character encoding, must\n";
	print "be based on the same string database, and must be for different\n";
	print "platforms.\n\n";
	print "The first file specified is treated as the \"main\" translation,\n";
	print "with the other files creating overrides.\n\n";
	print "Generate the language files as usual by using makelang.pl\n";
	exit 1;
}

# Avoid clobbering existing files
my $output = shift;
die "$output already exists.\n" if -e $output;

# Setup
my $fileno = 0;				# Count number of files
my @header;					# Top header from first file
my @platforms;				# List of platforms
my @mainordering;			# Ordering of keys for the first file
my %info;					# Info block
my %maintranslation;		# Translation read from first file
my %comment;				# Comment attached to keys
my %numberofplatforms;		# Count number of platforms string is used in
my %platformtranslation;	# Translations for other platforms
my %platformordering;		# Ordering for other platforms

# Process all the language files
foreach my $lngfile (@ARGV)
{
	open LNG, '<', $lngfile or die "Cannot read $lngfile: $!\n";
	print "Loading $lngfile ...\n";

	# Keep track of where in the file we are. 0 for header, 1 for
	# [Info] block, 2 for [Translation] block.
	my $where = 0;

	# Keep track of comment attached to a string
	my $comment = '';

	# Information for this file
	my $platform = '';
	my %translation;	# Translation read from this file
	my @ordering;		# Ordering of keys for this file
	
	LINE: while (<LNG>)
	{
		chomp;
		$where = 1, next LINE if /^\[Info\]/;
		$where = 2, next LINE if /^\[Translation\]/;

		die "Platform not defined in [Info] block"
			if 2 == $where && $platform eq '';
		
		if (0 == $fileno && 0 == $where)
		{
			# Keep the file header from the first file only
			push @header, $_;
		}
		elsif (1 == $where)
		{
			# Info section
			if (/^([A-Za-z\.]+)=\"?([^"]*)/)
			{
				my ($key, $value) = ($1, $2);
				if (defined $info{$key})
				{
					# Check that the meta-data is identical
					die "Mismatched header $key in $lngfile\n"
						if $info{$key} ne $value;
				}
				else
				{
					$info{$key} = $value;
				}
			}
			if (/^Version\.(.*)=/)
			{
				# Platform name is in the Version header
				$platform = $1;
				foreach my $otherplatform (@platforms)
				{
					die "Platform $platform re-defined by $lngfile\n"
						if $otherplatform eq $platform;
				}
				push @platforms, $platform;
			}
		}
		elsif (2 == $where)
		{
			$comment = '', next LINE if /^; (General|Menu)( message box)? strings/;

			# Translation section
			if (/^; .*$/)
			{
				# Remember comment
				$comment .= $_ . "\n";
			}
			elsif (/^(-?[0-9]+)="(.*)"$/)
			{
				# Regular translated string
				my ($key, $string) = ($1, $2);

				if (0 == $fileno)
				{
					# For the first file, just record all the strings
					$maintranslation{$key} = $string;
					$comment{$key} = $comment;
					$numberofplatforms{$key} = 1;
					push @mainordering, $key;
				}
				else
				{
					# Check if the translation differs from the first
					# file, or if it is new
					if (!defined $maintranslation{$key} ||
					    $maintranslation{$key} ne $string)
					{
						$translation{$key} = $string;
						$comment{$key} = $comment;
						push @ordering, $key;
					}

					# Keep track of strings used on different platforms
					# (even if caption differs)
					++ $numberofplatforms{$key}
						if defined $numberofplatforms{$key};
				}
			}
			elsif (/^$/)
			{
				# Empty line signifies end of comment span
				$comment = '';
			}
		}
	}
	close LNG;

	# Remember the differences
	if ($#ordering >= -1)
	{
		$platformordering{$platform} = \@ordering;
		$platformtranslation{$platform} = \%translation;
	}
	
	# Set up for next file
	++ $fileno;
}

# Create the merged language file
open NEW, '>', $output
	or die "Unable to create $output";
print "Writing $output ...\n";

# Print header
foreach my $header (@header)
{
	if ($header =~ /^; Created/)
	{
		# Update "Created" date
		my @date = localtime;
		$header = sprintf("; Created on %04d-%02d-%02d %02d:%02d",
			$date[5] + 1900, $date[4] + 1, $date[3],
			$date[2], $date[1]);
	}

	print NEW $header, "\n";
}

# Print [Info] block
print NEW "[Info]\n";
foreach my $header ('Language', 'LanguageName', 'Charset',
					map({("Build.$_", "Version.$_")} @platforms), 'DB.version')
{
	die "Header \"$header\" undefined" unless defined $info{$header};
	print NEW "; The string below is the language name in its own language\n"
		if $header eq 'LanguageName';
	my $quote = ($info{$header} =~ /^[0-9\.]+$/) ? '' : '"';
	print NEW $header, '=', $quote, $info{$header}, $quote, "\n";
}
print NEW "\n";

# Handle the strings from the main platform: Start by outputting the
# strings shared with at least one other platform (even with different
# captions), then output the strings specific for it.
my $mainplatform = shift @platforms;
foreach my $section (0, 1)
{
	print NEW "[Translation]\n" if $section == 0;
	print NEW "[$mainplatform]\n" if $section == 1;

	my $previouscomment = '';
	foreach $key (@mainordering)
	{
		my $showinsection = ($numberofplatforms{$key} > 1) ? 0 : 1;
		if ($showinsection == $section)
		{
			# Output comment, if applicable
			if ($previouscomment ne $comment{$key})
			{
				print NEW "\n" if $previouscomment ne '';
				$previouscomment = $comment{$key};
				print NEW $previouscomment if $previouscomment ne '';
			}

			# Output the string
			print NEW "${key}=\"$maintranslation{$key}\"\n";
		}
	}
	print NEW "\n";
}

# Output the platform-specific strings from other platforms
foreach my $platform (@platforms)
{
	my $platformordering = $platformordering{$platform};
	my $platformtranslation = $platformtranslation{$platform};
	
	print NEW "[$platform]\n";

	my $previouscomment = '';
	foreach $key (@{$platformordering})
	{
		# Output comment, if applicable
		if ($previouscomment ne $comment{$key})
		{
			print NEW "\n" if $previouscomment ne '';
			$previouscomment = $comment{$key};
			print NEW $previouscomment if $previouscomment ne '';
		}

		# Output the string
		print NEW "${key}=\"", ${$platformtranslation}{$key}, "\"\n";
	}

	print NEW "\n";
}

close NEW;

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
