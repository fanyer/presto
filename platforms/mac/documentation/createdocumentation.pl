#!/usr/bin/perl

###############################################################
#
# Script to build the Hack's documentation
#
###############################################################

# If the NaturalDocs directory doesn't exist then unzip the zip
if (!(-e "NaturalDocs"))
{
	system("unzip NaturalDocs.zip -d NaturalDocs");
}

# If the output directory doesn't exist then create it
if (!(-e "output"))
{
	mkdir("output");
}

# Flip to the NaturalDocs folder
chdir("NaturalDocs");

# Build the documentation
system("perl NaturalDocs -i ../../../../ -o HTML ../output -p ../settings");

