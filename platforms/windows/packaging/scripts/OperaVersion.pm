package OperaVersion;
use strict;
use warnings;

use XML::Twig;
use File::Spec;

sub Get
{
	my $source_dir = shift;
	my $output_dir = shift;

	# Path to the quick-version.h
	my $opera_quick_versions_file = File::Spec->catfile($source_dir, "adjunct", "quick", "quick-version.h");
	
	if (-e $opera_quick_versions_file)
	{
		# Path to the opera_version.xml
		my $opera_version_file_in = File::Spec->catfile($source_dir, "platforms", "windows", "packaging", "scripts", "opera_version.xml");

		# Path to the temp opera_version.xml
		my $opera_version_file_out = File::Spec->catdir($output_dir, "opera_version.xml");

		# Check if the output folder exists, if not create it
		my $opera_version_folder_out = File::Spec->catdir($opera_version_file_out, File::Spec->updir());

		if (! -d $opera_version_folder_out) 
		{ 
			mkdir($opera_version_folder_out);
		}

		# Preprocess the operaversions.xml to replace the version defines
		my $compiler_cmd = "cl /EP /FI \"$opera_quick_versions_file\" \"$opera_version_file_in\" > \"$opera_version_file_out\"";
		system($compiler_cmd);

		# Remove all the blank lines at the start of the file
		# Read in the file
		open(version_file, $opera_version_file_out);
		my @version_file_lines = <version_file>;
		close(version_file);

		# Write out the file
		open version_file, '>', $opera_version_file_out;
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
				print version_file $line;
				$output_started = 1;
			}
		}
		close version_file;    

		# Read the file and get the versions
		my $opera_version_file = XML::Twig->new();
		$opera_version_file->parsefile($opera_version_file_out) or die "Unable to open opera_version.xml.\n";
		
		# Read the version from the file!
		return ($opera_version_file->root->first_child('version')->text,
				$opera_version_file->root->first_child('year')->text);
	}
	
	return (0, 0);
}

1;