#!perl  # -*- tab-width: 4; fill-column: 80 -*-

############################################################ 
#
# This script is designed to build a skin.zip file for
# use in Opera by reading the skin.ini file and then only
# including the images listed in it. 
#
# First the skin.ini will be processed with the 
# feature_scanner to remove and feature defines
#
# NOTE: This file is used OUTSIDE of the UBS so don't use 
# any common stuff here, you have to pass it in!
#
############################################################ 

use strict;

package skin_builder;

# Perl packages
use Cwd;
use File::Path;
use File::Basename;
use File::Copy;

# Our packages
require package::feature_scanner;

############################################################ 
#
# BuildSkinLayout takes a folder containing a skin.ini file 
# loads it up and copys the entire directory structure to 
# the location specified and removed all unwanted images 
# from the skin
#
############################################################ 

sub BuildSkinLayout
{
	my $fs = shift;					# Feature scanner object
	my $skin_src_dir = shift;
	my $skin_dest_dir = shift;
    my $add_hires_files = shift || 0;
    my $verbose = shift || 0;

	# Make sure we have the source and destination folders
	if (!defined($skin_src_dir) || $skin_src_dir eq "" ||
		!defined($skin_dest_dir) || $skin_dest_dir eq "")
	{
		print "No source or destination folders specified!\n";
		return 0;
	}

	#print $skin_src_dir."\n" if $verbose;
	#print $skin_dest_dir."\n" if $verbose;

	# Kill everything in the dest folder first
	rmtree($skin_dest_dir);

	# Create the new destination folder
	mkpath($skin_dest_dir);
	
	# Build paths to src and dest skin.ini files
	my $src_skin_file = File::Spec->catfile($skin_src_dir, "skin.ini");
	my $dest_skin_file = File::Spec->catfile($skin_dest_dir, "skin.ini");

	# Check the src skin.ini file exists
	if (!(-e $src_skin_file))
	{
#		print "No source skin.ini found.\n" if $verbose;
		return 0;
	}

	# First of all copy over the skin.ini file using the feature scanner
	# so it removes all the features that are not supported
	$fs->CopyFile($src_skin_file, $dest_skin_file);
	
	# Check the dest skin.ini file was copied over
	if (!(-e $dest_skin_file))
	{
#		print "No destination skin.ini created.\n" if $verbose;
		return 0;
	}

	# Load up the file and loop the lines 1 by 1 looking for a filename 
	# and if found add it to a list an arroy of files to copy
	open(SKIN_FILE, File::Spec->catfile($skin_dest_dir, "skin.ini"));
	my @skin_file_lines = <SKIN_FILE>;
	close(SKIN_FILE);

	my @files_to_copy;
			
	# Loop each line and build the files to copy list	
	foreach my $line (@skin_file_lines) 
	{
		# check if this line starts with a semi colon and ignore it if it does
		if (!($line =~ m/^\s*;/))
		{
			# Check if the line ends with an image extension (.png, .jpg, .gif .svg)
			if ($line =~ m!=\s*(.*)\.(png|jpg|jpeg|gif|svg).*$!)
			{
				my $filename = "$1.$2";

				# Mac skins can have additional hires images with @2x added
				my $filename_hires = "$1\@2x.$2";

				#Check if the file is already included in the array
				my $is_there = grep $_ eq $filename, @files_to_copy;
				if (!$is_there)
				{
					push(@files_to_copy, $filename);

					# Only add the hires file if it actually exists
					my $src_file = File::Spec->catfile($skin_src_dir, $filename_hires);
					if ($add_hires_files && -e $src_file)
					{
						push(@files_to_copy, $filename_hires);
					}
				}
			}
		}


	}
	
	# print number of files to copy
	print @files_to_copy." skin files found\n" if $verbose;

	# copy over all of the files!
	foreach my $file (@files_to_copy) 
	{
#		print $file."\n" if $verbose;

		my $src_file = File::Spec->catfile($skin_src_dir, $file);
		my $dest_file = File::Spec->catfile($skin_dest_dir, $file);

#		print $src_file."\n" if $verbose;
#		print $dest_file."\n" if $verbose;
		
		# Create the dest directory if it doesn't exist
		my $dest_dir = File::Basename::dirname($dest_file);
		#print $dest_dir."\n" if $verbose;
		if (!(-e $dest_dir))
		{
			mkpath($dest_dir);
		}
		
		# Copy the file over
		copy($src_file, $dest_file);
	}
	
	return 1;
}

1; # so the require or use succeeds
