#!/usr/bin/perl -w

# Generate input file for Symbian cnvtool program from a Unicode data file.
# Copyright 2003 Opera Software ASA

use integer;

# Read command line
if ($#ARGV != 2)
{
	print "Usage: $0 unicode-table UID output-file\n";
	exit 0;
}
my ($input, $uid, $output) = @ARGV;

open IN, '<', $input	or die "Cannot read $input: $!\n";
open OUT, '>', $output	or die "Cannot write $output: $!\n";

# Write header
print OUT "# Created from $input\n";
print OUT "UID $uid\n\n";
print OUT "Endianness Unspecified\n";
print OUT "ReplacementForUnconvertibleUnicodeCharacters 0x3F\n\n"; # "?"
print OUT "StartForeignVariableByteData\n";
print OUT "0x00 0xFF 0\n";
print OUT "EndForeignVariableByteData\n\n";

# Read input
my @mapping = (-1) x 256;
while (<IN>)
{
	next if /^#/;
	if (/^0x([0-9A-Fa-f]{2})\s+0x([0-9A-Fa-f]{4})/)
	{
		$mapping[hex($1)] = hex($2);
	}
}
close IN;

# Make sure ASCII is one-to-one mapped
foreach $ascii (0x00 .. 0x7F)
{
	die "Not ASCII derived"
		if $mapping[$ascii] != $ascii && $mapping[$ascii] != -1;
}

# Check if 0x80 and up is directly mapped (ISO control codes and more)
my $top = 0x7F; # ASCII
foreach $iso (0x80 .. 0xFF)
{
	last if $mapping[$iso] != $iso;
	$top = $iso;
	$mapping[$iso] = -1; # Forget this mapping now
}

# Print section for converting to UCS-2
print OUT "StartForeignToUnicodeData\n";
printf OUT "0 0 0x00 %#04x Direct {}\n", $top; # ASCII and any other direct

# Print high-bit ranges
my $start = -1;
foreach $highbit (0x80 .. 0xFF)
{
	if (-1 == $mapping[$highbit])
	{
		# Undefined code point, print range if we were in one
		if ($start != -1)
		{
			printf OUT "0 0 %#04x %#04x IndexedTable16 {}\n",
			       $start, $highbit - 1;
			$start = -1;
		}
	}
	else
	{
		# Defined code point, start range if we weren't in one
		if (-1 == $start)
		{
			$start = $highbit;
		}
	}
}

# Print last range
printf OUT "0 0 %#04x 0xff IndexedTable16 {}\n", $start if $start != -1;
print OUT "EndForeignToUnicodeData\n\n";

# Print section for converting from UCS-2
print OUT "StartUnicodeToForeignData\n";

printf OUT "0 0 0x0000 %#06x Direct 1 {}\n", $top; # ASCII and any other direct

# Find lowest and highest Unicode code point for the rest of the code
# points
my ($low, $high) = (65536, -1);
foreach $highbit (0x80 .. 0xFF)
{
	next if -1 == $mapping[$highbit];

	$low = $mapping[$highbit] if $mapping[$highbit] < $low;
	$high= $mapping[$highbit] if $mapping[$highbit] > $high;
}

printf OUT "0 0 %#06x %#06x KeyedTable1616 1 {}\n", $low, $high
	if $high != -1;
print OUT "EndUnicodeToForeignData\n";

exit 0;


