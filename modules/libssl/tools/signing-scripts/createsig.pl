#! /usr/bin/perl

if($#ARGV <3)
{
   die("usage: $0 prefix signature input output");
}

$prefix = $ARGV[0];
$filesig = $ARGV[1];
$file1 = $ARGV[2];
$file2 = $ARGV[3];

print "prefix $prefix\n".
      "source file $file1\n".
      "signature file $filesig\n".
      "output file $file2\n";

open(inputfile, $file1) or die("could not open source file $file1");
open(signaturfile, $filesig) or die("could not open signature file $filesig");
open(outputfile, ">" . $file2) or die("could not open output file $file2");

printf(outputfile "%s ", $prefix);
while(<signaturfile>)
{
	$_ =~ s/[\r\n]//g;
	printf(outputfile "%s", $_);
}

printf (outputfile "\n");
print outputfile <inputfile>;

close inputfile;
close signaturfile;
close outputfile;
