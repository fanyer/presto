package Shipping; # -*- tab-width: 4; fill-column: 80 -*-
# Combine an XML manifest with platform opfolder description to set up package contents.
use strict;

require XML::Twig;
require package::feature_scanner;
require package::skin_builder;

use File::Path;
use File::Basename;
use File::Spec;
use File::Path;
use Cwd qw(getcwd);


sub new {
    # new Shipping("config/linuxdirs.xml", "/source/root/, "/temp", feature_scanner_object, \@langs, 'unix', 'deb', 'ubuntu'); # args after first are targets
    my $class = shift;
    my $config = shift;
    my $source_dir = shift;
    my $temp_dir = shift;
    my $fs = shift;
    my $langs = shift;
    my $regions = shift;
    
    my $self = { 'targets' =>  join(' ', @_), 'path' => {} };
    # Valid opfolder id names are
    # map(sub { s/OPFILE_(.*)_FOLDER/$1/ }, OpFileFolder enum names)
	
    my $tree = XML::Twig->new()->safe_parsefile($config);
    # Example: ubs/platforms/unix/package/data/linuxdirs.xml

	# Hold the source ¾temp path and feature scanner internally
	$self->{_sourcedir} = $source_dir;
	$self->{_tempdir} = $temp_dir;
	$self->{_feature_scanner_object} = $fs;
	$self->{_languages} = $langs;
	$self->{_regions} = $regions;

	if ($tree) {
		bless $self, $class;
		$self->_scan_config($tree->root(), '');
		return $self;
	}
	# return undefined
}

sub _scan_config ($$$) {
    # Allow opfolders within one another in the platform config, to indicate directory hierarchy
    ( my $self, my $tree, my $root ) = @_;
    for my $fold ($tree->children('opfolder')) {
		my $path = $fold->att('path');
		$path =~ s:/+$::; # Strip any dangling slashes
		$path =~ s:^/+:: if $root; # and leading ones if we're not first path component
		my $dir = $path && $root ? "$root/$path" : "$root$path";
		$self->{path}->{$fold->att('id')} = $dir;
		$self->_scan_config($fold, $dir);
    }
}

sub folderDir ($$) {
    my $self = shift;
    my $folder = shift;
    $folder =~ s/OPFILE_(.*)_FOLDER/$1/; # in case someone passes a real enum name
    return $self->{path}->{$folder};
}

sub parseManifest ($$$$) {
    ( my $self, my $file, my $handler, my $verbose ) = @_;
    $verbose = 0 unless defined $verbose;
	
    my $tree = XML::Twig->new()->safe_parsefile($file);
    # Example: adjunct/quick/setup/package.xml
	return unless $tree;
	$tree = $tree->root();
    my @fold = $tree->children('opfolder');
    my $fs = $self->{_feature_scanner_object};
    my $tempdir = $self->{_tempdir};
    my $sourcedir = $self->{_sourcedir};

	# KIll and recreate the temp path
	File::Path::rmtree($tempdir);
	File::Path::mkpath($tempdir);

    my %path = %{$self->{path}};
    for my $fold ( @fold ) {
	
		my $dest = $path{$fold->att('id')};
		next unless defined $dest;
		my @src = $fold->children('source');
		# print "Targets: " . $self->{targets} if $verbose;
		# print "Folder ID is " . $fold->att('id') . "\n" if $verbose;
		# For each target in opfolder element do in depth search
		# of other elements and check @defines against $self->{targets}.
		for ($fold->children('target')) {
			# The @stack is for explicit recurssion.
			my ($target, $idx, @defines, @stack) = ($_, 0, (), ());
			while ($target) {
			
				# The $idx holds state for children nodes.
				if (($target->children('target'))[$idx]) {
					push @defines, $target->att('type');
					push @stack, ($target, ++$idx);
					# Next target, son to $target.
					$target = ($target->children('target'))[$idx - 1];
					# Traverse deeper.
					$idx = 0;
					next;
				}

				# If node without children check @defines against
				# $self->{targets} and add source elements to @src.
				if (!$target->children('target')) {
					push @defines, $target->att('type');
					my $add = 1;
					for my $def (@defines) {
						# We have to match all targets to add source.
						$add = 0 if (!($self->{targets} =~ /( |^)$def( |$)/));
					}
					push @src, $target->children('source') if ($add);
					# print "Add = " . $add . "\n" if $verbose;
					pop @defines;
					pop @defines;
				}

				# Go back in recursive call.
				$idx = pop @stack;
				$target = pop @stack;
			}
		}
		for my $src ( @src ) {
			my $path = $src->att('path');
			my @each = $src->children('file');
			push @each, $src->children('zip');
			push @each, $src->children('concat');
			push @each, $src->children('lang');
			for ($src->children('target')) {
				my $targ = $_->att('type');
				if ($self->{targets} =~ /( |^)$targ( |$)/)
				{
					push @each, $_->children('file');
					push @each, $_->children('zip');
					push @each, $_->children('concat');
					push @each, $_->children('lang');
				}
			}
			for my $each (@each) {
				my $name = $each->att('name');
				my $from =  $each->att('from') || $name;
				my $override =  $each->att('override');
				my $plat;
				for ( split(' ', $self->{targets}) ) {
					$plat = $each->att($_);
					last if defined $plat;
				}
				$plat = $name unless defined $plat;
				
				$from = "$path/$from" if $path;
				if ($handler->can('replacePathVariables'))
				{
					$from = $handler->replacePathVariables($from);
				}
				$override = "$path/$override" if defined($override) && $override ne "" && $path;
				my $to = "$dest/$plat";
				
				my $optional = ($each->att('optional') || 'false') eq 'true';
				
				if ($each->gi eq "zip")
				{ 
					my $filter = $each->att('filter');

					# Adjust the from to the full path
					$from = File::Spec->catdir($sourcedir, $from);

					if (defined($filter) && $filter eq "skin")
					{
						my ($filename, $directories, $suffix) = fileparse($to, '.zip');
						my $to_temp = File::Spec->catdir($tempdir, $filename);

						# For now, only support hires skin files on Mac
						my $hires = 0;
						if ($self->{targets} =~ /\bmac\b/)
						{
							$hires = 1;
						}

						# Remove and recreate the path
						File::Path::mkpath($to_temp);

#						print "Zipping: ".$from." : ".$to_temp."\n" if $verbose;

						# Apply the special skin processing first
						skin_builder::BuildSkinLayout($fs, $from, $to_temp, $hires, $verbose);

						$from = $to_temp;
					}
					
					if ($from =~ /\[LANG\]/ || $to =~ /\[LANG\]/)
					{
						foreach my $lang (@{$self->{_languages}})
						{
							my $sys_lang = $handler->getSystemLanguage($lang);
							
							my $from_lang = $from;
							$from_lang =~ s/\[LANG\]/$lang/g;
							
							my $to_lang = $to;
							$to_lang =~ s/\[LANG\]/$sys_lang/g;
							
							if (!$optional || -e ($from_lang))
							{
								$handler->zipFile($from_lang, $to_lang, $each);
							}
						}
					}
					elsif (!$optional || -e ($from))
					{
						# Zip the folder to the destination location
						$handler->zipFile($from, $to, $each);
					}
				}
				elsif ($each->gi eq "concat")
				{
					# Adjust the from to the full path
					$from = File::Spec->catfile($sourcedir, $from);
					
					if (!$optional || -e ($from))
					{
						$handler->concatFiles($from, $to, $each);
					}
				}
				else
				{
					my $process = $each->att('process');

					# Adjust the from to the full path
					$from = File::Spec->catfile($sourcedir, $from);
					if (defined($override) && $override ne "")
					{
						$override = File::Spec->catfile($sourcedir, $override);
					}

					if (defined($process) && $process eq "sigpaths")
					{
						my $to_temp = File::Spec->catfile($tempdir, File::Basename::basename($to));

						# copy from temporary file only if the platform changed the contents						
						if ($handler->fixSigPaths($from, $to_temp)) {
							$from = $to_temp;
						}
					}

					if (defined($process) && $process eq "regionini")
					{
						my $to_temp = File::Spec->catfile($tempdir, File::Basename::basename($to));

						# copy from temporary file only if the platform changed the contents
						if ($handler->fixRegionIni($from, $to_temp)) {
							$from = $to_temp;
						}
					}

					if (defined($process) && $process eq "ini")
					{
						my $to_temp = File::Spec->catfile($tempdir, File::Basename::basename($to));

						# Preprocess the file first
						$fs->CopyFile($from, $to_temp);
						
						$from = $to_temp;
					}
					
					if (defined($process) && $process eq "lang")
					{
						my $cwd = getcwd();
						chdir File::Spec->catdir($sourcedir, "data", "strings", "scripts");
						
#						print("Languages to build: ".join(",", @{$self->{_languages}})."\n") if $verbose;

						foreach my $lang (@{$self->{_languages}})
						{
							my $sys_lang = $handler->getSystemLanguage($lang);

							my $from_lang = $from;
							$from_lang =~ s/\[LANG\]/$lang/g;

							my $override_lang = undef;
							if (defined($override) && $override ne "")
							{
								$override_lang = $override;
								$override_lang =~ s/\[LANG\]/$lang/g;
							}
							
							my $to_lang = $to;
							$to_lang =~ s/\[LANG\]/$sys_lang/g;
							
							#Skip building the language file if an up to date copy is already present (speeds up builds for devs)
							my @mtimes = ((stat($from_lang))[9], (stat("../build.strings"))[9], (stat("../english.db"))[9], (stat("../../../adjunct/quick/english_local.db"))[9]);
							next if ($handler->canSkipRebuild($to_lang,  (reverse sort { $a <=> $b } @mtimes)[0]));
							
							my $to_temp = File::Spec->catdir($tempdir, $lang.".lng");
							
							my $makelang_command = "perl makelang.pl ".$handler->getMakelangArgs($lang)." -L -c 9 -u -b \"../build.strings\" -o \"$to_temp\" -t \"$from_lang\"";
							if (defined($override_lang))
							{
								$makelang_command .= " -T \"$override_lang\"";
							}
							$makelang_command .= " \"../english.db\" \"../../../adjunct/quick/english_local.db\"";
							print $makelang_command."\n";
							system($makelang_command);
							
							if (!$optional || -e ($to_temp))
							{
								$handler->shipFile($to_temp, $to_lang, $each);
							}
						}
						chdir $cwd;
						
						next;
					}
					
					if ($from =~ /\[REGION\]/ || $to =~ /\[REGION\]/)
					{
						foreach my $region (@{$self->{_regions}})
						{
							my $from_region = $from;
							$from_region =~ s/\[REGION\]/$region/g;

							my $to_region = $to;
							$to_region =~ s/\[REGION\]/$region/g;

							if ($from =~ /\[LANG\]/ || $to =~ /\[LANG\]/)
							{
								foreach my $lang (@{$self->{_languages}})
								{
									my $sys_lang = $handler->getSystemLanguage($lang);

									my $from_region_lang = $from_region;
									$from_region_lang =~ s/\[LANG\]/$lang/g;

									my $to_region_lang = $to_region;
									$to_region_lang =~ s/\[LANG\]/$sys_lang/g;

									if (!$optional || -e ($from_region_lang))
									{
										$handler->shipFile($from_region_lang, $to_region_lang, $each);
									}
								}
							}
							elsif (!$optional || -e ($from_region))
							{
								$handler->shipFile($from_region, $to_region, $each);
							}
						}
					}
					elsif ($from =~ /\[LANG\]/ || $to =~ /\[LANG\]/)
					{
						foreach my $lang (@{$self->{_languages}})
						{
							my $sys_lang = $handler->getSystemLanguage($lang);
							
							my $from_lang = $from;
							$from_lang =~ s/\[LANG\]/$lang/g;
							
							my $to_lang = $to;
							$to_lang =~ s/\[LANG\]/$sys_lang/g;
							
							if (!$optional || -e ($from_lang))
							{
								$handler->shipFile($from_lang, $to_lang, $each);
							}
						}
					}
					elsif (!$optional || -e ($from))
					{
						# The default is to ship the file
						$handler->shipFile($from, $to, $each);
					}
				}
			}
		}
	}

	return 1;
}

1;
# pod2html --infile=Shipping.pm --title="Shipping Opera" --outfile=wherever.html
# extracts the following documentation:
__END__

=head1 Mapping Opera sources to delivered locations.

Constructor takes path to platform-specific XML configuring where each
C<OpFileFolder> belongs in the file system and name of platform (used in
aliasing attributes of file elements), followed (optionally) by a list of target
names (any target name with spaces in it shall be treated as the result of
splitting it on spaces).

Exported method C<parseManifest> takes the name of an XML manifest file and a
handler object, which must support a method C<< ->shipFile(src, dest, node) >>;
this and the XML manifest's form are discussed further below.  Return from
C<parseManifest> is true on success.

C<OpFileFolder> enum members are here identified by the part of their
name between C<OPFILE_> prefix and C<_FOLDER> suffix; those believed
relevant to this module are:

=over

=item C<RESOURCES>

Read-only config (Opera's home directory on Windows)

=item C<INI>

Read-only ini (C<dialog.ini>, C<standard_*.ini>)

=item C<STYLE>

CSS for internally-generated pages

=item C<USERSTYLE>

Sample user style-sheets

=item C<GLOBALPREFS>

Global default ini file's home

=item C<FIXEDPREFS>

Fixed default ini file's home

=item C<LANGUAGE>

Default language files

=item C<IMAGE>

Internal images

=item C<DEFAULT_SKIN>

Read-only default skins

=item C<JSPLUGIN>

(no documentation available)

=back

These names are used as values for the C<id> attribute of C<opfolder> elements
in both XML files.  In the platform configuration passed to the constructor,
C<opfolder> elements have a C<path> attribute, specifying where on the
destination file-system the relevant C<OpFileFolder> is expected to go.  Use
relative paths if you want your packages to be relocatable.  One C<opfolder> may
be nested inside another, in which case the latter's path is deemed to be
relative to the former's; an empty path in this case means the two are the same
directory.

The XML manifest format has C<opfolder> elements containing C<source> elements
containing C<file> elements; at each level, elements may be wrapped in a
C<target> element to leave them out unless the target element's C<type>
attribute was one of the target names passed to our constructor.  Each XML
manifest file is associated with an implicit root directory (canonically that of
the Opera source tree) from under which to copy files: the path attribute of
each source element is relative to this root.

Each C<file> element specifies a file to be copied from the source directory.
It may have further attributes: the file element's C<XML::Twig> element is
passed to the handler object's C<< ->shipFile() >> as third parameter: second
parameter is the destination directory for the C<opfolder> and first is the full
name of the file to be copied, relative to the XML manifest's root directory.
The handler object's C<< ->shipFile() >> should check the element for an
attribute matching the target name and prefer to install with that name, if
present.  Otherwise, the C<name> attribute should be used as the installed
file's name.

To cope with the need (thanks to C<java>) for renaming, a file element may have
a C<from> attribute indicating the source file's actual name: this should be
omitted in preference to the C<name> attribute when the two coincide (which they
usually should).  Note that (C<from> or) C<name> may specify a path that
includes a sub-directory component; this is mainly useful when the structure
under the source directory accurately reflects that under the C<opfolder>
(e.g. the C<LANGUAGE> folder's C<en/> subdirectory matches that under
C<data/translations/>).

The mapping from C<OpFileFolder> names (either with both the C<OPFILE_> prefix
and the C<_FOLDER> suffix, or with neither) to actual directory paths is
exported by the C<folderDir> method.  Clients are encouraged to use this in
preference to hard-coding these mappings in packaging scripts.

There is, of course, nothing to prevent a platform from including special
entries in the XML file it uses to configure C<OpFileFolder>: these need not
match actual C<OpFileFolder> entries (though they should, e.g. by including a
platform prefix, ensure that there is no danger of them matching something which
may one day be added to C<OpFileFolder>).  The C<Shipping> object thus created
shall then be able to digest XML manifests which use the same names to identify
C<opfolder> elements.

=cut
