#! /usr/bin/perl

if($#ARGV <1)
{
   die("usage: $0 variablename input");
}

$currentyear = ((gmtime)[5])[0]+1900;
$variablename = $ARGV[0];
$filein = $ARGV[1];
$fileout = $filein;
$fileout =~ s/(\.[\w\-]*$)/\.h/;
if ($fileout !~ /\.[\w\-]*$/) { $fileout .= ".h"; }
$fileout =~ s/.keys//;

open(inputfile, $filein) or die("could not open source file $filein");
open(outputfile, ">" . $fileout) or die("could not open output file $fileout");

print outputfile  <<END_OF_HEADER ;
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-$currentyear Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

static const unsigned char $variablename\[] = {
END_OF_HEADER

binmode inputfile;

printf (outputfile  "\t");

$linelen = 0;
while(defined($char = getc inputfile))
{
	if($linelen >0)
	{
		printf (outputfile ",");
	}
	if($linelen  >= 16)
	{
	   printf (outputfile "\n\t");
	   $linelen=0;
	}
	
	printf (outputfile "0x%.2x ",ord($char));
	$linelen ++;
}

printf (outputfile "\n};\n");

close filein;
close outputfile;

