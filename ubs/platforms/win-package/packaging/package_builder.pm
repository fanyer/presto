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

package package_builder;

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
	
sub new
{
	my $class = shift;
	my $self = {};
	
	bless $self, $class;
	return $self;
}

sub SetSettings
{
	my $self = shift;
	my ($args) = @_;
	
	$self->{_build_nr} = $args->{build_nr} if (defined($args->{build_nr}));
	$self->{_version} = $args->{version} if (defined($args->{version}));
	$self->{_product} = $args->{product} if (defined($args->{product}));
	$self->{_labs_name} = $args->{labs_name} if (defined($args->{labs_name}));
	$self->{_build_output} = $args->{build_output} if (defined($args->{build_output}));
	$self->{_platform} = $args->{platform} if (defined($args->{platform}));
	$self->{_languages} = $args->{languages} if (defined($args->{languages}));
	$self->{_lang_options} = $args->{lang_options} if (defined($args->{lang_options}));
	$self->{_regions} = $args->{regions} if (defined($args->{regions}));
	$self->{_package_name} = $args->{package_name};
	$self->{_autoupdate_package_name} = $args->{autoupdate_package_name};

	$self->{_source_dir} = $args->{source_dir} if (defined($args->{source_dir}));
	$self->{_target_dir} = $args->{target_dir} if (defined($args->{target_dir}));
	$self->{_temp_dir} = $args->{temp_dir} if (defined($args->{temp_dir}));
	$self->{_packaging_dir} = $args->{packaging_dir} if (defined($args->{packaging_dir}));
	
	$self->{_sign} = $args->{sign} if (defined($args->{sign}));
	
	$self->{log_object} = $args->{log_object} if (defined($args->{log_object}));


	$self->{_files_list}			= File::Spec->catfile($self->{_target_dir}, "files_list");
	$self->{_operaprefs_package}	= File::Spec->catfile($self->{_target_dir}, "operaprefs_default.ini");
	$self->{_installer_archive}		= File::Spec->catfile($self->{_target_dir}, File::Spec->updir(), "Opera.7z");
	$self->{_installer_package}		= defined($self->{_package_name})?File::Spec->catfile($self->{_target_dir}, File::Spec->updir(), $self->{_package_name}):undef;
	$self->{_autoupdate_package}	= defined($self->{_autoupdate_package_name})?File::Spec->catfile($self->{_target_dir}, File::Spec->updir(), $self->{_autoupdate_package_name}):undef;
	$self->{_i18ndata_dir}			= File::Spec->catdir($self->{_source_dir}, "data", "i18ndata");
	$self->{_i18ndata_tables_dir}	= File::Spec->catdir($self->{_i18ndata_dir}, "tables");
	$self->{_local_encoding_bin}	= File::Spec->catfile($self->{_i18ndata_tables_dir}, "encoding.bin");
	$self->{_package_encoding_bin}	= File::Spec->catfile($self->{_target_dir}, "encoding.bin");
	$self->{_setup_dir}				= File::Spec->catdir($self->{_source_dir}, "adjunct", "quick", "setup");
	$self->{_common_dir}			= File::Spec->catdir($self->{_source_dir}, "platforms", "ubscommon");
	$self->{_temp_shipping_dir}		= File::Spec->catdir($self->{_temp_dir}, "shipping_build");
	$self->{_package_xml}			= File::Spec->catfile($self->{_source_dir}, "adjunct", "resources", "package.xml");
	$self->{_binaries_package_xml}	= File::Spec->catfile($self->{_source_dir}, "platforms", "windows", "packaging", "binaries_package_2010.xml");
	$self->{_platform_dirs}			= File::Spec->catfile($self->{_source_dir}, "platforms", "windows", "packaging", "windirs.xml");
	$self->{_custom_features}		= File::Spec->catfile($self->{_source_dir}, "adjunct", "quick", "custombuild-features.h");
	$self->{_platform_features}		= File::Spec->catfile($self->{_source_dir}, "platforms", "windows", "features.h");
	$self->{_quick_features}		= File::Spec->catfile($self->{_source_dir}, "adjunct", "quick", "quick-features.h");
	$self->{_generated_features}	= File::Spec->catfile($self->{_source_dir}, "modules", "hardcore", "features", "profile_desktop.h");

	if (!defined($self->{_product}) || $self->{_product} == 0)
	{
		$self->{_sfxmodule}			= File::Spec->catfile($self->{_packaging_dir}, "packaging", "tools", "7zSfx", "7zsd.sfx");
	}
	else
	{
		$self->{_sfxmodule}			= File::Spec->catfile($self->{_packaging_dir}, "packaging", "tools", "7zSfx", "7zsd_Next.sfx");
	}
	
	$self->{_sfxconfig}				= File::Spec->catfile($self->{_packaging_dir}, "packaging", "tools", "7zSfx", "config.txt");
	$self->{_sfxconfig_autoupdate}	= File::Spec->catfile($self->{_packaging_dir}, "packaging", "tools", "7zSfx", "config_autoupdate.txt");
	
	$self->{_success} = 1;
}

sub Build
{
	my $self = shift;
	# Build the files/folders we use all the time

	# Save the current directory so we can go back at the end!
	my $cwd = getcwd;
	
	$self->{log_object}->inf("Packaging script beginning") if (defined($self->{log_object}));


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
	my @shipping_targets = ("windows", "embedded");

	# List of file to check and in the order of preference
	my @features_files = ($self->{_custom_features}, $self->{_platform_features}, $self->{_quick_features}, $self->{_generated_features});

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

	# Add the x64/i386 target
	if ($self->{_platform} =~ /x64/i)
	{
		push(@shipping_targets, "x64");
	}
	else
	{
		push(@shipping_targets, "i386");
	}
	
	# Add any targets from the features
	push(@shipping_targets, $fs->GetPackagingTargets(@features_files));

	# Debug
	#foreach my $shipping_target (@shipping_targets)
	#{
	#	print $shipping_target."\n";
	#}

	# Setup the shipping object
	my $ship = new Shipping($self->{_platform_dirs}, $self->{_source_dir}, $self->{_temp_shipping_dir}, $fs, $self->{_languages}, $self->{_regions}, @shipping_targets);

	print $self->{_i18ndata_dir}."\n";

	# Is there a i18ndata checkout so we can create the chartable?
	if (-e $self->{_i18ndata_dir})
	{
		# Change to the tables folder
		chdir($self->{_i18ndata_tables_dir});
		
		# Generate the chartable
		system("python make.py tables-all.txt");

		# Generate the encodings.bin
		system("perl utilities\\gettablelist.pl -e .");

		# Go back to the same directory we earlier
		chdir($cwd);
		
		# Copy the chartable into the package
		if (-e $self->{_local_encoding_bin})
		{
			if (copy($self->{_local_encoding_bin}, $self->{_package_encoding_bin}))
			{
				print FILESLIST File::Spec->abs2rel($self->{_package_encoding_bin}, $self->{_target_dir})."\n";
			}
			else
			{
				print "Warning : Failed copying $self->{_local_encoding_bin} to $self->{_package_encoding_bin}. $!\n";
				$self->{log_object}->err("Failed copying $self->{_local_encoding_bin} to $self->{_package_encoding_bin}. $!") if (defined($self->{log_object}));
				$self->{_success} = 0;
			}
		}
		else
		{
			print "Warning: Failed creating encoding.bin \n";
			$self->{log_object}->err("Failed creating encoding.bin") if (defined($self->{log_object}));
			$self->{_success} = 0;
		}
	}

	# Copy executables using the shipping object and the package.xml
	if (!$ship->parseManifest($self->{_binaries_package_xml}, $self))
	{
		print "Warning: Failed parsing $self->{_binaries_package_xml} \n";
		$self->{log_object}->err("Failed parsing $self->{_binaries_package_xml}") if (defined($self->{log_object}));
		$self->{_success} = 0;
	}

	# Copy everything using the shipping object and the package.xml
	if (!$ship->parseManifest($self->{_package_xml}, $self))
	{
		print "Warning: Failed parsing $self->{_package_xml} \n";
		$self->{log_object}->err("Failed parsing $self->{_package_xml}") if (defined($self->{log_object}));
		$self->{_success} = 0;
	}

	close FILESLIST;
	
	if ($self->{_product})
	{
		open OPERAPREFS, '>', $self->{_operaprefs_package};
		print OPERAPREFS "[System]\n";
		print OPERAPREFS "Opera Product=".$self->{_product}."\n";
		if (defined ($self->{_labs_name}))
		{
			print OPERAPREFS "Opera Labs Name=".$self->{_labs_name}."\n";
		}
		close OPERAPREFS;
	}

	chdir ($self->{_target_dir});
	unlink($self->{_installer_archive});

	my $signtool_command = "signtool sign /n \"Opera Software ASA\" /d \"Opera web browser\" /du \"http://www.opera.com\" /t \"http://timestamp.verisign.com/scripts/timstamp.dll\" ";
	
	if (defined ($self->{_installer_package}) || defined ($self->{_autoupdate_package_name}))
	{
		print "7Zipping the Opera Installer:\n";
		$self->{log_object}->inf("7Zipping the Opera Installer") if (defined($self->{log_object}));
		
		system("7z a -t7z -mx9 -y \"$self->{_installer_archive}\" . -xr!profile -xr!thumbs.db -xr!debug_log.txt -xr!debug.txt -xr!7z.log > 7z.log");
		if ($? != 0)
		{
			print "Warning: Failed 7zipping $self->{_installer_archive}.\n";
			$self->{log_object}->err("Failed 7zipping $self->{_installer_archive}.") if (defined($self->{log_object}));
			$self->{_success} = 0;
		}
	}
	if (defined ($self->{_installer_package}))
	{
		print "Adding 7-zip SFX header to build $self->{_installer_package}\n";
		$self->{log_object}->inf("Adding 7-zip SFX header to build $self->{_installer_package}.") if (defined($self->{log_object}));
		
		system("copy /b \"$self->{_sfxmodule}\" + \"$self->{_sfxconfig}\" + \"$self->{_installer_archive}\" \"$self->{_installer_package}\"");
		if ($? != 0)
		{
			print "Warning: Failed writing $self->{_installer_package}.\n";
			$self->{log_object}->err("Failed writing $self->{_installer_package}.") if (defined($self->{log_object}));
			$self->{_success} = 0;
		}
		elsif ($self->{_sign})
		{
			system("$signtool_command \"$self->{_installer_package}\"");
			if ($? != 0)
			{
				print "Warning: Failed signing $self->{_installer_package}.\n";
				$self->{log_object}->err("Failed signing $self->{_installer_package}.") if (defined($self->{log_object}));
				$self->{_success} = 0;
			}
		}
	}
	if (defined ($self->{_autoupdate_package}))
	{
		print "Adding 7-zip SFX header to build $self->{_autoupdate_package}\n";
		$self->{log_object}->inf("Adding 7-zip SFX header to build $self->{_autoupdate_package}.") if (defined($self->{log_object}));
		
		system("copy /b \"$self->{_sfxmodule}\" + \"$self->{_sfxconfig_autoupdate}\" + \"$self->{_installer_archive}\" \"$self->{_autoupdate_package}\"");
		if ($? != 0)
		{
			print "Warning: Failed writing $self->{_autoupdate_package}.\n";
			$self->{log_object}->err("Failed writing $self->{_autoupdate_package}.") if (defined($self->{log_object}));
			$self->{_success} = 0;
		}
		elsif ($self->{_sign})
		{
			system("$signtool_command \"$self->{_autoupdate_package}\"");
			if ($? != 0)
			{
				print "Warning: Failed signing $self->{_autoupdate_package}.\n";
				$self->{log_object}->err("Failed signing $self->{_autoupdate_package}.") if (defined($self->{log_object}));
				$self->{_success} = 0;
			}
		}
	}
			
	chdir($cwd);
	
	return $self->{_success};
}

sub shipFile ($$$$)
{
	( my $self, my $name, my $dest, my $each ) = @_;
	$name =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
	$dest =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
	# This is hack of the century and should be removed, but legacy from the old system!!!!
	$dest =~ s/^Opera[\\\/]//; # Remove the "Opera" folder it just represents the INSTALLDIR.

	my $s = $name;
	my $d = File::Spec->catfile($self->{_target_dir}, $dest);

	# Create the dest directory if it doesn't exist
	my $dest_dir = File::Basename::dirname($d);
	if (!(-e $dest_dir))
	{
		File::Path::mkpath($dest_dir);
	}

	$self->{log_object}->inf("Copying $s to $d.") if (defined($self->{log_object}));

	# Copy file $s, $d
	if (File::Copy::copy($s, $d))
	{
		print FILESLIST $dest."\n";
	}
	else
	{
		print "Warning : Failed copying $s to $d. $!\n";
		$self->{log_object}->err("Failed copying $s to $d. $!") if (defined($self->{log_object}));
		$self->{_success} = 0;
	}
}

sub zipFile ($$$$)
{
	( my $self, my $path, my $dest, my $each ) = @_;
	$path =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
	$dest =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
	# This is hack of the century and should be removed, but legacy from the old system!!!!
	$dest =~ s/^Opera[\\\/]//; # Remove the "Opera" folder it just represents the INSTALLDIR.

	my $s = $path;
	my $d = File::Spec->catfile($self->{_target_dir}, $dest);

	# Create the dest directory if it doesn't exist
	my $dest_dir = File::Basename::dirname($d);
	#print $dest_dir."\n";
	if (!(-e $dest_dir))
	{
		File::Path::mkpath($dest_dir);
	}

	print "Zipping $s to $d\n";
	$self->{log_object}->inf("Zipping $s to $d.") if (defined($self->{log_object}));

	# Save the current directory so we can go back at the end!
	my $cwd = Cwd::getcwd;

	# We need to change into the folder so that the zip file has the 
	# correct paths internally
	if (-e $s)
	{
		chdir ($s);
		unlink($d);
		system("7z a -tzip -y \"$d\" . -xr!CVS -xr!thumbs.db -xr!7z.log > 7z.log");
		if ($? != 0)
		{
			print "Warning: Failed zipping $d.\n";
			$self->{log_object}->err("Failed zipping $d.") if (defined($self->{log_object}));
			$self->{_success} = 0;
		}

		# Go back to the same directory we earlier
		chdir($cwd);
		print FILESLIST $dest."\n";
	}
}

sub concatFiles ($$$$) {
	( my $self, my $path, my $dest, my $each ) = @_;
	$path =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
	$dest =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
	# This is hack of the century and should be removed, but legacy from the old system!!!!
	$dest =~ s/^Opera[\\\/]//; # Remove the "Opera" folder it just represents the INSTALLDIR.

	my $s = $path;
	my $d = File::Spec->catfile($self->{_target_dir}, $dest);

	# Create the dest directory if it doesn't exist
	my $dest_dir = File::Basename::dirname($d);
	#print $dest_dir."\n";
	if (!(-e $dest_dir))
	{
		File::Path::mkpath($dest_dir);
	}

	print "Concatenating $s to $d\n";
	$self->{log_object}->inf("Concatenating $s to $d.") if (defined($self->{log_object}));

	unlink($d);
	system("copy /b /y \"$s\" \"$d\"");
	if ($? != 0)
	{
		print "Warning: Failed concatenating $d.\n";
		$self->{log_object}->err("Failed concatenating $d.") if (defined($self->{log_object}));
		$self->{_success} = 0;
	}

	print FILESLIST $dest."\n";
}

sub canSkipRebuild($$)
{
	( my $self, my $dest, my $last_change_time) = @_;
	
	$dest =~ s/\//\\/g; # If the path contains unix /-es, replace with windows \-es.
	# This is hack of the century and should be removed, but legacy from the old system!!!!
	$dest =~ s/^Opera[\\\/]//; # Remove the "Opera" folder it just represents the INSTALLDIR.
	my $d = File::Spec->catfile($self->{_target_dir}, $dest);
	
	if ( -e $d && (stat($d))[9] > $last_change_time)
	{
		print FILESLIST $dest."\n";
		return 1;
	}
	
	return 0;
}

sub getMakelangArgs()
{
	my $self = shift;
	return "-E -p Win,$self->{_version},$self->{_build_nr} $self->{_lang_options}";
}

sub getSystemLanguage($)
{
	( my $self, my $lang ) = @_;
	return $lang;
}

sub replacePathVariables($)
{
	( my $self, my $path ) = @_;
	$path =~ s/\[CONFIGURATION\]/$self->{_build_output}/g;
	return $path;
}

sub fixSigPaths ($$$)
{
	return 0;
}

sub fixRegionIni($$$) { return 0; }

return 1;
