#!/usr/bin/perl -w
#
#Simple Join
#2011 Aaron Beaton
#
#Simple joining of PO files
#
#The script tries to automatically prefer valid translations, that is, if one file has a valid translation
#whereas the other file has not, the valid one will be picked regardless of which file it is in. If the same string
#is valid in both files, then the first file will be preferred.
#
#Uses now a slightly modified version of Locale::PO parser instead of the heavily modified Opera::PO.
#It is easy to switch to use Opera::PO though, but due lack of time from my part this
#is left as an excersice for someone else, as Opera::PO needs some repairs as it is discarding comments and obsolete strings.
#
#Authored by granqvist@opera.com

use localepo;
use strict;
use warnings;
use Term::ANSIColor qw(:constants);

# Handle error, stolen from Peter
$SIG{'__DIE__'} = \&diehandler;

#Paths to the PO files and attributes
my $firstpo = "";
my $secondpo = "";
my $outputpo = "";
my $attributes = "";

#Arrays to hold the strings
my @strings1 = ();
my @strings2 = ();
my @outputarr = ();
my @outputnewstrs = ();
my @obsoletes = ();

#Utility variables
my $firstenc = ""; #encoding for the first file
my $secondenc = ""; #encoding for the second file

if (($ARGV[0]) && ($ARGV[0] =~ m/help/)) 
{
	print "\nUsage: 'sjoin.pl first.po second.po output.po'\n";
	print "String from first.po is preferred over second.po, unless the string is not valid in first.po but is in second.po, in which case the string from second.po is kept.\n";
	
	exit 0;
} elsif ($ARGV[2]) {
	$firstpo = $ARGV[0];
	$secondpo = $ARGV[1];
	$outputpo = $ARGV[2];
} elsif (!$ARGV[2]) {
	die "Usage: 'sjoin.pl first.po second.po output.po'\n sjoin.pl -help for more information.\n";
}

#Pre-checking the files
#I'm allowing the output to overwrite an existing po, even if it is one of the pos given as an argument, because this his handy especially
#when running this from a wrapper script, handling several joins in series.
die "$firstpo does not exist\n" unless -f $firstpo;
die "$secondpo does not exist\n" unless -f $secondpo;

open (ONE, "<$firstpo");

while (my $fline = <ONE>)
{
	if ($fline =~ m/charset/)
	{
		($firstenc) = $fline =~ m/charset=(.*?)\\n/;
		$firstenc = lc($firstenc);
		last;
	}
}

if (! $firstenc)
{
	die "***FATAL ERROR***\nHeader of file $firstpo seems to be malformed or missing. Please check the file.\n";
}

close (ONE);

open (TWO, "<$secondpo");

while (my $sline = <TWO>)
{
	if ($sline =~ m/charset/)
	{
		($secondenc) = $sline =~ m/charset=(.*?)\\n/;
		$secondenc = lc($secondenc);
		last;
	}
}

close (TWO);

if (! $secondenc)
{
	die "***FATAL ERROR***\nHeader of file $secondpo seems to be malformed or missing. Please check the file.\n";
}




if ($firstenc ne $secondenc)
{
	die "***Fatal error***\n The files are not using the same encoding! ($firstenc vs $secondenc)\n";
}

my $firstref = Locale::PO->load_file_asarray($firstpo);
my $secndref = Locale::PO->load_file_asarray($secondpo);

@strings1 = @{$firstref};
@strings2 = @{$secndref};


&combine();
&output();


sub combine
{

	my @valids = ();
	my @news = ();
	my @obsoleted = ();
	my $reflangname = '';
	my $origlangname = '';
	
	foreach my $origblock(@strings2)
	{

		
		my $wasmatched = 0;
		
		foreach my $refblock(@strings1)
		{
			#We always keep the header from the first.po
			if ((!$refblock->{handled}) && (($refblock->{msgstr} =~ m/Project-Id-Version: Opera/) || ($refblock->{msgid} =~ m/"<LanguageName>"/) || ($refblock->{msgid} =~ m/"<LanguageCode>"/)))
			{
				# Store the language name for use later on
				if ($refblock->{msgid} =~ m/"<LanguageName>"/)
				{
					$reflangname = $refblock->{msgstr}
				}
				$wasmatched = 1;
				$refblock->{handled} = 1;
				#Make sure the header is formatted correctly
				if($refblock->{msgstr} =~ m/Project-Id-Version: Opera/)
				{
					my $string = $refblock->{msgstr};
					$string =~ s/\\n([\w])/\\n"\n"$1/g;
					$string = "\"\"\n" . $string;
					$refblock->{msgstr} = $string;
				}
				push(@valids, $refblock);
			}
			
			
			#discarding header from the second.po
			if ((!$origblock->{handled}) && (($origblock->{msgstr} =~ m/Project-Id-Version: Opera/) || ($origblock->{msgid} =~ m/"<LanguageName>"/) || ($origblock->{msgid} =~ m/"<LanguageCode>"/)))
			{
				# Store the language name for use later on
				if ($origblock->{msgid} =~ m/"<LanguageName>"/)
				{
					$origlangname = $origblock->{msgstr}
				}
				$wasmatched = 1;
				$origblock->{handled} = 1;
			}
		
			if ((!$refblock->{handled}) && (!$origblock->{handled}) && (defined($refblock->{reference})) && (defined($origblock->{reference})) && ($refblock->{reference} eq $origblock->{reference}))
			{
				$wasmatched = 1;
				$refblock->{handled} = 1;
				$origblock->{handled} = 1;
				
				#cases where we want to favor secondpo instead of firstpo
				if ((!$origblock->{fuzzy}) && ($refblock->{fuzzy})) {
					#it wasn't fuzzy in the original (secondpo), but now it is (firstpo) - keep it fuzzy for review
					#$refblock->{fuzzy} = 0;
					push (@valids, $refblock);
					#note: I believe the below elsif will never match due to the first condition
				} elsif (($origblock->{msgstr} !~ m/\s*/) && ($refblock->{msgstr} =~ m/\s*/)) {
					#and the translated one
					push (@valids, $origblock);
				} else {
				
					#Same string in both po files, lets keep the string from the first.po
					#We can't actually detect obsoletes here, but I'll leave this here for future us as a reminder
					if ($refblock->{obsolete})
					{
						push (@obsoleted, $refblock);
					} else {
						#if it's fuzzy, we should keep the fuzzy mark as it should be reviewed
						#if($refblock->{fuzzy})
						#{ 
							#$refblock->{fuzzy} = 0;
						#}
						push (@valids, $refblock);
					}
				}

				last;
			}
		}
		
		if (!$wasmatched)
		{
			#The string didn't have a match in the first.po, we'll keep the string from the second
			#the obsolete part shouldn't be needed anymore
			if ($origblock->{obsolete})
			{	
				push (@obsoleted, $origblock);
			} else {
				push (@valids, $origblock);
			}
			$origblock->{handled} = 1;
		}	
	}
	
	foreach my $stringblock(@strings1)
	{
		#String was only in the first.po, so lets keep it
		if (! $stringblock->{handled})
		{
			if ($stringblock->{obsolete})
			{
				push (@obsoleted, $stringblock);
			
			} else {
				# If it's a new string marked fuzzy, we should keep the fuzzy mark as it should be reviewed
				#if($stringblock->{fuzzy})
				#{ 
					#$stringblock->{fuzzy} = 0;
				#}
				push (@news, $stringblock);
			}
		}
	}

	# Compare the language names and print a warning in red if it has changed
	if($reflangname ne $origlangname)
	{
		print RED, "WARNING: The language name has been changed!\n", RESET;
	}
	
	push(@outputarr, @valids);
	push(@outputarr, @news);
	
	#we need to check that the obsoletes are not duplicates of active strings
	foreach my $obsolete(@obsoleted)
	{
		my $matched = 0;
		foreach my $valid(@valids)
		{
			if ($valid->{msgid} eq $obsolete->{msgid})
			{
				$matched = 1;
				last;
			}
		}
		
		if (!$matched)
		{
			push(@outputarr, $obsolete);
		}
	}
}

sub output
{
	open (TRG, ">$outputpo") or die "$!";
	
	foreach my $block(@outputarr)
	{
		
		if ($block->{comment})
		{
			my $tempcomment = $block->{comment};
			chomp($tempcomment);
			$tempcomment =~ s/\n/\n#\. /gi;
			print TRG "# " . $tempcomment . "\n" ;
		}
		
		if ($block->{automatic})
		{
			my $tempcomment = $block->{automatic};
			chomp($tempcomment);
			$tempcomment =~ s/\n/\n#\. /gi;
			print TRG "#. " . $tempcomment . "\n" ;
			
		}
		
		#print TRG $block->{sco} if defined $block->{sco};
		print TRG "#: " . $block->{reference} . "\n" if defined $block->{reference};
		
		my $atrs = "#";
		if ($block->{fuzzy})
		{
			$atrs .= ", fuzzy";
		}
		
		if ($block->{c_format})
		{
			$atrs .= ", c-format";
		}
		
		if ($block->{qt_format})
		{
			$atrs .= ", qt-format";
		}
		
		if ($block->{python_format})
		{
			$atrs .= ", python-format";
		}
		
		if ($atrs =~ m/^#,/)
		{
			print TRG $atrs . "\n";
		}
		
		
		#print TRG $block->{attrs} if defined $block->{attrs};
		
		if ($block->{obsolete})
		{
            print TRG "#~ msgctxt " . $block->{msgctxt} . "\n" if defined $block->{msgctxt};
			print TRG "#~ msgid " . $block->{msgid} . "\n" if defined $block->{msgid};
			print TRG "#~ msgstr " . $block->{msgstr} . "\n" if defined $block->{msgstr};
		} else {
            print TRG "msgctxt " . $block->{msgctxt} . "\n" if defined $block->{msgctxt};
			print TRG "msgid " . $block->{msgid} . "\n" if defined $block->{msgid};
			print TRG "msgstr " . $block->{msgstr} . "\n" if defined $block->{msgstr};
		}
		
		print TRG "\n";
	}
	
	close (TRG);
}

sub diehandler
{
	print "Error!\n", shift;

	# Wait for exit on MSWIN
	if ($^O eq 'MSWin32')
	{
		print "Press <Enter> to exit:";
		my $notused = <STDIN>;
	}

	exit 1;
}
