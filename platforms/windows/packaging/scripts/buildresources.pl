#!perl

use strict;
use warnings;
use lib qw(../../ubs/common);
use lib qw(../../ubs/common/package);
use lib qw(../../ubs/platforms/win-package/packaging);
use lib qw(packaging/scripts);

###############################################################################
#
# build_resources.pl
#
# Perl script which is called directly from the Opera solution to build a
# package using the package_builder module.
#	
# Author: julienp@opera.com
#
###############################################################################

#Don't run when the build system is running
exit if (defined $ENV{'BUILD_SYSTEM_BUILD'} && $ENV{'BUILD_SYSTEM_BUILD'} == 1);

my $solution_dir 	= $ARGV[0];
my $target_dir 		= $ARGV[1];
my $build_type 		= $ARGV[2];
my $compiler_path	= $ARGV[3];
my $platform		= $ARGV[4];

# Remove all trailing spaces from the command line args. This is super hacky but
# required to make arguments passed from the VS command line work with paths
# that have spaces.
$solution_dir =~ s/\s+$//;
$target_dir =~ s/\s+$//;
$build_type =~ s/\s+$//;
$compiler_path =~ s/\s+$//;
$platform =~ s/\s+$//;

# Debugging
print($solution_dir."\n");
print($target_dir."\n");
print($build_type."\n");
print($compiler_path."\n");
print($platform."\n");

# Include modules
use File::Basename;
use File::Path;
use File::Spec;
use File::Copy;
use Cwd;

# Our modules
use BrowserJS;
use PublicDomain;
use OperaVersion;

use package_builder;

# Build the files/folders we use all the time
$target_dir			= File::Spec->catdir($target_dir, File::Spec->updir(), File::Spec->updir(), $build_type);
my $source_dir			= File::Spec->catdir($solution_dir, File::Spec->updir(), File::Spec->updir());
my $temp_dir			= File::Spec->catdir($target_dir, File::Spec->updir(), "VS_Output", "temp");
my $packaging_dir		= File::Spec->catdir($source_dir, "ubs", "platforms", "win-package");

my $build_nr_file		= File::Spec->catfile($source_dir, "platforms", "windows", "windows_ui", "res", "#BuildNr.rci");
my $lang_list_file		= File::Spec->catfile($source_dir, "adjunct", "resources", "lang_list.txt");
my $lang_list_file_override = File::Spec->catfile($source_dir, "adjunct", "resources", "lang_list_override.txt");
my $region_list_file	= File::Spec->catfile($source_dir, "adjunct", "resources", "region_list.txt");

# Save the current directory so we can go back at the end!
my $cwd = getcwd;

open BUILD_NR, $build_nr_file;

my $build_nr;

while (<BUILD_NR>)
{
	if ($_ =~ /^\#define VER_BUILD_NUMBER (\d+)$/)
	{
		$build_nr = $1;
		last;
	}
}
close BUILD_NR;

# Check if the temp dir exists, if not create it
if (!(-e $temp_dir))
{
	print "Creates: $temp_dir\n";
	File::Path::mkpath($temp_dir);
}
else
{
	print "Will not create: $temp_dir\n";
}

# Generating the other locales
if (-e $lang_list_file_override) { $lang_list_file = $lang_list_file_override; }
open LANG_LIST, $lang_list_file;

my @languages;

while (<LANG_LIST>)
{
	chomp();
	unless ($_ eq "" or $_ =~ /^#/) # skip comments and empty lines
	{
		push(@languages, $_);
	}
}
close LANG_LIST;

open REGION_LIST, $region_list_file;

my @regions;

while (<REGION_LIST>)
{
	chomp();
	unless ($_ eq "" or $_ =~ /^#/) # skip comments and empty lines
	{
		push(@regions, $_);
	}
}
close REGION_LIST;

chdir($compiler_path);
my @version = OperaVersion::Get($source_dir, $temp_dir);
chdir($cwd);

#my $pub_domains = PublicDomain->new();
#$pub_domains->setUAVersion($version[0]);
##Hack to not attempt download more than once on dev machines, in case there is no network
#$pub_domains->{_max_attempts} = 1;
#$pub_domains->getFile(File::Spec->catdir($source_dir, "adjunct", "resources", "defaults"), "public_domains.dat") if (defined($pub_domains));

#my $bjs = BrowserJS->new();
#$bjs->setUAVersion($version[0]);
##Hack to not attempt download more than once on dev machines, in case there is no network
#$bjs->{_max_attempts} = 1;
#$bjs->getFile($temp_dir, "browser.js") if (defined($bjs));
#my $browser_js = File::Spec->catfile($temp_dir, "browser.js");

#foreach my $lang (@languages)
#{
#	if ($lang =~ m/^zh-/i && -e $browser_js)
#	{
#		copy($browser_js, File::Spec->catfile($source_dir, "adjunct", "resources", "locale", $lang, "browser.js"));
#	}
#}

if ($build_type =~ /Instrumented/i)
{
	$build_type = "PGO";
}

my $package_builder = new package_builder();

$package_builder->SetSettings({
	build_nr => $build_nr,
	version => $version[0],
	build_output => $build_type,
	platform => $platform,
	lang_options => "",
	source_dir => $source_dir,
	target_dir => $target_dir,
	temp_dir => $temp_dir,
	packaging_dir => $packaging_dir,
	sign => 0,
	log_object => undef,

	product => 0,
	languages => \@languages,
	regions => \@regions,
	package_name => undef,
	autoupdate_package_name => undef,
	
});

$package_builder->Build();

exit;
