#!/usr/bin/perl -w
#						-*- tab-width: 4; -*-
#
# File name:  gettablelist.pl
# Contents:   List, and possibly extract or convert, the contents of a
#             chartables.bin file
#
# Copyright 2002-2011 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

require v5.6.0;

# External code and variables
use Getopt::Std;
use vars qw/$opt_x $opt_f $opt_l $opt_b/;

# Global data
my @date = gmtime;
my $year = $date[5] + 1900;

# Check parameters
$VERSION = '1.0';
$Getopt::Std::STANDARD_HELP_VERSION = 1;
unless (getopts('x:f:lbc:e:'))
{
	HELP_MESSAGE();
}

if ($opt_x)
{
	unless (-d $opt_x)
	{
		print $opt_x, " does not exist!\n";
		exit 1;
	}
}

if ($opt_e)
{
	unless (-d $opt_e)
	{
		print $opt_e, " does not exist!\n";
		exit 1;
	}
}

die "-l and -b are mutually exclusive.\n" if $opt_l && $opt_b;

my $little = 1;
if ($opt_l)
{
	# Little endian
	$short = 'v';
	$long = 'V';
}
elsif ($opt_b)
{
	# Big endian
	$short = 'n';
	$long = 'N';
	$little = 0;
}
else
{
	# Machine endian
	$short = 'S';
	$long = 'L';
	$little = unpack('V', pack('L', 42)) == 42;
}

my $chartables = $little ? 'chartables.bin' : 'chartables-be.bin';
$chartables = $opt_f if defined $opt_f;

# Read chartables.bin -----------------------------------------------
# Open table file
open BIN, '<', $chartables
	or die "Cannot read $chartables: $!\n";
binmode BIN;

# Read header
# Number of tables stored as 16-bit value
read BIN, $numtables, 2;
my $numtables = unpack($short, $numtables);
print "$numtables tables in $chartables\n";

my @tables = ();
while ($numtables --)
{
	# Table length (32-bit), length of name (1 octet)
	my $data;
	read BIN, $data, 5;
	my ($tablelen, $namelen) = (unpack($long . 'C', $data));
	my $tabledisksize = $tablelen;
	# Check if this is a no_rev_tables compressed table
	if ($tablelen > 0x10000) # There is (was) a 65536 byte table
	{
		$tabledisksize = $tablelen >> 16;
		$tablelen      = $tablelen & 0xFFFF;
	}

	# Name of table
	my $name;
	read BIN, $name, $namelen;

	# Offset of table (32-bit, does not include the header)
	read BIN, $data, 4;
	my $tableofs = unpack($long, $data);

	unless (defined $opt_e)
	{
		if ($tablelen != $tabledisksize)
		{
			printf "Ofs %05x len %05x (disk %05x) %s\n", $tableofs, $tablelen, $tabledisksize, $name;
		}
		else
		{
			printf "Ofs %05x len %05x %s\n", $tableofs, $tablelen, $name;
		}
	}
	push @tables, $name, $tableofs, $tablelen, $tabledisksize
		if $tablelen > 0;
}

my $headerlen = tell(BIN);

# Read tables
my (%table, %tablelen);
my $tablecount = 0;
my $namelen = 0;
while ($#tables > -1)
{
	# Meta data
	my ($tablename, $ofs, $len, $disklen) =
		(shift @tables, shift @tables, shift @tables, shift @tables);

	# Copy data
	seek(BIN, $headerlen + $ofs, 0);
	my $data;
	read BIN, $data, $disklen;
	$table{$tablename} = $data;
	$tablelen{$tablename} = $len;
	++ $tablecount;
	$namelen += length($tablename) + 1;
}

# Create .tbl files -------------------------------------------------
if ($opt_x)
{
	# Extract TBL files
	foreach my $name (keys %table)
	{
		# Create output
		open TBL, '>', "$opt_x/$name.tbl"
			or die "Cannot create $opt_x/$name.tbl: $!\n";
		print TBL $table{$name};
		close TBL;
	}
}

# Create encoding.bin -----------------------------------------------
if ($opt_e)
{
	# Write output file
	open BIN, '>', "${opt_e}/encoding.bin"
		or die "Cannot write encoding.bin: $!\n";
	binmode BIN;

	# Header
	my $headersize =
		  8							# UINT16 magic
									# UINT16 header_size
									# UINT16 number_of_tables
									# UINT16 reserved (0)
		+ 4 * ($tablecount + 1)		# UINT32 table_data[number_of_tables + 1]
		+ (4 + 2 + 1) * $tablecount	# UINT32 final_size[number_of_tables]
									# UINT16 table_name[number_of_tables]
									# UINT8  table_info[number_of_tables]
		+ $namelen					# char names[...]
		+ ($namelen % 2);			# char padding[0..1]
		;
	die "Header to big ($headersize)\n" if $headersize > 0xFFFF;

	print BIN pack($short x 4, 0xFE01, $headersize, $tablecount, 0);
	my $tblptr = $headersize;	# Beginning of table data
	my $nameptr = $headersize - $namelen - ($namelen % 2); # Beginning of name data
	my $orignameptr = $nameptr;

	# UINT32 table_data[number_of_tables + 1]
	my %padding;
	foreach my $table (sort keys %table)
	{
		my $disklen = length($table{$table});
		$padding{$table} = $disklen % 2;
		print BIN pack($long, $tblptr);
		$tblptr += $disklen;
		++ $tblptr if $padding{$table};
	}
	print BIN pack($long, $tblptr);

	# UINT32 final_size[number_of_tables]
	foreach my $table (sort keys %table)
	{
		print BIN pack($long, $tablelen{$table});
	}

	# UINT16 table_name[number_of_tables]
	foreach my $table (sort keys %table)
	{
		print BIN pack($short, $nameptr);
		$nameptr += length($table) + 1;
	}

	# UINT8 table_info[number_of_tables]
	foreach my $table (sort keys %table)
	{
		my $bit = 0;
		$bit |= 1 if length($table{$table}) != $tablelen{$table}; # Compressed
		$bit |= 0x80 if $padding{$table}; # Add padding byte
		print BIN pack('C', $bit);
	}

	# char names[...]
	my $filesize = tell(BIN);
	die "nameptr calculation wrong ($orignameptr<>$filesize)\n"
		unless $orignameptr == $filesize;
	foreach my $table (sort keys %table)
	{
		print BIN pack('Z*', $table);
	}

	# Padding to make the header have even size
	print BIN pack('c', 0) if $namelen % 2;

	# Table data
	$filesize = tell(BIN);
	die "headersize calculation wrong ($headersize<>$filesize)\n"
		unless $headersize == $filesize;

	foreach my $table (sort keys %table)
	{
		print BIN $table{$table};
		print BIN pack('c', 0) if $padding{$table} % 2; # Padding
	}

	$filesize = tell(BIN);
	die "filesize calculation wrong ($tblptr<>$filesize)\n"
		unless $tblptr == $filesize;
	close BIN;

	# Create .cpp version
	open BIN, '<', "${opt_e}/encoding.bin"
		or die "Cannot read back encoding.bin: $!\n";
	binmode BIN;
	my $encodingbin;
	read(BIN, $encodingbin, $filesize);
	close BIN;

	open CPP, '>', "${opt_e}/encodingbin.cpp"
		or die "Cannot write encodingbin.cpp: $!\n";
	print CPP <<"EOM";
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-$year Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** This file is generated by data/i18ndata/tables/utilities/gettablelist.pl
** - do not edit manually.
*/

#include "core/pch.h"

#if defined ENCODINGS_HAVE_ROM_TABLES
#include "modules/encodings/tablemanager/romtablemanager.h"

const unsigned char encodingtables_char[$filesize] = {
EOM

	for (my $i = 0; $i < $filesize; ++ $i)
	{
	#	print CPP "\t\"" if ($i % 8) == 0;
		print CPP sprintf("0x%02X, ", ord(substr($encodingbin,$i,1)));
		print CPP "\n" if ($i % 8) == 7;
	}
	#print CPP "\"\n" if (($filesize - 1) % 8) != 7;
	print CPP "\n};\n";
	print CPP "const unsigned int *g_encodingtables()\n";
	print CPP "{\n";
	print CPP "\treturn reinterpret_cast<const unsigned int *>(encodingtables_char);\n";
	print CPP "}\n";
	print CPP "#endif\n";
	close CPP;
}

# Display help message ----------------------------------------------
sub HELP_MESSAGE
{
	print "Usage: $0 [-f file] [-x directory] [-l] [-b]\n";
	print "Displays contents of chartables.bin\n\n";
	print "  -f file         Alternative name for chartables.bin file\n";
	print "  -x directory    Extract the tables into .tbl files to directory\n";
	print "  -e directory    Create encoding.bin and encodingbin.cpp files in directory\n";
	print "  -l              Force little endian mode\n";
	print "  -b              Force big endian mode\n";
	exit 1;
}
