#!/usr/bin/perl -w

# Create mappings between Unicode codepoints and the X font encodings that
# contain them
# Copyright 2001 Opera Software ASA
# Written by Peter Krefting

# External code and variables
use Getopt::Std;
use vars qw/$opt_c $opt_r $opt_h/;
use integer;

# Sanity check
unless (-e 'sources/big5.txt')
{
	print STDERR "You must start this script from the directory it is placed in.\n";
	exit;
}

# Check parametners
unless (getopts('crh:'))
{
	print "Usage: $0 [-c|-r|-h nameslist]\n";
	print "Creates a mapping table from Unicode to X Windows encodings.\n\n";
	print "  -c   Create a C++ style table with mappings.\n";
	print "  -r   Raw list usable for piping through uniq to check number of intervals.\n";
	print "  -h   Create rally human readable list using specified NamesList.txt\n";
	exit;
}

if (defined $opt_h && ! -e $opt_h)
{
	print "Cannot find $opt_h: $!\n";
	exit;
}

# Map from X encoding (as given in fonts) to the files containing their
# definitions.
%xencodings = (
	"big5-0" => "big5.txt",
	"cns11643.1992-1" => "cns11643.txt",
#	"dec-dectech"
	"gb2312.1980-0"	=> "gb2312.txt",
	"iso646.1991-irv" => "ascii",
	"iso8859-1" => "iso8859-1",
	"iso8859-2" => "8859-2.txt",
	"iso8859-3" => "8859-3.txt",
	"iso8859-4" => "8859-4.txt",
	"iso8859-5" => "8859-5.txt",
	"iso8859-6" => "8859-6.txt",
	"iso8859-7" => "8859-7.txt",
	"iso8859-8" => "8859-8.txt",
	"iso8859-9" => "8859-9.txt",
	"iso8859-10" => "8859-10.txt",
	"iso8859-11" => "iso8859-11.txt",
	"iso8859-13" => "8859-13.txt",
	"iso8859-14" => "8859-14.txt",
	"iso8859-15" => "8859-15.txt",
	"windows-1250" => "cp1250.txt",
	"windows-1251" => "cp1251.txt",
	"windows-1252" => "cp1252.txt",
	"windows-1253" => "cp1253.txt",
	"windows-1254" => "cp1254.txt",
	"windows-1255" => "cp1255.txt",
	"windows-1256" => "cp1256.txt",
	"windows-1257" => "cp1257.txt",
	"windows-1258" => "cp1258.txt",
	"jisx0201.1976-0" => "jis0201.txt",
	"jisx0208.1983-0" => "jis0208.txt",
	"jisx0212.1990-0" => "jis0212.txt",
	"koi8-r" => "koi8-r.txt",
#	"koi8-1" is a subset
	"ksc5601.1987-0" => "ksc5601.txt",
	"ksx1001.1997-0" => "ksx1001.txt",
#	"omron_udc_zh-0"
#	"sisheng_cwnn-0"
#	"tis620.2529-1"
	"viscii1.1-1" => "viscii.txt",
);

# Map from X encodings (as given in fonts) to the constants used in the C code
%cmapping = (
	"big5-0" => "BIG5",
	"cns11643.1992-1" => "CNS11643_PLANE1",
	"cns11643.1992-2" => "CNS11643_PLANE2",
	# plane 3-13 not in 16-bit Unicode
	"cns11643.1992-14" => "CNS11643_PLANE14",
#	"dec-dectech"
	"gb2312.1980-0"	=> "GB2312",
	"iso646.1991-irv" => "ASCII",
	"iso8859-1" => "ISO8859_1",
	"iso8859-2" => "ISO8859_2",
	"iso8859-3" => "ISO8859_3",
	"iso8859-4" => "ISO8859_4",
	"iso8859-5" => "ISO8859_5",
	"iso8859-6" => "ISO8859_6",
	"iso8859-7" => "ISO8859_7",
	"iso8859-8" => "ISO8859_8",
	"iso8859-9" => "ISO8859_9",
	"iso8859-10" => "ISO8859_10",
	"iso8859-11" => "ISO8859_11",
	"iso8859-13" => "ISO8859_13",
	"iso8859-14" => "ISO8859_14",
	"iso8859-15" => "ISO8859_15",
	"windows-1250" => "CP1250",
	"windows-1251" => "CP1251",
	"windows-1252" => "CP1252",
	"windows-1253" => "CP1253",
	"windows-1254" => "CP1254",
	"windows-1255" => "CP1255",
	"windows-1256" => "CP1256",
	"windows-1257" => "CP1257",
	"windows-1258" => "CP1258",
	"jisx0201.1976-0" => "JIS0201",
	"jisx0208.1983-0" => "JIS0208",
	"jisx0212.1990-0" => "JIS0212",
	"koi8-r" => "KOI8R",
#	"koi8-1" is a subset
	"ksc5601.1987-0" => "KSC5601",
	"ksx1001.1997-0" => "KSX1001",
#	"omron_udc_zh-0"
#	"sisheng_cwnn-0"
#	"tis620.2529-1"
	"viscii1.1-1" => "VISCII",
);

# Supplementary C encodings
%cmappingextra = (
	"big5-0" => "big5p-0 big5.eten-0",
	"gb2312.1980-0" => "gb2312.80 gb2312.80&gb8565.88-0",
	"jisx0208.1983-0" => "jisx0208.1990-0 jisc6226.1978-0",
	"koi8-r" => "koi8-1",
);

# Initialize codepoint table
%unicode = ();

for (my $i = 0; $i < 65536; $i ++)
{
	$unicode{$i} = "";
}

# Read data
foreach $charset (sort keys %xencodings)
{
	my $file = $xencodings{$charset};
	my $hasascii = 
		$charset ne "gb2312.1980-0" && $charset !~ /^jis/ &&
	    $charset !~ /^ks[cx]/ && $charset ne "big5-0";

	# CNS 11643 is special
	if ("cns11643.1992-1" eq $charset)
	{
		open MAPPING, "sources/$file" or die "Cannot read file $file: $!\n";
		while (<MAPPING>)
		{
			if (/^(0x[12E])[0-9A-Fa-f]{4}\s+(0x[0-9A-Fa-f]{4})/)
			{
				my $plane = hex($1);
				my $codepoint = hex($2);
				$unicode{$codepoint} .= 'cns11643.1992-' . $plane . ' ';
			}
		}		
	
		next;
	}

	# Almost everything contains ASCII
	if ($hasascii)
	{
		foreach $i (0 .. 127)
		{
			$unicode{$i} .= "$charset ";
		}
		next if "ascii" eq $file;
	}

	# ISO 8859-1 is simple
	if ("iso8859-1" eq $file)
	{
		foreach $i (128 .. 255)
		{
			$unicode{$i} .= "iso8859-1 ";
		}
		next;
	}

	# Setup
	my $colidx = 1;
	$colidx = 2
		if $file eq "jis0208.txt" or
		   $file eq "koi8-u.txt" or
		   $file eq "viscii.txt";

	# Read mapping
	open MAPPING, "sources/$file" or die "Cannot read file $file: $!\n";
	while (<MAPPING>)
	{
		my $char = (split)[$colidx];
		if (defined $char &&
		    ($char =~ /^(0x....),?$/ || $char =~ /^([0-9A-Fa-f]{4}),?$/))
		{
			my $codepoint;
			$codepoint = hex($1);
			$unicode{$codepoint} .= "$charset "
				unless $codepoint < 128 && $hasascii;
		}
	}
}

# Remove any references to the replacement character
$unicode{0xFFFD} = "";

# Print results
if ($opt_c)
{
	# Print C++ macro definitions for each charset identifier
	print "/* Bit definitions for charsets */\n";
	print "/*      Charset constant Bitfield in table     Primary X name */\n";
	my $bit = '0000000000000001';

	foreach $charset (sort keys %cmapping)
	{
		printf "const CharSetFlags %-16s(0x%s, 0x%s); /* %s */\n",
			$cmapping{$charset}, substr($bit, 0, 8), substr($bit, 8), $charset;
		unless ($bit =~ s/08/10/)
		{
			$bit =~ s/4/8/;
			$bit =~ s/2/4/;
			$bit =~ s/1/2/;
		}
	}

	# Print C++ mapping table for charset to bitcode
	my $first = 1;
	print "\n\n/* Conversion table from X name to bit definition */\n";
	print "struct xcharsets\n{\n\tconst char *xcharsetname;\n";
	print "\tCharSetFlags mappingbit;\n} xcharsetmapping[] =\n{\n";

	foreach $charset (sort keys %cmapping)
	{
		print ",\n" unless $first;
		printf "\t{ \"%s\", %s }", $charset, $cmapping{$charset};
		if (defined $cmappingextra{$charset})
		{
			my @charsets = split / /, $cmappingextra{$charset};
			foreach $extra (@charsets)
			{
				printf ",\n\t{ \"%s\", %s }", $extra, $cmapping{$charset};
			}
		}

		$first = 0;
	}
	printf ",\n\t{ 0, 0 }" unless $first;
	print "\n};\n\n";

	# Print C++ table
	print "/* Unicode data table */\n";
	print "CharSetFlags charsets[0x10000] =\n{\n";
}
elsif ($opt_h)
{
	open NAMES, "$opt_h"
		or die "Unable to open: $opt_h: $!\n";
	while (<NAMES>)
	{
		if (/^([0-9A-Fa-f]{4})\s+(.*)$/)
		{
			$name[hex($1)] = $2;
		}
	}
}

for (my $i = 0; $i < 0xFFFE; $i ++)
{
	if ($opt_c)
	{
		# C code
		if ($unicode{$i} eq "")
		{
			printf "\t/* %04x */ 0, /* No charsets */\n", $i;
		}
		else
		{
			my @charsets = split / /, $unicode{$i};
			printf "\t/* %04x */ ", $i;
			for (my $j = 0; $j <= $#charsets; $j ++)
			{
				die unless defined $cmapping{$charsets[$j]};
				print " | " if $j;
				print $cmapping{$charsets[$j]};
			}
			print ",\n";
		}
	}
	elsif ($opt_r)
	{
		# Raw list
		print $unicode{$i}, "\n";
	}
	else
	{
		# Human readable list
		print $name[$i], ":\n" if defined $name[$i];
		printf "%04x %s\n",$i,$unicode{$i};
	}
}

print "\t/* fffe */ 0, /* Not a character */\n" if $opt_c;
print "\t/* ffff */ 0  /* Not a character */\n" if $opt_c;
print "};\n" if $opt_c;
