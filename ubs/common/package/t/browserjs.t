#!/usr/bin/perl -w

use strict;

use File::Spec;
use File::Path;
use Test::More tests => 10;

use File::Spec::Functions qw(rel2abs);
use File::Basename;

# Set up absolute paths so this test can be run from anywhere
my $script_dir = dirname( __FILE__);
my $base_dir = rel2abs(File::Spec->catdir($script_dir, File::Spec->updir()));
push(@INC, $base_dir);

# Set up absolute paths so this test can be run from anywhere
#my ($volume_path, $directories_path, $file_path) = File::Spec->splitpath(File::Spec->rel2abs($0));
#my $script_dir = defined($volume_path) && $volume_path ne "" ? File::Spec->catdir($volume_path, $directories_path) : $directories_path;
#my $common_dir = File::Spec->catdir($script_dir, File::Spec->updir());
#push(@INC, $common_dir);



# Start the tests!
use_ok("BrowserJS"); 
require_ok("BrowserJS"); 

my $browser_js = BrowserJS->new();

# Check if the object was created
ok(defined $browser_js, "Browserjs was created");

# Make a temp folder for holding stuff
my $temp_folder = File::Spec->catdir($script_dir, "data", $^T);
mkpath($temp_folder);

# make a testfilename
my $file_name = "browser.js";

#check that getFile doesn't work until setUAVersion has been called
ok(!$browser_js->getFile($temp_folder, $file_name), "The getFile method worked");

$browser_js->setUAVersion("11.00");

# Check if the getFile method works
ok($browser_js->getFile($temp_folder, $file_name), "The getFile method worked");

# Check if the file is really there
ok(-e File::Spec->catfile($temp_folder, $file_name), "The file is stored where it is supposed to be");

my $content = "";
{
	local $/;
	open(FILE, File::Spec->catfile($temp_folder, $file_name));
	$content = <FILE>;
	close(FILE);
}
$browser_js->{_response} = $content;

# Check if the file is really there
ok($browser_js->checkFile(), "The checkFile method returns true when it is the right file");

# Load up some other response that is wrong for this browser js type
$content = "";
{
	local $/;
	open(FILE, File::Spec->catfile($script_dir,"data", "testfile"));
	$content = <FILE>;
	close(FILE);
}
$browser_js->{_response} = $content;

# Check if the file is really there
ok(!$browser_js->checkFile, "The checkFile method fails when its the wrong file");

# Checks if the getFile method fails when only directory parameter sendt.
ok(!$browser_js->getFile($script_dir), "The getFile method fails when no filename is sent");

# Checks if the GetFile method fails when bogus directory parameter is sendt.
ok(!$browser_js->getFile("file\blabla/blaaa"), "The getFile method fails on invalid directory parameter sendt");

# Kill the temp folder
rmtree($temp_folder);
