#!perl # -*- tab-width: 4; fill-column: 80 -*-

###########################
# Grab the build options
#
# default is to build with just the bonjourhosts in Debug
#
# -c : Clean build
# -r : Release build
# -a : Add all listed extra helper machines outside the subnet
#

## Build up the command line
my $cmd_prebuild = "cd ../; xcodebuild -project Opera.xcodeproj -target \"PreBuild Scripts\" -configuration";
my $cmd = "cd ../; xcodebuild -project Opera.xcodeproj -target OperaApp -configuration";

# Set the build style
if ($ARGV[0] eq "-r" || $ARGV[1] eq "-r" || $ARGV[2] eq "-r" || $ARGV[3] eq "-r")
{
	$cmd_prebuild = $cmd_prebuild." Release";
	$cmd = $cmd." Release";
}
else
{
	$cmd_prebuild = $cmd_prebuild." Debug";
	$cmd = $cmd." Debug";
}

# Add clean cleaning
if ($ARGV[0] eq "-c" || $ARGV[1] eq "-c" || $ARGV[2] eq "-c" || $ARGV[3] eq "-c")
{
	$cmd_prebuild = $cmd_prebuild." clean";
	$cmd = $cmd." clean";
}

if ($ARGV[0] eq "-u" || $ARGV[1] eq "-u" || $ARGV[2] eq "-u" || $ARGV[3] eq "-u")
{
	$cmd_prebuild = $cmd_prebuild." ARCHS='ppc i386'";
	$cmd = $cmd." ARCHS='ppc i386'";
}

$cmd = $cmd." OTHER_LDFLAGS_i386=-Wl";

# Set the machines to not distribute the build to
# $cmd = $cmd." -nodistribute";

# Add all the other possible machines. (It doesn't matter if you mix arch's since
# xcode will only use machines of the same arch.
# $cmd = $cmd." -buildhosts '10.20.15.41 10.20.15.131 10.20.22.53 10.20.22.88 10.20.19.243 10.20.19.72'";

#print ($cmd."\r\n");
system($cmd_prebuild);
system($cmd);
