# -*- tab-width: 4; fill-column: 80 -*-

package FileCache;

use File::Path;
use Digest::MD5 qw(md5_hex);

sub new($$$) {
	my ($class, $root, $preseed) = @_;
	my $self = {};
	$self->{root} = $root;
	$self->{preseed} = $preseed;
	File::Path::mkpath($root);
	return bless $self, $class;
}

sub get($$) {
	my ($self, $key) = @_;
	my $hash = md5_hex($key);
	my $entry = "$self->{root}/$hash";
	return $entry if -f $entry;
	if ($self->{preseed}) {
		$entry = "$self->{preseed}/$hash";
		return $entry if -f $entry;
	}
	return undef;
}

sub put($$$) {
	my ($self, $key, $file) = @_;
	my $hash = md5_hex($key);
	my $entry = "$self->{root}/$hash";
	unlink $entry;
	link $file, $entry or File::Copy::copy $file, $entry;
}

sub putString($$$) {
	my ($self, $key, $string) = @_;
	my $hash = md5_hex($key);
	my $entry = "$self->{root}/$hash";
	unlink $entry;
	local *FILE;
	open FILE, '>', $entry;
	print FILE $string;
}

sub getString($$) {
	my ($self, $key) = @_;
	my $string;
	my $entry = $self->get($key);
	if (defined $entry) {
		local *FILE;
		open FILE, '<', $entry;
		while (<FILE>) {
			$string .= $_;
		}
		return $string;
	} else {
		return undef;
	}
}


sub preseed($$$) {
	my ($self, $key, $dest) = @_;
	my $hash = md5_hex($key);
	return "$self->{root}/$hash", "$dest/$hash";
}

1;
