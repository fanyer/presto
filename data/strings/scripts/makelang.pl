#!/usr/bin/perl -w
# -*- Mode: perl; tab-width: 4; indent-tabs-mode: t; coding: iso-8859-1; -*-
#
# File name:	makelang.pl
# Contents:		Extracts strings from the database to create header
#				files for the locale module, *.lng files, translation
#				templates, as well as other file formats.
#
# Copyright (C) 2002-2012 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.
#
# NB! Whenever you make changes to this script, please make sure there is
# a valid test case for the part you modified in the selftest.pl script
# (add a new test if there isn't one), and make sure all tests pass after
# you are done with your modifications. If your change does indeed change
# the format of the output files, please modify the selftest reference
# output accordingly.
#
# Written and maintained by Peter Krefting (peter@opera.com)
# Symbian specific changes made by Daniel Lehtovirta (daniell@opera.com)
# Symbian Series 60 specific changes made by Per Johan Groland (perjg@opera.com)
# Makefile support made by Magne Zachrisen (magnez@opera.com)

# Handle error
$SIG{'__DIE__'} = \&diehandler;

# Keep a copy of the command line before getopts changes it, so that we
# can record it verbatim if we create a makefile.
my $commandline = join (' ', ($0, @ARGV));

# External code and variables
use Getopt::Std qw/getopts/;
use vars qw/$opt_i $opt_p $opt_t $opt_o $opt_l $opt_v $opt_q $opt_r $opt_k
			$opt_s $opt_L $opt_c $opt_d $opt_S $opt_T $opt_C $opt_u $opt_H
			$opt_P $opt_R $opt_b $opt_B $opt_m $opt_E $opt_U $opt_e/;
use integer;
use FindBin;
use lib $FindBin::Bin;
use po;
use Text::Wrap qw/wrap/;
use strict;

# Initialize global parameters.

# All available scopes should be listed here, with the same spelling as is
# used in english.db.
%main::scope = (
	# Common UI
	ui		=> 0,	# Include Quick UI strings
	infopanel	=> 0,	# Infopanel strings (Linux SDK uses these, but not ui)

	# Platforms
	windows		=> 0,	# Include Windows strings
	qt		=> 0,	# Include Qt strings
	mac		=> 0,	# Incluce Mac strings
	# much of what the following (and qt) want should be feature-controlled
	bream		=> 0,	# Do not include Bream strings
	brew		=> 0,	# Do not include Brew strings
	brew_att	=> 0,	# Do not include Brew-AT&T strings
	brew_kddi	=> 0,	# Do not include Brew-KDDI strings
	brew_generic	=> 0,	# Do not include Brew Generic product strings
	kyocera		=> 0,	# Do not include Kyocera strings
	symbian		=> 0,	# Do not include Symbian strings
	series60ui	=> 0,	# Do not include Symbian Series 60 strings
	nanox		=> 0,	# Do not include NanoX strings
	ezx		=> 0,	# Do not include Ezx strings
	chameleon	=> 0,	# Do not include Chameleon strings
	juix		=> 0,	# Do not include Juix strings
	pp		=> 0,	# PowerParts
	winmobile	=> 0,	# Do not include Windows Mobile strings
	nfl		=> 0,	# Do not include Newfoundland strings
	nitro		=> 0,	# Do not include nitro strings
	hokkaido	=> 0,	# Do not include hokkaido strings
	candlebox	=> 0,	# Do not include Candlebox strings
	wince		=> 0,	# Do not include WinCE strings
	phantom		=> 0,	# Do not include Phantom strings
	victoria	=> 0,	# Do not include Victoria strings
	alba		=> 0,	# Do not include Alba strings
	bazarek		=> 0,	# Do not include Bazarek strings
	mobemulator	=> 0,	# Do not include Opera Mobile Emulator strings

	# Core components
	base			=> 0,	# Strings always needed by core code
	coremswin		=> 0,	# #ifdef #MSWIN in core code, maybe should be "windows" scope?
	corequick		=> 0,	# #ifdef #QUICK in core code
	java			=> 0,	# FEATURE_JAVA_*
	fileupload		=> 0,	# FEATURE_FILE_UPLOAD
	externalapps	=> 0,	# #ifdef NO_EXTERNAL_APPLICATIONS
	wml				=> 0,	# FEATURE_WML
	ssl				=> 0,	# FEATURE_SSL
	ftp				=> 0,	# FEATURE_FTP
	ecmascript		=> 0,	# FEATURE_ECMASCRIPT
	operaurl		=> 0,	# FEATURE_URL_OPERA
	xml				=> 0,	# FEATURE_XML
	prefsdownload	=> 0,	# FEATURE_PREFS_DOWNLOAD
	webforms2		=> 0,	# FEATURE_WEBFORMS2
	ssp				=> 0,	# USER_PROFILE_SUPPORT
	usercss			=> 0,	# FEATURE_LOCAL_CSS_FILES
	jil			=> 0,	# FEATURE_DOM_JIL_API

	# Other Components
	m2			=> 0,	# Include m2 strings
	im			=> 0,	# Do not include IM strings
	voice		=> 0,	# Include voice strings
	cbs			=> 0,	# Strings for Opera Platform-Live!Cast-CBS
	bt			=> 0,	# Include BitTorrent strings
	momail		=> 0,	# Strings for My Opera Mail. Not for use in .lng-files.
	dragonfly	=> 0,	# Strings for Dragonfly. Not for use in .lng-files.
	wininstaller	=> 0,	# Strings for the windows installer. Not for use in .lng-files.
	miniclient	=> 0,	# Strings for the Opera Mini client. Not for use in .lng-files.
	miniserver	=> 0,	# Strings for the Opera Mini server. Not for use in .lng-files.
	operacom	=> 0,	# Strings for m.opera.com. Not for use in .lng-files.
	ironmaiden	=> 0,	# Strings for addons.opera.com. Not for use in .lng-files.
	tvstore		=> 0,	# Strings for addons.opera.com. Not for use in .lng-files.
	internal	=> 0,	# Strings we don't want to show up public builds yet.
	sdk		=> 0,	# Strings for the Opera SDK

	# Deprecated (see README.html)
	unsorted	=> 0,	# Unsorted, possibly unused (in core-1 anyway), strings
	obsolete	=> 0,	# Strings not referenced in code (script, nov 2004)
);

# Scopes that are not to be included in the locale-enum.inc are to be listed
# here. Any scope that is not used by code that is compiled into the main
# Opera executable on any platform should be listed here.
%main::killscope = (
	wininstaller	=> 1,
	miniclient		=> 1,
	miniserver		=> 1,
	obsolete		=> 1,
);

my $highest = -1; # Highest number (total)
my $current_compatibility_version = 9; # Default for -c parameter
$Text::Wrap::columns = 75;
my $need_binmode = 0;
my @generatedfiles = (); # Populated with list of generated files
my @temporaryfiles = (); # Populated with list of temporary files
my %stringlist = (); # Populated with string list if -b is given

# If using a UTF-8 locale, we need to tell Perl not to try to translate
# input to/from UTF-8. The ":bytes" binmode translation is only available
# from Perl 5.8, though.
if ((defined $ENV{'LC_ALL'} && $ENV{'LC_ALL'} =~ /utf/i) ||
	(defined $ENV{'LC_CTYPE'} && $ENV{'LC_CTYPE'} =~ /utf/i) ||
	(defined $ENV{'LANG'} && $ENV{'LANG'} =~ /utf/i))
{
	$need_binmode = 1;
	eval { require 5.008; };
}

# Read command line
$main::VERSION = &findversion;
$Getopt::Std::STANDARD_HELP_VERSION = 1;
$Getopt::Std::STANDARD_HELP_VERSION = 1; # Duplicate to avoid run-time warning
unless (getopts('i:b:Bnp:t:o:l::vqr:sLc:d:S:T:CkuHPR:mEUe'))
{
	HELP_MESSAGE();
}

# Set up directories
# Source directory
my $subdir = '';
$subdir = 'master/' if -e 'master/makelang.pl';
$subdir = 'strings/' if -e 'strings/scripts/makelang.pl';
$subdir = 'data/localedata/' if -e 'data/localedata/makelang.pl';
$subdir = 'data/strings/' if -e 'data/strings/scripts/makelang.pl';
$subdir = '../' if -e './makelang.pl';
if (defined $opt_S)
{
	$subdir = $opt_S;
	$subdir .= '/' unless substr($subdir, -1, 1) eq '/';
}

# Destination directory
my $destdir = $subdir;
if (defined $opt_d)
{
	die "-d: \"$opt_d\" does not exist\n" unless -d $opt_d;
	$destdir = $opt_d;
	$destdir .= '/' unless substr($destdir, -1, 1) eq '/';
}

# If there is a generated Makefile, use it to create the language file.
# But make sure we do not recurse forever.
my $mkfilename = "${destdir}LocaleMakefile.mak";
if (defined $opt_m && !defined $ENV{'CALLED_THROUGH_MAKEFILE'} && -e $mkfilename)
{
	$ENV{'CALLED_THROUGH_MAKEFILE'} = 1;
	my $cmd = "make -f $mkfilename";
	print $cmd . "\n" unless $opt_q;
	system($cmd);
	exit(0);
}

# Check for database files
my @databases;
if ($#ARGV == -1)
{
	# Standard database file
	push @databases, "${subdir}english.db";

	# Sanity check
	if (!-e "${subdir}english.db")
	{
		die "${subdir}english.db not found, please use -S.\n"
			unless defined $opt_S;
		die "english.db not found in $subdir specified by -S.\n";
	}
}
else
{
	# Specified database file(s)
	die "-r cannot be combined with custom databases" if defined $opt_r;
	die "-S cannot be combined with custom databases" if defined $opt_S;

	foreach my $database (@ARGV)
	{
		die "$database (specified on command line) not found.\n"
			if !-e $database;
	}
	push @databases, @ARGV;
}

# Check parameters
my $verbose = 0;
$verbose ++ if defined $opt_v;
$verbose -- if defined $opt_q;

# Make sure all scopes are disabled.
foreach my $scope (keys %main::scope)
{
	$main::scope{$scope} = 0;
}

# Prefer fetching string list from build.strings
if (defined $opt_b)
{
	# Cannot use build.strings and the old scopes at the same time
	die "-b and -i cannot be combined!\n" if defined $opt_i;

	# Read string list from file
	print "Loading $opt_b..." if $verbose > 0;
	my $count = 0;
	open STRINGS, "<$opt_b"
		or die "Cannot open file $opt_b: $!\n";
	while (<STRINGS>)
	{
		next if /^$/;
		next if /^#/;
		# remove CR LF or LF or CR safely even if a DOS file is
		# parsed on a Unix system or the other way round:
		s/\015?\012?$//;
		s/^\s+//;
		s/\s+$//;
		die "Duplicate entry in $opt_b: $_" if defined $stringlist{$_};
		$stringlist{$_} = 1;
		++ $count;
	}
	close STRINGS;
	die "No strings found in $opt_b" if !$count;
	print "  $count strings found\n" if $verbose > 0;
}
# Else use a scope list
elsif (defined $opt_i)
{
	# Read scope list from file if one was given on the command line
	if (-f $opt_i)
	{
		open SCOPE, "<$opt_i"
			or die "Cannot open file $opt_i: $!\n";
		# remove CR LF or LF or CR safely even if a DOS file is
		# parsed on a Unix system or the other way round:
		my @scopelist = map { s/\015?\012?$//; $_ } <SCOPE>;
		close SCOPE;
		$opt_i = join(',', @scopelist);
	}

	# Then enable selected scopes
	foreach my $scope (split(/,/, $opt_i))
	{
		if ($scope eq 'ALL')
		{
			# Enable everything except for "obsolete" if we say "ALL"
			foreach my $enable (keys %main::scope)
			{
				$main::scope{$enable} = 1 unless $enable eq 'obsolete';
			}
		}
		else
		{
			die "Undefined scope referenced in -i: $scope\n"
				unless defined $main::scope{$scope};
			$main::scope{$scope} = 1;
		}
	}
}

if ((defined $opt_P || defined $opt_e) && !defined $opt_i && !defined $opt_b)
{
	# Include all scopes by default when generating PO file
	foreach my $enable (keys %main::scope)
	{
		$main::scope{$enable} = 1 unless $enable eq 'obsolete';
	}
}

if (defined $opt_H)
{
	if (defined $opt_L)
	{
		# $opt_L and $opt_H can only be combined when using -l series60ui
		unless (defined $opt_l && $opt_l eq 'series60ui')
		{
			die "-L and -H cannot be combined!\n";
		}
	}
	if (defined $opt_l)
	{
		# $opt_L and $opt_H can only be combined when using -l series60ui
		unless (defined $opt_l && $opt_l eq 'series60ui')
		{
			die "-l and -H cannot be combined!\n";
		}
	}
	if (defined $opt_p)
	{
		die "-p and -H cannot be combined!\n";
	}
	if (defined $opt_i)
	{
		# $opt_i and $opt_H can only be combined when using -l series60ui
		unless (defined $opt_l && $opt_l eq 'series60ui')
		{
			die "-i and -H cannot be combined!\n";
		}
	}
	if (defined $opt_t)
	{
		# $opt_t and $opt_H can only be combined when using -l series60ui
		unless (defined $opt_l && $opt_l eq 'series60ui')
		{
			die "-t and -H cannot be combined!\n";
		}
	}
}

if (defined $opt_P || defined $opt_e)
{
	# Cannot do much else when generating the opera.pot/en.po file
	die "-P/-e and -l cannot be combined" if defined $opt_l;
	die "-P/-e and -L cannot be combined" if defined $opt_L;
	die "-P/-e and -H cannot be combined" if defined $opt_H;
	die "-P/-e and -p cannot be combined" if defined $opt_p;
	die "-P/-e and -t cannot be combined" if defined $opt_t;
	die "-P/-e and -o cannot be combined" if defined $opt_o;
	die "-P/-e and -s cannot be combined" if defined $opt_s;
	die "-P/-e and -C cannot be combined" if defined $opt_C;
	die "-P/-e and -k cannot be combined" if defined $opt_k;
	die "-P and -e cannot be combined" if defined $opt_P && defined $opt_e;

	# Make sure we always use the latest format towards translators,
	# this requires matching updates to the makepot.pl, if used.
	if (defined $opt_c)
	{
		die "-P/-e requires \"-c${current_compatibility_version}\""
			if $opt_c != $current_compatibility_version;
	}
	else
	{
		$opt_c = $current_compatibility_version;
	}
}

my ($targetplatform, $targetversion, $targetbuild);
if (defined $opt_p)
{
	($targetplatform, $targetversion, $targetbuild) = split /,/, $opt_p;
	if ($targetplatform !~ /^[A-Z][a-z][a-z]/)
	{
		die "-p: \"$targetplatform\" does not look like a platform name\nExpected format: Capital letter followed by at least two lower-case letters\n";
	}
	if (!defined $targetversion || !defined $targetbuild)
	{
		die "-p: \"$opt_p\" must be on the form platform,version,build\n";
	}
	if ($targetversion !~ /^[0-9]+(\.[0-9]+)*/)
	{
		die "-p: \"$targetversion\" does not look like a version number\nExpected format: <digits> <full stop> <digits>\n";
	}
	if ($targetbuild !~ /^[\w-]+$/)
	{
		die "-p: \"$targetbuild\" does not look like a build number.\nThe number must consist entirely of a combination of alphanumeric (plus '_' and '-') characters.\n";
	}
}

# Check for consistency
if (defined $opt_p && !defined $opt_i && !defined $opt_b)
{
	print "Warning: You should specify -b when using -p\n"
		unless defined $opt_P;
}

if ((defined $opt_l || defined $opt_L) && !defined $opt_t)
{
	# Generating a *.lng file (-l/-L) without specifying a translation (-t).
	die "You must specify -t when using -l or -L\n";
}

if (defined $opt_t && !(defined $opt_p || defined $opt_l))
{
	die "You must specify either -p or -l when using -t\n";
}

if (defined $opt_T && !defined $opt_t)
{
	die "-T requires -t\n";
}

if (defined $opt_R && !defined $opt_t)
{
	die "-R requires -T\n";
}

if (defined $opt_c)
{
	# Check compatibility version.
	$opt_c = int($opt_c);
	# Currently only compatibility version 9 (core-2) is allowed.
	if ($opt_c < 9 || $opt_c > 9)
	{
		die "Illegal value for -c\n";
	}
}
else
{
	print "Warning: Assuming compatibility level $current_compatibility_version (-c $current_compatibility_version).\n";
	$opt_c = $current_compatibility_version;
}

if (defined $opt_m && !defined $opt_t)
{
	die "-m requires -t\n";
}

if ($opt_c >= 9 && defined $opt_i && !(defined $opt_P || defined $opt_e))
{
	die "-i is only allowed with -P or -e; use -b when building localized versions\n";
}

# Check scopes to include
unless ($opt_b)
{
	if (!defined $opt_i && !(defined $opt_P || defined $opt_e))
	{
		die "You need to specify strings to include (-b).\n";
	}

	my $count = 0;
	print "These scopes will be included:\n  " if $verbose > 0;
	foreach my $scope (sort keys %main::scope)
	{
		if ($main::scope{$scope})
		{
			print "$scope " if $verbose > 0;
			$count ++
		}
	}
	print "(none)" if $verbose > 0 && $count == 0;
	print "\n" if $verbose > 0;
	die "No strings included in output. Check -b or -i parameters.\n" if $count == 0;
}

# Read the databases
my ($dbversion, $idarray, $numhash, $hashnumhash, $captionhash, $descriptionhash,
	$scopehash, $sectionhash, $cformathash);
if (defined $opt_r)
{
	# Read a specific database version from CVS or Git
	print "Opening english.db revision $opt_r\n" if $verbose > 0;
	if (-d "${subdir}CVS")
	{
		open DB, "cvs -q update -p -r$opt_r ${subdir}english.db|"
			or die "Unable to run cvs command: $!\n";
	}
	else
	{
		die "-r: Cannot use CVS revisions with Git: $opt_r\n"
			if $opt_r =~ /^1\./;
		open DB, "git show $opt_r:data/strings/english.db|"
			or die "Unable to run git command: $!\n";
	}
	if ($need_binmode)
	{
		binmode DB, ":bytes";
	}

	($dbversion, $idarray, $numhash, $hashnumhash, $captionhash, $descriptionhash,
	 $scopehash, $sectionhash, $cformathash)
		= &parsedb(\*DB, "english.db:$opt_r");

	close DB or
		die $! ? "Problems while running cvs command: $!\n" : "Exit status $? from cvs\n";
}
else
{
	# Read and join (potentially) several databases
	foreach my $database (@databases)
	{
		print "Opening $database\n" if $verbose > 0;
		open DB, "<$database"
			or die "Cannot open $database: $!\n";
		if ($need_binmode)
		{
			binmode DB, ":bytes";
		}

		my ($thisdbversion, $thisidarray, $thisnumhash, $thishashnumhash,
			$thiscaptionhash, $thisdescriptionhash, $thisscopehash,
			$thissectionhash, $thiscformathash)
			= &parsedb(\*DB, $database);

		close DB;

		if (-1 == $#{$idarray})
		{
			# Initial database, just copy contents over
			($dbversion, $idarray, $numhash, $hashnumhash, $captionhash, $descriptionhash,
			 $scopehash, $sectionhash, $cformathash) =
				($thisdbversion, $thisidarray, $thisnumhash, $thishashnumhash,
				 $thiscaptionhash, $thisdescriptionhash, $thisscopehash,
				 $thissectionhash, $thiscformathash);
		}
		else
		{
			# Sanity check
			die "DBversion in $database is $thisdbversion, should be $dbversion"
				if $thisdbversion != $dbversion;

			# Join with current database, merging the various sections
			my @newidarray;
			my $currentsection = substr(${$idarray}[0], 0, 1);
			JOIN: for (my $i = 0; $i <= $#{$idarray} + 1; ++ $i)
			{
				# First add all the entries from the previous database
				# to the array, up until we encounter a new section,
				# or the end of the list
				if ($i == $#{$idarray} + 1 ||
					substr(${$idarray}[$i], 0, 1) ne $currentsection)
				{
					# Now push the list of new identifiers from the
					# newly read database, and copy its data over
					# from the new hashes
					foreach my $newid (@$thisidarray)
					{
						if (substr($newid, 0, 1) eq $currentsection)
						{
							# If there are duplicates, they must be
							# identical to their previous definitions
							if (defined ${$numhash}{$newid})
							{
								if (${$numhash}{$newid} ne ${$thisnumhash}{$newid} ||
								    ${$hashnumhash}{$newid} ne ${$thishashnumhash}{$newid} ||
								    ${$captionhash}{$newid} ne ${$thiscaptionhash}{$newid} ||
								    defined ${$descriptionhash}{$newid} != defined ${$thisdescriptionhash}{$newid} ||
								    (defined ${$descriptionhash}{$newid} && (${$descriptionhash}{$newid} ne ${$thisdescriptionhash}{$newid})) ||
								    ${$scopehash}{$newid} ne ${$thisscopehash}{$newid} ||
								    defined ${$cformathash}{$newid} != defined ${$thiscformathash}{$newid} ||
								    (defined ${$cformathash}{$newid} && (${$cformathash}{$newid} ne ${$thiscformathash}{$newid})))
								{
									die "String $newid found in $database differs from previous definition";
								}
							}
							else
							{
								# Push to array
								push @newidarray, $newid;

								# Add to hashes
								${$numhash}{$newid} = ${$thisnumhash}{$newid};
								${$hashnumhash}{$newid} = ${$thishashnumhash}{$newid};
								${$captionhash}{$newid} = ${$thiscaptionhash}{$newid};
								${$descriptionhash}{$newid} = ${$thisdescriptionhash}{$newid};
								${$scopehash}{$newid} = ${$thisscopehash}{$newid};
								${$cformathash}{$newid} = ${$thiscformathash}{$newid}
									if defined ${$thiscformathash}{$newid};
							}
						}
					}

					last JOIN if $i == $#{$idarray} + 1;
					$currentsection = substr(${$idarray}[$i], 0, 1);
				}

				# Push the old list
				push @newidarray, ${$idarray}[$i];
			}
			$idarray = \@newidarray;
		}
	}
}

# Select what to generate
# Default: generate header files (use -H do say so explicitly)
my $generatehdr  = defined $opt_H || (!defined $opt_L && !defined $opt_t && !defined $opt_P && !defined $opt_e);
# Other output formats
my $generatepot  = defined $opt_P;	# Generate opera.pot
my $generateen   = defined $opt_e;	# Generate en.po
my $generatelng  = defined $opt_L && !defined $opt_l && !$generatepot;	# Generate .lng
my $generateplat = defined $opt_l;	# Generate platform-specific files
my $generatemap  = $generatehdr;	# Generate string id mapping files
my $generatemake = defined $opt_m; # Generate Makefile
# Output flags
my $includedescription = defined $opt_k; # Add translator hints to *.lng file

# Global data
my @date = gmtime;

if ($generatehdr)
{
	# Create new locale-enum include file
	print "Creating locale-enum.inc\n" if $verbose > 0;
	open ENUMH, ">${destdir}locale-enum.inc"
		or die "Cannot create locale-enum.inc: $!\n";
	push @generatedfiles, "${destdir}locale-enum.inc";
	if ($need_binmode)
	{
		binmode ENUMH, ":bytes";
	}

	print ENUMH "// This file is generated by makelang.pl and should NOT be edited directly.";
}

# Load translations
my ($pohashref, $overridepohashref, $po_enc, $po2_enc);
if (defined $opt_t)
{
	($opt_t, $po_enc, $pohashref) = &loadpo($opt_t, $opt_R);

	if (defined $opt_T)
	{
		($opt_T, $po2_enc, $overridepohashref) = &loadpo($opt_T, $opt_R);
	}
}

# Setup *.lng file name
my $lngname;
my $langcode;
my $langstr;
my $translator;

if (defined $opt_o)
{
	$lngname = $opt_o;
}
elsif (defined $opt_t)
{
	$lngname = $opt_t;
	$lngname =~ s#^(.*)[\\/]##;		# Remove initial path component
	$lngname =~ s/\.pot?$/.lng/;	# Change extension to .lng
	$lngname .= '.lng' unless $lngname =~ /\.lng$/;	# Add .lng if missing
	$lngname = $destdir . $lngname;
}

if (defined $opt_t)
{
	# Get language id
	my $langcodeobj = ${$pohashref}{'"<LanguageCode>"'};
	$langcode = defined $langcodeobj ? $langcodeobj->msgstr : 'x-undefined';
	$langcode =~ s/^"(.*)"$/$1/;
	$langcode =~ s/^<(.*)>$/$1/;
	die "$opt_t: <LanguageCode> not defined\n"
		if $langcode eq '' || $langcode eq 'x-undefined';
	die "$opt_t: <LanguageCode> not in ISO 639-1 format: \"$langcode\"\n"
		unless $langcode =~ /^(?:[a-z][a-z](?:[-_][a-z]+)?|[ix]-.*)$/i;

	# Get language name
	my $langstrobj = ${$pohashref}{'"<LanguageName>"'};
	$langstr = defined $langstrobj ? $langstrobj->msgstr : 'undefined';
	$langstr =~ s/^"(.*)"$/$1/;
	$langstr =~ s/^<(.*)>$/$1/;
	die "$opt_t: <LanguageName> not defined\n"
		if $langstr eq '' || $langstr eq 'undefined';
	if ($langstr =~ /^<(.*)>$/)
	{
		# Strip left-over brackets
		$langstr = $1;
	}

	# Get encoding
	my $poheaderobj = ${$pohashref}{'""'};
	my $poheader = $poheaderobj->msgstr;
	my $encstr = '';
	if ($poheader =~ /;\s*charset=([^\s\\]*)\s*\\/i)
	{
		$encstr = lc($1);
	}
	else
	{
		die "$opt_t: Unable to find Charset label in PO header:\n$poheader\n";
	}

	# Require UTF-8
	die "$opt_t: Must be utf-8 encoded\n" unless $encstr eq 'utf-8';

	# Check that encoding in header matches actual encoding
	if ($po_enc ne 'utf-8' && $po_enc ne 'us-ascii')
	{
		die "$opt_t: File is not UTF-8 encoded but header says so\n";
	}

	# Get translator name
	if ($poheader =~ /Last-Translator:\s+([^\\]+)\s*\\n/i)
	{
		$translator = $1 unless $1 eq 'FULL NAME <EMAIL@ADDRESS>';
	}

	# Check for consistency in the override PO file
	if (defined $opt_T)
	{
		my $overridepoheaderobj = ${$overridepohashref}{'""'};
		my $overridepoheader = $overridepoheaderobj->msgstr;
		my $encstr2;

		# Check encoding
		if ($overridepoheader =~ /;\s*charset=([^\s\\]*)\s*\\/i)
		{
			$encstr2 = lc($1);
		}
		else
		{
			die "$opt_T: Unable to find Charset label in PO header:\n$poheader\n";
		}

		# Require UTF-8
		die "$opt_T: Must be utf-8 encoded\n" unless $encstr2 eq 'utf-8';

		# Check that encoding in header matches actual encoding
		if ($po2_enc ne 'utf-8' && $po2_enc ne 'us-ascii')
		{
			die "$opt_T: File is not UTF-8 encoded but header says so\n";
		}

		# Check for credits
		my $translator2 = '';
		if ($overridepoheader =~ /Last-Translator:\s+([^\\]+)\s*\\n/i)
		{
			$translator2 = $1 unless $1 eq 'FULL NAME <EMAIL@ADDRESS>';
		}

		if ($translator2 ne '' && (!defined $translator || $translator2 ne $translator))
		{
			$translator .= ' and ' if defined $translator;
			$translator .= $translator2;
		}
	}
}

# Create new LNG file
if ($generatelng)
{
	die "Internal error: No language file name specified\n" unless defined $lngname;
	die "Internal error: Language code not defined\n" unless defined $langcode;
	die "Internal error: Language name not defined\n" unless defined $langstr;
	print "Creating $lngname" if $verbose > 0;
	print " using translation from $opt_t" if $verbose > 0 && defined $opt_t;
	print " and $opt_T" if $verbose > 0 && defined $opt_T;
	print "\n" if $verbose > 0;
	open LNG, ">$lngname"
		or die "Cannot create $lngname: $!\n";
	push @generatedfiles, $lngname;
	if ($need_binmode)
	{
		binmode LNG, ":bytes";
	}

	print LNG "; Opera language file version 2.0\n";
	print LNG "; For internal testing only, may not be distributed.\n"
		unless defined $opt_p;
	print LNG "; Copyright ", chr(194), chr(169), " 1995-", $date[5] + 1900,
		" Opera Software ASA. All rights reserved.\n";
	my $isodate = sprintf("%04d-%02d-%02d %02d:%02d",
						  $date[5] + 1900, $date[4] + 1, $date[3],
						  $date[2], $date[1]);
	if (defined $opt_t && defined $translator)
	{
		print LNG "; Translated by $translator\n";
	}
	print LNG "; Created on $isodate\n";
	print LNG "; Lines starting with ; (like this) are comments and need not be translated\n\n"
		if $includedescription;
	print LNG "[Info]\n";
	print LNG "Language=\"$langcode\"\n";
	print LNG "; The string below is the language name in its own language\n"
		if $includedescription;
	print LNG "LanguageName=\"$langstr\"\n";
	print LNG "Charset=\"utf-8\"\n";

	# Writing direction is stored in the string S_HTML_DIRECTION
	my $directionobj = ${$pohashref}{'"S_HTML_DIRECTION"::"ltr"'};
	$directionobj = ${$pohashref}{'"1850176044::S_HTML_DIRECTION"'} unless defined $directionobj;
	my $directionstr = defined $directionobj ? $directionobj->msgstr : '"ltr"';
	# Sanity check - direction should be either "ltr" or "rtl"
	if ($directionstr ne '"ltr"' && $directionstr ne '"rtl"')
	{
		print "Warning: $opt_t: Invalid S_HTML_DIRECTION string: $directionstr assuming ltr.\n";
		$directionobj->msgstr('ltr');
	}
	print LNG 'Direction=', $directionstr eq '"rtl"' ? 1 : 0, "\n";

	if (defined $targetplatform)
	{
		print LNG "Build.$targetplatform=$targetbuild\n";
		print LNG "Version.$targetplatform=$targetversion\n";
	}

	print LNG "DB.version=$dbversion\n\n";
	print LNG "[Translation]\n";
}

if ($generatehdr)
{
	# Create new locale-dbversion include file
	print "Creating locale-dbversion.h\n" if $verbose > 0;
	open DBH, ">${destdir}locale-dbversion.h"
		or die "Cannot create locale-dbversion.h: $!\n";
	push @generatedfiles, "${destdir}locale-dbversion.h";
	if ($need_binmode)
	{
		binmode DBH, ":bytes";
	}
	print DBH "// This file is generated by makelang.pl and should NOT be edited directly.\n\n";
	print DBH "/** Version number for the string database sources.\n";
	print DBH " * This number is matched to the external *.lng files to ensure that the\n";
	print DBH " * file is consistent with what Opera is expecting.\n";
	print DBH " */\n";
	print DBH "#define LANGUAGE_DATABASE_VERSION $dbversion\n";
	close DBH;
}

# Used for generating C++ files
my $cppfile = '';

# Used for generating Symbian files
my $symbian_ra_file;
my $symbian_rls_file;
my $symbian_str_file;
my $symbian_h_file;
my $symbian_s60_ldb_file;
my $symbian_s60_stringcount;
my $symbian_s60_untranslated_stringcount;
my $symbian_s60_fuzzy_stringcount;

# Used for generating binary files
my $binaryname = '';
my %languagestrings;
my $languagestringscount;

if ($generateplat)
{
	# Create C++ file
	$cppfile = 'qt_languagestrings.inc' if $opt_l eq 'qt';
	$cppfile = 'KyoLanguageStrings.inc' if $opt_l eq 'kyocera';
	$cppfile = 'gogi_languagestrings.inc' if $opt_l eq 'gogi';

	if($opt_l eq 'symbian')
	{
		$symbian_h_file = 'oprstring.h';
		my $file_start;

		if($opt_t)
		{
			my @split_string = split(/\\|\//, $opt_t);

			foreach my $str (@split_string)
			{
				if($str =~ /.po/)
				{
					my @split_string2 = split(/.po/, $str);
					$file_start = $split_string2[0];
				}
			}

			$symbian_ra_file = "$file_start.ra";
			$symbian_rls_file = "$file_start.rls";
			$symbian_str_file = "$file_start.str";
		}
		else
		{
			print "Using default variables for the files\n" if $verbose > 0;
			$symbian_ra_file = 'en.ra';
			$symbian_rls_file = 'en.rls';
			$symbian_str_file = 'en.str';
		}
	}
	elsif ($opt_l eq 'series60ui')
	{
		die "Internal error: Language code not defined\n" unless defined $langcode;
		$symbian_s60_ldb_file = "opera.$langcode.ldb";
		$symbian_s60_stringcount = 0;
		$symbian_s60_untranslated_stringcount = 0;
		$symbian_s60_fuzzy_stringcount = 0;
	}
	elsif ($opt_l eq 'binary')
	{
		require Text::Iconv;
		Text::Iconv->raise_error(1);

		if (defined $opt_o)
		{
			$binaryname = $opt_o;
		}
		else
		{
			$binaryname = $lngname;
			$binaryname =~ s/\.lng/.bin/g;
		}
	}
	else
	{
		die "-l: \"$opt_l\" unrecognized\n" if $cppfile eq '';
	}

	if($opt_l eq 'symbian')
	{
		#Open the files that we need.
		print "Creating $symbian_ra_file, $symbian_rls_file and $symbian_str_file\n" if $verbose > 0;
		open RA_FILE, ">$destdir$symbian_ra_file"
			or die "Cannot create $symbian_ra_file: $!\n";
		push @generatedfiles, "$destdir$symbian_ra_file";
		open RLS_FILE, ">$destdir$symbian_rls_file"
			or die "Cannot create $symbian_rls_file: $!\n";
		push @generatedfiles, "$destdir$symbian_rls_file";
		open STR_FILE, ">$destdir$symbian_str_file"
			or die "Cannot create $symbian_str_file: $!\n";
		push @generatedfiles, "$destdir$symbian_str_file";
		open H_FILE, ">$destdir$symbian_h_file"
			or die "Cannot create $symbian_h_file: $!\n";
		push @generatedfiles, "$destdir$symbian_h_file";

		if ($need_binmode)
		{
			binmode RA_FILE, ":bytes";
			binmode RLS_FILE, ":bytes";
			binmode STR_FILE, ":bytes";
		}

		print RLS_FILE "CHARACTER_SET UTF8\n";
		print STR_FILE "CHARACTER_SET UTF8\n";
	}
	elsif ($opt_l eq 'series60ui')
	{
		print "Creating $symbian_s60_ldb_file" if $verbose > 0;
		print " using translation from $opt_t" if $verbose > 0 && defined $opt_t;
		print "\n" if $verbose > 0;

		open S60_LDB_FILE, ">$destdir$symbian_s60_ldb_file"
			or die "Cannot create $symbian_s60_ldb_file: $!\n";
		push @generatedfiles, "$destdir$symbian_s60_ldb_file";

		#Add header
		print S60_LDB_FILE "/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-\n";
		print S60_LDB_FILE " *\n";
		print S60_LDB_FILE " * Copyright (C) 2000-", $date[5] + 1900, " Opera Software AS.  All rights reserved.\n";
		print S60_LDB_FILE " *\n";
		print S60_LDB_FILE " * This file is part of the Opera web browser.\n";
		print S60_LDB_FILE " * It may not be distributed under any circumstances.\n";
		print S60_LDB_FILE " *\n";
		print S60_LDB_FILE " * This file is automatically generated by makelang.pl using the file";
		print S60_LDB_FILE (defined $opt_t) ? "\n * '$opt_t'" : " 'english.db'";
		print S60_LDB_FILE "\n *\n * New strings should be added in english.db\n";
		print S60_LDB_FILE " */\n\n";
		print S60_LDB_FILE "//  APPLICATION INFORMATION\n";
		print S60_LDB_FILE "#define ELanguage	ELangEnglish	// Caption language\n\n";
	}
	elsif ($opt_l eq 'binary')
	{
		print "Creating $binaryname\n" if $verbose > 0;
	}
	else
	{
		print "Creating $cppfile file\n" if $verbose > 0;
		open CPPLANG, ">$destdir$cppfile" or die "Cannot create $cppfile: $!\n";
		push @generatedfiles, "$destdir$cppfile";
		if ($need_binmode)
		{
			binmode CPPLANG, ":bytes";
		}

		print CPPLANG "// Message text strings - autogenerated by makelang.pl\n";
		print CPPLANG "// Copyright © 1995-", $date[5] + 1900, " Opera Software AS. All rights reserved.\n";
		print CPPLANG "// Created on ", scalar gmtime, "\n\n";
	}
}

# Generate POT file
my $potname = '';
if ($generatepot || $generateen)
{
	my $filename = $generatepot ? 'opera.pot' : 'en.po';
	if (-d "${subdir}../en/")
	{
		$potname = "${subdir}../en/${filename}";
	}
	elsif (-d "${subdir}../translations/en/")
	{
		$potname = "${subdir}../translations/en/${filename}";
	}

	if (defined $opt_d || $potname eq '')
	{
		$potname = $destdir . $filename;
	}
}

if ($generatepot || $generateen)
{
	eval { use POSIX; };
	open POT, ">$potname" or die "Cannot create $potname: $!\n";
	push @generatedfiles, $potname;

	print POT   "# This is an Opera translation file in .po format.\n";
	print POT qq'msgid ""\n';
	print POT qq'msgstr ""\n';
	print POT qq'"Project-Id-Version: Opera, dbversion $dbversion\\n"\n';
	print POT strftime('"POT-Creation-Date: %Y-%m-%d %H:%M', localtime);
	my $timezone = (localtime(time))[2] - (gmtime(time))[2];
	printf POT qq'%+03d00\\n"\n', $timezone;
	if ($generateen)
	{
		print POT strftime('"PO-Revision-Date: %Y-%m-%d %H:%M', localtime);
		printf POT qq'%+03d00\\n"\n', $timezone;
	}
	else
	{
		print POT qq'"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n"\n';
	}
	print POT qq'"Last-Translator: FULL NAME <EMAIL\@ADDRESS>\\n"\n';
	print POT qq'"MIME-Version: 1.0\\n"\n';
	print POT qq'"Content-Type: text/plain; charset=utf-8\\n"\n';
	print POT qq'"Content-Transfer-Encoding: 8bit\\n"\n\n';

	print POT   "#. Language name in its own language\n";
	print POT qq'msgid "<LanguageName>"\n';
	print POT $generateen ? qq'msgstr "English"\n\n' : qq'msgstr ""\n\n';

	print POT   "#. Two letter language code\n";
	print POT qq'msgid "<LanguageCode>"\n';
	print POT $generateen ? qq'msgstr "en"\n\n' : qq'msgstr ""\n\n';
}

# Output the data from the database

# Setup
my $istranslated = 0;
my $warnings = 0;
my $isfuzzy = 0;
my $splitsection = 0;
my %locmap = ();
my $number = 0;
my $olddescription = '';
my $enumnum = 0;
my $enumline = '';
my $currentprefix = '';

foreach my $id (@$idarray)
{
	my $translationwarning = '';
	my $newsection;
	my $description = ${$descriptionhash}{$id};
	my $num = ${$numhash}{$id};

	# Print section header if a new section was found
	if (substr($id, 0, 1) ne $currentprefix)
	{
		# Write any unknowns from the build.strings file for this section
		if (defined $opt_b && $currentprefix ne '')
		{
			&handleunknowns($currentprefix);
		}

		# Remember new prefix
		$currentprefix = substr($id, 0, 1);

		$splitsection = 1;

		$newsection = ${$sectionhash}{$currentprefix};
		$newsection =~ s/;.*$//;
		if ($generatelng)
		{
			print LNG "\n; $newsection\n" if $includedescription;
		}
	}

	# Arbitrary number to work around the MSVC++ "debug information module
	# size exceeded" error.
	$splitsection = 1 if $number > 1536;

	if ($splitsection)
	{
		if ($generatehdr)
		{
			++ $enumnum;
			printf ENUMH $enumline, '' if $enumline ne '';
			$enumline = '';
			print ENUMH "};\n" if $enumnum > 1;
			print ENUMH "\n/** Part $enumnum of the locale string enumeration.\n";
			if (defined $newsection)
			{
				print ENUMH " * $newsection\n";
			}
			else
			{
				print ENUMH " * (Continued from previous section)\n";
			}
			print ENUMH " */\n";
			print ENUMH "enum StringList$enumnum {\n";
			print ENUMH "NOT_A_STRING = 0,\n" if 1 == $enumnum;
			if (1 == $enumnum)
			{
				print ENUMH "S_NOT_A_STRING = 0,\n";
				print ENUMH "D_NOT_A_STRING = 0,\n";
				print ENUMH "M_NOT_A_STRING = 0,\n";
			}
		}

		$splitsection = 0;
		$number = 0;
	}

	# Find the displayable number for this entry.
	my $displaynum = ${$hashnumhash}{$id};

	# Check for translation
	my $caption = ${$captionhash}{$id};
	if (defined $opt_t)
	{
		# Use the data from the PO file to translate $caption.
		# In old-style PO files, the msgid is on the form "number::string".
		# In the new-style PO files, the msgctxt is the string name and
		# the msgid is the string, which is represented as "msgctxt::msgid"
		# in the PO file. During the transition phase, we need to support
		# both formats.
		my $obsoletepoidstr = $num . '::' . $caption;
		$obsoletepoidstr =~ s/"/\\"/g;
		$obsoletepoidstr = '"' . $obsoletepoidstr . '"';

		my $poidstr = $caption;
		# Empty strings have "[]" in their msgid in the msgctxt format.
		$poidstr = '[]' if $poidstr eq '';
		$poidstr =~ s/"/\\"/g;
		$poidstr = '"' . $id . '"::"' . $poidstr . '"';

		# Check if we need to use the new or old hash key
		my $mainpoidstr = $poidstr;
		$mainpoidstr = $obsoletepoidstr
			if defined ${$pohashref}{$obsoletepoidstr};
		my $overridepoidstr = $poidstr;
		$overridepoidstr = $obsoletepoidstr
			if defined ${$overridepohashref}{$obsoletepoidstr};

		if (defined ${$pohashref}{$mainpoidstr} ||
			defined ${$overridepohashref}{$overridepoidstr})
		{
			# Translation exists in either the normal file or the override
			# file.
			my $newcaption;
			$isfuzzy = 0;
			my $cformat = 0;
			my $qtformat = 0;
			my $pythonformat = 0;
			my $usingT = 0;

			if (defined ${$pohashref}{$mainpoidstr})
			{
				$newcaption = ${$pohashref}{$mainpoidstr}->msgstr;
				$isfuzzy = 1 if ${$pohashref}{$mainpoidstr}->has_flag('fuzzy');
				$cformat = 1 if ${$pohashref}{$mainpoidstr}->has_flag('c-format');
				$qtformat = 1 if ${$pohashref}{$mainpoidstr}->has_flag('qt-format');
				$pythonformat = 1 if ${$pohashref}{$mainpoidstr}->has_flag('python-format');
			}
			if (defined ${$overridepohashref}{$overridepoidstr})
			{
				my $newercaption = ${$overridepohashref}{$overridepoidstr}->msgstr;
				if ($newercaption ne '' &&
					!${$overridepohashref}{$overridepoidstr}->has_flag('fuzzy'))
				{
					# Exists, is non-empty and non-fuzzy - use it!
					if ($verbose > 0)
					{
						if (defined $newcaption)
						{
							print "  Overriding $newcaption\n";
							print "    with $newercaption\n";
							print "    for $id\n";
						}
						else
						{
							print "  $id only defined in alternate translation.\n";
						}
					}
					$newcaption = $newercaption;
					$usingT = 1;
					$isfuzzy = 0;
					$cformat = 1 if ${$overridepohashref}{$overridepoidstr}->has_flag('c-format');
					$qtformat = 1 if ${$overridepohashref}{$overridepoidstr}->has_flag('qt-format');
					$pythonformat = 1 if ${$overridepohashref}{$overridepoidstr}->has_flag('python-format');
				}
			}
			die "More than one of c-format, qt-format and python-format defined in PO file\nfor $id ($num)."
				if $cformat + $qtformat + $pythonformat > 1;

			if ($isfuzzy)
			{
				$translationwarning =
					"Warning: $opt_t: fuzzy translation of $id ($num) ignored.\n" if $verbose > 0;
			}
			elsif (!defined $newcaption)
			{
				$translationwarning =
					"Warning: String $id ($num) defined in translation but not translated.\n"
					unless $opt_u;
			}
			else
			{
				# Translation is up-to-date, use it
				my $usable = 1;

				# Sanity check for c-format.
				# We must ignore S_{EMAIL,TELNET}_CLIENT_SETUP_HELP_TEXT since
				# they aren't *really* a c-format strings, they just look that
				# way.
				if ($cformat &&
					$newcaption ne '""' &&
					$id !~ /S_(EMAIL|TELNET)_CLIENT_SETUP_HELP_TEXT/)
				{
					# First remove %% as they wreck the test
					my ($oldstr, $newstr) = ($caption, $newcaption);
					$oldstr =~ s/%%//g;
					$newstr =~ s/%%//g;

					# Compare all the format specifiers in turn by
					# splitting the string on %'s, meaning we get
					# an array of strings where each entry contains the
					# format specifier and any text that follows it.
					my (undef, @old) = split('%', $oldstr);
					my (undef, @new) = split('%', $newstr);
					if ($#old != $#new)
					{
						$usable = 0;
						$translationwarning =
							"Warning: " .
							($usingT ? $opt_T : $opt_t) .
							": incorrect number of c-format templates for $id ($num).\n";
					}
					my %pykeys = ();
					TEMPLATE: while ($usable && $#old >= 0)
					{
						my ($old, $new) = (shift @old, shift @new);
						# Uppercase S in regexp is Symbian-specific.
						# These regexps should be kept in synch with
						# the c-format when generating .pot files below.
						# Search for <CFORMAT>.
						if ($old =~ /^[-+ 0-9]?\.?[0-9]*([lh]?[duisSefgc]|s)/)
						{
							my $oldformat = $1;
							if ($new =~ /^[-+ 0-9]?\.?[0-9]*([lh]?[duisSefgc]|s)/)
							{
								my $newformat = $1;
								if ($oldformat ne $newformat)
								{
									$usable = 0;
									$translationwarning =
										"Warning: " .
										($usingT ? $opt_T : $opt_t) .
										": c-format template mismatch for $id ($num).\n";
								}
							}
							else
							{
								$usable = 0;
								$translationwarning =
									"Warning: " .
									($usingT ? $opt_T : $opt_t) .
									": c-format specifier \"\%" .
									substr($new, 0, 10) .
									"\" unrecognized for $id ($num).\n";
							}
						}
						elsif ($old =~ /^\(([^\)]+)\)s/)
						{
							my $oldformat = $1;
							if ($new =~ /^\(([^\)]+)\)s/)
							{
								my $newformat = $1;
								if ($oldformat ne $newformat)
								{
									$pykeys{$oldformat} = defined $pykeys{$oldformat} ? $pykeys{$oldformat} + 1 : 1;
									$pykeys{$newformat} = defined $pykeys{$newformat} ? $pykeys{$newformat} - 1 : -1;
								}
							}
							else
							{
								$usable = 0;
								$translationwarning =
									"Warning: " .
									($usingT ? $opt_T : $opt_t) .
									": python-format specifier \"\%" .
									substr($new, 0, length($oldformat) + 3) .
									"\" unrecognized for $id ($num).\n";
							}
						}
						else
						{
							die "english.db: Unrecognized format specifier \"\%" .
								substr($old, 0, 10) .
								"\" for \"$id\"\nin original string \"$caption\".\n";
						}
					}
					my @newkeys = ();
					my @oldkeys = ();
					foreach my $key ( keys %pykeys )
					{
						if ($pykeys{$key} > 0)
						{
							push @oldkeys, $key;
						}
						elsif ($pykeys{$key} < 0)
						{
							push @newkeys, $key;
						}
					}
					if (@newkeys || @oldkeys)
					{
						my @bad;
						push @bad, 'lost ' . join(', ', @oldkeys) if @oldkeys;
						push @bad, 'gained ' . join(', ', @newkeys) if @newkeys;
						$usable = 0;
						$translationwarning =
							"Warning: " .
							($usingT ? $opt_T : $opt_t) .
							": mismatched python-format templates (" .
							joine('; ', @bad) . ") for $id ($num).\n";
					}
				}

				# Sanity check for qt-format.
				if ($qtformat &&
					$newcaption ne '""')
				{
					# Find all % signs in the original string that are
					# followed by a number from 1 through 99, and make
					# sure the translation contains the same ones (not
					# necessarily in the same order).

					# Extract templates from original.
					my %template;
					while ($caption =~ /%([1-9][0-9]|[0-9])/g)
					{
						$template{$1} = 1;
					}

					# Extract templates from translation.
					QTFORMATCHECK: while ($newcaption =~ /%([1-9][0-9]|[0-9])/g)
					{
						if (defined $template{$1})
						{
							# Matches template in original.
							if (-- $template{$1} != 0)
							{
								# Already seen in translation.
								$usable = 0;
								$translationwarning =
									"Warning: " .
									($usingT ? $opt_T : $opt_t) .
									": duplicated qt-format template \%$1 for $id ($num).\n";
								last QTFORMATCHECK;
							}
						}
						else
						{
							# Does not match template in original.
							$usable = 0;
							$translationwarning =
								"Warning: " .
								($usingT ? $opt_T : $opt_t) .
								": undefined qt-format template \%$1 for $id ($num).\n";
							last QTFORMATCHECK;
						}
					}

					# Check that all parameters were seen.
					if ($usable)
					{
						foreach my $parameter (keys %template)
						{
							if ($template{$parameter} != 0)
							{
								$usable = 0;
								$translationwarning =
									"Warning: " .
									($usingT ? $opt_T : $opt_t) .
									": unmatched qt-format template \%$parameter for $id ($num).\n";
							}
						}
					}
				}

				# Sanity check for python-format
				if ($pythonformat &&
					$newcaption ne '""')
				{
					# Find all % signs in the original string that are
					# followed by a parenthesized variable name, and
					# check that the translation names all the same
					# variables.
					# NB: Simple c-format specifiers are ignored, they
					# would probably trigger the c-format check above
					# instead.

					# Extract variables from original.
					my (%template, %type);
					while ($caption =~ /%\((.+?)\)([-#0 +]?(?:\*|[0-9]+)?(?:\.(?:\*|[0-9]+))?[hlL]?[cdeEfFgGiosuxX])/g)
					{
						if (defined $template{$1})
						{
							++ $template{$1};
						}
						else
						{
							$template{$1} = 1;
							$type{$1} = $2;
						}
					}

					# Extract variables from translation
					PYTHONFORMATCHECK: while ($newcaption =~ /%\((.+?)\)([-#0 +]?(?:\*|[0-9]+)?(?:\.(?:\*|[0-9]+))?[hlL]?[cdeEfFgGiosuxX])/g)
					{
						if (defined $template{$1})
						{
							-- $template{$1};
							if ($type{$1} ne $2)
							{
								# Does not use the same format as the original.
								$usable = 0;
								$translationwarning =
									"Warning: " .
									($usingT ? $opt_T : $opt_t) .
									": bad type for python-format variable \%($1)$2 for $id ($num).\n";
								last PYTHONFORMATCHECK;
							}
						}
						elsif (!defined $template{$1})
						{
							# Does not match template in original.
							$usable = 0;
							$translationwarning =
								"Warning: " .
								($usingT ? $opt_T : $opt_t) .
								": undefined python-format variable \%($1)$2 for $id ($num).\n";
							last PYTHONFORMATCHECK;
						}
					}

					# Check that all variables were seen.
					if ($usable)
					{
						foreach my $parameter (keys %template)
						{
							if ($template{$parameter} != 0)
							{
								$usable = 0;
								$translationwarning =
									"Warning: " .
									($usingT ? $opt_T : $opt_t) .
									": mismatched number of uses of python-format variable \%($parameter) for $id ($num).\n";
							}
						}
					}
				}

				# Reformat to fit
				$newcaption =~ s/^"(.*)"$/$1/;
				$newcaption =~ s/\\"/\"/g;

				# Sanity check for initial num::
				if ($usable && $newcaption =~ /^([0-9]+)::/)
				{
					if ($1 eq $num)
					{
						$translationwarning =
							"Warning: " .
							($usingT ? $opt_T : $opt_t) .
							": Stripping string number for $id ($num).\n";
						$newcaption = (split(':', $newcaption, 3))[2];
					}
					else
					{
						$usable = 0;
						$translationwarning =
							"Warning: " .
							($usingT ? $opt_T : $opt_t) .
							": Malformed translation for $id ($num).\n";
					}
				}

				# Sanity check for initial and final newlines
				if ($usable && substr($caption, 0, 2) eq '\n' &&
					$newcaption ne '' && substr($newcaption, 0, 2) ne '\n')
				{
				#	$usable = 0;
					$translationwarning =
						"Warning: " .
						($usingT ? $opt_T : $opt_t) .
						": Translation missing leading \"\\n\" sequence for $id ($num).\n";
				}

				if ($usable && substr($caption, -2, 2) eq '\n' &&
					$newcaption ne '' && substr($newcaption, -2, 2) ne '\n')
				{
					$usable = 0;
					$translationwarning =
						"Warning: " .
						($usingT ? $opt_T : $opt_t) .
						": Translation missing trailing \"\\n\" sequence for $id ($num).\n";
				}

				# Keep if proper
				if ($usable)
				{
					# Check that we had anything
					if ($newcaption eq '')
					{
						#$usable = 0;
						$translationwarning = "Warning: Untranslated string $id ($num):\n" .
							'  "' . $caption . qq'"\n'
							unless defined $opt_u;
					}
					else
					{
						if ($newcaption eq '[]')
						{
							$caption = '';
						}
						else
						{
							$caption = $newcaption;
						}
						$istranslated = 1;
					}
				}
				else
				{
					$translationwarning .= "Ignoring translation for $id\n";
				}
			}
		}
		else
		{
			$translationwarning =
				"Warning: Cannot find string $id ($num) in translation file $opt_t:\n" .
				'  "' . $caption . qq'"\n\n' .
				"Make sure the string is in the PO file, and matches the original exactly.\n\n"
				unless defined $opt_u;
		}
	}

	# Strip ampersand if requested, but only in menu strings
	if (defined $opt_s && $id =~ /^M/)
	{
		$caption =~ s/&//g;
		if ($langcode eq 'ja' || $langcode =~ /^zh/)
		{
			# Also strip Asian-style hotkey if stripping ampersands
			$caption =~ s/\([A-Za-z0-9]\)$//;
		}
	}

	# Strip ellipsis if requested
	$caption =~ s/\xE2\x80\xA6/.../g if defined $opt_E;

	# Remap Romanian characters if requested
	if (defined $opt_U)
	{
		$caption =~ s/\xC8\x98/\xC5\x9E/g; # S-comma -> S-cedilla
		$caption =~ s/\xC8\x99/\xC5\x9F/g; # s-comma -> s-cedilla
		$caption =~ s/\xC8\x9A/\xC5\xA2/g; # T-comma -> T-cedilla
		$caption =~ s/\xC8\x9B/\xC5\xA3/g; # t-comma -> t-cedilla
	}

	# Check whether the string should be included everywhere
	my $include = 0;
	my $notinheader = 1;
	if (defined $opt_b)
	{
		# Check if string is listed in the build.strings file
		if (defined $stringlist{$id})
		{
			$include = 1;
			$stringlist{$id} --; # Remember that string was included
		}
		$notinheader = !$include;
	}
	else
	{
		# Old scope-based inclusion/exclusion
		my $forceexclude = 0;
		foreach my $scope (split(/,/, ${$scopehash}{$id}))
		{
			if ($scope =~ /^not-/)
			{
				$scope =~ s/^not-//;
				die "english.db: Unrecognized scope not-$scope for $id\n"
					unless defined $main::scope{$scope};
				$forceexclude |= $main::scope{$scope};
			}
			else
			{
				die "english.db: Unrecognized scope $scope for $id\n"
					unless defined $main::scope{$scope};
				$include |= $main::scope{$scope};
				if (!defined $main::killscope{$scope})
				{
					$notinheader = 0;
				}
			}
		}

		# Exclude if we found a matching "not", but not if we're generating
		# a PO file (opera.pot or en.po).
		$include &= !$forceexclude unless ($generatepot || $generateen);
	}

	# Print to enumeration file
	if ($generatehdr && !$notinheader)
	{
		# Print previous entry
		if ($enumline ne '')
		{
			printf ENUMH $enumline, ',';
		}

		# Cannot print until we know if there is an entry following
		# this one, so remember it
		$enumline = "$id = ${displaynum}%s ";
		if (defined $description)
		{
			my $escdescription = $description;
			$escdescription =~ s/%/%%/g;
			$enumline .= "///< $escdescription";
		}
		$enumline .= "\n";

		# Remember mapping
		$locmap{$num} = $id
			if $generatemap && defined $num && $num != $displaynum;
	}

	# Print to language files
	if ($include)
	{
		# Check if there was a problem with the translation
		if (defined $opt_t && $translationwarning ne '')
		{
			print $translationwarning if $verbose >= 0;
			++ $warnings;
		}

		# Print to *.lng file
		if ($generatelng)
		{
			# Separate by empty line when description changes
			if ($includedescription && defined $olddescription)
			{
				print LNG "\n" if !defined $description or $description ne $olddescription;
			}

			# Write description unless told not to
			if ($includedescription && defined $description &&
				(!defined $olddescription || $description ne $olddescription))
			{
				my @wrapped = wrap('', '; ', $description);
				foreach my $wrappedline (@wrapped)
				{
					print LNG "; $wrappedline\n";
				}
			}

			# Comment if translation is missing
			if (defined $opt_t && !$istranslated)
			{
				print LNG "; Untranslated string\n" unless defined $opt_u;
			}
			print LNG "$displaynum=\"$caption\"\n";
		}

		# Remember description
		if (defined $description)
		{
			$olddescription = $description;
		}
		else
		{
			undef $olddescription;
		}

		# Print to platform-specific files
		if ($generateplat)
		{
			# Symbian UIQ ---
			if($opt_l eq 'symbian')
			{
				my $str = "RESOURCE TBUF R\_\U$id\E { buf=\"$caption\"; } // $id\n";
				print STR_FILE $str;
				$str = "RESOURCE TBUF R\_\U$id\E { buf=RLS_\U$id\E; } // $id\n";
				print RA_FILE $str;
				$str = "    case Str::$id: return R\_\U$id\E; // $displaynum\n";
				print H_FILE $str;

				my $escaped = $caption;
				$escaped =~ s/\"/\\\"/g;

				$str = "rls_string RLS\_\U$id\E \"$escaped\"\n";
				print RLS_FILE "/*&\n";
				print RLS_FILE "\@localize         yes\n";
				my $rlsdescription;
				if (defined $description && $description ne '')
				{
					$rlsdescription = $description;
				}
				else
				{
					$rlsdescription = 'N/A';
				}
				print RLS_FILE "\@description      $rlsdescription\n";
				print RLS_FILE "\@uicontext        ";
				print RLS_FILE "FreeText\n" if $id =~ /^[DS]/;
				print RLS_FILE "CommandLongText\n" if $id =~ /^M/;
				print RLS_FILE "\@restriction      none\n"; # FIXME
				print RLS_FILE "*/\n";

				print RLS_FILE $str . "\n";
			}
			# Symbian Series 60 ---
			elsif ($opt_l eq 'series60ui')
			{
				# String prefixes are not used in S60 files, also strings
				# must be lowercase
				my $idx = index $id, "_";
				my $new = lc(substr($id,$idx+1));
				my $str;
				# replace " in caption with \", this is for english.db only
				# PO files already have \" in them
				$caption =~ s/"/\\"/g;

				if ($istranslated || !defined($opt_t))
				{
					$str = "#define $new \"$caption\"\n";
				}
				else
				{
					if ($isfuzzy)
					{
						$str = "// Fuzzy string '$new'\n" .
							"#define $new \"$caption\"\n";
						$symbian_s60_fuzzy_stringcount++;
					}
					else
					{
						$str = "// Untranslated string '$new'\n" .
							"#define $new \"$caption\"\n";
						$symbian_s60_untranslated_stringcount++;
					}
				}

				print S60_LDB_FILE $str;
				$symbian_s60_stringcount++;
			}
			# Binary language file (Rappakalja 3 and later) ---
			elsif ($opt_l eq 'binary')
			{
				$languagestrings{$displaynum} = $caption;
				$languagestringscount ++;
			}
			# C++ include file ---
			else
			{
				# Print to C++ file
				# $escaped: string with " escaped.
				my $escaped = $caption;
				$escaped =~ s/\"/\\\"/g;
				# $escapedx: string with non-ASCII characters \xXXXX escaped.
				my $escapedx = $escaped;
				# FIXME: Does not handle non-BMP characters!
				$escapedx =~ s/([\xC2-\xDF])([\x80-\xBF])/sprintf("\\x%04X",((ord($1)&0x1F)<<6)|((ord($2)&0x3F)))/eg;
				$escapedx =~ s/([\xE0-\xEF])([\x80-\xBF])([\x80-\xBF])/sprintf("\\x%04X",((ord($1)&0x0F)<<12)|((ord($2)&0x3F)<<6)|((ord($3)&0x3F)))/eg;
				die "Cannot encode non-BMP character"
					if $escaped =~ /[\xF0-\xF7][\x80-\xBF][\x80-\xBF][\x80-\xBF]/;
				# $escapedx =~ s/([\xF0-\xF7])([\x80-\xBF])([\x80-\xBF])([\x80-\xBF])/sprintf("\\x%04X",((ord($1)&0x03)<<18)|((ord($2)&0x3F)<<12)|((ord($3)&0x3F)<<6)|((ord($4)&0x3F)))/eg;
				# A \xXXXX followed by a hexadecimal digit needs to be further
				# escaped.
				while ($escapedx =~ /\\x[0-9A-F]{4}[0-9A-Fa-f]/)
				{
					$escapedx =~ s/(\\x[0-9A-F]{4})([0-9A-Fa-f])/sprintf("$1\\x%04X",ord($2));/eg;
				}

				print CPPLANG "\tcase $displaynum: // $id\n";
				if ($opt_l eq 'qt')
				{
					print CPPLANG "\t\tqs = Translate(\"$escaped\");\n";
				}
				elsif ($opt_l eq 'kyocera')
				{
					print CPPLANG "\t\ts.Set(\"$escaped\");\n";
				}
				elsif ($opt_l eq 'gogi')
				{
					# Save space by only using UNI_L() if needed.
					if ($escapedx eq $escaped)
					{
						print CPPLANG "\t\ts.SetL(\"$escapedx\");\n";
					}
					else
					{
						print CPPLANG "\t\ts.SetL(UNI_L(\"$escapedx\"));\n";
					}
				}

				print CPPLANG "\t\tbreak;\n";
			}
		}

		# Print to POT (translation template) file
		if ($generatepot || $generateen)
		{
			my @escaped;

			# Escape the string
			my $string = $caption;
			$string =~ s/"/\\"/g;

			# Check if c-format
			# These regexp should be kept in synch with the c-format
			# when checking .po file translations above. Search for
			# <CFORMAT>.
			my $cformat = 0;
			$cformat = 1 if $string =~ /%[-+ 0-9]?\.?[0-9]*([lh]?[duisSefgc]|s)/;
			# Allow overriding from the string database
			$cformat = ${$cformathash}{$id} if defined ${$cformathash}{$id};

			# Check if qt-format
			my $qtformat = 0;
			if (!$cformat)
			{
				# Regexp also matches %02d and friends, so don't do this test
				# if we already detected c-format.
				$qtformat = 1 if $string =~ /%[0-9]/;
			}

			# Check if python-format
			my $pythonformat = 0;
			if ($string =~ /%\((.+?)\)([-#0 +]?(?:\*|[0-9]+)?(?:\.(?:\*|[0-9]+))?[hlL]?[cdeEfFgGiosuxX])/g)
			{
				die "Mixing c-format and python-format specifiers for $id"
					if $cformat;
				die "Mixing qt-format and python-format specifiers for $id"
					if $qtformat;
				$pythonformat = 1;
			}

			# Opera-specific extension to be able to translate to and from
			# empty strings
			if ($caption eq '')
			{
				print POT "#. The original string is empty. To make the gettext tools understand that\n";
				print POT "#. the translated string should be empty and not be considered untranslated,\n";
				print POT "#. translate this into the string \"[]\".\n";
				$string = '[]';
			}

			# Print header
			if (defined $description && $description ne '')
			{
				if (length($description) > 75)
				{
					my @lines = &splitstring($description);
					foreach my $line (@lines)
					{
						chop $line while $line =~ /\s$/;
						print POT "#. $line\n";
					}
				}
				else
				{
					print POT "#. $description\n";
				}
			}

			if ($string =~ /^~~~/)
			{
				# Obsoletion marker
				substr($string, 0, 3) = '';
				print POT "#. This string has flagged as obsolete.\n";
			}

			my $scope = ${$scopehash}{$id};
			print POT "#. Scope: $scope\n" if $scope ne '';
			print POT "#: $id:",
				unpack('L', pack('l', $num)) & 0x7FFFFFFF, "\n";
			print POT "#, c-format\n" if $cformat;
			print POT "#, qt-format\n" if $qtformat;
			print POT "#, python-format\n" if $pythonformat;
			print POT qq'msgctxt "$id"\n';

			if (length($string) > 75 || $string =~ /\\n/)
			{
				# Split string into several lines
				print POT qq'msgid ""\n';
				my @lines = &splitstring($string);
				foreach my $line (@lines)
				{
					print POT qq'"$line"\n';
				}
			}
			else
			{
				print POT qq'msgid "$string"\n';
			}
			if ($generateen)
			{
				if (length($string) > 75 || $string =~ /\\n/)
				{
					# Split string into several lines
					print POT qq'msgstr ""\n';
					my @lines = &splitstring($string);
					foreach my $line (@lines)
					{
						print POT qq'"$line"\n';
					}
					print POT "\n";
				}
				else
				{
					print POT qq'msgstr "$string"\n\n';
				}
			}
			else
			{
				print POT qq'msgstr ""\n\n';
			}
		}
	}
	else
	{
		print "  Excluding $id (no included scope in ${$scopehash}{$id})\n"
			if $verbose > 0 && !defined $opt_b;
	}

	# Always count entry, even if it is not included
	$number ++;

	die "Too many warnings, please check settings (need \"-u\"?).\n" if $warnings > 50;
}

if (defined $opt_b)
{
	&handleunknowns($currentprefix);
}

close LNG if $generatelng;



if ($generatehdr)
{
	# Leftovers
	printf ENUMH $enumline, '' if $enumline ne '';
	print ENUMH "};\n\n";

	# Need 10 enumerations
	while ($enumnum ++ < 10)
	{
		print ENUMH "enum StringList$enumnum {};\n";
	}

	# Print end marker
	print ENUMH "// End of auto-generated content\n";
	close ENUMH;
}

if ($generatemap)
{
	# Create new locale-map include file
	print "Creating locale-map.inc\n" if $verbose > 0;
	open LOCMAP, ">${destdir}locale-map.inc"
		or die "Cannot create locale-map.inc: $!\n";
	push @generatedfiles, "${destdir}locale-map.inc";
	if ($need_binmode)
	{
		binmode LOCMAP, ":bytes";
	}

	print LOCMAP "// This file is generated by makelang.pl and should NOT be edited directly.\n";
	foreach my $map (sort { $a <=> $b } keys %locmap)
	{
		print LOCMAP "\t{ $map, Str::$locmap{$map} },\n";
	}
	print LOCMAP "// End of auto-generated content\n";

	close LOCMAP;
}

if ($generateplat)
{
	if($opt_l eq 'symbian')
	{
		#Closing the Output files
		close RA_FILE;
		close RLS_FILE;
		close STR_FILE;
		close H_FILE;
	}
	elsif($opt_l eq 'series60ui')
	{
		print S60_LDB_FILE "// Total number of strings:              $symbian_s60_stringcount\n";
		print S60_LDB_FILE "// Total number of fuzzy strings:        $symbian_s60_fuzzy_stringcount\n"
			if ($symbian_s60_fuzzy_stringcount > 0);
		print S60_LDB_FILE "// Total number of untranslated strings: $symbian_s60_untranslated_stringcount\n"
			if ($symbian_s60_untranslated_stringcount > 0);
		close S60_LDB_FILE;

		if ($symbian_s60_untranslated_stringcount > 0 || $symbian_s60_fuzzy_stringcount > 0)
		{
			my $u_strings = ($symbian_s60_untranslated_stringcount > 1 || $symbian_s60_untranslated_stringcount == 0) ? "strings" : "string";
			my $f_strings = ($symbian_s60_fuzzy_stringcount > 1 || $symbian_s60_fuzzy_stringcount == 0) ? "strings" : "string";
			print "Warning: Untranslated or fuzzy strings in '$opt_t'. See '$symbian_s60_ldb_file' for details\n" .
				  "         $symbian_s60_untranslated_stringcount untranslated $u_strings, " .
				  "$symbian_s60_fuzzy_stringcount fuzzy $f_strings\n";
		}
	}
	elsif ($opt_l eq 'binary')
	{
		&outputbinary;
	}
	else
	{
		# Close C++ output file
		close CPPLANG;
	}
}

# Write makefile with dependencies for later use
if ($generatemake)
{
	my $deps = '';
	if (defined $opt_t)
	{
		$deps .= $opt_t;
	}
	if (defined $opt_T)
	{
		$deps .= " ${opt_T}";
	}
	if (defined $opt_b)
	{
		$deps .= " ${opt_b}";
	}

	# Get a list of generated files (so far)
	my $filelist = join(' ', @generatedfiles);

	# Create the Makefile
	open MAKEFILE, ">$mkfilename"
		or die "Cannot create $mkfilename: $!";
	push @generatedfiles, $mkfilename;
	print MAKEFILE "${filelist}: ${deps} ${subdir}english.db $0\n";
	print MAKEFILE "\t$commandline\n\n";
	close MAKEFILE;
}

# Announce that we are done
if ($verbose >= 0)
{
	# List files generated
	print "The following files have been created:\n";

	if ($generatelng)
	{
		print "* $lngname\n";
	}

	if ($generatehdr)
	{
		print "* ${destdir}locale-enum.inc (compatibility level $opt_c)\n";
		print "* ${destdir}locale-dbversion.h\n";
	}

	if ($generatemap)
	{
		print "* ${destdir}locale-map.inc\n";
	}

	if ($generateplat)
	{
		if($opt_l eq 'symbian')
		{
			print "* $destdir$symbian_ra_file\n";
			print "* $destdir$symbian_rls_file\n";
			print "* $destdir$symbian_str_file\n";
			print "* $destdir$symbian_h_file\n";
		}
		elsif($opt_l eq 'series60ui')
		{
			print "* $destdir$symbian_s60_ldb_file\n";
		}
		elsif ($opt_l eq 'binary')
		{
			print "* $destdir$binaryname\n";
		}
		else
		{
			print "* $destdir$cppfile\n";
		}
	}

	if ($generatepot || $generateen)
	{
		print "* $potname\n";
	}

	if ($generatemake)
	{
		print "* $mkfilename\n";

	}

	# List special conditions
	if ($generatehdr && !defined $opt_d)
	{
		print "\nWarning: *.inc and *.h should be copied to the modules/locale/ directory to\n";
		print "take effect. Please use the -d parameter to do this automatically.\n";
	}

	if ($generateplat && !defined $opt_d)
	{
		print "\nWarning: The platform-specific files need to be copied to the appropriate\n";
		print "directories to take effect.\n";
	}

	if ($generatemake)
	{
		if (defined $ENV{'CALLED_THROUGH_MAKEFILE'})
		{
			print "\nReran with: ${commandline}\n";
			print "as specified by $mkfilename\n";
		}
		else
		{
			print "\nRun \"$0 -m";
			print " -d ${destdir}" if $destdir ne './';
			print "\"\n";
			print "to rerun with the supplied command line using the Makefile\n";
		}
	}
}

if ($warnings > 0)
{
	print $warnings, ' warnings';
	print ' (re-run without "-q" to see all)' if defined $opt_q;
	print ".\n";
}

if ($#temporaryfiles >= 0)
{
	if (unlink(@temporaryfiles) != $#temporaryfiles + 1)
	{
		print STDERR "Problems cleaning up: $!\n";
	}
}

sub diehandler
{
	print STDERR "Fatal error: ", shift;
	if ($#generatedfiles >= 0)
	{
		if (unlink(@generatedfiles) != $#generatedfiles + 1)
		{
			print STDERR "Problems cleaning up: $!\n";
		}
	}
	if ($#temporaryfiles >= 0)
	{
		if (unlink(@temporaryfiles) != $#temporaryfiles + 1)
		{
			print STDERR "Problems cleaning up: $!\n";
		}
	}
	exit 1;
}

sub HELP_MESSAGE
{
	#               1         2         3         4         5         6         7         8
	#      12345678901234567890123456789012345678901234567890123456789012345678901234567890
	print "Usage: makelang.pl [-b build.strings file] [-P [-i scopes|-i file]] [-e]\n";
	print "                   [-p platform,version,build] [-t translation] [-T override]\n";
	print "                   [-u] [-o file] [-l type] [-L] [-H] [-m] [-s] [-C]\n";
	print "                   [-c version] [-d dest] [-r rev] [-S source] [-v|-q]\n";
	print "                   [database files]\n";
	print "Creates various localization files from the string database.\n\n";
	print "--- Output selection ---\n";
	print "  -H         Generate header files (default).\n";
	print "  -L         Generate language files (LNG file or -l format files).\n";
	print "  -l type    Generate platforms specific files.\n";
	print "  -P         Generate translation template (POT).\n";
	print "  -e         Generate a fully populated en.po file.\n";
	print "  -m         Generate Makefile with dependencies for later use. Records the\n";
	print "             current set of parameters, and will force them on later -m calls.\n";

	print "\n--- Language and platform options ---\n";
	print "  -b strings Selects strings to include from a build.strings file\n";
	print "  -p p,v,b   Define target platform parameter. The parameter is a comma\n";
	print "             separated list with platform abbreviation, a build number\n";
	print "             and a version number. These numbers are only for display\n";
	print "             purposes, and not used by LanguageManager itself.\n";
	print "  -t file    Generate a translation with the specified .po file.\n";
	print "  -T file    PO file with translations to override from the -t file.\n";
	print "  -u         Do not warn about untranslated strings\n";
	print "  -B         Fatal error when string in build.strings is missing in database\n";
	print "  -i scopes  Select scopes for POT file (comma separated, or \"ALL\")\n";
	print "  -i file    Select scopes for POT file from file (one per line)\n";

	print "\n--- Output options ---\n";
	print "  -o file    Name of language file to create\n";
	print "  -s         Strip ampersands (&) and Asian-style hotkeys from menu items\n";
	print "  -k         Add comments/translation hints to *.lng files\n";
	print "  -E         Replace ellipsis (U+2026) with three full stops\n";
	print "  -U         Remap Romanian comma-below to cedilla-below\n";
	print "  -c version Select compatibility version for generated files\n";
 	print "  -d dest    Destination directory for generated files\n";

	print "\n--- Input options ---\n";
	print "  -r rev     Use this CVS/Git revision of english.db instead of current\n";
	print "  -R rev     Use this CVS/Git revision of the PO file(s) instead of current\n";
	print "  -S source  Source directory for input files\n";

	print "\n--- Debugging options ---\n";
	print "  -v | -q    Be verbose or be quiet\n";

	print "\n--- Database files ---\n";
	print "  A list of database files to read (defaults to english.db).\n";
	print "\n";

	print "Types for -l: \"qt\", \"kyocera\", \"gogi\", \"symbian\", \"series60ui\" and \"binary\".\n";
	print "Versions for -c: \"9\" (core-2).\n";
	exit 1;
}

sub djbhash
{
	my $id = shift;
	# djb2
	my $hash = 5381;
	foreach my $char (split('', $id))
	{
		$hash = $hash * 33 + ord($char);
	}
	# force 32-bit hash value in case perl is compiled with USE_64_BIT_INT
	return unpack("l", pack("L", $hash));
}

sub splitstring
{
	my $string = shift;
	my @output;

	SPLIT: while (length($string) > 75 || $string =~ /\\n/)
	{
		# Split at \n, or the last space before position 75.
		my $splitafter = index($string, '\n');
		if (-1 == $splitafter || $splitafter >= 75)
		{
			$splitafter = rindex($string, ' ', 75);
			if (rindex($string, '|', 75) > $splitafter)
			{
				$splitafter = rindex($string, '|', 75);
			}
			# Try searching forward if we didn't find a space
			$splitafter = index($string, ' ', 75) if -1 == $splitafter;
		}
		else
		{
			$splitafter ++; # Keep the "n" as well
		}
		last SPLIT if -1 == $splitafter;

		# Print up to the split point
		push @output, substr($string, 0, $splitafter + 1);
		$string = substr($string, $splitafter + 1);
	}
	push @output, $string if $string ne '';

	return @output;
}

# detects utf-16, utf-8 and 7-bit ascii
# returns: "us-ascii", "utf-8", "utf-16" or "x-unknown"
sub detect_encoding
{
	my $infile = shift;
	open INFILE, "<$infile"
		or return 'x-unknown';

	if ($need_binmode)
	{
		binmode INFILE, ":bytes";
	}

	# Check for UTF-16 BOM or nul bytes
	my $preamble;
	read INFILE, $preamble, 2;
	if ($preamble eq "\xFE\xFF" || $preamble eq "\xFF\xFE" ||
		$preamble =~ /\x00/) # i.e. "has a \x00 in it"
	{
		return 'utf-16';
	}
	else
	{
		seek INFILE, 0, 0; # No signature, go back to start of file
	}

	my $utf8 = q{ [\x00-\x7F]
		| [\xC2-\xDF][\x80-\xBF]
		| \xE0[\xA0-\xBF][\x80-\xBF]
		| [\xE1-\xEF][\x80-\xBF][\x80-\xBF]
		| \xF0[\x90-\xBF][\x80-\xBF][\x80-\xBF]
		| [\xF1-\xF7][\x80-\xBF][\x80-\xBF][\x80-\xBF]
		| \xF8[\x88-\xBF][\x80-\xBF][\x80-\xBF][\x80-\xBF]
		| [\xF9-\xFB][\x80-\xBF][\x80-\xBF][\x80-\xBF][\x80-\xBF]
		| \xFC[\x84-\xBF][\x80-\xBF][\x80-\xBF][\x80-\xBF][\x80-\xBF]
		| \xFD[\x80-\xBF][\x80-\xBF][\x80-\xBF][\x80-\xBF][\x80-\xBF]
	};

	my $valid_utf8 = 0;
	my $invalid_utf8 = 0;
	my $valid_ascii7 = 0;
	my $invalid_ascii7 = 0;

	while (my $line = <INFILE>)
	{
		# check for utf-8
		if   ($line !~ /^ (?:$utf8)* $/osx) { $invalid_utf8++; }
		else                                { $valid_utf8++;   }

		# check for 7-bit ascii
		if ($line =~ /^ ([\x00-\x7F])* $/x) { $valid_ascii7++;   }
		else                                { $invalid_ascii7++; }
	}

	close INFILE;

	return 'us-ascii' if $valid_ascii7 > 0 && $invalid_ascii7 == 0;
	return 'utf-8'    if $valid_utf8 > 0 && $invalid_utf8 == 0;
	return 'x-unknown';
}

# Output a binary language file
sub outputbinary
{
	# Format for the binary language files:
	#  FormatVersion: INT32 (also serves as byte order marker)
	#  DBVersion: INT32
	#  LanguageCodeLength: INT32
	#  LanguageCode: UTF16CHAR × LanguageCodeLength
	#  NumberOfEntries: INT32
	#  {
	#     StringId: INT32
	#     StringLength: INT32
	#     String: UTF16CHAR × StringLength
	#  } × NumberOfEntries
	my $targetenc = unpack ("c2", pack ("i", 1)) ? "utf-16le" : "utf-16be";
	my $converter = Text::Iconv->new('utf-8', $targetenc);

	open BIN, ">$binaryname"
		or die "Cannot create $binaryname: $!\n";
	push @generatedfiles, $binaryname;
	binmode BIN;
	my $ulangcode = $converter->convert($langcode);
	# FormatVersion, DBVersin, LanguageCodeLength
	print BIN pack('lll', 1, $dbversion, length($ulangcode) / 2);
	# LanguageCode
	print BIN $ulangcode;

	# NumberOfEntries
	print BIN pack('l', $languagestringscount);
	# Loop over entries
	foreach my $id (sort { $a <=> $b } keys %languagestrings)
	{
		# Remove escaped characters and convert to UTF-16
		my $string = $languagestrings{$id};
		$string =~ s/\\([\\nrt]|x..)/&unescape($1)/ge;
		my $ustring = $converter->convert($string);
		# StringId, StringLength
		print BIN pack('ll', $id, length($ustring) / 2);
		# String
		print BIN $ustring;
	}
	close BIN;
}

# Internal helper for unescaping strings
sub unescape
{
	my %seq = (
		"\\" => "\\",
		"n" => "\n",
		"r" => "\r",
		"t" => "\t",
	);
	my $esc = shift;

	return $seq{$esc} if defined $seq{$esc};

	if ($esc =~ /^x(..)/)
	{
		return chr(hex($1));
	}

	die "Internal error (unescape)";
}

# Load a PO file
sub loadpo
{
	my $filename = shift;
	my $revision = shift;

	# Cannot use parent directories in path when using CVS
	if (defined $revision && $filename =~ /^\.\./)
	{
		die "-R: Cannot use relative paths\n";
	}

	# Try to find the file if an unqualified file name was given
	if (! -f $filename)
	{
		$filename = $filename . '.po' if -e $filename . '.po';
		$filename = $filename . '/' . $filename . '.po'
			if -e $filename . '/' . $filename . '.po';
	}

	# Check out a local copy of the specified revision, if needed
	if (defined $revision)
	{
		my $tmpfilename = $filename . ".${revision}.$$";
		if (-d "${subdir}CVS")
		{
			open CVS, "cvs -q update -p -r$opt_R ${filename}|"
				or die "Unable to run cvs command: $!\n";
		}
		else
		{
			open CVS, "git show $opt_R:${filename}|"
				or die "Unable to run git command: $!\n";
		}
		open TMP, ">$tmpfilename"
			or die "Cannot write temporary file: ${tmpfilename}\n";
		push @temporaryfiles, $tmpfilename;
		while (<CVS>)
		{
			print TMP;
		}
		close CVS or die $! ? "Problems while running cvs command: $!\n"
							: "Exit status $? from cvs\n";
		close TMP;

		# Reference the version from CVS in error messages
		$filename = $tmpfilename;
	}

	print "Loading $filename\n" if $verbose > 0;

	# Sanity test: Make sure PO file is not UTF-16 encoded
	my $encoding = &detect_encoding($filename);
	die "$filename: File is UTF-16\n" if $encoding eq 'utf-16';

	# Read the translation from the PO file
	my $pohashref = Opera::PO->load_file_ashash($filename)
		or die "Cannot load $filename: $!\n";

	die "$filename: PO header missing\n"
		unless defined ${$pohashref}{'""'};

	return ($filename, $encoding, $pohashref);
}

# Print strings listed in build.strings that are not in the string
# database for this section, but only for internal language files
# (i.e when generating the .inc files). Warn for external language
# files.
sub handleunknowns
{
	my $prefix = shift;

	# Find unknown strings
	my %unknown;
	foreach my $key (keys %stringlist)
	{
		if ($stringlist{$key} == 1 && substr($key, 0, 1) eq $prefix)
		{
			my $warning = "Unknown string $key defined in $opt_b\n";
			die $warning if defined $opt_B;
			if ($generatehdr)
			{
				$unknown{$key} = 1;
			}
			else
			{
				print $warning;
				++ $warnings;
			}
		}
	}

	# Print unknowns to LNG file and header files
	my $unknowns = 0;
	foreach my $unknown (sort keys %unknown)
	{
		my $hashnum = &djbhash($unknown);

		if ($generatelng)
		{
			print LNG "\n; Unknown strings\n" if $unknowns ++ == 0;
			print LNG "${hashnum}=\"$unknown UNDEFINED\"\n";
		}

		if ($generatehdr)
		{
			printf ENUMH $enumline, ',' if $enumline ne '';
			$enumline = "$unknown = ${hashnum}%s ///< Undefined\n";
		}
		else
		{
			die "Internal error: unknowns ($unknown) not supported here\n";
		}
	}

	print "$unknowns unknown strings in section $prefix\n"
		if $unknowns > 0 && $verbose > 0;
}

# Parse a DB file, outputting the parsed output. Any parsing error is fatal
# and handled through die.
sub parsedb
{
	# Get parameters
	my ($dbfh, $dbfile) = @_;

	# Initialize variables
	my (@id, %num, %hashnum, %caption, %description, %scope, %cformat, %section);
	my ($id, $num, $hashnum, $caption, $description, $scope, $cformat);
	my $issection = 0;
	my $isend = 0;
	my $hasdbversion = 0;
	my $hastop = 0;
	my %name = ();
	my %number = ();
	my %hashed = ();
	my $currentprefix = '';

	# Read database
	print "Reading database\n" if $verbose > 0;
	while (<$dbfh>)
	{
		# remove CR LF or LF or CR safely even if a DOS file is
		# parsed on a Unix system or the other way round:
		s/\015?\012?$//;
		my ($newentry, $newsection, $newnum, $newhashnum);

		# New section encountered
		if (/^\[(.+)\]$/)
		{
			$newsection = $1;
			if ($newsection =~ /all start with ([A-Z])$/)
			{
				$currentprefix = $1;
			}
			$section{$currentprefix} = $newsection;
			$issection = 1;
		}
		# Caption
		elsif (/^(.*)\.caption="(.*)"$/)
		{
			if (!defined $id)
			{
				die "$dbfile: Stray caption line:\n$_";
			}
			elsif ($1 ne $id)
			{
				die "$dbfile: Mismatched ID: $id not matched by\n$_";
			}
			elsif (defined $caption)
			{
				die "$dbfile: Already have caption\n$_";
			}

			$caption = $2;

			if ($caption =~ /(\\[^trnx\\])/)
			{
				die "$dbfile: Invalid escape sequence \"$1\" in caption:\n$_";
			}
		}
		# Description
		elsif (/^(.*)\.description="\s*(.*)\s*"$/)
		{
			if (!defined $id)
			{
				die "$dbfile: Stray description line:\n$_";
			}
			elsif ($1 ne $id)
			{
				die "$dbfile: Mismatched ID: $id not matched by\n$_";
			}
			elsif (defined $description)
			{
				die "$dbfile: Already have description:\n$_";
			}

			$description = $2;
		}
		# C-format flag
		elsif (/^(.*)\.cformat="\s*(.*)\s*"$/)
		{
			if (!defined $id)
			{
				die "$dbfile: Stray description line:\n$_";
			}
			elsif ($1 ne $id)
			{
				die "$dbfile: Mismatched ID: $id not matched by\n$_";
			}
			elsif (defined $cformat)
			{
				die "$dbfile: Already have description:\n$_";
			}

			$cformat = $2;
		}
		# Scope
		elsif (/^(.*)\.scope="(.*)"$/)
		{
			if (!defined $id)
			{
				die "$dbfile: Stray scope line:\n$_";
			}
			elsif ($1 ne $id)
			{
				die "$dbfile: Mismatched ID: $id not matched by\n$_";
			}
			elsif (defined $scope)
			{
				die "$dbfile: Already have scope:\n$_";
			}

			$scope = $2;
		}
		# Id tag
		elsif (/^(.*)=(-?[0-9]+)$/)
		{
			die "$dbfile: ID outside a section: $_" if !$issection;
			$newentry = $1;
			die "$dbfile: String in wrong section (prefix should be \"$currentprefix\"):\n $_"
				unless $currentprefix eq substr($newentry, 0, 1);
			$newnum = $2;
			$newhashnum = &djbhash($newentry);
			if ($newnum == -1)
			{
				# Always use hashed number for unnumbered strings.
				$newnum = $newhashnum;
			}
			elsif ($newnum <= 0 || $newnum > 2147483647)
			{
				die "$dbfile: Invalid number $newnum for string $newentry\n";
			}
			elsif ($newhashnum == 0)
			{
				die "$dbfile: $newentry hashes to zero, please change identifier\n";
			}

			die "$dbfile: Invalid characters in entry $newentry\n"
				if $newentry !~ /^[A-Za-z0-9_]+$/;

			die "$dbfile: Number $newnum already defined as $name{$newnum} before\n$_"
				if defined $name{$newnum};
			$name{$newnum} = $newentry;
			die "$dbfile: Name $newentry already defined as $number{$newentry} before\n$_"
				if defined $number{$newentry};
			$number{$newentry} = $newnum;

			die "$dbfile: Hash collission between $newentry and $hashed{$newhashnum}\n$_"
				if defined $hashed{$newhashnum};
			$hashed{$newhashnum} = $newentry;

			if ($newnum > $highest && $newnum < 100000)
			{
				$highest = $newnum;
			}
		}
		# Database version
		elsif (/^\@dbversion\s+([0-9]+)$/)
		{
			die "$dbfile: DB version inside block:\n$_" if $hastop;
			die "$dbfile: DB version defined twice:\n$_" if $hasdbversion;

			$hasdbversion = $1;
		}
		# End of database marker
		elsif (/^\.end/)
		{
			$isend = 1;
		}
		# Improper line breaks
		elsif (/\r$/)
		{
			die "$dbfile: Unrecognized line terminator (Windows file on Unix, or vice versa?)";
		}
		# Unrecognized lines
		elsif (!/^(?:;.*)?$/)
		{
			die "$dbfile: Syntax error: \"$_\"";
		}

		# Remember completed entry when start of next entry is found
		if (($newentry || $newsection || $isend) && defined $id)
		{
			die "$dbfile: No scope defined for $id" unless defined $scope;
			die "$dbfile: No caption defined for $id" unless defined $caption;

			# Store away
			push @id, $id; # Remember ordering
			$num{$id} = $num;
			$hashnum{$id} = $hashnum;
			$caption{$id} = $caption;
			$description{$id} = $description;
			$scope{$id} = $scope;
			$cformat{$id} = $cformat if defined $cformat;

			undef $id;
			undef $num;
			undef $hashnum;
			undef $caption;
			undef $description;
			undef $scope;
			undef $cformat;
		}

		# Exit if the end of database marker is found
		last if $isend;

		# Remember data if a new entry was found
		if ($newentry)
		{
			$id = $newentry;
			$num = $newnum;
			$hashnum = $newhashnum;
		}

		# Print section header if a new section was found
		if ($newsection)
		{
			if (!$hastop)
			{
				die "$dbfile: No database version defined:\n$_" if !$hasdbversion;
				$hastop = 1;
			}

			print "  Section \"$newsection\"\n" if $verbose > 0;
		}
	}

	# Check for consistency
	die "$dbfile: Missing end of file marker in database\n" unless $isend;

	return ($hasdbversion, \@id, \%num, \%hashnum, \%caption, \%description, \%scope, \%section, \%cformat);
}

# Find self version
# Needed for --version
sub findversion
{
	my $path = $0;
	$path =~ s/makelang\.pl//;
	$path .= '../module.about';
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
