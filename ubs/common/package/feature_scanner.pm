#!perl  # -*- tab-width: 4; fill-column: 80 -*-

############################################################
#
# Processes feature files to determine if a feature is on,
# and copies resources, with feature prefixes, copying lines
# for features that are on, ignoring lines for features that
# are off.
#
# NOTE: This file is used OUTSIDE of the UBS so don't use
# any common stuff here, you have to pass it in!
#
############################################################

use strict;

package feature_scanner;

use File::Copy;


#############################
# LIMITATIONS
# ===========
# Doesn't use the compiler pre-processor therefore
# Doesn't handle commented out lines
#   This is especially bad if you comment out a feature in a higher up file (i.e. platform) but have it
#   defined in a different way in the common file (i.e. quick)
# Doesn't handle #ifdef's inside a file where there can be multiple and different entries
# If there are multiple entries, including #ifdef-ed or commented out, it will return NO for the feature
#
#############################


#############################
# my $fs = feature_scanner->new(@array_of_files_to_parse);
#
# Takes an array of files it will parse looking for the fixed feature defines listed
# below in the function.
#
# The order of the file in the array is VERY IMPORTANT. It should be the order in which the compiler will
# use the files. In the case of Mac desktop it's quick/custombuild-features.h, mac/features.h and
# quick/quick-features.h. This should be very similar for all desktop platforms
#
# Use newVerbose instead of new if you want debug output.
#
#############################

sub new
{
	my $proto = shift;
	my $class = ref($proto) || $proto;
	my $self  = {};
	bless($self, $class);

	$self->{verbose} = 0;

	$self->init(@_);
	return $self;
}

sub newVerbose
{
	my $proto = shift;
	my $class = ref($proto) || $proto;
	my $self  = {};
	bless($self, $class);

	$self->{verbose} = 1;

	$self->init(@_);
	return $self;
}

sub init
{
    my $self = shift;
	my @features_files = @_;

	# Debug
	#foreach my $features_file (@features_files)
	#{
	#	print "Files: ".$features_file."\n" if $self->{verbose};
	#}

	# Fixed list of features to include in the scan
	# This is the place to add any new features that need to be scanned and
	# used by this perl module, you must add entries for each of the
	# targets in the @targets_to_features array below
	my @features_to_scan = ("FEATURE_WEBSERVER",
							"FEATURE_MEDIA",
							"FEATURE_SELFTEST",
							"FEATURE_INTEGRATED_DEVTOOLS",
							"FEATURE_SUPPORT_SYNC_NOTES",
							"FEATURE_HTML_COMPOSE",
							"FEATURE_OBML2D",
							"FEATURE_TAB_THUMBNAILS",
							"FEATURE_WIDGET_RUNTIME",
							"FEATURE_WEB_TURBO_MODE",
							"FEATURE_SPEED_DIAL_GLOBAL_CONFIG",
							"FEATURE_SHOW_DISCOVERED_DEVICES",
							"FEATURE_FULL_UNITE_SERVICES",
							"FEATURE_STUB_UNITE_SERVICES",
							"FEATURE_DOM_GEOLOCATION",
							"FEATURE_IRC");

	# These targets are the resource targets used by the shipping packaging system
	# and must match the above @features_to_scan array above
	my @targets_to_features =  ("alien",
								"media",
								"selftest",
								"devtools",
								"syncnotes",
								"htmlcompose",
								"obml2d",
								"tabbedthumbnails",
								"widgetruntime",
								"turbomode",
								"speeddialconfig",
								"asd",
								"fullservices",
								"stubservices",
								"geolocation",
								"ircsupport");

	# Set up the 2 members that will hold the lists of features and targets
	# which will be used in the function calls below e.g.
	# FEATURE_WEBSERVER|FEATURE_MEDIA|FEATURE_SELFTEST|
	# alien|media|no_target|
	$self->{_FEATURES_ENABLED} = "";
	$self->{_TARGETS_ENABLED}  = "";

	# loop the features
	for (my $index = 0; $index < scalar(@features_to_scan); $index++)
	{
		my $feature_name = $features_to_scan[$index];

		# Start with no nos, and no yess
		my $yes = 0;
		my $no = 0;

		print $feature_name."\n" if $self->{verbose};

		# Loop the files checking for the feature, if we find it then just
		# return the setting and stop
		foreach my $features_file (@features_files)
		{
			# Reset the counts
			$yes = 0;
			$no = 0;

			print $features_file."\n" if $self->{verbose};

			# Open the file and read all the lines
			open(ISON_FILE, '<', $features_file) or next; # File problem so just skip
			my @lines = <ISON_FILE>;
			close(ISON_FILE);

			foreach my $line (@lines)
			{
				$line =~ s|//.*$||;

				# Check if the feature is on
				if ($line =~ m/define\s*$feature_name\s*YES/)
				{
					print $line if $self->{verbose};
					# feature was set to YES
					$yes += 1;
				}

				# Check if the feature is off
				if ($line =~ m/define\s*$feature_name\s*NO/)
				{
					print $line if $self->{verbose};
					# feature was set to NO
					$no += 1;
				}
			}

			# Stop processing the next file if the entry was found
			if (($yes + $no) > 0)
			{
				last;
			}
		}

		# If there was at least 1 yes and no nos then return save the feature as set
		if ($yes > 0 && $no == 0)
		{
			# Add to the feature
			$self->{_FEATURES_ENABLED} .= $feature_name."|";

			# Add to target list
			$self->{_TARGETS_ENABLED} .= $targets_to_features[$index]."|";
		}
	}

	# Debug
	#print $self->{_FEATURES_ENABLED}."\n" if $self->{verbose};
	#print $self->{_TARGETS_ENABLED}."\n" if $self->{verbose};
}

#########################################
# my @targets = GetPackagingTargets();
#
# Returns an array that holds all the packaging targets which can be passed to
# the shipping script
#
#########################################

sub GetPackagingTargets
{
	my $self = shift;

	# return targets enabled as an array
	return split(/\|/, $self->{_TARGETS_ENABLED});
}

#########################################
# $fs->IsOn("FEATURE_NAME");
#
# Checks if an individual feature is set to YES
#
#########################################

sub IsOn
{
	my $self = shift;
	my $feature_name = shift; # Feature to check

	# searches features enabled for the feature
	if ($self->{_FEATURES_ENABLED} =~ m/$feature_name\|/)
	{
		return 1;
	}

	return 0;
}

### All of the following should move to create.pl !  See notes on bug 294884.

#########################################
# $fs->NeedsProcessing($src);
#
# Returns true if the file needs special processing.
# This is currently when the file is a text file and ends in ".ini".
#
#########################################

sub NeedsProcessing
{
	my $self = shift;
	my $src  = shift;	# Source file to copy from

	return (-T $src && $src =~ m/.ini$/i);
}


#########################################
# $fs->CopyFile($src, $dest, (optional) $force);
#
# Copies a file, this is used for coping of resource files into a package and
# conditionally processing any lines which are prepended with FEATURE_NAME or
# !FEATURE_NAME. A line prepended with FEATURE_NAME will only be included if the
# feature is enabled; an exclamation point inverts the check. The function will
# only have this behaviour if $force == 1 or NeedsProcessing returns true (see
# above).
#
# This is used so ini files can now contain text that is removed if a feature is not on.
# e.g.
# FEATURE_WEBSERVER [Kiosk Reset Dialog]
# FEATURE_WEBSERVER Title = D_KIOSK_RESET_TITLE
# FEATURE_WEBSERVER Group0, 0, , 10, 10, 330, 25, Center, End
# FEATURE_WEBSERVER MultilineEdit, 0, Edit, 0, 0, 330, 25, Fixed, End
#
# Having the above line in dialog.ini will make sure that the dialog is only included
# when the FEATURE_WEBSERVER is set to YES
#
# This behaviour is on a line by line basis. The only logical operator available
# is ! (negation), and you can only use one feature define per line. All symbols
# to check for must start with FEATURE_.
#
# At the time of creation this function is intented to be used for all *.ini files and
# the quick/english_local.db (with the force flag of course, on the .db file)
#
# All other files are simply copied over
#
#########################################

sub CopyFile
{
	my $self = shift;
	my $src  = shift;	# Source file to copy from
	my $dest = shift;	# Destination file to copy to
	my $force = shift;	# Set to force use the copy   (Optional)

	print $src."\n" if $self->{verbose};
	print $dest."\n" if $self->{verbose};

	# Check it's a text file and ends in ".ini" or forced
	if ((defined($force) && $force == 1) || $self->NeedsProcessing($src))
	{
		# ini Text file
		#print "ini Text file\n" if $self->{verbose};

		# Open the file and read all the lines
		open(FEATURE_SRC_FILE, '<', $src) or return 0; # Couldn't open source file so error
		my @lines = <FEATURE_SRC_FILE>;
		close(FEATURE_SRC_FILE);

		# Create the new file
		open(FEATURE_DEST_FILE, '>', $dest) or return 0; # Couldn't create dest file so error

		# Loop through all the lines in the source file
		foreach my $line (@lines)
		{
			# First check for ;FEATURE, which people forget and do
			if ($line =~ m/^\s*;\s*!?FEATURE_[_0-9A-Za-z]+\s/)
			{
				# ignore this line
			}
			elsif ($line =~ m/^\s*!?FEATURE_[_0-9A-Za-z]+\s/)
			{
				# check for FEATURE_ or !FEATURE_ prefix
				# Strip leading spaces
				$line =~ s/^\s*//;

				my $feature_name = $line;

				# Take the next whitespace and everything that follows and replace it with nothing
				$feature_name =~ s/\s.*\n//;

				# If the condition starts with !, invert the check and strip the ! from $feature_name
				my $pass_condition = 1;
				if ($feature_name =~ /^!/)
				{
					$feature_name =~ s/^!//;
					$pass_condition = 0;
				}

				#print $feature_name."\n" if $self->{verbose};

				# look for the feature in the features enabled list
				if ($self->IsOn($feature_name) == $pass_condition)
				{
					# if the condition is satisfied, remove FEATURE part then print line and trailing whitespace
					$line =~s/^!?$feature_name\s*//;
					# Write the line into the file
					print FEATURE_DEST_FILE $line;
				}
			}
			else
			{
				# No prefix so just write it into the file
				print FEATURE_DEST_FILE $line;
			}
		}

		# close the new file down
		close(FEATURE_DEST_FILE);
	}
	else
	{
		#print "Binary file\n" if $self->{verbose};

		# Do a straight copy
		copy($src, $dest) or return 0;
	}

	# Copy succeeded
	return 1;
}

1; # so the require or use succeeds
