#!/usr/bin/perl -w
# 
# File name:          makespell.pl
# Contents:           Converts the string database into a text file which can
#                     be fed to a spelling checker, such as ispell. The fixes
#                     must be manually added back to the english.db, however.
#
# Copyright © 2003-2006 Opera Software ASA. All rights reserved.
# This file is part of the Opera web browser.  It may not be distributed
# under any circumstances.

require v5.6.0;

# Handle error
$SIG{'__DIE__'} = \&diehandler;

# Open files
if (open DB, '<', 'english.db')
{
	open TXT, '>', 'english.txt'
		or die "Cannot create english.txt: $!\n";
}
elsif (open DB, '<', '../english.db')
{
	open TXT, '>', '../english.txt'
		or die "Cannot create english.txt: $!\n";
}
else
{
	die "Cannot open english.db: $!\n";
}

# Parse database
$num = 0;
while (<DB>)
{
	if (/^.*\s*=\s*([0-9]+)$/)
	{
		$num = $1;
	}

	# Read the captions
	if (/^.*\.caption\s*=\s*"(.*)"\s*$/)
	{
		# Strip out stuff that would just confuse the spelling checker
		my $txt = $1;
		$txt =~ s/&//g;
		$txt =~ s/%l?[suid]//g;
		$txt =~ s/\\[tnr]/ /g;
		print TXT $num, ' ' if $num;
		print TXT $txt, "\n";
		$num = 0;
	}
}

print <<'EOM';
The file "english.txt" has now been created and can be fed to a spelling
checker, such as ispell. The corrections made during the spelling check
must however be manually fed back into the english.db file to take effect.
EOM

sub diehandler
{
	print "Fatal error: ", shift;

	# Wait for exit on MSWIN
	if ($^O eq 'MSWin32')
	{
		print "Press <Enter> to exit:";
		my $notused = <STDIN>;
	}

	exit 0;
}
