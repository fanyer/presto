# -*- tab-width: 4; fill-column: 80 -*-

package Handlers::FileHandler;

use strict;
use warnings;

use MacroProcessor;

sub new($$$) {
	my ($class, $conf, $dest, $delayed_macros) = @_;
	my $self = {};
	$self->{dest} = $dest;
	$self->{conf} = $conf;
	$self->{eh} = $conf->{eh};
	$self->{manifest} = [];
	$self->{special_locations} = {};
	$self->{path_processor} = new MacroProcessor($self->{conf});
	$self->{delayed_macros} = $delayed_macros;
	return bless $self, $class;
}

sub shipFile($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	my $loc = $self->prepareLocation($dest, $xml) or return;
	my ($compressed_loc, $compress_command, $compressed_dest);
	my $compress = $xml->{att}->{compress};
	if ($compress) {
		if ($compress eq 'gzip') {
			$compressed_dest = "$dest.gz";
			$compress_command = "gzip -f9 '$loc'";
		} elsif ($compress eq 'bzip2') {
			$compressed_dest = "$dest.bz2";
			$compress_command = "bzip2 -f '$loc'";
		} else {
			return $self->{eh}->error("Unsupported compression method $compress");
		}
		$compressed_loc = $self->prepareLocation($compressed_dest, $xml) or return;
	}
	my $mode = ($xml->{att}->{mode} or 'file');
	my $furtherProcessing = 0;
	my $perm;
	my $linkTarget = '';
	if ($mode eq 'file' || $mode eq 'so') {
		$perm = 0644;
	} elsif ($mode eq 'bin' || $mode eq 'script') {
		$perm = 0755;
	} elsif ($mode eq 'link') {
		$perm = 0; # don't chmod
	} else {
		return $self->{eh}->error("Unknown mode $mode for $dest");
	}
	my $buildroot = $self->{conf}->{source};
	my $relSource = $source;
	if ($source =~ m|^\Q$buildroot\E/(.*)$|) {
		# Prefer file from a buildroot directory, if one exists
		$relSource = $1;
		foreach my $b (@{$self->{conf}->{buildroot}}) {
			if (-e "$b/$relSource") {
				$source = "$b/$relSource";
				$buildroot = $b;
				last;
			}
		}
	}
	my $process_method = $xml->{att}->{process} || '';
	if ($process_method eq 'macro') {
		my $processor = new MacroProcessor($self->{conf});
		$processor->process($source, $loc, $self->{delayed_macros}) or return 0;
		$furtherProcessing = $processor->needFurtherProcessing();
		$self->{conf}->{dep_tracker}->add($source) if $self->{conf}->{dep_tracker};
	} elsif (($mode eq 'bin' || $mode eq 'so') && $self->{conf}->{strip}) {
		my $rel = File::Spec->abs2rel($source, $buildroot);
		my $stripped = $self->{conf}->{cache}->get("strip:$rel");
		if ($stripped) {
			link $stripped, $loc or File::Copy::copy $stripped, $loc or return $self->{eh}->error("Cannot install $source as $dest");
		} else {
			if (system("$self->{conf}->{strip_command} -o '$loc' '$source'") != 0)
			{
				return $self->{eh}->error("Cannot strip $source");
			}
			$self->{conf}->{cache}->put("strip:$rel", $loc);
			$self->{conf}->{repack_bundle}->addCache("strip:$rel") if $self->{conf}->{repack_bundle};
		}
		$self->{conf}->{dep_tracker}->add($source) if $self->{conf}->{dep_tracker};
	} elsif ($mode eq 'link') {
		my $target = File::Spec->catfile($self->{conf}->{shipping}->folderDir($xml->{att}->{linkdir}), ($xml->{att}->{linkfile} or ''));
		my (undef, $path, undef) = File::Spec->splitpath($dest);
		$path =~ s|^/||;
		$linkTarget = File::Spec->abs2rel($target, $path);
		symlink $self->{path_processor}->interpolate($linkTarget, $linkTarget, 0), $loc or return $self->{eh}->error("Cannot create symlink $dest -> $target");
	} else {
		return $self->{eh}->error("Missing file: $source (to be installed as $dest)") unless -f $source;
		link $source, $loc or File::Copy::copy $source, $loc or return $self->{eh}->error("Cannot install $source as $dest");
		if (($xml->{att}->{process} || '') ne '') {
			$relSource = $xml->parent('source')->{att}->{path} . '/' . ($xml->{att}->{from} || $xml->{att}->{name});
		} elsif ($source =~ m|^$self->{conf}->{temp_root}/|) {
			$relSource = undef;
		}
		if ($relSource) {
			$self->{conf}->{repack_bundle}->add("$buildroot/$relSource", $relSource) if $self->{conf}->{repack_bundle};
			$self->{conf}->{dep_tracker}->add("$buildroot/$relSource") if $self->{conf}->{dep_tracker};
		}
	}
	if ($compress)
	{
		if (system($compress_command) != 0)
		{
			return $self->{eh}->error("Cannot compress $compressed_dest");
		}
		$loc = $compressed_loc;
		$dest = $compressed_dest;
	}
	my @stat = lstat($loc) or return $self->{eh}->error("Cannot stat $loc");
	if ($perm) {
		chmod $perm, $loc or return $self->{eh}->error("Cannot set mode on $loc") if ($stat[2] & 0777) != $perm;
	}
	push @{$self->{manifest}}, {
		file => $self->{path_processor}->interpolate($dest, $dest, $self->{delayed_macros}),
		mode => $mode,
		size => $stat[7],
		loc => $loc,
		link => $self->{path_processor}->interpolate($linkTarget, $linkTarget, $self->{delayed_macros}),
		scandeps => ($mode eq 'bin' || $mode eq 'so') && (($xml->{att}->{scandeps} || 'false') eq 'true'),
		suffix => ($xml->{att}->{suffix} || 'false') eq 'true',
		macro => $furtherProcessing
	} unless $dest =~ m|^@@[-_A-Za-z0-9]+/|;
}

sub zipFile($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	$self->{last_ok} = 0;
	my $loc = $self->prepareLocation($dest, $xml) or return;
	if (system("cd '$source' && zip -qr9 '$loc' *") != 0) {
		return $self->{eh}->error("Cannot zip directory: $source (to be installed as $dest)");
	}
	my $origSource;
	if (($xml->{att}->{filter} || '') ne '') {
		$origSource = $self->{conf}->{source} . '/' . $xml->parent('source')->{att}->{path} . '/' . ($xml->{att}->{from} || $xml->{att}->{name});
	} elsif ($source !~ m|^$self->{conf}->{temp_root}/|) {
		$origSource = $source;
	}
	if ($origSource) {
		$self->{conf}->{repack_bundle}->addDir($origSource) if $self->{conf}->{repack_bundle};
		$self->{conf}->{dep_tracker}->addDir($origSource) if $self->{conf}->{dep_tracker};
	}
	my @stat = lstat($loc) or return $self->{eh}->error("Cannot stat $loc");
	push @{$self->{manifest}}, {
		file => $self->{path_processor}->interpolate($dest, $dest, $self->{delayed_macros}),
		mode => 'file',
		size => $stat[7],
		loc => $loc,
		link => '',
		scandeps => 0,
		suffix => ($xml->{att}->{suffix} || 'false') eq 'true',
		macro => 0
	} unless $dest =~ m|^@@[-_A-Za-z0-9]+/|;
}

sub concatFiles($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	my $loc = $self->prepareLocation($dest, $xml) or return;
	my @source_items = glob $source or return $self->{eh}->error("Cannot concatenate $source as $dest: source files not found");
	local *OUT;
	open OUT, '>', $loc or return $self->{eh}->error("Cannot concatenate $source as $dest: write error");
	foreach my $source_item (@source_items) {
		File::Copy::copy $source_item, *OUT or return $self->{eh}->error("Cannot concatenate $source as $dest: cannot copy $source_item");
		$self->{conf}->{repack_bundle}->add($source_item) if $self->{conf}->{repack_bundle};
	}
	close OUT;
	if ($self->{conf}->{dep_tracker}) {
		my $origin = $self->{conf}->{source} . '/' . $xml->parent('source')->{att}->{path};
		$self->{conf}->{dep_tracker}->addDir($origin, File::Spec->abs2rel($source, $origin));
	}
	my @stat = lstat($loc) or return $self->{eh}->error("Cannot stat $loc");
	push @{$self->{manifest}}, {
		file => $self->{path_processor}->interpolate($dest, $dest, $self->{delayed_macros}),
		mode => 'file',
		size => $stat[7],
		loc => $loc,
		link => '',
		scandeps => 0,
		suffix => ($xml->{att}->{suffix} || 'false') eq 'true',
		macro => 0
	} unless $dest =~ m|^@@[-_A-Za-z0-9]+/|;
}

sub prepareLocation($$$) {
	my ($self, $dest, $xml) = @_;
	my $prefix = $self->{dest};
	if ($dest =~ m|^@@([-_A-Za-z0-9]+)/(.*)$|) {
		$prefix = $self->{special_locations}->{$1} or return $self->{eh}->error("Unknown special location $1");
		$dest = $2;
	}
	my $loc = File::Spec->catfile($prefix, $self->{path_processor}->interpolate($dest, $dest, 0));
	my (undef, $path, undef) = File::Spec->splitpath($loc);
	File::Path::mkpath($path);
	if (($xml->{att}->{override} || 'false') eq 'true') {
		return $self->{eh}->error("Overriding a non-existent file $dest") unless -e $loc;
		@{$self->{manifest}} = grep $_->{loc} ne $loc, @{$self->{manifest}};
	} elsif (-e $loc) {
		return $self->{eh}->error("Trying to overwrite $dest") if grep $_->{loc} eq $loc, @{$self->{manifest}};
	}
	unlink $loc;
	return $loc;
}

sub setSpecialLocation($$$) {
	my ($self, $name, $dest) = @_;
	$self->{special_locations}->{$name} = $dest;
}

sub done($) {
	my $self = shift;
	return 1;
}

sub getSystemLanguage($$) {
	my ($self, $lang) = @_;
	return $lang;
}

sub getManifest($) {
	my $self = shift;
	return $self->{manifest};
}

sub getFileSummary($$) {
	my ($self, $type) = @_;
	return $self->{eh}->error("Unknown FILES parameter $type");
}

1;
