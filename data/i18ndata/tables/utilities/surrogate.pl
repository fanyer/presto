#!/usr/bin/perl -w

$ucs = shift or die "Usage: $0 ucs\n";
$ucs = hex($ucs) if $ucs =~ /^0x/;
die "Range: 0x10000 - 0x10FFFF" if $ucs < 0x10000 || $ucs > 0x10FFFF;

# Surrogates spread out the bits of the UCS value shifted down by 0x10000
$ucs -= 0x10000;
$high = 0xD800 | ($ucs >> 10);		# 0xD800 -- 0xDBFF
$low  = 0xDC00 | ($ucs & 0x03FF);	# 0xDC00 -- 0xDFFF

printf "<%04x><%04x>\n", $high, $low;
