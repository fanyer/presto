# -*- tab-width: 4; fill-column: 80 -*-

package PackageConfig;

use strict;
use warnings;

use Shipping;
use ErrorHandler;
use Handlers::Devel;
use Handlers::Tarball;
use Handlers::Debian;
use Handlers::RPM;

sub new($$) {
	my ($class, $init) = @_;
	my $self = {};
	if ($init) {
		foreach (keys(%{$init})) {
			$self->{$_} = $init->{$_};
		}
	}
	return bless $self, $class;
}

sub prepare($) {
	my $self = shift;
	my @targets;
	push @targets, @{$self->{init_targets}};
	push @targets, 'unix', 'x11', $self->{os}, $self->{arch}, $self->{package_type}, $self->{product}, "$self->{package_type}:$self->{product}";
	if ($self->{product} eq 'opera-labs') {
		$self->{_suffix} = " Labs $self->{labs_name}";
		$self->{suffix} = lc $self->{_suffix};
		$self->{suffix} =~ s/ /-/g;
		$self->{usuffix} = uc $self->{suffix};
		$self->{product} = "opera$self->{suffix}";
		push @targets, $self->{product}, "$self->{package_type}:$self->{product}";
	} else {
		$self->{suffix} = $self->{product};
		$self->{suffix} =~ s/^opera//;
		$self->{_suffix} = $self->{suffix};
		$self->{_suffix} =~ s/-/ /g;
		$self->{_suffix} =~ s/(\w+)/\u$1/g;
		$self->{usuffix} = uc $self->{suffix};
		push @targets, 'opera-mainline', "$self->{package_type}:opera-mainline";
	}
	my $handler;
	my $arch = $self->{arch};
	my @compression;
	$self->{temp} = "$self->{temp_root}/$self->{product}.$self->{package_type}";
	if ($self->{os} eq 'FreeBSD' || $self->{package_type} eq 'deb') {
		$arch = 'amd64' if $arch eq 'x86_64';
	}
	if ($self->{os} eq 'Linux') {
		@compression = qw(xz bz2);
		$self->{prefix} = '/usr';
	} elsif ($self->{os} eq 'FreeBSD') {
		@compression = qw(xz);
		$self->{prefix} = '/usr/local';
	} else {
		die "Unsupported OS $self->{os}";
	}
	$self->{eh} = new ErrorHandler();
	push @targets, ($self->{package_type} eq 'devel' ? 'debug' : 'release');
	my $profile_header_path = 'modules/hardcore/features/profile_desktop.h';
	my $profile_header;
	foreach my $item (@{$self->{buildroot}}) {
		if (-f "$item/src/$profile_header_path") {
			$profile_header = "$item/src/$profile_header_path";
			last;
		}
	}
	$profile_header = "$self->{source}/$profile_header_path" unless $profile_header;
	my @feature_files = (
		"$self->{source}/adjunct/quick/custombuild-features.h",
		"$self->{source}/adjunct/quick/quick-features.h");
	foreach my $feature_file (@feature_files) {
		$self->{repack_bundle}->add($feature_file) if $self->{repack_bundle};
		$self->{dep_tracker}->add($feature_file) if $self->{dep_tracker};
	}
	push @feature_files, $profile_header;
	$self->{repack_bundle}->add($profile_header, $profile_header_path) if $self->{repack_bundle};
	$self->{dep_tracker}->add($profile_header) if $self->{dep_tracker};
	{
		local *DEVNULL;
		open DEVNULL, '>', '/dev/null';
		select DEVNULL;
		$self->{fs} = new feature_scanner(@feature_files) or die('Failed to initialize the feature scanner');
		select STDOUT;
	}
	push @targets, $self->{fs}->GetPackagingTargets();
	$self->{targets} = [@targets];
	if ($self->{package_type} eq 'devel') {
		$self->{ship_conf} = "$self->{source}/platforms/paxage/layout/tar.layout.xml";
	} else {
		$self->{ship_conf} = "$self->{source}/platforms/paxage/layout/$self->{package_type}.layout.xml";
	}
	$self->{shipping} = new Shipping($self->{ship_conf}, $self->{source}, "$self->{temp}/processed", $self->{fs}, $self->{languages}, $self->{regions}, @targets)
		or die("Failed to load $self->{ship_conf}");
	$self->{repack_bundle}->add($self->{ship_conf}) if $self->{repack_bundle};
	$self->{dep_tracker}->add($self->{ship_conf}) if $self->{dep_tracker};
	if ($self->{package_type} eq 'tar') {
		$self->{prefix} = '/usr/local';
		$self->{stem} = "$self->{product}-$self->{version}-$self->{build}.$arch." . lc($self->{os});
		$self->{compression} = [@compression];
		$self->{fh} = new Handlers::Tarball($self);
	} elsif ($self->{package_type} eq 'deb') {
		$self->{stem} = "$self->{product}_$self->{version}.$self->{build}_$arch";
		$self->{fh} = new Handlers::Debian($self);
	} elsif ($self->{package_type} eq 'rpm') {
		$self->{stem} = "$self->{product}-$self->{version}-$self->{build}.$arch";
		$self->{fh} = new Handlers::RPM($self);
	} elsif ($self->{package_type} eq 'devel') {
		$self->{prefix} = $self->{develdir};
		$self->{debugger} = 'exec gdb -ex run --args' unless $self->{debugger};
		$self->{strip} = 0;
		$self->{fh} = new Handlers::Devel($self);
	} else {
		die "Unsupported package type $self->{package_type}";
	}
}

sub run($$) {
	my ($self, $dry_run) = @_;
	$self->{eh}->reset();
	File::Path::mkpath($self->{temp});
	foreach my $manifest (@{$self->{manifest_files}}) {
		$self->{shipping}->parseManifest($manifest, $self->{fh}) or die("Failed to load $manifest");
		$self->{repack_bundle}->add($manifest) if $self->{repack_bundle};
		$self->{dep_tracker}->add($manifest) if $self->{dep_tracker};
	}
	$self->{fh}->done() if $self->{eh}->ok() && !$dry_run;
	return $self->{eh}->ok();
}

1;
