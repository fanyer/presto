# -*- tab-width: 4; fill-column: 80 -*-

package ErrorHandler;

use strict;
use warnings;

sub new($) {
	my $class = shift;
	my $self = {};
	$self->{errors} = 0;
	return bless $self, $class;
}

sub error($$) {
	my ($self, $msg) = @_;
	print STDERR "$msg\n";
	$self->{errors}++;
	return undef;
}

sub reset($) {
	my $self = shift;
	$self->{errors} = 0;
}

sub ok($) {
	my $self = shift;
	return $self->{errors} == 0;
}

1;
