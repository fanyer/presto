#!/usr/bin/perl

###############################################################
#
# Script to strip the symbols from the release Opera
# framework and put a copy of the  unstripped framework in
# the OperaApp folder to use as a "map" file
#
###############################################################

# Grab the $PROJECT_DIR from the command line
my $project_dir 	 	= $ARGV[0];
my $built_products_dir 	= $ARGV[1];
my $build_type	 		= $ARGV[2];

# Build the folders we use all the time
my $framework_dir	= $built_products_dir."/Opera.app/Contents/Frameworks/Opera.framework/Versions/A";
my $mac_dir			= $project_dir;

# Make sure we have all the parameters
if (!defined($project_dir) || !defined($built_products_dir) || !defined($build_type) || 
	$project_dir eq ""  || $built_products_dir eq "" || $build_type eq "")
{
	die "Stripping script is missing parameters\n";
}

if (!($build_type =~ m/^debug$/i))
{
	printf("Stripping symbols\n");
	
	# Check if the release framework even exists (this is to fix first time debug builds)
	if (-e $framework_dir."/Opera")
	{
	#	print("Opera Framework Found\n");
		# Strip out the symbols
		system("strip -ur -s  \"".$mac_dir."/opera.exp\" \"".$framework_dir."/Opera\" -o \"".$built_products_dir."/Opera\"");
	
		# Check if the stripped file exists
		if (-e $built_products_dir."/Opera")
		{
	#		print("Stripped Opera Framework Found\n");
	
			# Swap the stripped and unstripped Opera frameworks over
			system("cp -f  \"".$framework_dir."/Opera\" \"".$built_products_dir."/OperaUnstripped\"");
			system("cp -f  \"".$built_products_dir."/Opera\" \"".$framework_dir."/Opera\"");
			
			exit; # Exit since everything worked
 		}
	}

	die "Stripping of Opera Framework failed\n";
}
else
{
	printf("I'm not stripping!\n");
}

#print("Done\n");
