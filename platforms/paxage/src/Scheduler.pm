# -*- tab-width: 4; fill-column: 80 -*-

package Scheduler;

use strict;
use warnings;
use IO::File;

sub new($) {
	my $class = shift;
	my $self = {};
	$self->{tasks} = [];
	return bless $self, $class;
}

sub scheduleCommand($$$) {
	my ($self, $command, $output) = @_;
	push @{$self->{tasks}}, {command => $command, output => $output};
}

sub done($) {
	my $self = shift;
	my @out_files;
	my $res = 1;
	foreach my $task (@{$self->{tasks}}) {
		my $handle = new IO::File;
		open $handle, '|-' , $task->{command} or die "Cannot run command: $task->{command}";
		$task->{handle} = $handle;
	}
	foreach my $task (@{$self->{tasks}}) {
		if (close $task->{handle}) {
			push @out_files, $task->{output};
		} else {
			print STDERR "Failed to compress $task->{output}\n";
			$res = 0;
		}
	}
	$self->{out_files} = [@out_files];
	return $res;
}

sub taskCount($) {
	my $self = shift;
	return scalar @{$self->{tasks}};
}

sub filesProduced($)
{
	my $self = shift;
	return @{$self->{out_files}};
}

1;
