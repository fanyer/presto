# -*- tab-width: 4; fill-column: 80 -*-

package DepTracker;

use strict;
use warnings;
use File::Find;

use ErrorHandler;

sub new($$) {
	my ($class, $conf) = @_;
	my $self = {};
	$self->{conf} = $conf;
	$self->{eh} = new ErrorHandler();
    $self->{files} = {};
	return bless $self, $class;
}

sub add($$) {
    my ($self, $file) = @_;
    $self->{files}->{File::Spec->abs2rel($file, $self->{conf}->{source})} = 1;
}

sub addDir($$;$) {
    my ($self, $dir, $glob) = @_;
    my $wanted = sub {
        my $pattern;
        if ($glob) {
            $pattern = qr(\Q$glob\E);
            $pattern =~ s|\\\*|[^/]*|g;
            $pattern =~ s|\\\?|[^/]|g;
            $pattern =~ s|\\([\[\]])|$1|g;
            $pattern = qr($pattern);
        }
        if (-d $File::Find::name || !$glob || File::Spec->abs2rel($File::Find::name, $dir) =~ $pattern) {
            $self->add($File::Find::name);
        }
    };
    File::Find::find($wanted, $dir);
}

sub done($) {
    my $self = shift;
    my $depFile = $self->{conf}->{timestamp} . '.d';
	local (*OUT, *TIMESTAMP);
	open OUT, '>', $depFile or return $self->{eh}->error("Cannot write $depFile");
    print OUT "$self->{conf}->{timestamp}:";
    foreach my $file (sort keys %{$self->{files}}) {
        print OUT " \\\n\t$file";
    }
    print OUT "\n";
    open TIMESTAMP, '>', $self->{conf}->{timestamp} or return $self->{eh}->error("Cannot write $self->{conf}->{timestamp}");
    utime undef, undef, *TIMESTAMP;
}

1;
