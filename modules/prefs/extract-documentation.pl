#!/usr/bin/perl -w
#								-*- tab-width: 4; -*-
#
# Extract preferences documentation
#
# Copyright (C) 2004-2012 Opera Software ASA.  All rights reserved.
#
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

# FIXME: This script should either be updated to generate data from the
# new txt source files directly, or generation of documentation should
# be integrated into the make-prefs.pl script.

# External code and variables
use Getopt::Std;
use vars qw/$opt_e $opt_c $opt_o/;
use strict;

require 5.006;

unless (getopts('eco:'))
{
	print "usage: $0 [-e] [-c] [files...]\n";
	print "Extract preferences documentation.\n\n";
	print "Please run this script from the top-level source directory!\n\n";
	print "files  Files to extract from (if unspecified tries to detect)\n";
	print "  -e   Adjust headers for external documentation.\n";
	print "  -c   Collect all documentation into one list.\n";
	print "  -o   Specify top-level directory for generated files.\n";
}

my $outfile = 'modules/prefs/documentation/settings.html';
if ($opt_e)
{
	$outfile = 'settings.html';
}
my $outroot = '.';
if ($opt_o)
{
	$outroot = $opt_o;
}

# List of collections to extract data from (.cpp and .h are appended later)
my %collections = (
	'modules/prefs/prefsmanager/collections/pc_app'	   => 'External applications',
	'modules/prefs/prefsmanager/collections/pc_core'	   => 'Core preferences',
	'modules/prefs/prefsmanager/collections/pc_display'   => 'Display settings',
	'modules/prefs/prefsmanager/collections/pc_doc'	   => 'Document settings',
	'modules/prefs/prefsmanager/collections/pc_files'     => 'Files and paths',
	'modules/prefs/prefsmanager/collections/pc_fontcolor' => 'Fonts and colors',
	'modules/prefs/prefsmanager/collections/pc_geoloc'    => 'Geolocation settings',
	'modules/prefs/prefsmanager/collections/pc_jil'     => 'JIL API',
	'modules/prefs/prefsmanager/collections/pc_js' 	   => 'JavaScript',
#	'modules/prefs/prefsmanager/collections/pc_m2' 	   => 'M2 settings',
#	'modules/prefs/prefsmanager/collections/pc_macos'     => 'MacOS specific',
#	'modules/prefs/prefsmanager/collections/pc_mswin'     => 'Windows specific',
	'modules/prefs/prefsmanager/collections/pc_network'   => 'Networking',
	'modules/prefs/prefsmanager/collections/pc_opera_account' => 'Opera Account',
	'modules/prefs/prefsmanager/collections/pc_print'     => 'Printing',
	'modules/prefs/prefsmanager/collections/pc_sync' 	   => 'Sync settings',
	'modules/prefs/prefsmanager/collections/pc_tools'     => 'Developer tools settings',
	'modules/prefs/prefsmanager/collections/pc_unix'	   => 'Unix specific',
#	'modules/prefs/prefsmanager/collections/pc_ui' 	   => 'User interface',
#	'modules/prefs/prefsmanager/collections/pc_voice'     => 'Voice browser',
	'modules/prefs/prefsmanager/collections/pc_webserver' => 'Web server',
);

# Prepare output
open my $docfile, '>', "$outroot/$outfile" or die "Cannot write $outfile: $!\n";

# Retrieve the section name mapping
my %section = &getsectionmapping('modules/prefs/prefsmanager/opprefscollection');

# Retrieve the default values from module.tweaks and erasettings.h
my %defaults = &getdefaults('modules/prefs/module.tweaks', 'modules/prefs/prefsmanager/collections/erasettings.h');

&htmlhead($docfile);

my $total = 0;
my $settings;

foreach my $collection (@ARGV ? @ARGV : sort keys %collections)
{
	$settings = &getcollection($collection, \%section, \%defaults, $settings);
	my $shortname = $collection;
	$shortname =~ s#.*/##g;
	$total += &dumpsettings($docfile, defined $collections{$shortname} ? $collections{$shortname} : $shortname, $settings)
		if !$opt_c;
}

if ($opt_c)
{
	$total = dumpsettings($docfile, 'All settings', $settings);
}

&htmlfoot($docfile, $total);
close $docfile;

# ------------------------------------------------------------------------
# getsectionmapping
# Read the section name mapping as defined by OpPrefsCollection
sub getsectionmapping
{
	my $filename = shift;
	my %mapping;

	# The enumeration is defined in the header file
	my $hfilename = $filename . '.h';
	$hfilename =~ s#src/(?!.*/)]##g if ! -e $hfilename;
	open my $hfile, '<', $hfilename or die "Cannot read ${hfilename}: $!\n";
	SKIP: while (<$hfile>)
	{
		last SKIP if /enum IniSection/;
	}

	# The section names are defined in the C++ file
	open my $cfile, '<', $filename . '.cpp' or die "Cannot read ${filename}.cpp: $!\n";
	SKIP: while (<$cfile>)
	{
		last SKIP if /INITSECTIONS/;
	}

	# Map the enumerations in the header file to the names in the C++ file
	# and store them in a hash
	MATCH: while (1)
	{
		my ($hline, $cline);
		my ($doneh, $donec) = (0, 0);

		# Find next interesting line in the header file
		FINDH: while (<$hfile>)
		{
			if (/^\s+(S[A-Za-z0-9_]+)(?:\s*=\s*[0-9]+)?[,\s]/)
			{
				$hline = $1;
				$doneh = 1 if $hline eq 'SNone'; # Sentinel value
				last FINDH;
			}
		}
		die "Ran out of input data in ${hfilename}\n" unless defined $hline;

		# Find next interesting line in the C++ file
		FINDC: while (<$cfile>)
		{
			if (/^\s+K\("(.*)"\)/)
			{
				$cline = $1;
				last FINDC;
			}
			elsif (/^}/)
			{
				$donec = 1;
				last FINDC;
			}
		}
		die "Ran out of input data in ${filename}.cpp\n"
			unless defined $cline || $donec;

		# Sanity checks
		die "${filename}.cpp defines more sections than ${hfilename}"
			if $doneh && !$donec;
		die "${filename}.cpp defines less sections than ${hfilename}"
			if $donec && !$doneh;
		last MATCH if $donec && $doneh;

		# Store
		$mapping{$hline} = $cline;
	}

	close $cfile;
	close $hfile;
	return %mapping;
}

# ------------------------------------------------------------------------
# getdefaults
# Read the default values from the tweaks file and erasettings.h
sub getdefaults
{
	my %mapping;

	# Read tweaks
	my $tweaksfilename = shift;

	open my $tweakfile, '<', $tweaksfilename or die "Cannot read ${tweaksfilename}: $!\n";
	my ($tweak, $define, $value);
	while (<$tweakfile>)
	{
		if (/^(TWEAK\S+)\s/)
		{
			# Remove knowledge of previous setting, so that we properly
			# ignore "features light"-type settings.
			undef $tweak;
			undef $define;
			undef $value;

			# Remember the name
			$tweak = $1;
		}
		elsif (/Define\s+:\s+([A-Za-z0-9_]+)\s*$/)
		{
			# Remember the preprocessor define used
			$define = $1;
		}
		elsif (/Value\s+:\s+(.*?)\s*$/)
		{
			# Remember the value
			$value = $1;
			$value =~ s/UNI_L\((.*)\)/$1/; # Kill off UNI_L() construct
		}

		# If we have all the data we need, record it
		if (defined $tweak && defined $define && defined $value)
		{
			$mapping{$define} = $value;

			# Forget about this
			undef $tweak;
			undef $define;
			undef $value;
		}
	}

	close $tweakfile;

	# Read erasettings.h
	my $erafilename = shift;
	open my $erafile, '<', $erafilename or die "Cannot read ${erafilename}: $!\n";
	my $include = 0;
	while (<$erafile>)
	{
		$include = 1, next if / \(default\) / or / \(use tweak value directly\) /;
		$include = 0 if m"^//";
		next unless $include;

		if (/^#\s*define\s+(\S+)\s+(\S+)$/)
		{
			my ($define, $value) = ($1, $2);
			$value = $mapping{$value} if defined $mapping{$value};
			$mapping{$define} = $value;
		}
	}

	close $erafile;

	return %mapping;
}

# ------------------------------------------------------------------------
# getcollection
# Read all settings defined by a collection
sub getcollection
{
	my ($filename, $sections, $defaults, $settings) = @_;
	my %fontcolordoc = $filename =~ 'font' ? &readfontcolordoc : ();
	my %setting;

	# Read the enumerations
	my (@stringprefs, @integerprefs, @fileprefs, @colorprefs);
	my (@stringdocs,  @integerdocs,  @filedocs,  @colordocs);
	my $hfile;
	open $hfile, '<', "$outroot/${filename}_h.inl"
		or die "Cannot read $outroot/${filename}_h.inl: $!\n";
	while (<$hfile>)
	{
		my $ignore = 0;
		if (/enum (string|integer|file|customcolor)pref/)
		{
			# Read the enumeration list. Since the lists are all written
			# the same way, they can be handled by the same code.

			my ($prefarr, $docarr);
			if ($1 eq 'string')
			{
				$prefarr = \@stringprefs;
				$docarr = \@stringdocs;
			}
			elsif ($1 eq 'integer')
			{
				$prefarr = \@integerprefs;
				$docarr = \@integerdocs;
			}
			elsif ($1 eq 'file')
			{
				$prefarr = \@fileprefs;
				$docarr = \@filedocs;
			}
			elsif ($1 eq 'customcolor')
			{
				$prefarr = \@colorprefs;
				$docarr = \@colordocs;
			}

			PREF: while (<$hfile>)
			{
				$ignore = 0 if /^#endif/;
				$ignore = 1 if /^#if\s+0/;
				next if $ignore;
				next if /^\s*\/\//;
				last PREF if /^\s*}/;
				if (m'^\s+([A-Za-z0-9_]+)(?:\s*=\s*[0-9]+)?\s*,\s*(?:///< (.*)$)?')
				{
					push @$prefarr, $1;
					push @$docarr,  $2;
				}
			}
		}
	}
	close $hfile;

	# Read the definitions
	open my $cfile, '<', "$outroot/${filename}_c.inl"
		or die "Cannot read $outroot/${filename}_c.inl: $!\n";
	while (<$cfile>)
	{
		if (/(?:INIT(STRINGS|INTS|CUSTOMCOLORS)|const struct PrefsCollection.*::(string|integer)prefdefault)/)
		{
			# Handle the list of integer and string preferences, including
			# default values. Since both are three-column lists, the code
			# can be merged into one routine.

			my $type;
			if (defined $2)
			{
				$type = $2;
			}
			else
			{
				$type = $1 eq 'STRINGS' ? 'string' : 'integer';
				$type = 'color' if $1 eq 'CUSTOMCOLORS';
			}
			my ($prefarr, $docarr);
			my $ignore = 0;
			if ($type eq 'string')
			{
				$prefarr = \@stringprefs;
				$docarr = \@stringdocs;
			}
			elsif ($type eq 'integer')
			{
				$prefarr = \@integerprefs;
				$docarr = \@integerdocs;
			}
			elsif ($type eq 'color')
			{
				$prefarr = \@colorprefs;
				$docarr = \@colordocs;
			}

			PREF: while (<$cfile>)
			{
				next if /^\s*\/\//;
				next if /INIT(START|END)/;
				$ignore = 0 if /^#endif/;
				$ignore = 1 if /^#if\s+0/;
				next if $ignore;
				last PREF if /\(\s*SNone/;

				# P(Section, Key, Value)
				if (/P\(\s*(S[A-Za-z0-9_]+)\s*,\s*"([^"]+)",\s*(.*)\s?\)/)
				{
					die "Unknown section '$1'" unless defined $sections->{$1};
					die "Too much data in ${filename}.cpp: $_\n"
						if -1 == $#$prefarr;

					my ($section, $key, $value) = ($sections->{$1}, $2 ,$3);
					my ($name, $doc) = (shift @$prefarr, shift @$docarr);
					$value =~ s/\s+$//; # Kill the final whitespace, if any
					$value =~ s/UNI_L\((.*)\)/$1/; # Kill off UNI_L() construct
					$value =~ s/^\(int\)\s+//; # Kill type cast

					my $istweakable;
					if (($istweakable = defined $defaults->{$value}))
					{
						$value = $defaults->{$value};
					}

					my $hashkey = $section . '|' . $key;
					if (defined $setting{$hashkey})
					{
						warn "[$section] $key defined in both " .
						     $setting{$hashkey}[4] . " and $filename\n";
					}
					$settings->{$hashkey} =
						[ $type, $value, $name, $doc, $filename, $istweakable ];
				}
				# I(Section, Key, Value, Type)
				elsif (/I\(\s*(S[A-Za-z0-9_]+)\s*,\s*"([^"]+)",\s*(.*)\s*,\s*prefssetting::(integer|boolean)\s*\)/)
				{
					die "Unknown section '$1'" unless defined $sections->{$1};
					die "Too much data in ${filename}.cpp: $_\n"
						if -1 == $#$prefarr;

					my ($section, $key, $value, $localtype) = ($sections->{$1}, $2 ,$3, $4);
					my ($name, $doc) = (shift @$prefarr, shift @$docarr);
					$value =~ s/\s+$//; # Kill the final whitespace, if any
					$value =~ s/UNI_L\((.*)\)/$1/; # Kill off UNI_L() construct
					$value =~ s/^\(int\)\s+//; # Kill type cast

					my $istweakable;
					if (($istweakable = defined $defaults->{$value}))
					{
						$value = $defaults->{$value};
					}

					if ($localtype eq 'boolean')
					{
						if ($value eq '0' || lc($value) eq 'false')
						{
							$value = 'false (0)';
						}
						elsif ($value eq '1' || lc($value) eq 'true')
						{
							$value = 'true (1)';
						}
						elsif ($value =~ /^[0-9]+$/)
						{
							die "Illegal boolean value $value for $key in ${filename}.cpp";
						}
					}

					my $hashkey = $section . '|' . $key;
					if (defined $setting{$hashkey})
					{
						warn "[$section] $key defined in both " .
						     $setting{$hashkey}[4] . " and $filename\n";
					}
					$settings->{$hashkey} =
						[ $localtype, $value, $name, $doc, $filename, $istweakable ];
				}
				# CC(Section, Key, Value)
				elsif (/CC\(\s*(S[A-Za-z0-9_]+)\s*,\s*"([^"]+)",\s*(.*)\s*\)/)
				{
					die "Unknown section '$1'" unless defined $sections->{$1};
					die "Too much data in ${filename}.cpp: $_\n"
						if -1 == $#$prefarr;

					my ($section, $key, $value) = ($sections->{$1}, $2 ,$3);
					my ($name, $doc) = (shift @$prefarr, shift @$docarr);
					$value =~ s/\s+$//; # Kill the final whitespace, if any
					$value =~ s/UNI_L\((.*)\)/$1/; # Kill off UNI_L() construct
					$value =~ s/^\(int\)\s+//; # Kill type cast

					my $istweakable;
					if (($istweakable = defined $defaults->{$value}))
					{
						$value = $defaults->{$value};
					}

					my $hashkey = $section . '|' . $key;
					if (defined $setting{$hashkey})
					{
						warn "[$section] $key defined in both " .
						     $setting{$hashkey}[4] . " and $filename\n";
					}
					$settings->{$hashkey} =
						[ 'color', $value, $name, $doc, $filename, $istweakable ];
				}
			}
			die("Too little data in ${filename}.cpp: " .
			    $prefarr->[0] . " not matched\n") if $#$prefarr != -1;

		}
		elsif (/INITFILES/)
		{
			# Special handling of file preferences, which are written
			# in a five-column list.

			my $ignore = 0;
			PREF: while (<$cfile>)
			{
				last PREF if /\(\s*SNone/;
				next if /^\s*\/\//;
				next if /INIT(START|END)/;
				$ignore = 0 if /^#endif/;
				$ignore = 1 if /^#if\s+0/;
				next if $ignore;

				# F(Section, Key, Folder, Filename)
				if (/F\(\s*(S[A-Za-z0-9_]+)\s*,\s*"([^"]+)",\s*(?:\/\*[^*]+\*\/)?\s*(OPFILE_[A-Z_]+_FOLDER|OpFileFolder\(-1\)),\s*(.*)\s*,.*\s?\)/)
				{
					die "Unknown section '$1'" unless defined $sections->{$1};
					die "Too much data in ${filename}.cpp: $_\n"
						if -1 == $#fileprefs;

					my ($section, $key, $folder, $value) = ($sections->{$1}, $2 ,$3, $4);
					my ($name, $doc) = (shift @fileprefs, shift @filedocs);
					$value =~ s/\s+$//; # Kill the final whitespace, if any
					$value =~ s/UNI_L\("(.*)"\)/$1/; # Kill off UNI_L() construct

					my $istweakable;
					if (($istweakable = defined $defaults->{$value}))
					{
						$value = $defaults->{$value};
					}

					my $hashkey = $section . '|' . $key;
					if (defined $setting{$hashkey})
					{
						warn "[$section] $key defined in both " .
						     $setting{$hashkey}[4] . " and $filename\n";
					}
					my $path = "{$folder}/$value";
					$path = "unknown" if $value eq 'NULL' || $folder eq 'OpFileFolder(-1)';
					$settings->{$hashkey} =
						[ 'file', $path, $name, $doc, $filename, $istweakable ];
				}
			}
		}
		elsif (/INITFOLDERS/)
		{
			# Special handling of directory preferences, which do
			# not have any compiled-in defaults.

			my $doc;

			DIR: while (<$cfile>)
			{
				last DIR if /INITEND/;
				chomp;

				# Read the directory definitions and their documentation.
				if (m'^\t// (.*)$')
				{
					if (defined $doc)
					{
						$doc .= " $1";
					}
					else
					{
						$doc = $1;
					}
				}
				elsif (m'\tD\(\s*(OPFILE_.*_FOLDER),\s*(S[A-Za-z0-9_]+),\s*"([^"]*)"\s*\)')
				{
					die "Unknown section '$2'" unless defined $sections->{$2};

					my ($folder, $section, $key) = ($1, $sections->{$2}, $3);
					my $hashkey = $section . '|' . $key;
					if (defined $setting{$hashkey})
					{
						warn "[$section] $key defined in both " .
						     $setting{$hashkey}[4] . " and $filename\n";
					}
					$settings->{$hashkey} =
						[ 'directory', '', $folder, $doc, '&ndash;', 0 ];
					undef $doc;
				}
			}

			die("Too little data in ${filename}.cpp: " .
			    $fileprefs[0] . " not matched\n") if $#fileprefs != -1;
		}
	}
	close $cfile;

	# Read special documentation comments in the cpp files.
	# Handle hardcoded font and color preferrences
	open my $cfile2, '<', "${filename}.cpp"
		or die "Cannot read ${filename}.cpp: $!\n";
	while (<$cfile2>)
	{
		if (/INIT(FONTS|COLORS)/)
		{
			# Special handling of font and color settings, which use
			# system enumerations and define a section at a time.

			my $type = $1 eq 'FONTS' ? 'font' : 'color';
			my $ignore = 0;

			PREF: while (<$cfile2>)
			{
				last PREF if /};/;
				next if /^\s*\/\//;
				next if /INIT(START|END)/;
				$ignore = 0 if /^#endif/;
				$ignore = 1 if /^#if\s+0/;
				next if $ignore;

				# S(OP_SYSTEM_FONT_*, name) and C(OP_SYSTEM_COLOR_*, name)
				if (/[SC]\(\s*(OP_SYSTEM_(?:FONT|COLOR)_[A-Za-z0-9_]+),\s*(?:UNI_L\()?"([^"]+)"/)
				{
					my ($enum, $setting) = ($1, $2);
					my ($name, $doc);
					if ($type eq 'color')
					{
						$name = $sections->{'SColors'} . '|' . $setting;
						$doc = $fontcolordoc{$enum};
					}
					else
					{
						$name = $sections->{'SFonts'} . '|' . $setting;
						$doc = $fontcolordoc{$enum};
					}
					if (defined $setting{$name})
					{
						warn "$name defined in both " .
						     $setting{$name}[4] . " and $filename\n";
					}
					$settings->{$name} = [ $type, undef, $enum, $doc, $filename, 0 ];
				}
				# Hackery:
				# C(static_cast<OP_SYSTEM_COLOR>(CSS_VALUE_*), name)
				elsif (/C\(\s*static_cast<OP_SYSTEM_COLOR>\((CSS_VALUE_([A-Za-z0-9_]+))\),\s*(?:UNI_L\()?"([^"]+)"/)

				{
					my ($enum, $setting) = ($1, $3);
					my $doc = "Color for CSS class $2";
					my $name = $sections->{'SColors'} . '|' . $setting;
					$settings->{$name} = [ 'color', undef, $enum, $doc, $filename, 0 ];
				}
			}
		}
		elsif (m"^\s*/\*DOC$") # " # un-confuse emacs font-lock !
		{
			# Special documentation for dynamic sections

			my ($section, $name, $key, $type, $value, $description);
			PREFDOC: while (<$cfile2>)
			{
				last PREFDOC if m"\*/";
				if (/\*([a-z]+)=(.*)$/)
				{
					$section     = $2 if $1 eq 'section';
					$name        = $2 if $1 eq 'name';
					$key         = $2 if $1 eq 'key';
					$type	     = $2 if $1 eq 'type';
					$value       = $2 if $1 eq 'value';
					$description = $2 if $1 eq 'description';
				}
			}

			if (defined $section && defined $key && defined $type &&
			    defined $value && defined $description)
			{
				my $hashkey = $section . '|&lt;' . $key . '&gt;';
				if (defined $setting{$hashkey})
				{
					warn "[$section] $key defined in both " .
					     $setting{$hashkey}[4] . " and $filename\n";
				}
				$settings->{$hashkey} =
					[ $type, $value, $name, $description, $filename, 0 ];
			}
			else
			{
				warn "Incomplete doc comment in $filename";
			}
		}
	}
	close $cfile;

	return $settings;
}

# ------------------------------------------------------------------------
# readfontcolordoc
# Read documentation for system fonts and colors from OpSystemInfo,
# if we can

sub readfontcolordoc
{
	my %doc;

	my $pifile;
	if (open $pifile, '<', 'modules/pi/ui/OpUiInfo.h' or
	    open $pifile, '<', 'modules/pi/OpSystemInfo.h')
	{
		while (<$pifile>)
		{
			if (/enum\s+OP_SYSTEM_COLOR\s+{/)
			{
				COLOR: while (<$pifile>)
				{
					last COLOR if /};/;
					next if /^\s*\/\//;
					if (m'(OP_SYSTEM_COLOR_[A-Za-z1-9_]+),?\s*///< (.*)$')
					{
						$doc{$1} = $2;
					}
				}
			}
			elsif (/enum\s+OP_SYSTEM_FONT\s+{/)
			{
				FONT: while (<$pifile>)
				{
					last FONT if /};/;
					next if /^\s*\/\//;
					if (m'(OP_SYSTEM_FONT_[A-Za-z1-9_]+),?\s*///< (.*)$')
					{
						$doc{$1} = $2;
					}
				}
			}
		}
	}
	else
	{
		warn "Cannot read modules/pi/OpSystemInfo.h: $!\n";
	}
	return %doc;
}

# ------------------------------------------------------------------------
# dumpsettings
# Dump all settings for a collection in HTML format
sub dumpsettings
{
	my ($fh, $collection, $settings) = @_;

	# HTML heading
	my $id = $collection;
	$id =~ s/ //g;
	$id =~ tr/A-Z/a-z/;
	print $fh " <h2 id=\"$id\">$collection</h2>\n";
	print $fh " <table>\n";
	print $fh '  <tr class="head"><th>Setting<th>Type<th>Default value<th>Name<th>Description';
	print $fh "<th>Collection" if $opt_c;
	print $fh "\n";

	my $currentsection = '';
	my $num;
	my $total = 0;
	my $colspan = $opt_c ? 6 : 5;

	# Iterate over all the stored settings and display them
	foreach my $setting (sort keys %$settings)
	{
		my ($section, $key) = split /\|/, $setting;
		if ($section ne $currentsection)
		{
			if ($opt_c && $total)
			{
				print $fh " </table>\n";

				print $fh " <table>\n";
				print $fh "  <tr class='head'><th>Setting<th>Type<th>Default value<th>Name<th>Description<th>Collection\n";
			}
			print $fh "  <tr class='section'><th colspan=${colspan}>[$section]\n";
			$currentsection = $section;
			$num = 0;
		}

		# Retrieve and display
		my ($type, $value, $name, $doc, $filename, $tweakable) =
			@{$settings->{$setting}};

		$doc = '<em>Undocumented!</em>' if !defined $doc;
		$value = '<em>Unknown</em>' if !defined $value;

		$value =~ s/(_|::)/$1&shy;/g; # Allow linebreaks
		$name  =~ s/([a-z])([A-Z])/$1&shy;$2/g; # Allow linebreaks

		$filename =~ s#^.*/##g;
		$filename =~ s/pc_//;

		my $valueclass = '';
		$valueclass = ' class="tweak"' if $tweakable;

		print $fh "  <tr><td>$key<td>$type<td$valueclass>$value<td>$name<td>$doc";
		print $fh "<td>$filename" if $opt_c;
		print $fh "\n";
		++ $total;
	}
	print $fh " </table>\n";

	%$settings = ();
	return $total;
}

# ------------------------------------------------------------------------
# htmlhead
# Print a HTML header for the document
sub htmlhead
{
	my $fh = shift;
	my ($css, $link);

	if ($opt_e)
	{
		$css = 'coredoc/coredoc.css';
		$link = '';
	}
	else
	{
		$css = '../../coredoc/coredoc.css';
		$link = '<link rel="contents" href="../../coredoc/index.html" type="text/html" title="Core API">';
	}

	print $fh <<"EOM";
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN">
<html lang="en" dir="ltr">
<head>
 <meta http-equiv="Content-Type" content="text/html;charset=iso-8859-1">
 <title>Opera preferences</title>
 <link rev="made" href="mailto:peter\@opera.com">
 <link rel="stylesheet" href="${css}" type="text/css" media="all">
 ${link}
 <style type="text/css">
  tr.section th { padding-left: 2em; }
  .tweak { color: navy; font-weight: bold; }
  tr:nth-child(odd) td { background: #ffe; }
  tr:nth-child(even) td { background: #ffc; }
  ul#tocUl { list-style: none; padding-left: 0; }
 </style>
 <script type="text/javascript" src="../../coredoc/createtoc.js"></script>
</head>

<body>
 <h1>Opera preferences</h1>
 <p id="toc">
  <!-- To regenerate: make -C ../prefsmanager/collections
	   or run modules/prefs/extract-documentation.pl -->
  Settings <span class="tweak">marked like this</span> are controlled by
  the tweaks system, and may vary between builds.
 </p>
EOM
} # "} # un-confuse emacs font-lock !

# ------------------------------------------------------------------------
# htmlfoot
# Print a HTML footer for the document
sub htmlfoot
{
	my ($fh, $total) = @_;
	my $time = scalar(localtime);

	print $fh " <p>Generated on $time; $total preferences listed.</p>\n";
	print $fh "</body>\n</html>\n";
}
