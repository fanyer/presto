#!/usr/bin/perl

###############################################################
#
# Script to create any files by default to make building faster
#
###############################################################

# Grab the $PROJECT_DIR from the command line
my $project_dir 	= $ARGV[0];

# Build the folders we use all the time
my $base_dir = $project_dir."/../..";

# Check if the doxyrun.ini exists
if (!(-e $base_dir."/doxyrun.ini")) {
	# Create a new doxyrun.ini so we DO NOT
	# build the documentation by default
	open(FILE,">$base_dir/doxyrun.ini");
	print FILE "# format of this file:\n";
	print FILE "# module_name interval\n";
	print FILE "# \"#\" first on a line means comment\n";
	print FILE "# the module name \"ALL\" is reserved and matches all modules (surprise!)\n";
	print FILE "# valid interval values:\n";
	print FILE "#  always\n";
	print FILE "#  hourly\n";
	print FILE "#  daily\n";
	print FILE "#  weekly\n\n";

	# On mac we DO NOT want to run this so set it to never
	print FILE "ALL never\n";
	close(FILE);
}

