#!/usr/bin/perl -w
#
# File name:  maketest.pl
# Contents:   Generate test documents from the table sources.
#
# Copyright 2002-2006 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

# Configuration
$outdir = 'charset';	# Output directory
$indir = '../sources';	# Source directory
$unihan = 'Unihan.txt';	# Path to Unicode's Unihan database
$names = 'NamesList.txt'; # Path to Unicode's NamesList database

# List of encodings and the source files they are listed in
%charsets = (
	'iso-8859-1' => '8859-1.txt',
	'iso-8859-2' => '8859-2.txt',
	'iso-8859-3' => '8859-3.txt',
	'iso-8859-4' => '8859-4.txt',
	'iso-8859-5' => '8859-5.txt',
	'iso-8859-6' => '8859-6.txt',
	'iso-8859-7' => '8859-7.txt',
	'iso-8859-8' => '8859-8.txt',
	'iso-8859-9' => '8859-9.txt',
	'iso-8859-10' => '8859-10.txt',
	'iso-8859-11' => '8859-11.txt',
	'iso-8859-13' => '8859-13.txt',
	'iso-8859-14' => '8859-14.txt',
	'iso-8859-15' => '8859-15.txt',
	'iso-8859-16' => '8859-16.txt',

	'windows-1250' => 'cp1250.txt',
	'windows-1251' => 'cp1251.txt',
	'windows-1252' => 'cp1252.txt',
	'windows-1253' => 'cp1253.txt',
	'windows-1254' => 'cp1254.txt',
	'windows-1255' => 'cp1255.txt',
	'windows-1256' => 'cp1256.txt',
	'windows-1257' => 'cp1257.txt',
	'windows-1258' => 'cp1258.txt',
	'windows-874' => 'cp874.txt',
	'windows-932' => 'cp932.txt',
	'windows-936' => 'cp936.txt',
	'windows-950' => 'cp950.txt',

	'ibm866' => 'cp866.txt',

	'euc-tw' => 'cns11643.txt',

	'Big5' => 'big5-2003.txt',
	'Big5-HKSCS' => 'big5hkscs.txt',
	'GB2312' => 'gb2312.txt',
	'GB18030' => 'gb-18030-2000.xml',
	'euc-jp' => 'jis0208.txt',
	'koi8-r' => 'koi8-r.txt',
	'koi8-u' => 'koi8-u.txt',
	'euc-kr' => 'ksc5601.txt',
	'tcvn' => 'tcvn_uni.txt',
	'viscii' => 'viscii.txt',
	'x-vps' => 'vps_uni.txt',
	'windows-sami-2' => 'ws2.txt',
);

# Map to obtain real MIME names for some encodings
%mime = (
	'windows-932' => 'shift_jis',
	'windows-936' => 'gbk',
	'windows-950' => 'big5',
);

# To achieve uniform input, some source files require transformations
%transform = (
	'Big5-HKSCS' => \&hkscstransform,
	'GB2312' => \&gbtransform,
	'GB18030' => \&gb18030transform,
	'euc-jp' => \&jistransform,
	'euc-tw' => \&cnstransform,
	'koi8-u' => \&koi8transform,
	'tcvn' => \&tcvntransform,
	'viscii' => \&visciitransform,
	'windows-sami-2' => \&ws2transform,
	'x-vps' => \&vpstransform,
);

# Create and open index page
open DOC, '>' . $outdir . '/index.html'
	or die "Cannot create $outdir/index.html: $!\n";
print DOC <<'EOM';
<html>
 <head>
  <title>Encoding test documents</title>
  <link rel='stylesheet' href='tests.css'>
 </head>

 <body>
  <ol>
EOM

# Read the Unihan database
%han = ();

if (open HAN, $unihan)
{
	print "Loading UniHan\n";
	my $data = 0;
	while (<HAN>)
	{
		next if /^#/;
		next unless /\tkDefinition\t/;
		chomp;
		my ($cp, undef, $text) = split /\t/;
		$text =~ s/([\x80-\xFF]+)/&utf8toent($1)/ge;
		$cp =~ s/U\+([0-9A-F]+)/hex($1)/e;
		$han{$cp} = $text;
		$data ++;
	}
	print " $data entries loaded\n";
}
else
{
	warn "Unihan database is not available. Tried to open Unihan.txt:\n$!\n";
}

# Read the NamesList database
%name = ();

if (open NAMES, $names)
{
	print "Loading NamesList\n";
	my $data = 0;
	while (<NAMES>)
	{
		next if /^#/;
		chomp;
		if (/^([0-9A-F]{4,6})\t(.*)$/)
		{
			my ($cp, $text) = (hex($1), $2);
			$name{$cp} = $text;
			$data ++;
		}
	}
	print " $data entries loaded\n";
}
else
{
	warn "NamesList not available. Tried to open NamesList.txt:\n$!\n";
}

# Generate all the documents
print "Generating test pages:\n";

foreach $charset (sort charset_compare keys %charsets)
{
	# Progress meter
	print ' ', $charset, "\n";

	# Open the relevant files
	my $input  = $indir . '/' . $charsets{$charset};
	my $output = $outdir . '/' . $charset . '.html';
	my $mime = $mime{$charset} || $charset;

	open IN,  $input     or die "Cannot read $input: $!\n";
	open OUT, ">$output" or die "Cannot create $output: $!\n";

	print DOC "   <li><a href='$charset.html'>$charset</a>";
	print DOC " ($mime)" if $mime ne $charset;
	print DOC "\n";

	print OUT "<html>\n <head>\n  <title>$charset";
	print OUT " ($mime)" if $mime ne $charset;
	print OUT "</title>\n  <meta http-equiv='content-type' content='text/html;";
	print OUT "charset=$mime'>\n  <link rel='stylesheet' href='tests.css'>\n";
	print OUT " </head>\n\n <body>\n";

	my $mode = -1;
	my $intable = 0;
	my $dintable = 0;

	my $transform;
	$transform = $transform{$charset} if defined $transform{$charset};
	my %data;

	while (<IN>)
	{
		# Transform input if required
		$_ = &$transform($_) if defined $transform;

		# Skip garbage data
		next if /^#/;
		next if /DBCS LEAD BYTE/;
		next if /UNDEFINED/;

		# Split line
		chomp;
		my @line = split /\t/;
		next if $#line < 2;

		# Decode line
		my ($legacy, $unicode, $text) = ($line[0], $line[1], $line[$#line]);
		$legacy  =~ s/0x([0-9A-F]+)/hex($1)/ie;
		$unicode =~ s/0x([0-9A-F]+)/hex($1)/ie;
		$text =~ s/^#//;

		# Get data from Unihan, if available
		$text = $han{$unicode} if defined $han{$unicode};
		# Get data from NamesList, if still unknown
		$text = $name{$unicode}
			if ($text eq '' || $text eq '#') && defined $name{$unicode};

		# HTMLize
		$text =~ s/&/&amp;/;
		$text =~ s/</&lt;/;
		$text =~ s/>/&gt;/;

		# Ignore control codes
		next if $unicode < 32;

		my @packed = ($unicode, $text);
		$data{$legacy} = \@packed;
	}

	foreach my $legacy (sort legacy_compare keys %data)
	{
		my ($unicode, $text) = @{$data{$legacy}};
		
		# Check operation mode
		if ($legacy < 256 && $mode != 0)
		{
			# Initiate single-byte mode
			print OUT "  <table>\n   <caption>Single-byte</caption>\n";
			print OUT "   <tr><th colspan=2>$mime<th colspan=3>Unicode\n";
			$intable = 1;
			$mode = 0;
		}
		elsif ($legacy > 255 && $legacy < 65536 && $mode < 1)
		{
			# Initiate double-byte mode
			$dintable = 0;
			$mode = -2;
		}
		elsif ($legacy > 65535 && $mode < 65536)
		{
			# Initiate four-byte mode
			$dintable = 0;
			$mode = -3;
		}

		# Split MBCS code points into components
		my ($one, $two, $row, $col) = (0, 0, 0, 0);
		my $newmode = 0;
		if ($mode > 65535 || -3 == $mode)
		{
			$one = int($legacy / 256 / 256 / 256);
			$two = int(($legacy / 256 / 256) & 255);
			$row = int(($legacy / 256) & 255);
			$col = $legacy & 255;
			$newmode = $row + $two * 256 + $one * 65536;
		}
		elsif ($mode > 0 || -2 == $mode)
		{
			$row = int($legacy / 256);
			$col = $legacy & 255;
			die if $row == 0;
			$newmode = $row;
		}

		# Start new MBCS mode if requested
		if ($mode != $newmode)
		{
			my ($len, $hrow);
			if ($one > 0)
			{
				$len = 'Four';
				$hrow = sprintf("%02X%02X%02X", $one, $two, $row);
			}
			else
			{
				$len = 'Double';
				$hrow = sprintf("%02X", $row);
			}

			if ($dintable)
			{
				print DOUT "  </table>\n <p><a href='$output'>Return</a></p>\n";
				print DOUT " </body>\n</html>\n";
				close DOUT;
			}

			# Create sub-document for this part of the MBCS encoding
			my $ddocument = $charset . '-' . lc($hrow) . ".html";
			my $doutput = $outdir . '/' . $ddocument;
			open DOUT, ">$doutput" or die "Cannot create: $doutput: $!\n";

			if (!$intable)
			{
				print OUT "  <table>\n";
				print OUT "   <tr><th colspan=2>Legacy<th colspan=3>Unicode\n";
				$intable = 1;
			}

			print OUT "   <tr><td colspan=2><a href='$ddocument'>=$hrow";
			print OUT "xx</a><td colspan=3>$len-byte sequences\n";

			print DOUT "<html>\n <head>\n  <title>$charset";
			print DOUT " ($mime)" if $mime ne $charset;
			print DOUT " lead $hrow</title>\n  <meta http-equiv='content-type' ";
			print DOUT "content='text/html;charset=$mime'>\n";
			print DOUT "  <link rel='stylesheet' href='tests.css'>\n";
			print DOUT " </head>\n\n <body>\n";

			print DOUT "  <table>\n   <caption>$len-byte lead $hrow</caption>\n";
			print DOUT "   <tr><th colspan=2>$mime<th colspan=3>Unicode\n";
			$mode = $row + $two * 256 + $one * 65536;
			$dintable = 1;
		}

		# Get printable and binary version of the legacy code point
		my ($hlegacy, $cp);
		if (0 == $mode)
		{
			$hlegacy = sprintf("=%02X", $legacy);
			$cp = pack('C', $legacy);
		}
		elsif ($mode < 65536)
		{
			$hlegacy = sprintf("=%04X", $legacy);
			$cp = pack('n', $legacy);
		}
		else
		{
			$hlegacy = sprintf("=%08X", $legacy);
			$cp = pack('N', $legacy);
		}

		# Get printable version of the Unicode code point
		my $hunicode = sprintf("U+%04X", $unicode);

		# Write information to the relevant output file
		if (0 == $mode)
		{
			print OUT "   <tr><td>$hlegacy<td> $cp <td>$hunicode<td>&#$unicode;<td class=u>$text\n";
		}
		else
		{
			print DOUT "   <tr><td>$hlegacy<td> $cp <td>$hunicode<td>&#$unicode;<td class=u>$text\n";
		}
	}

	# Finish up page for this encoding
	print OUT "  </table>\n" if $intable;
	print OUT " </body>\n</html>\n";
	close OUT;

	if ($dintable)
	{
		print DOUT "  </table>\n  <p><a href='$output'>Main table</a></p>\n";
		print DOUT " </body>\n</html>\n";
		close DOUT;
	}
}

# Finish up index document
print DOC <<'EOM';
  </ol>
 </body>
</html>
EOM

# Convert UTF-8 string to HTML entity
sub utf8toent
{
	my $cp = unpack ('U', shift);
	return "&#$cp;";
}

# Transform CNS11643 input to our required format
sub cnstransform
{
	$line = shift;
	if (/^0x1([2-7][0-F])([2-7][0-F])/)
	{
		my $new = sprintf("0x%02X%02X", hex($1) | 128, hex($2) | 128);
		$line =~ s/^[^\t]+/$new/;
	}
	elsif (/^0x([2E])([2-7][0-F])([2-7][0-F])/)
	{
		my $new = sprintf("0x8EA%s%02X%02X", $1, hex($2) | 128, hex($3) | 128);
		$line =~ s/^[^\t]+/$new/;
	}

	return $line;
}

# Transform GBK input to our required format
sub gbtransform
{
	$line = shift;
	if (/^0x([2-7][0-F])([2-7][0-F])/)
	{
		my $new = sprintf("0x%02X%02X", hex($1) | 128, hex($2) | 128);
		$line =~ s/^[^\t]+/$new/;
	}
	return $line;
}

# Transform GB18030 input to our required format
sub gb18030transform
{
	$line = shift;
	if ($line =~ /<a u="([0-9A-F]{4,6})" b="([0-9A-F ]+)"\/>/)
	{
		my ($legacy, $unicode) = ($2, $1);
		$legacy =~ s/ //g;
		return "0x$legacy\t0x$unicode\t#";
	}
	return '';
}

# Transform KOI-8 input to our required format
sub koi8transform
{
	$line = shift;
	$line =~ s/^\s+\d\d\d\s+([0-9A-F]{2})\s+U([0-9A-F]{4})\s+/0x$1\t0x$2\t/;
	return $line;
}

# Transform TCVN input to our required format
sub tcvntransform
{
	$line = shift;
	$line =~ s/^(0x[0-9a-f]{2})    (0x[0-9a-f]{4})  ...... /$1\t$2\t/;
	return $line;
}

# Transform viscii input to our required format
sub visciitransform
{
	$line = shift;
	$line =~ s/^(\d{1,3})\t[^\t]{2,3}\t([0-9A-F]{4})\t/$1\t0x$2\t/;
	return $line;
}

# Transform vps input to our required format
sub vpstransform
{
	$line = shift;
	$line =~ s/^(0x..)\s+(0x....)  ...\s+/$1\t$2\t/;
	return $line;
}

# Transform windows-sami-2 input to our required format
sub ws2transform
{
	$line = shift;
	$line =~ s/   /\t/g;
	return $line;
}

# Transform jis-0208 input to our required format
sub jistransform
{
	$line = shift;
	if (/^0x....\t0x([2-7][0-F])([2-7][0-F])/)
	{
		my $new = sprintf("0x%02X%02X", hex($1) | 128, hex($2) | 128);
		$line =~ s/^0x....\t0x..../$new/;
	}
	return $line;
}

# Transform Big5-HKSCS input to our required format
sub hkscstransform
{
	$line = shift;
	$line =~ s/^[0-9A-F]{4}\s+([0-9A-F]{4,5})\s+([0-9A-F]{4})/0x$2\t0x$1\t#/;
	return $line;
}

# Custom sort function for charsets (mirrors the one in make.py)
sub charset_compare
{
	# First all ISO 8859 encodings, numerically
	if (substr($a, 0, 8) eq "iso-8859")
	{
		if (substr($b, 0, 8) eq "iso-8859")
		{
			return substr($a, 9) <=> substr($b, 9)
		}
		else
		{
			return -1 # ISO encodings first
		}
	}
	if (substr($b, 0, 8) eq "iso-8859")
	{
		return 1
	}

	# Then all UTF encodings, numerically
	if (substr($a, 0, 3) eq "utf")
	{
		if (substr($b, 0, 3) eq "utf")
		{
			return substr($a, 4) <=> substr($b, 4)
		}
		else
		{
			return -1
		}
	}
	if (substr($b, 0, 3) eq "utf")
	{
		return 1
	}

	# Then all Windows encodings, numerically
	if (substr($a, 0, 7) eq "windows" and substr($a, 8) ne "sami-2")
	{
		if (substr($b, 0, 7) eq "windows" and substr($b, 8) ne "sami-2")
		{
			return substr($a, 8) <=> substr($b, 8)
		}
		else
		{
			return -1
		}
	}
	if (substr($b, 0, 7) eq "windows" and substr($b, 8) ne "sami-2")
	{
		return 1
	}

	# Then all other encodings, alphabetically
	return $a cmp $b
}

# Custom search function for legacy codepoints
sub legacy_compare
{
	my $ldiff = length($a) - length($b);
	return $ldiff if $ldiff;
	return $a cmp $b;
}
