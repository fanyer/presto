#!perl

use strict;
use warnings;

###############################################################################
#
# package_builder.pm
#
# Perl module which puts together all the resources needed for the windows
# installer package, then builds said package
#
# Author: julienp@opera.com
#
###############################################################################

package package_builder_old;

# Include modules
use File::Basename;
use File::Copy;
use File::Path;
use File::Spec;
use Cwd;

use lib qw(../../../common/package);
use Shipping;
use feature_scanner;
use skin_builder;
	
# ShippingCallback object which controls how to copy the files
{
	package ShippingCallback;
	# Call-back for Shipping.pm
	sub new ($$) {
		my $class = shift;
		my $self = {};

		# This needs to be an absolute path so that the zipping will work
		$self->{dest_root_dir} = File::Spec->rel2abs(shift);
		$self->{files_list} = shift;
		$self->{build_output} = shift;

		bless $self, $class;
		return $self;
	}

	sub shipFile ($$$$) {
		( my $self, my $name, my $dest, my $each ) = @_;
		$name =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
		$name =~ s/\[CONFIGURATION\]/$self->{build_output}/g;
		$dest =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
		# This is hack of the century and should be removed, but legacy from the old system!!!!
		$dest =~ s/^Opera[\\\/]//; # Remove the "Opera" folder it just represents the INSTALLDIR.

		my $s = $name;
		my $d = File::Spec->catfile($self->{dest_root_dir}, $dest);

		# Create the dest directory if it doesn't exist
		my $dest_dir = File::Basename::dirname($d);
		if (!(-e $dest_dir))
		{
			File::Path::mkpath($dest_dir);
		}

#		print "Copy: ".$s." : ".$d."\n";

		# Copy file $s, $d
		if (File::Copy::copy($s, $d))
		{
			my $handler = $self->{files_list};
			print $handler $dest."\n";
		}
		else
		{
			print File::Spec->rel2abs(__FILE__) . "(" . __LINE__ . "): warning : Failed copying $s to $d. $!\n";
		}
	}

	sub zipFile ($$$$) {
		( my $self, my $path, my $dest, my $each ) = @_;
		$path =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
		$dest =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
		# This is hack of the century and should be removed, but legacy from the old system!!!!
		$dest =~ s/^Opera[\\\/]//; # Remove the "Opera" folder it just represents the INSTALLDIR.

		my $s = $path;
		my $d = File::Spec->catfile($self->{dest_root_dir}, $dest);

		# Create the dest directory if it doesn't exist
		my $dest_dir = File::Basename::dirname($d);
		#print $dest_dir."\n";
		if (!(-e $dest_dir))
		{
			File::Path::mkpath($dest_dir);
		}

		print "ZipFile: ".$s." : ".$d."\n";

		# Save the current directory so we can go back at the end!
		my $cwd = Cwd::getcwd;

		# We need to change into the folder so that the zip file has the 
		# correct paths internally
		if (-e $s)
		{
			chdir ($s);
			unlink($d);
			system("7z a -tzip -y \"$d\" . -xr!CVS -xr!thumbs.db -xr!7z.log > 7z.log");
	
			# Go back to the same directory we earlier
			chdir($cwd);
			my $handler = $self->{files_list};
			print $handler $dest."\n";
		}
	}
}

sub new
{
	my $class = shift;
	my $self = {};
	
	$self->{_build_nr} = shift;
	$self->{_version} = shift;
	$self->{_build_output} = shift;
	$self->{_languages} = shift;
	$self->{_lang_options} = shift;
	$self->{_package_name} = shift;
	$self->{_autoupdate_package_name} = shift;

	$self->{_source_dir} = shift;
	$self->{_target_dir} = shift;
	$self->{_temp_dir} = shift;
	$self->{_packaging_dir} = shift;
	
	$self->{_browser_js_src} = shift;
	$self->{_public_domain_src} = shift;
	
	$self->{_sign} = shift;


	$self->{_files_list}		= File::Spec->catfile($self->{_target_dir}, "files_list");
	$self->{_installer_archive}	= File::Spec->catfile($self->{_target_dir}, File::Spec->updir(), "Opera.7z");
	$self->{_installer_package}	= File::Spec->catfile($self->{_target_dir}, File::Spec->updir(), $self->{_package_name}) if (defined($self->{_package_name}));
	$self->{_autoupdate_package}	= File::Spec->catfile($self->{_target_dir}, File::Spec->updir(), $self->{_autoupdate_package_name}) if (defined($self->{_autoupdate_package_name}));
	$self->{_public_domain_dst}	= File::Spec->catfile($self->{_target_dir}, "defaults", "public_domains.dat");
	$self->{_i18ndata_dir}		= File::Spec->catdir($self->{_source_dir}, "data", "i18ndata");
	$self->{_i18ndata_tables_dir}	= File::Spec->catdir($self->{_i18ndata_dir}, "tables");
	$self->{_local_encoding_bin}	= File::Spec->catfile($self->{_i18ndata_tables_dir}, "encoding.bin");
	$self->{_package_encoding_bin}	= File::Spec->catfile($self->{_target_dir}, "encoding.bin");
	$self->{_setup_dir}		= File::Spec->catdir($self->{_source_dir}, "adjunct", "quick", "setup");
	$self->{_common_dir}		= File::Spec->catdir($self->{_source_dir}, "platforms", "ubscommon");
	$self->{_temp_shipping_dir}	= File::Spec->catdir($self->{_temp_dir}, "shipping_build");
	$self->{_package_xml}		= File::Spec->catfile($self->{_source_dir}, "adjunct", "quick", "setup", "package.xml");
	$self->{_binaries_package_xml}	= File::Spec->catfile($self->{_source_dir}, "platforms", "windows", "packaging", "binaries_package_2010.xml");
	$self->{_platform_dirs}		= File::Spec->catfile($self->{_source_dir}, "platforms", "windows", "packaging", "windirs.xml");
	$self->{_custom_features}	= File::Spec->catfile($self->{_source_dir}, "adjunct", "quick", "custombuild-features.h");
	$self->{_platform_features}	= File::Spec->catfile($self->{_source_dir}, "platforms", "windows", "features.h");
	$self->{_quick_features}	= File::Spec->catfile($self->{_source_dir}, "adjunct", "quick", "quick-features.h");

	$self->{_makelang_script}	= File::Spec->catfile($self->{_source_dir}, "data", "strings", "scripts", "makelang.pl");
	$self->{_build_strings}		= File::Spec->catfile($self->{_source_dir}, "data", "strings", "build.strings");
	$self->{_english_db}		= File::Spec->catfile($self->{_source_dir}, "data", "strings", "english.db");
	$self->{_local_english_db}	= File::Spec->catfile($self->{_source_dir}, "adjunct", "quick", "english_local.db");
	$self->{_package_lang_file}	= File::Spec->catfile($self->{_target_dir}, "locale", "en", "en.lng");

	$self->{_sfxmodule}		= File::Spec->catfile($self->{_packaging_dir}, "packaging", "tools", "7zSfx", "7zsd.sfx");
	$self->{_sfxconfig}		= File::Spec->catfile($self->{_packaging_dir}, "packaging", "tools", "7zSfx", "config.txt");
	$self->{_sfxconfig_autoupdate}	= File::Spec->catfile($self->{_packaging_dir}, "packaging", "tools", "7zSfx", "config_autoupdate.txt");
	$self->{_signcode}		= File::Spec->catfile($self->{_packaging_dir}, "packaging", "tools", "signcode.exe");

	bless $self, $class;
	return $self;
}

sub Build
{
	my $self = shift;
	# Build the files/folders we use all the time

	# Save the current directory so we can go back at the end!
	my $cwd = getcwd;


	# Check if the destination dir exists, if not create it
	if (!(-e $self->{_target_dir}))
	{
		print "Creates: $self->{_target_dir}\n";
		File::Path::mkpath($self->{_target_dir});
	}
	else
	{
		print "Will not create: $self->{_target_dir}\n";
	}

	open FILESLIST, '>', $self->{_files_list};

	# Create the default target set for packaging
	my @shipping_targets = ("windows", "embedded", "voice");

	# List of file to check and in the order of preference
	my @features_files = ($self->{_custom_features}, $self->{_platform_features}, $self->{_quick_features});

	# Debug
	#foreach my $features_file (@features_files)
	#{
	#	print "Files: ".$features_file."\n";
	#}

	# Create the feature scanner object
	my $fs = feature_scanner->new(@features_files);

	# Add the release/debug target
	if ($self->{_build_output} =~ /debug/i)
	{
		push(@shipping_targets, "debug");
	}
	else
	{
		push(@shipping_targets, "release");
	}

	# Add any targets from the features
	push(@shipping_targets, $fs->GetPackagingTargets(@features_files));

	# Debug
	#foreach my $shipping_target (@shipping_targets)
	#{
	#	print $shipping_target."\n";
	#}

	# Setup the shipping object
	my $ship = new Shipping($self->{_platform_dirs}, $self->{_source_dir}, $self->{_temp_shipping_dir}, $fs, undef, @shipping_targets);

	print $self->{_i18ndata_dir}."\n";

	# Is there a i18ndata checkout so we can create the chartable?
	if (-e $self->{_i18ndata_dir})
	{
		# Change to the tables folder
		chdir($self->{_i18ndata_tables_dir});
		
		# Generate the chartable
		system("python make.py tables-windows8.txt");

		# Generate the encodings.bin
		system("perl utilities\\gettablelist.pl -e .");

		# Go back to the same directory we earlier
		chdir($cwd);
		
		# Copy the chartable into the package
		if (-e $self->{_local_encoding_bin})
		{
			copy($self->{_local_encoding_bin}, $self->{_package_encoding_bin}) or print File::Spec->rel2abs(__FILE__) . "(" . __LINE__ . "): warning : Failed copying $self->{_local_encoding_bin} to $self->{_package_encoding_bin}. $!\n";
			print FILESLIST File::Spec->abs2rel($self->{_package_encoding_bin}, $self->{_target_dir})."\n";
		}
	}

	# Copy executables using the shipping object and the package.xml
	if (!$ship->parseManifest($self->{_binaries_package_xml}, new ShippingCallback($self->{_target_dir}, *FILESLIST, $self->{_build_output})))
	{
		return 0;
	}

	# Copy everything using the shipping object and the package.xml
	if (!$ship->parseManifest($self->{_package_xml}, new ShippingCallback($self->{_target_dir}, *FILESLIST, $self->{_build_output})))
	{
		return 0;
	}

	copy($self->{_public_domain_src}, $self->{_public_domain_dst});

	chdir($self->{_temp_dir});
	# Generating the en.lng file
	$self->BuildLngFile("en", $self->{_package_lang_file});
	
	foreach my $lang (@{$self->{_languages}})
	{
		$self->AddLanguageFilesFor($lang);
	}

	close FILESLIST;

	chdir ($self->{_target_dir});
	unlink($self->{_installer_archive});

	my $signcode_command = "\"".$self->{_signcode}. "\" -spc \"C:\\opera2010.spc\" -v \"C:\\opera2010.pvk\" -n \"Opera web browser\" -i \"http://www.opera.com\" -a MD5 -t \"http://timestamp.verisign.com/scripts/timstamp.dll\" ";
	
	if (defined ($self->{_installer_package}) || defined ($self->{_autoupdate_package_name}))
	{
		print "Zipping the Opera Installer:\n";
		system("7z a -t7z -mx9 -y \"$self->{_installer_archive}\" . -xr!profile -xr!thumbs.db -xr!debug_log.txt -xr!debug.txt -xr!7z.log > 7z.log");
	}
	if (defined ($self->{_installer_package}))
	{
		print "Copying the Opera Installer to \"$self->{_installer_package}\"\n";
		system("copy /b \"$self->{_sfxmodule}\" + \"$self->{_sfxconfig}\" + \"$self->{_installer_archive}\" \"$self->{_installer_package}\"");
		if ($self->{_sign})
		{
			system("$signcode_command \"$self->{_installer_package}\"");
		}
	}
	if (defined ($self->{_autoupdate_package_name}))
	{
		print "Copying the Opera Installer to \"$self->{_autoupdate_package}\"\n";
		system("copy /b \"$self->{_sfxmodule}\" + \"$self->{_sfxconfig_autoupdate}\" + \"$self->{_installer_archive}\" \"$self->{_autoupdate_package}\"");
		if ($self->{_sign})
		{
			system("$signcode_command \"$self->{_autoupdate_package}\"");
		}
	}
			
	chdir($cwd);
	
	return 1;
}

sub BuildLngFile
{
	my $self = shift;
	my $lng_id  = shift;
	my $lng_file = shift;
	
	# Build lng file from po (need to have the location of the data-directory, opera version and buildno.
	#####################################################################################################
	
	return 0 if (!(-e $self->{_makelang_script}) || !(-e $self->{_english_db}) || !(-e $self->{_local_english_db}) || !(-e $self->{_build_strings}));
	my $makelang_command = "perl \"$self->{_makelang_script}\" -c 9 -q -u -E -p Win,$self->{_version},$self->{_build_nr} -o \"$lng_file\" -b \"$self->{_build_strings}\" $self->{_lang_options}";
	
	if ($lng_id ne "en") # po file only needed when building something other than english.
	{
		my $po_file = File::Spec->catfile($self->{_source_dir}, "data", "translations", "$lng_id", "$lng_id.po");
		$makelang_command    .= " -t \"$po_file\"";
		return 0 if ( !(-e $po_file)); # We need the po file, but not if we are making english.
		if ((-e $lng_file) && (stat($lng_file))[9] > (stat($po_file))[9] && (stat($lng_file))[9] > (stat($self->{_english_db}))[9] && (stat($lng_file))[9] > (stat($self->{_local_english_db}))[9])
		{
			print FILESLIST File::Spec->abs2rel($lng_file, $self->{_target_dir})."\n";
			return 1;
		}
	}
	else
	{
		if ((-e $lng_file) && (stat($lng_file))[9] > (stat($self->{_english_db}))[9] && (stat($lng_file))[9] > (stat($self->{_local_english_db}))[9])
		{
			print FILESLIST File::Spec->abs2rel($lng_file, $self->{_target_dir})."\n";
			return 1;
		}
	}
	
	# Add the english.db and english_local.db databases
	$makelang_command .= " \"$self->{_english_db}\" \"$self->{_local_english_db}\"";
	
	my $result = `$makelang_command`; # Run the makelang command.
	if ($? != 0) # Pick up any errors.
	{
		return 0;
	}
	print FILESLIST File::Spec->abs2rel($lng_file, $self->{_target_dir})."\n";
	
	return 1;	
}

sub AddLanguageFilesFor
{
	my $self = shift;
	my $language = shift;
    
	# English localization files should *NOT* be added, the english.lng will be added separately.
	return 1 if ($language eq "en");
    
	my $locales_dir = File::Spec->catdir($self->{_target_dir}, "locale", $language);
	File::Path::mkpath($locales_dir);	
	my $transl_dir = File::Spec->catdir($self->{_source_dir}, "data", "translations", $language);
	my $package_file; my $source_file;

	# Find and copy the bookmarks file.
	$package_file = File::Spec->catfile($locales_dir, "bookmarks.adr");
	$source_file = File::Spec->catfile($transl_dir, "default.adr");
	copy($source_file, $package_file) if (-e $source_file) and print FILESLIST File::Spec->abs2rel($package_file, $self->{_target_dir})."\n";
	
	# Find and copy search.ini file.
	$package_file = File::Spec->catfile($locales_dir, "search.ini");
	$source_file = File::Spec->catfile($transl_dir, "search.ini");
	copy($source_file, $package_file) if (-e $source_file) and print FILESLIST File::Spec->abs2rel($package_file, $self->{_target_dir})."\n";
	
	# Find and copy the speeddials file.
	$package_file = File::Spec->catfile($locales_dir, "standard_speeddial.ini");
	$source_file = File::Spec->catfile($transl_dir, "speeddial_default.ini");
	copy($source_file, $package_file) if (-e $source_file) and print FILESLIST File::Spec->abs2rel($package_file, $self->{_target_dir})."\n";
	
	# Find and copy the turbosettings file.
	$package_file = File::Spec->catfile($locales_dir, "turbosettings.xml");
	$source_file = File::Spec->catfile($transl_dir, "turbosettings.xml");
	copy($source_file, $package_file) if (-e $source_file) and print FILESLIST File::Spec->abs2rel($package_file, $self->{_target_dir})."\n";
    	
	# Find and copy license file (if it exists)
	$package_file = File::Spec->catfile($locales_dir, "license.txt");
	$source_file = File::Spec->catfile($transl_dir, "license.txt");
	copy($source_file, $package_file) if (-e $source_file) and print FILESLIST File::Spec->abs2rel($package_file, $self->{_target_dir})."\n";
	
	# Need to generate the lng file for the language.
	$package_file = File::Spec->catfile($locales_dir, "$language.lng");
	$self->BuildLngFile($language, $package_file);
	    
	if ($language =~ m/^zh-/i)
	{
		$package_file = File::Spec->catfile($locales_dir, "browser.js");
		copy($self->{_browser_js_src}, $package_file) if (-e $self->{_browser_js_src}) and print FILESLIST File::Spec->abs2rel($package_file, $self->{_target_dir})."\n";
	}
		
	return 1;
}

return 1;
