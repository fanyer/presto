# -*- tab-width: 4; fill-column: 80 -*-

package RepackBundle;

use strict;
use warnings;

use ErrorHandler;

sub new($$) {
	my ($class, $conf) = @_;
	my $self = {};
	$self->{conf} = $conf;
	$self->{eh} = new ErrorHandler();
	$self->{temp} = "$self->{conf}->{temp_root}/RB";
	$self->{tar} = system('gtar --version >/dev/null 2>&1') == 0 ? 'gtar' : 'tar'; # Use gtar if it exists
	return bless $self, $class;
}

sub add($$$) {
	my ($self, $file, $as) = @_;
	unless ($as) {
		$as = File::Spec->abs2rel($file, $self->{conf}->{source});
	}
	return $self->{eh}->error("$as is outside of the source tree, cannot include in the repack bundle") if $as =~ m|^../|;
	my $dest = "$self->{temp}/$as";
	return if -f $dest;
	my (undef, $path, undef) = File::Spec->splitpath($dest);
	File::Path::mkpath($path);
	link $file, $dest or File::Copy::copy $file, $dest or return $self->{eh}->error("Cannot include $file in the repack bundle");
}

sub addDir($$) {
	my ($self, $dir) = @_;
	opendir(my $dh, $dir) or return $self->{eh}->error("Cannot open directory $dir for inclusion in the repack bundle");
	for my $entry (readdir $dh) {
		if (-f "$dir/$entry") {
			$self->add("$dir/$entry");
		} else {
			$self->addDir("$dir/$entry") unless $entry eq '.' || $entry eq '..';
		}
	}
	closedir $dh;
}

sub addCache($$) {
	my ($self, $key) = @_;
	my ($from, $to) = $self->{conf}->{cache}->preseed($key, "$self->{temp}/preseed");
	File::Path::mkpath("$self->{temp}/preseed");
	link $from, $to or File::Copy::copy $from, $to or return $self->{eh}->error("Cannot include cache entry $key in the repack bundle");
}

sub done($) {
	my $self = shift;
	local *OUT;
	open OUT, '>', "$self->{temp}/repack.sh" or return $self->{eh}->error("Cannot write repack.sh");
	print OUT "#!/bin/sh\n";
	print OUT "cd \"\${0%/*}\"\n";
	print OUT ". ./flags.inc\n";
	print OUT "exec \$PACKAGE_SCRIPT \\\n";
	print OUT "\t--os \$PACKAGE_OS \\\n";
	print OUT "\t--arch \$PACKAGE_ARCH \\\n";
	print OUT "\t--version \$PACKAGE_VERSION \\\n";
	print OUT "\t--build \$PACKAGE_BUILD \\\n";
	print OUT "\t\$PACKAGE_MANIFEST \\\n";
	print OUT "\t\$PACKAGE_TARGETS \\\n";
	print OUT "\t\$PACKAGE_DEFINES \\\n";
	print OUT "\t\$PACKAGE_TYPES \\\n";
	print OUT "\t\"\$\@\"\n";
	open OUT, '>', "$self->{temp}/flags.inc" or return $self->{eh}->error("Cannot write flags.inc");
	print OUT "PACKAGE_SCRIPT=\"platforms/paxage/package\"\n";
	print OUT "PACKAGE_OS=\"$self->{conf}->{os}\"\n";
	print OUT "PACKAGE_ARCH=\"$self->{conf}->{arch}\"\n";
	print OUT "PACKAGE_VERSION=\"$self->{conf}->{version}\"\n";
	print OUT "PACKAGE_BUILD=\"$self->{conf}->{build}\"\n";
	print OUT "PACKAGE_MANIFEST=\"" . join(' ', map {'--manifest ' . File::Spec->abs2rel(Cwd::realpath($_), $self->{conf}->{source})} @{$self->{conf}->{manifest_files}}) . "\"\n";
	print OUT "PACKAGE_TARGETS=\"" . join(' ', map {"--target $_"} @{$self->{conf}->{init_targets}}) . "\"\n";
	print OUT "PACKAGE_DEFINES=\"" . join(' ', map {"--define $_=$self->{conf}->{defines}->{$_}"} keys %{$self->{conf}->{defines}}) . "\"\n";
	print OUT "PACKAGE_TYPES=\"" . join(' ', @{$self->{conf}->{package_types}}) . "\"\n";
	close OUT;
	chmod 0755, "$self->{temp}/repack.sh";
	my $outfile = "RB-opera-$self->{conf}->{version}-$self->{conf}->{build}.$self->{conf}->{arch}.$self->{conf}->{os}.tar.xz";
	$self->{conf}->{scheduler}->scheduleCommand("cd '$self->{temp}' && $self->{tar} cJf '$self->{conf}->{outdir}/$outfile' --owner 0 --group 0 *", $outfile);
	return $self->{eh}->ok();
}

1;
