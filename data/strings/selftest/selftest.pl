#!/usr/bin/perl -w
#
# File name:    selftest.pl
# Contents:     Unit testing for the strings module. This script runs
#               various tests on the scripts included in this module
#               to ensure that they output the expected format.
#
# Copyright (C) 2007-2011 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.
#
# Written and maintained by Peter Krefting (peter@opera.com)

# Setup
chdir "data/strings/selftest"      if     -e "data/strings/selftest/selftest.pl";
chdir "selftest"                   if     -e "selftest/selftest.pl";
die "Cannot find makelang.pl"      unless -e "../scripts/makelang.pl";
die "Cannot find english.db"       unless -e "english.db";
die "Cannot find selftest.strings" unless -e "selftest.strings";
die "Cannot find reference/"       unless -d "reference";
mkdir "testoutput"                 unless -d "testoutput";
die "Could not create testoutput/: $!\n"
                                   unless -d "testoutput";

# Select perl binary (Windows cannot call the script directly)
my $perlbin = 'perl';

# Check command line
if ($#ARGV == 0 && $ARGV[0] =~ /--?h(elp)?/)
{
	print "Usage: $0 [regexp ...]\n";
	print "Runs selftests for the strings module.\n\n";
	print "  regexp    Only run whose names are matched by this regexp.\n";
	exit 0;
}

# Opera core code output
&test('0-03', 'Headers - level 9', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -b selftest.strings -H -c 9');
&test('0-04', 'Headers - level 9 - multiple dbs', $perlbin . ' ../scripts/makelang.pl -d testoutput -b extra.strings -H -c 9 english.db extra.db');
&test('0-06', 'Headers - level 9 - multiple dbs - pass dup', $perlbin . ' ../scripts/makelang.pl -d testoutput -b selftest.strings -H -c 9 english.db dup-pass.db');
&test('0-07', 'Headers - level 9 - multiple dbs - fail dup', $perlbin . ' ../scripts/makelang.pl -d testoutput -b selftest.strings -H -c 9 english.db dup-fail.db', 1);

# LNG file output
&test('1-01', 'LNG without translation - fail', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k', 1);
&test('1-03', 'LNG - level 9', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k -t en.po');
&test('1-04', 'LNG - level 9 - multiple dbs', $perlbin . ' ../scripts/makelang.pl -d testoutput -b extra2.strings -L -p Selftest,42.0,4711 -c 9 -k -t en.po english.db extra.db');
&test('1-05', 'LNG - level 9 - strip ampersand', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -s -k -t en.po');
&test('1-06', 'LNG - level 9 - multiple dbs - pass dup', $perlbin . ' ../scripts/makelang.pl -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k -t en.po english.db dup-pass.db');
&test('1-07', 'LNG - level 9 - multiple dbs - fail dup', $perlbin . ' ../scripts/makelang.pl -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k -t en.po english.db dup-fail.db', 1);
&test('1-08', 'LNG - level 9 - without comments', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -t en.po');

# Translated LNG file output
&test('2-03', 'Translated LNG - level 9 (non-msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-04', 'Translated LNG - level 9 - multiple dbs (non-msgctxt)', $perlbin . ' ../scripts/makelang.pl -d testoutput -t selftest.po -b extra2.strings -L -p Selftest,42.0,4711 -c 9 -k english.db extra.db');
&test('2-05', 'Translated LNG - level 9 - strip amperand (non-msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -s -k');
&test('2-06', 'Translated LNG - PO files with ignorable errors (non-msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-errors.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-07', 'Translated LNG - c-format error (non-msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-cformat.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-08', 'Translated LNG - qt-format error (non-msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-qtformat.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-09', 'Translated LNG - fuzzy strings (non-msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-fuzzy.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-0A', 'Translated LNG - level 9 - strip comments (non-msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9');

&test('2-13', 'Translated LNG - level 9 (msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-ctxt.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-14', 'Translated LNG - level 9 - multiple dbs (msgctxt)', $perlbin . ' ../scripts/makelang.pl -d testoutput -t selftest-ctxt.po -b extra2.strings -L -p Selftest,42.0,4711 -c 9 -k english.db extra.db');
&test('2-15', 'Translated LNG - level 9 - strip amperand (msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-ctxt.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -s -k');
&test('2-16', 'Translated LNG - PO files with ignorable errors (msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-errors-ctxt.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-17', 'Translated LNG - c-format error (msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-cformat-ctxt.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-18', 'Translated LNG - qt-format error (msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-qtformat-ctxt.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-19', 'Translated LNG - fuzzy strings (msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-fuzzy-ctxt.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k');
&test('2-1A', 'Translated LNG - level 9 - strip comments (msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t selftest-ctxt.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9');
&test('2-1B', 'Translated LNG - python-format error (msgctxt)', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t pythonformat.po -b pythonformat.strings -L -p Selftest,42.0,4711 -c 9 -k');

&test('2-21', 'No language code - fail', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t no-langcode.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k', 1);
&test('2-22', 'No language name - fail', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t no-langname.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k', 1);
&test('2-23', 'Non-UTF-8 PO file - fail', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t us-ascii.po -b selftest2.strings -L -p Selftest,42.0,4711 -c 9 -k', 1);

&test('2-31', 'S_HTML_DIRECTION = ltr', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t direction-ltr.po -b direction.strings -L -p Selftest,42.0,4711 -c 9');
&test('2-32', 'S_HTML_DIRECTION = rtl', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t direction-rtl.po -b direction.strings -L -p Selftest,42.0,4711 -c 9');
&test('2-33', 'S_HTML_DIRECTION = bad', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -t direction-bad.po -b direction.strings -L -p Selftest,42.0,4711 -c 9');

# POT file output
&test('3-01', 'POT file', $perlbin . ' ../scripts/makelang.pl -d testoutput -P -c 9 english.db');
&test('3-02', 'POT file - multiple dbs', $perlbin . ' ../scripts/makelang.pl -d testoutput -P -c 9 english.db extra.db');
&test('3-03', 'POT file - by scope', $perlbin . ' ../scripts/makelang.pl -d testoutput -P -c 9 -i selftest.scopes english.db');

&test('3-11', 'en.po file', $perlbin . ' ../scripts/makelang.pl -d testoutput -e -c 9 english.db');
&test('3-12', 'en.po file - multiple dbs', $perlbin . ' ../scripts/makelang.pl -d testoutput -e -c 9 english.db extra.db');
&test('3-13', 'en.po file - by scope', $perlbin . ' ../scripts/makelang.pl -d testoutput -e -c 9 -i selftest.scopes english.db');

# C++ file output
&test('4-01', 'C++ - GOGI', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -l gogi -c 9 -t en.po');
&test('4-02', 'C++ - UIQ', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -l symbian -c 9 -t en.po');
&test('4-03', 'C++ - S60', $perlbin . ' ../scripts/makelang.pl -S . -d testoutput -b selftest2.strings -L -p Selftest,42.0,4711 -l series60ui -c 9 -t en.po');

# Test driver code
sub test
{
	# Get parameters
	my ($test, $description, $cmdline, $shouldfail) = @_;

	# Check command line if test is supposed to run
	if ($#ARGV >= 0)
	{
		my $dotest = 0;
		CHECK: foreach my $regexp (@ARGV)
		{
			$dotest = 1, last CHECK if $test =~ /^$regexp/;
		}
		unless ($dotest)
		{
			print "$test - disabled\n";
			return;
		}
	}

	# Print banner
	print "$test - $description\n";

	# Clean up any lefterovers from previous tests
	map { unlink } glob ("testoutput/*");

	# Run script
	my $fail = 0;
	system($cmdline) == 0 or ++ $fail;
	if ($fail)
	{
		if (defined $shouldfail && $shouldfail)
		{
			print "Script failed (expected)\n";
			$fail = 0;
		}
		else
		{
			print "FAIL: Could not run script or script failed\n";
		}
	}
	elsif ($shouldfail)
	{
		$fail = 1;
		print "FAIL: Script should have failed but did not\n";
	}

	# Compare to reference files
	my $files = 0;
	foreach my $reffile (grep { -f } glob("reference/$test/*"))
	{
		++ $files;
		my $target = "testoutput/" . (split(m"/", $reffile))[2];
		print "Test: Compare $reffile and $target\n";
		if (open TARGET, "<$target")
		{
			if (open SRC, "<$reffile")
			{
				# Compare line-by-line, ignoring any commented lines.
				# Commented lines are either ";" (LNG files), "//" (C++
				# files), "**" (multi-line C comments) or a line with
				# "POT-Creation-Date", "PO-Revision-Date" or "Copyright",
				# since the date will be different on each run.

				# Slurp entire file.
				my @reference = <SRC>;
				my @target =    <TARGET>;

				# Loop over the reference file, comparing it to the target line
				my $targetline = 0;
				REF: for (my $refline = 0; $refline <= $#reference; ++ $refline)
				{
					# Ignore all line terminators
					$reference[$refline] =~ s/\015?\012?$//g;
					# Find next non-comment line in reference
					unless ($reference[$refline] =~ m'^(;|//|\*\*|"POT-Creation-Date|"PO-Revision-Date|.* Copyright )')
					{
						# Find next non-comment line in target
						while ($targetline <= $#target && $target[$targetline] =~ m'^(;|//|\*\*|"POT-Creation-Date|"PO-Revision-Date|.* Copyright )')
						{
							$targetline ++;
						}
						# Check if we ran out of target file to compare with
						if ($targetline > $#target)
						{
							++ $fail;
							print "FAIL: Output file is too short. Not matched:\n";
							print "  Ref<$refline>: ", $reference[$refline], "\n";
							last REF;
						}
						# Ignore all line terminators
						$target[$targetline] =~ s/\015?\012?$//g;
						# Make sure files are identical
						if ($target[$targetline] ne $reference[$refline])
						{
							++ $fail;
							print "FAIL: File contents not matched:\n";
							print "  Gen<$targetline>: ", $target[$targetline], "\n";
							print "  Ref<$refline>: ", $reference[$refline]. "\n";
							$targetline = $#target + 1, last REF;
						}
						$targetline ++;
					}
				}
				# Check if we ran out of reference file to compare with
				EXTRA: while ($targetline <= $#target)
				{
					unless ($target[$targetline] =~ m'^(;|//|\*\*|"POT-Creation-Date|"PO-Revision-Date)')
					{
						++ $fail;
						print "FAIL: Output file is too long. Not matched:\n";
						print "  Gen<$targetline>: ", $target[$targetline], "\n";
						last EXTRA;
					}
					$targetline ++;
				}
				close SRC;
			}
			else
			{
				warn "Could not read $reffile";
			}
			close TARGET;

			# Remove the target file to flag that we have compared it and
			# that it was okay.
			unlink $target or die "Cannot unlink $target: $!\n" unless $fail;
		}
		else
		{
			++ $fail;
			print "FAIL: Could not find $target\n";
		}
	}
	die if $fail;

	# Check that we have the same amount of files in the reference and target
	# directories. Otherwise stop here (if these are new tests, the tester
	# could now copy the files to the reference directory)
	my $stop = 0;
	if (!$files && !$shouldfail)
	{
		print "FAIL: Could not find reference files in reference/$test/\n";
		$stop = 1;
	}

	# Check that there are no files left over
	foreach my $genfile (glob("testoutput/*"))
	{
		++ $fail;
		$stop = 1;
		print "FAIL: Unknown output file $genfile\n";
	}

	if ($stop)
	{
		die "Stopping with new files, update reference/$test/ if necessary\n";
	}
	
	print "PASS\n" unless $fail;
}
		
