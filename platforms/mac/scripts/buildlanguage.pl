#!/usr/bin/perl

############################################################################
#
# Used as an indpentent run to create a language file when needed
# Pass the language code on the command line
#


my ($langcode, $buildnum) = @ARGV;# or die "usage: $0 [language_code] [build_number]";


my $language_scripts_dir		= "../../../data/strings/scripts/";
my $language_translations_dir 	= "../../translations/";

system("cd ".$language_scripts_dir."; perl makelang.pl -s -u -t ".$language_translations_dir.$langcode."/".$langcode.".po -p Mac,9.50,".$buildnum." -b ../build.strings -c 9 ../english.db ../../../adjunct/quick/english_local.db");
system("cd ".$language_scripts_dir."; perl makelang.pl -s -u -t ".$language_translations_dir.$langcode."/".$langcode.".po -T ".$language_translations_dir.$langcode."/mac.po -p Mac,9.50,".$buildnum." -b ../build.strings -c 9 ../english.db ../../../adjunct/quick/english_local.db");


