# -*- tab-width: 4; fill-column: 80 -*-

package Handlers::Devel;

use strict;
use warnings;

use Handlers::FileHandler;

sub new($$) {
	my ($class, $conf) = @_;
	my $self = {};
	$self->{conf} = $conf;
	$self->{eh} = $conf->{eh};
	$self->{fh} = new Handlers::FileHandler($self->{conf}, $self->{conf}->{prefix}, 0);
	return bless $self, $class;
}

sub shipFile($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	my $file = File::Spec->catfile($self->{conf}->{prefix}, $dest);
	unlink $file, "$file.gz", "$file.bz2";
	$self->{fh}->shipFile($source, $dest, $xml);
}

sub zipFile($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	unlink File::Spec->catfile($self->{conf}->{prefix}, $dest);
	$self->{fh}->zipFile($source, $dest, $xml);
}

sub concatFiles($$$$) {
	my ($self, $source, $dest, $xml) = @_;
	unlink File::Spec->catfile($self->{conf}->{prefix}, $dest);
	$self->{fh}->concatFiles($source, $dest, $xml);
}

sub done($) {
	my $self = shift;
	return $self->{fh}->done();
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
	return $self->{fh}->getFileSummary($type);
}

sub fixSigPaths ($$$) { return 0; }

sub fixRegionIni($$$) { return 0; }

1;
