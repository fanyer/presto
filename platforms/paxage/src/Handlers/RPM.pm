# -*- tab-width: 4; fill-column: 80 -*-

package Handlers::RPM;

use strict;
use warnings;

use Handlers::FileHandler;

sub new($$) {
	my ($class, $conf) = @_;
	my $self = {};
	$self->{conf} = $conf;
	$self->{eh} = $conf->{eh};
	$self->{stem} = $self->{conf}->{stem};
	$self->{rpmroot} = "$self->{conf}->{temp}/rpmbuild";
	$self->{fh} = new Handlers::FileHandler($self->{conf}, "$self->{rpmroot}/BUILDROOT/$self->{conf}->{prefix}", 0);
	$self->{fh}->setSpecialLocation('RPM', $self->{rpmroot});
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
	File::Path::mkpath("$self->{rpmroot}/RPMS/$self->{conf}->{arch}");
	$self->{conf}->{scheduler}->scheduleCommand(<<EOF, "$self->{stem}.rpm");
set -e
export HOME=$self->{conf}->{temp}
export MAGIC=/usr/share/file/magic
rpmbuild -bb --quiet --buildroot $self->{rpmroot}/BUILDROOT --target $self->{conf}->{arch}-pc-linux $self->{rpmroot}/SPECS/$self->{conf}->{product}.spec >/dev/null
ln -f $self->{rpmroot}/RPMS/$self->{conf}->{arch}/$self->{stem}.rpm $self->{conf}->{outdir}/$self->{stem}.rpm
EOF
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
	if ($type eq 'requires') {
		my $res = $self->{conf}->{cache}->getString('rpm:requires');
		return $res if defined $res;
		my $listfilename = "$self->{conf}->{temp}/scandeps.list";
		local *LISTFILE;
		open LISTFILE, '>', $listfilename or return $self->{eh}->error("Cannot write $listfilename");
		foreach (sort {$a->{file} cmp $b->{file}} @{$self->{fh}->getManifest()})
		{
			print LISTFILE "$_->{loc}\n" if $_->{scandeps};
		}
		close LISTFILE;
		my $command = `rpm --eval '%__find_requires'`;
		chomp $command;
		if (!$command) {
			return $self->{eh}->error("Cannot detect the find-requires command");
		}
		local *REQUIRES;
		open REQUIRES, '-|', "$command <$listfilename" or return $self->{eh}->error("Cannot run the find-requires command");
		while (<REQUIRES>) {
			$res .= $_;
		}
		close REQUIRES;
		unless ($res) {
			return $self->{eh}->error("The find-requires command produced no output");
		}
		$self->{conf}->{cache}->putString('rpm:requires', $res);
		$self->{conf}->{repack_bundle}->addCache('rpm:requires') if $self->{conf}->{repack_bundle};
		return $res;
	} elsif ($type =~ /^subtree:(.*)$/) {
		my @res;
		my $branch = $self->{conf}->{shipping}->folderDir($1);
		foreach (sort {$a->{file} cmp $b->{file}} @{$self->{fh}->getManifest()})
		{
			if ($_->{file} =~ m|^$branch/|) {
				push @res, "$self->{conf}->{prefix}/$_->{file}";
			}
		}
		return join "\n", @res;
	} else {
		return $self->{fh}->getFileSummary($type);
	}
}

sub fixSigPaths ($$$) { return 0; }

sub fixRegionIni($$$) { return 0; }

1;
