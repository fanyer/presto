#!perl  # -*- tab-width: 4; fill-column: 80 -*-

############################################################ 
#
# Module used to unpack and modify existing packages
#
############################################################ 

use strict;

package repacker;

use File::Spec;
use File::Basename;

use LWP::UserAgent;

sub new
{
	my $class = shift;
	my $self = {};
	
	$self->{_working_dir} = shift;
	$self->{_unpack_cache} = shift;
	
	$self->{_files_list} = undef;
	$self->{_package_dir} = undef;
	$self->{_package_ready} = 0;
	$self->{_opera_next} = undef;
	
	$self->{_packaging_dir} = dirname(File::Spec->rel2abs(__FILE__));
	
	if (!(-d $self->{_unpack_cache}) || !(-d $self->{_working_dir}))
	{
		return undef;
	}

	bless $self, $class;
	return $self;	
}

sub GetPackageOnline
{
	my $build_nr = shift;
	my $opera_next = shift;
	my $ver_major = shift;
	my $ver_minor = shift;
	my $architecture = shift;
	my $destination = shift;
	
	if ($build_nr !~ /[\d]{4,5}/)
	{
		print "Invalid build number: $build_nr\n";
		return 0;
	}
	
	my $ver = $ver_major.".".$ver_minor;

	my $source_exe = "http://bcd-portal.oslo.opera.com/builds/desktop/windows/".$ver."/".$build_nr."/Opera-".($opera_next?"Next-":"").$ver."-".$build_nr.".".$architecture.".autoupdate.exe";
	my $destination_exe = File::Spec->catfile($destination, "Opera_package.exe");
	
	if (-e $destination_exe)
	{
		return $destination_exe;
	}

	print "Downloading $source_exe to $destination_exe";
	
	my $lwp_ua = LWP::UserAgent->new();
	
	my $req = new HTTP::Request(GET=>$source_exe);
	$req->authorization_basic("bcd-bot", "o3xp,bq?");
	my $resp = $lwp_ua->request($req, $destination_exe);
	
	if (!(-e $destination_exe))
	{
		return 0;
	}

	return $destination_exe;	
}

sub Unpack
{
	my $package = shift;
	my $destination = shift;
	
	if (!(-e $package))
	{
		return 0;
	}
	
	print "Extracting $package to $destination:\n";
	system("7z x \"$package\" -o\"$destination\"");
	
	return 1;
}

sub PrepareRepack
{
	my $self = shift;
	
	my $ver_major = shift;
	my $ver_minor = shift;
	my $build_nr = shift;
	my $architecture = shift;
	my $opera_next = shift;
	
	$self->{_opera_next} = $opera_next;
	
	my $ver = $ver_major.$ver_minor;
	my $package_folder = File::Spec->catdir($self->{_unpack_cache}, "Opera-".($opera_next?"Next-":"").$ver."-".$build_nr.".".$architecture);
	my $unpacked_folder = File::Spec->catdir($package_folder, "package");
	
	print "Preparing repack of opera".($opera_next?" Next":"")." $ver_major.$ver_minor $build_nr $architecture\n";
	
	if (!(-d $unpacked_folder))
	{
		print "Unpacked folder not found in cache. Downloading.\n";
	
		if (!(-d $package_folder))
		{
			mkdir($package_folder);
		}
		
		my $package = GetPackageOnline($build_nr, $opera_next, $ver_major, $ver_minor, $architecture, $package_folder);
		
		if (!$package)
		{
			return 0;
		}
			
		if (Unpack($package, $unpacked_folder) == 0)
		{
			return 0;
		}
	}
		
	#Cleans the folder where the package will be put together
	File::Path::rmtree($self->{_working_dir}, {keep_root=>1});
	
	$self->{_package_dir} = File::Spec->catdir($self->{_working_dir}, "package");
	mkdir $self->{_package_dir};
	
	print "Copying $unpacked_folder to $self->{_package_dir}: \n";
	
	my $copy_command = "xcopy \"$unpacked_folder\\*.*\" \"".$self->{_package_dir}."\" /e /v /q /y";
	system($copy_command);
	
	my $files_list_file = File::Spec->catfile($self->{_package_dir}, 'files_list');
	
	if (!(-e $files_list_file))
	{
		print "Files list not found: $files_list_file\n";
		return 0;
	}
	
	open FILES_LIST, $files_list_file;
	
	$self->{_files_list} = [<FILES_LIST>];
	chomp @{$self->{_files_list}};
	close FILES_LIST;

	$self->{_package_ready} = 1;
}

sub FilterLanguages()
{
	my $self = shift;
	my $languages_array = shift;
	my %languages;
	@languages{@{$languages_array}} = ();
	
	if (!$self->{_package_ready})
	{
		return 0;
	}
	
	print "Filtering languages list\n";
	
	my $locale_folder = File::Spec->catdir($self->{_package_dir}, 'locale');
	opendir LOCALES, $locale_folder;
	while (my $langdir = readdir(LOCALES))
	{
		if (!($langdir eq '.') && !($langdir eq '..') && !($langdir eq 'en') && !(exists $languages{$langdir}))
		{
			my $full_lang_dir = File::Spec->catdir($locale_folder, $langdir);
			File::Path::rmtree($full_lang_dir);
		}
		
		if (($langdir eq 'en') && !(exists $languages{$langdir}))
		{
			opendir EN_LOCALE, File::Spec->catdir($locale_folder, $langdir);
			while (my $enfile = readdir(EN_LOCALE))
			{
				if (!($enfile eq '.') && !($enfile eq '..') && !($enfile eq 'license.txt'))
				{
					unlink File::Spec->catfile($locale_folder, $langdir, $enfile);
				}
			}
			closedir EN_LOCALE;
		
		}
	}
	closedir LOCALES;
	
	my $i;
	for ($i = 0; $i < scalar @{$self->{_files_list}}; $i++)
	{
		my $item = $self->{_files_list}->[$i];
		if ($item =~ /^locale\\([^\\]+)\\/ && !(exists $languages{$1}) && !($item eq 'locale\\en\\license.txt'))
		{
			splice(@{$self->{_files_list}}, $i, 1);
			$i--;
		}
	}
	
	return 1;
}

sub CopyNewContent()
{
	my $self = shift;
	my $content_folder = shift;

	if (!$self->{_package_ready})
	{
		return 0;
	}
	
	my $copy_cmd = "xcopy \"".$content_folder."\" \"".$self->{_package_dir}."\" /E /V /Q /Y";
	print "Copying files over: $copy_cmd\n";
	system($copy_cmd);
	
	return 1;
}

sub AddNewFilesList()
{
	my $self = shift;
	my $new_files_list = shift;
	
	if (!$self->{_package_ready})
	{
		return 0;
	}
	
	print "Adding new files to the files list\n";
	
	push(@{$self->{_files_list}}, @{$new_files_list});
	
	return 1;
}

sub Repack()
{
	my $self = shift;
	my $output_file = shift;
	my $output_autoupdate_file = shift;
	my $sign = shift;

	if (!$self->{_package_ready})
	{
		return 0;
	}
	
	my $files_list_file		= File::Spec->catfile($self->{_package_dir}, 'files_list');
	
	my $sfx_dir					= File::Spec->catdir($self->{_packaging_dir}, 'tools', '7zSfx');
	my $sfx_module;
	if (!$self->{_opera_next})
	{
		$sfx_module			= File::Spec->catfile($sfx_dir, '7zsd.sfx');
	}
	else
	{
		$sfx_module			= File::Spec->catfile($sfx_dir, '7zsd_Next.sfx');
	}
	my $sfx_config				= File::Spec->catfile($sfx_dir, 'config.txt');	
	my $sfx_autoupdate_config	= File::Spec->catfile($sfx_dir, 'config_autoupdate.txt');	
	
	my $installer_archive		= File::Spec->catfile($self->{_working_dir}, 'Opera installer.7z');
	
	my $signcode			=  File::Spec->catdir($self->{_packaging_dir}, 'tools', 'signcode');
	my $signcode_command = "\"".$signcode. "\" -spc \"C:\\opera2010.spc\" -v \"C:\\opera2010.pvk\" -n \"Opera web browser\" -i \"http://www.opera.com\" -a MD5 -t \"http://timestamp.verisign.com/scripts/timstamp.dll\" ";
	
	
	open FILES_LIST, ">", $files_list_file;
	print FILES_LIST join("\n", @{$self->{_files_list}})."\n";
	close FILES_LIST;
	
	my $cwd = Cwd::getcwd();
	chdir ($self->{_package_dir});
	unlink($installer_archive);
	
	print "Zipping the Opera Installer:\n";
	system("7z a -t7z -mx9 -y \"$installer_archive\" . -xr!profile -xr!thumbs.db -xr!debug_log.txt -xr!debug.txt -xr!7z.log");
	
	if (defined $output_file)
	{
		print "Copying the Opera Installer to \"$output_file\"\n";
		system("copy /b \"$sfx_module\" + \"$sfx_config\" + \"$installer_archive\" \"$output_file\"");
		if ($sign)
		{
			system("$signcode_command \"$output_file\"");
		}
	}
	if (defined $output_autoupdate_file)
	{
		print "Copying the Opera Installer to \"$output_autoupdate_file\"\n";
		system("copy /b \"$sfx_module\" + \"$sfx_autoupdate_config\" + \"$installer_archive\" \"$output_autoupdate_file\"");
		if ($sign)
		{
			system("$signcode_command \"$output_autoupdate_file\"");
		}
	}
	
	chdir ($cwd);
	
	return 1;
	
}

return 1;