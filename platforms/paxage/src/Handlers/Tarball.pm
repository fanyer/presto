# -*- tab-width: 4; fill-column: 80 -*-

package Handlers::Tarball;

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
	$self->{fh} = new Handlers::FileHandler($self->{conf}, "$self->{conf}->{temp}/$self->{stem}", 1);
	$self->{tar} = system('gtar --version >/dev/null 2>&1') == 0 ? 'gtar' : 'tar'; # Use gtar if it exists
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
	foreach (@{$self->{conf}->{compression}}) {
		my $option;
		if (/^gz$/) {
			$option = 'z';
		}
		elsif (/^bz2$/) {
			$option = 'j';
		}
		elsif (/^xz$/) {
			$option = 'J';
		}
		$self->{conf}->{scheduler}->scheduleCommand(
			"$self->{tar} c${option}f '$self->{conf}->{outdir}/$self->{stem}.tar.$_' -C '$self->{conf}->{temp}' --owner 0 --group 0 '$self->{stem}'",
			"$self->{stem}.tar.$_");
	}
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
	if ($type eq 'iterate') {
		my @res;
		foreach (sort {$a->{file} cmp $b->{file}} @{$self->{fh}->getManifest()})
		{
			push @res, $self->formatManifestLine($_);
		}
		return join "\n\t", @res;
	} else {
		return $self->{fh}->getFileSummary($type);
	}
}

sub formatManifestLine($$) {
	my ($self, $info) = @_;
	my ($method, $mode, $field3);
	if ($info->{mode} eq 'link') {
		$field3 = $info->{link};
		$method = 'L';
		$mode = 'L';
	} else {
		local *FILE;
		open FILE, $info->{loc} or return $self->{eh}->error("Cannot read $info->{loc}");
		binmode FILE;
		$field3 = Digest::MD5->new->addfile(*FILE)->hexdigest or return $self->{eh}->error("Cannot compute digest of $info->{loc}");
		$method = $info->{macro} ? 'P' : 'N';
		$mode = ($info->{mode} eq 'bin' || $info->{mode} eq 'script') ? 'X' : 'F';
	}
	return "\"\$@\" $method$mode $field3 $info->{file}";
}

sub fixSigPaths ($$$) { return 0; }

sub fixRegionIni($$$) { return 0; }

1;
