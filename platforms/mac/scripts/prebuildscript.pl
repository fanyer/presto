#!/usr/bin/perl
my $project_dir			= $ARGV[0];
my $target_build_dir	= $ARGV[1];
my $build_type			= $ARGV[2];
my $use_jumbo_compile	= $ARGV[3];
my $archs = $ARGV[4];

my $root_folder			= $project_dir."/../../";
my $scripts_dir			= $project_dir."/scripts/";

print("project_dir: ".$project_dir."\n");
print("target_build_dir: ".$target_build_dir."\n");
print("build_type: ".$build_type."\n");
print("use_jumbo_compile: ".$use_jumbo_compile."\n");
print("archs: ".$archs."\n");

my $update_checker_dir = "../../adjunct/autoupdate/autoupdate_checker/platforms/mac/";
if ($archs eq "i386") {
	print("Build opera_autoupdatechecker for i386\n");
	_runcommand("make -C \"".$update_checker_dir."\" -f Makefile.m32");
}
elsif ($archs eq "x86_64") {
	print("Build opera_autoupdatechecker for x86_64\n");
	_runcommand("make -C \"".$update_checker_dir."\" -f Makefile.m64");
}
else {
	print("Build opera_autoupdatechecker for i386 and x86_64\n");
	_runcommand("make -C \"".$update_checker_dir."\" -f Makefile.m64");
	_runcommand("mv \"".$update_checker_dir."build/opera_autoupdate\" \"".$update_checker_dir."build/opera_autoupdate64\"");
	_runcommand("make clean -C \"".$update_checker_dir."\" -f Makefile.m64");
	_runcommand("make -C \"".$update_checker_dir."\" -f Makefile.m32");
	_runcommand("mv \"".$update_checker_dir."build/opera_autoupdate\" \"".$update_checker_dir."build/opera_autoupdate32\"");
	_runcommand("lipo \"".$update_checker_dir."build/opera_autoupdate32\" \"".$update_checker_dir."build/opera_autoupdate64\" -create -output \"".$update_checker_dir."build/opera_autoupdate\"");
}

_runcommand("cd \"".$scripts_dir."\"; perl createdefaultfiles.pl \"".$project_dir."\"");
_runcommand("rm -rf \"".$target_build_dir."/Opera.app/Contents/Frameworks/Opera.framework/Versions/A/Headers\"");
_runcommand("rm -rf \"".$target_build_dir."/Opera.app/Contents/Frameworks/Opera.framework/Headers\"");

_runcommand("mkdir -p \"".$target_build_dir."/Opera.app/Contents/Frameworks/Opera.framework/Versions/A/Headers\"");

_runcommand("ln -fs \"Versions/A/Headers\" \"".$target_build_dir."/Opera.app/Contents/Frameworks/Opera.framework/Headers\"");

_runcommand("cp -f \"".$project_dir."/embrowser/EmBrowserInit.h\" \"".$target_build_dir."/Opera.app/Contents/Frameworks/Opera.framework/Versions/A/Headers/\"");

my $cmd = "cd \"".$target_build_dir."\"; python -W ignore ../../modules/hardcore/scripts/operasetup.py -D_MACINTOSH_ -DSIXTY_FOUR_BIT";
if ($build_type =~ m/^debug$/i)
{
	$cmd .= " -D_DEBUG -DFEATURE_SELFTEST=YES";
}
else
{
	$cmd = $cmd." -DFEATURE_SELFTEST=NO";
}
$cmd = $cmd." --mainline_configuration=current --platform_product_config=platforms/mac/product_config.h -I../.. --generate_pch";
my $status = _runcommand($cmd);
if (!$status) {
	exit 1;
}

$status = _runcommand("python \"".$root_folder."adjunct/desktop_util/search/generate_hardcoded_searches.py\"");
if (!$status) {
	exit 1;
}

$status = _runcommand("python \"".$scripts_dir."buildproject.py\" \"".$root_folder."\" \"".$scripts_dir."project-skeleton.txt\" ".$use_jumbo_compile);
if (!$status) {
	exit 1;
}

sub _runcommand
{
	my $cmd = shift;	# Command to run
	if (defined($cmd) && $cmd ne "")
	{
#	      print("\$cmd: ".$cmd."\n");
 
		# Run the command
		my @output = `$cmd`;
#	      print(@output."\n");
 
		# Get the return of the command run
		my $return_code = ($? >> 8);
		
		# Swap the return code to a true false scheme 
		# because I really don't care about the error itself
		if ($return_code == 0)
		{
			return 1; #(1, @output);
		}
	}
	
	return 0; #(0, "");
}
