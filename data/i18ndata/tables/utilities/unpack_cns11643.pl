#!/usr/bin/perl -w

$packed = shift or die "Usage: $0 packed-cns\n";
$packed = hex($packed) if $packed =~ /^0x/;

$plane = $packed >> 14;
$row   = (($packed >> 7) & (0x7F));
$cell  = ($packed & 0x7F);

$plane = 14 if $plane == 3;
printf "Plane %d, row %d, cell %d (%X%02X%02X)\n",$plane,$row,$cell,$plane,$row,$cell
