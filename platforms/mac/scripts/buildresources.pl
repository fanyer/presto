 #!/usr/bin/perl

###############################################################
#
# Script to create all of the resources (english only) so
# that we can run  Opera directly after building.
# Also useful so ini, language.. etc files can be edited
# in place and tested in debug.
#
###############################################################

my $project_dir 		= $ARGV[0];
my $target_build_dir 	= $ARGV[1];
my $build_type		 	= $ARGV[2];

print($project_dir."\n");
print($target_build_dir."\n");
print($build_type."\n");

# Build the files/folders we use all the time
my $source_dir			= $project_dir."/../../";
my $i18ndata_dir		= $source_dir."data/i18ndata/";
my $scripts_dir			= $project_dir."/scripts/";
my $ubs_common_dir		= $source_dir."ubs/common/";
my $ubs_package_dir		= $ubs_common_dir."package/";
my $build_scripts_dir	= $source_dir."build/scripts/";
my $package_xml			= $source_dir."adjunct/resources/package.xml";
my $lang_list_file 		= $source_dir."adjunct/resources/lang_list_mac.txt";
my $region_list_file	= $source_dir."adjunct/resources/region_list.txt";

#Set the common perl modules path
push(@INC, $ubs_common_dir, $ubs_package_dir, $scripts_dir);

# Perl packages
use File::Basename;
use File::Copy;

# Our modules
require Shipping;
require feature_scanner;
require skin_builder;

# Pull the version and build number from files

# Build the path to quick-version.h
my $opera_quick_versions_file = File::Spec->catfile($source_dir, "adjunct", "quick", "quick-version.h");

# Check quick-version.h exists	
if (!(-e $opera_quick_versions_file))
{
	die "quick-version.h file not found\n";
}

# Path to the opera_version.xml
my $opera_version_file_in = File::Spec->catfile($scripts_dir, "opera_version.xml");

# Path to the temp opera_version.xml
my $opera_version_file_out = File::Spec->catfile($build_scripts_dir, "opera_version.xml");

# Make sure the output dir exists
system("mkdir -p \"".$build_scripts_dir."\"");

# Run the platform preprocess command
system("/usr/bin/gcc -E -P -traditional -x c -include \"".$opera_quick_versions_file. "\" \"".$opera_version_file_in."\" -o \"".$opera_version_file_out."\"");

# Check the file was outputted
if (!(-e $opera_quick_versions_file))
{
	die "Unable to generate opera_version.xml file\n";
}

# Remove all the blank lines at the start of the file
# Read in the file
open(VERSION_FILE, $opera_version_file_out);
my @version_file_lines = <VERSION_FILE>;
close(VERSION_FILE);

# Write out the file
open VERSION_FILE, '>', $opera_version_file_out;
my $line;
my $output_started = 0;
foreach $line(@version_file_lines) 
{
	# Remove whitespace
	if ($output_started == 0)
	{
		$line =~ s/^\s+//;
		$line =~ s/\s+$//;
	}

	# Output the line
	if ($output_started == 1 || length($line) )
	{			
		print VERSION_FILE $line;
		$output_started = 1;
	}
}
close VERSION_FILE;    

# Read the file and get the versions
my $opera_version_file = XML::Twig->new();
if (!$opera_version_file->safe_parsefile($opera_version_file_out))
{
	die "Failed to parse opera_version.xml\n";
}	

# Read the version from the file!
my $version	= $opera_version_file->root->first_child('version')->text;
print("Version Detected: ".$version."\n"); 

# Read the build number from the file
my $build_number_file = File::Spec->catfile($source_dir, "platforms", "mac", "Resources", "buildnum.h");

# Check the build number file exists
if (!(-e $build_number_file))
{
	die "buildnum.h file not found\n";
}

# Read the file
open(BUILDNUM_FILE, $build_number_file);
my @buildnum_file_lines = <BUILDNUM_FILE>;
close(BUILDNUM_FILE);

my $build_num = -1;

# The first line has the build number so parse it out
if ($buildnum_file_lines[0] =~ m/^\s*#define\s+VER_BUILD_NUMBER+\s([0-9]{3,5})\s*$/)
{
	$build_num = $1;
}
else
{
	die "Unable to parse build number";
}

# Check we got a build number
if ($build_num < 0)
{
	die "Build number file contains errors should be #define VER_BUILD_NUMBER 40000";
}
print("Build Number Detected: ".$build_num."\n"); 

# Load language list 
# Check the lang list file exists
if (!(-e $lang_list_file))
{
	die "lang_list_mac.txt file not found\n";
}

# Load the languages
my @languages;

open(LANG_LIST, $lang_list_file);
while (<LANG_LIST>)
{
	chomp();
	unless ($_ eq "" or $_ =~ /^#/) # skip comments and empty lines
	{
		push(@languages, $_);
	}
}
close LANG_LIST;
print("Languages to build: ".join(",", @languages)."\n");

# Load region list

if (!(-e $region_list_file))
{
	die "region_list.txt file not found\n";
}

my @regions;

open(REGION_LIST, $region_list_file);
while (<REGION_LIST>)
{
	chomp();
	unless ($_ eq "" or $_ =~ /^#/) # skip comments and empty lines
	{
		push(@regions, $_);
	}
}
close REGION_LIST;

# Create the default target set for packaging
# Why there are two "mac"s I don't know, but it doesn't work without it
my @shipping_targets = ("mac", "embedded");

# Add the release/debug target
if ($build_type =~ m/^debug$/i)
{
	push(@shipping_targets, "debug");
}
else
{
	push(@shipping_targets, "release");
}

# List of file to check and in the order of preference
my @features_files = ($source_dir."adjunct/quick/custombuild-features.h",
						$source_dir."platforms/mac/features.h",
						$source_dir."adjunct/quick/quick-features.h",
						$source_dir."modules/hardcore/features/profile_desktop.h");

# Debug
#foreach my $features_file (@features_files)
#{
#	print "Files: ".$features_file."\n";
#}

# Create the feature scanner object
my $fs = feature_scanner->new(@features_files);

# Add any targets from the features
push(@shipping_targets, $fs->GetPackagingTargets(@features_files));

# Debug
#foreach my $shipping_target (@shipping_targets)
#{
#	print $shipping_target."\n";
#}

# Setup the shipping object
my $ship = new Shipping($scripts_dir."macdirs.xml", $source_dir, $source_dir."build/scripts/", $fs, \@languages, \@regions, @shipping_targets);

# Build the resource and english folders from the shipping object
my $res_dir			= $target_build_dir."/".$ship->folderDir('RESOURCES')."/";
my $english_dir		= $target_build_dir."/".$ship->folderDir('LANGUAGE')."/en.lproj/";

# Make the resource folder if it doesn't exist
system("mkdir -p \"".$res_dir."\"");
print $res_dir."\n";

print $i18ndata_dir."\n";

# Is there a i18ndata checkout so we can create the chartable?
if (-e $i18ndata_dir)
{
	# Generate the chartable
	system("cd \"".$i18ndata_dir."tables\"; python make.py --hash -l tables-all.txt");

	# Generate the encodings.bin
	system("cd \"".$i18ndata_dir."tables\"; perl utilities/gettablelist.pl -l -e .");

	# Copy the chartable into the package
	system("cp -f \"".$i18ndata_dir."tables/encoding.bin\" \"".$res_dir."\"");
}

# Copy everything using the shipping object and the package.xml
$ship->parseManifest($package_xml, new ShipCallback($target_build_dir."/", $version, $build_num));

# Copy over the opera executable for the update
system("mkdir -p \"".$target_build_dir."/Update.app/Contents/MacOS\"");
system("cp -f \"".$target_build_dir."/Opera.app/Contents/MacOS/Opera\" \"".$target_build_dir."/Update.app/Contents/MacOS/Update\"");

# Copy the whole update application into the executable
system("cp -fR \"".$target_build_dir."/Update.app\" \"".$target_build_dir."/Opera.app/Contents/Resources\"");

# Copy the LICENSE and opera_autoupdate_checker application into the executable
# system("cp -fR ../../adjunct/autoupdate/autoupdate_checker/platforms/mac/build/opera_autoupdate \"".$target_build_dir."/Opera.app/Contents/MacOS\"");
# system("cp -fR ../../adjunct/autoupdate/autoupdate_checker/licenses/LICENSES \"".$target_build_dir."/Opera.app/Contents/MacOS\"");

# Expand the Growl.Framework
system("rm -fR \"".$target_build_dir."/Opera.app/Contents/Frameworks/Growl.framework\" \"".$target_build_dir."/Opera.app/Contents/Frameworks/__MACOSX\"");
system("unzip -qq \"".$source_dir."platforms/mac/notifications/Growl.framework.zip\" -d \"".$target_build_dir."/Opera.app/Contents/Frameworks\"");
# cleanup after unzipping
system("rm -fR \"".$target_build_dir."/Opera.app/Contents/Frameworks/__MACOSX\"");
system("rm -f \"".$target_build_dir."/Opera.app/Contents/Frameworks/Growl.framework/.DS_Store\" \"".$target_build_dir."/Opera.app/Contents/Frameworks/Growl.framework/Versions/.DS_Store\" \"".$target_build_dir."/Opera.app/Contents/Frameworks/Growl.framework/Versions/A/.DS_Store\" \"".$target_build_dir."/Opera.app/Contents/Frameworks/Growl.framework/Versions/A/Resources/.DS_Store\"");

# Copy in the Growl dictionary
system("cp -f \"".$source_dir."platforms/mac/notifications/Growl Registration Ticket.growlRegDict\" \"".$target_build_dir."/Opera.app/Contents/Resources\"");

# ShipCallback object which controls how to copy the files
{
    package ShipCallback;
    # Call-back for Shipping.pm
    sub new ($$) {
		my $class = shift;
		my $self = {};

		$self->{dest_root_dir} = shift;
		$self->{version} = shift;
		$self->{build_num} = shift;

		bless $self, $class;
		return $self;
    }

    sub shipFile ($$$$) {
		( my $self, my $name, my $dest, my $each ) = @_;
		my $s = $name;
		my $d = $self->{dest_root_dir}.$dest;
		#print $source."\n";

		# Create the dest directory if it doesn't exist
		$dest_dir = File::Basename::dirname($d);
		#print $dest_dir."\n";
		if (!(-e $dest_dir))
		{
			system("mkdir -p \"".$dest_dir."\"");
		}

#		print "Copy: ".$s." : ".$d."\n";

		# Copy file $s, $d
		unlink($d);
		File::Copy::copy($s, $d);
    }

    sub zipFile ($$$$) {
		( my $self, my $name, my $dest, my $each ) = @_;
		my $s = $name;
		my $d = $self->{dest_root_dir}.$dest;
		my $fs = $self->{feature_scanner_object};
		#print $source."\n";

		# Create the dest directory if it doesn't exist
		$dest_dir = File::Basename::dirname($d);
		#print $dest_dir."\n";
		if (!(-e $dest_dir))
		{
			system("mkdir -p \"".$dest_dir."\"");
		}

#		print "Zip: ".$s." : ".$d."\n";

		# Copy file $s, $d using the feature scanner
		system("cd \"".$s."\"; rm -f \"".$d."\"; zip -q -r \"".$d."\" * -x \\\*CVS/\*");
    }

	sub concatFiles ($$$$) {
		( my $self, my $name, my $dest, my $each ) = @_;
		my $s = $name;
		my $d = $self->{dest_root_dir}.$dest;
		#print $source."\n";

		# Create the dest directory if it doesn't exist
		$dest_dir = File::Basename::dirname($d);
		print $dest_dir."\n";
		if (!(-e $dest_dir))
		{
			system("mkdir -p \"".$dest_dir."\"");
		}

		# Make sure all the spaces in the file name have a preceding 
		# backslash otherwise the glob will fail
		$s =~ s/\s{1}/\\ /g;
		#print "Concat: ".$s." : ".$d."\n";

		unlink($d);
		while (my $file = glob($s)) {
			#print "Concat File: ".$file." : ".$d."\n";
			system("cat \"".$file."\" >> \"".$d."\"");
		}
	}

	sub fixSigPaths ($$$) {
		( my $self, my $name, my $to_temp ) = @_;
		my %file_hash = ();
		local *IN, *OUT;

		open(IN, "<", $name);
		while (<IN>)
		{
			chomp;
			(my $_path, my $sig) = split /\s+/;
			my @path = split /\//, $_path;
			my $root = shift(@path);
			if ($root eq "locale")
			{
				my $iso = shift @path;
				unshift @path, $self->getSystemLanguage($iso).".lproj";
				$file_hash{join("/", @path)} = $sig;
			}
			elsif ($root eq "region" and @path == 3) # region/$REGION/$LANG/search.ini
			{
				my $region = shift @path;
				my $lang = shift @path;
				unshift @path, $self->getSystemLanguage($lang); # without ".lproj" in this case
				unshift @path, $root, $region;
				$file_hash{join("/", @path)} = $sig;
			}
			else
			{
				$file_hash{$_path} = $sig;
			}
		}
		close(IN);

		unlink($to_temp);

		open(OUT, ">", $to_temp);
		foreach my $key (sort(keys %file_hash))
		{
			print OUT $key." ".$file_hash{$key}."\n";
		}
		close(OUT);

		return 1;
	}

	# fixes names of languages in [Language] section of region.ini file
	sub fixRegionIni($$$) {
		( my $self, my $name, my $to_temp ) = @_;
		local *IN, *OUT;

		open(IN, "<", $name);
		unlink($to_temp);
		open(OUT, ">", $to_temp);

		my $section = "";

		while(<IN>)
		{
			# if in "Language" section translate LANGUAGEs in COUNTRY=LANGUAGE lines
			if ($section eq "Language" and /\s*(\w+)\s*=\s*([\w-]+)\s*/)
			{
				print OUT $1."=".$self->getSystemLanguage($2)."\n";
			}
			else
			{
				# word(s) in brackets - new section
				if (/\[([\w\s]+)\]/)
				{
					$section = $1;
				}
				print OUT $_;
			}
		}

		close(IN);
		close(OUT);
		return 1;
	}

	sub canSkipRebuild($$)	{
		( my $self, my $dest, my $last_change_time) = @_;
		my $d = $self->{dest_root_dir}.$dest;
		
		#print("canSkipRebuild: ".$d.", time: ".(stat($d))[9].", oldtime: ".$last_change_time."\n");
		
		if ( -e $d && (stat($d))[9] > $last_change_time)
		{
			return 1;
		}
		
		return 0;
	}
	
	sub getMakelangArgs()	{
		my $self = shift;
		return "-s -p Mac,$self->{version},$self->{build_num}";
	}
	
	sub getSystemLanguage($)
	{
		( my $self, my $langcode ) = @_;
		
		# First just copy over the langcode
		my $syslangcode = $langcode;
		
		# Convert Opera lang code to Mac system lang code
		if ($langcode =~ m/^nb$/i) # Case insensitive compare
		{ 
			$syslangcode = "no";
		}
		elsif ($langcode =~ m/^es-ES$/i) # Case insensitive compare
		{
			$syslangcode = "es";
		}  
		elsif ($langcode =~ m/^pt-BR$/i) # Case insensitive compare
		{
			$syslangcode = "pt";
		}  
		elsif ($langcode =~ m/^pt$/i) # Case insensitive compare
		{
			$syslangcode = "pt-PT";
		}  
	
		# String conversion to make the language code look really nice i.e. 'en_GB'
		$syslangcode =~ s/-/_/g; 											## Replace any '-' with '_'
		$syslangcode =~ tr/A-Z/a-z/;										## Make everything lower case
		if (index($syslangcode, "_") > 0) 
		{
			substr($syslangcode, index($syslangcode, "_")) =~ tr/a-z/A-Z/;	## Make everything after the '_' upper case
		}
		
		return $syslangcode;
	}
}
