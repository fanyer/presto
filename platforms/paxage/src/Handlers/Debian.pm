# -*- tab-width: 4; fill-column: 80 -*-

package Handlers::Debian;

use strict;
use warnings;
use Digest::MD5;

use Handlers::FileHandler;

sub new($$) {
	my ($class, $conf) = @_;
	my $self = {};
	$self->{conf} = $conf;
	$self->{eh} = $conf->{eh};
	$self->{stem} = $self->{conf}->{stem};
	$self->{fh} = new Handlers::FileHandler($self->{conf}, "$self->{conf}->{temp}/$self->{stem}$self->{conf}->{prefix}", 0);
	$self->{fh}->setSpecialLocation('DEBIAN', "$self->{conf}->{temp}/$self->{stem}/DEBIAN");
	return bless $self, $class;
}

sub shipFile($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	$self->{fh}->shipFile($source, $dest, $xml);
}

sub zipFile($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	$self->{fh}->zipFile($source, $dest, $xml);
}

sub concatFiles($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	$self->{fh}->concatFiles($source, $dest, $xml);
}

sub done($) {
	my $self = shift;
	$self->{fh}->done() or return 0;
	$self->{conf}->{scheduler}->scheduleCommand(
		"fakeroot dpkg-deb -Zlzma -z9 -b $self->{conf}->{temp}/$self->{stem} $self->{conf}->{outdir}/$self->{stem}.deb >/dev/null",
		"$self->{stem}.deb");
	return 1;
}

sub getSystemLanguage($$) {
	my ($self, $lang) = @_;
	return $self->{fh}->getSystemLanguage($lang);
}

sub getManifest($) {
	my $self = shift;
	return $self->{fh}->getManifest();
}

sub getFileSummary($$) {
	my ($self, $type) = @_;
	if ($type eq 'md5sums') {
		my @res;
		foreach (sort {$a->{file} cmp $b->{file}} @{$self->{fh}->getManifest()})
		{
			push @res, $self->formatManifestLine($_) unless $_->{file} =~ m|^DEBIAN/| || -l $_->{loc};
		}
		return join "\n", @res;
	} elsif ($type eq 'shlibdeps') {
		my $res = $self->{conf}->{cache}->getString('deb:shlibdeps');
		return $res if defined $res;
		my @binaries;
		foreach (@{$self->{fh}->getManifest()})
		{
			push @binaries, $_->{loc} if $_->{scandeps};
		}
		my $binlist = "'" . join("' '", @binaries) . "'";
		$res = `cd '$self->{conf}->{temp}' && mkdir -p debian && touch debian/control && dpkg-shlibdeps -O --warnings=0 $binlist && rm -rf debian`;
		return $self->{eh}->error('Failed to run dpkg-shlibdeps') if $? != 0;
		chomp $res;
		if ($res =~ /^shlibs:Depends=(.*)$/) {
			my @deps;
			foreach my $dep (split /, /, $1) {
				if ($dep =~ /^(.+) \(>= (.*)\)/) {
					# Drop version info not based on symbols (too strict).
					$dep = $1 unless -e "/var/lib/dpkg/info/$1.symbols";
				}
				# DSK-379258 Ugly hack to jump over extra libc6 dep on 32-bit
				if ($1 eq "libc6" and $2 =~ m/^2\.3\..*/) {
					next;
				}
				push @deps, $dep;
			}
			$res = join(', ', @deps);
			$self->{conf}->{cache}->putString('deb:shlibdeps', $res);
			$self->{conf}->{repack_bundle}->addCache('deb:shlibdeps') if $self->{conf}->{repack_bundle};
			return $res;
		} else {
			return $self->{eh}->error("Failed to run dpkg-shlibdeps");
		}
	} elsif ($type eq 'installedsize') {
		my $res = 0;
		foreach (@{$self->{fh}->getManifest()})
		{
			$res += $_->{size} unless $_->{file} =~ m|^DEBIAN/|;
		}
		return int(($res + 1023) / 1024);
	} else {
		return $self->{fh}->getFileSummary($type);
	}
}

sub formatManifestLine($$) {
	my ($self, $info) = @_;
	local *FILE;
	open FILE, $info->{loc} or return $self->{eh}->error("Cannot read $info->{loc}");
	binmode FILE;
	my $md5 = Digest::MD5->new->addfile(*FILE)->hexdigest or return $self->{eh}->error("Cannot compute digest of $info->{loc}");
	my $prefix = $self->{conf}->{prefix};
	$prefix =~ s|^/||;
	return "$md5  $prefix/$info->{file}";
}

sub fixSigPaths ($$$) { return 0; }

sub fixRegionIni($$$) { return 0; }

1;
