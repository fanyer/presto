# -*- tab-width: 4; fill-column: 80 -*-

package MacroProcessor;

use strict;
use warnings;

sub new($$) {
	my ($class, $conf) = @_;
	my $self = {};
	$self->{conf} = $conf;
	$self->{eh} = $conf->{eh};
	$self->{further} = 0;
	$self->{defines} = {};
	bless $self, $class;
	foreach (keys(%{$self->{conf}->{defines}}))
	{
		$self->define($_, $self->{conf}->{defines}->{$_});
	}
	$self->process($self->{conf}->{macro_file}, '/dev/null', 0) if $self->{conf}->{macro_file};
	return $self;
}

sub process($$$$) {
	my ($self, $source, $dest, $delayed) = @_;
	local *OUT;
	open OUT, '>', $dest or return $self->{eh}->error("Cannot write $dest");
	return $self->include($source, *OUT, '', $delayed);
}

sub needFurtherProcessing($) {
	my $self = shift;
	return $self->{further};
}

sub include($$$$$$) {
	my $self = shift;
	my $source = shift;
	local *OUT = shift;
	my $indent = shift;
	my $delayed = shift;
	local *IN;
	open IN, '<', $source or return $self->{eh}->error("Cannot read $source");
	$self->{conf}->{repack_bundle}->add($source) if $self->{conf}->{repack_bundle};
	$self->{conf}->{dep_tracker}->add($source) if $self->{conf}->{dep_tracker};
	my $disabled = 0;
	my @stack;
	my $line = 0;
	while (<IN>) {
		$line++;
		if (/^\s*\@\@if(n?)def\s+([-_A-Za-z0-9:]+)\s*$/) {
			my $status = ($self->isDefined($2) xor $1) ? 1 : 0;
			$disabled++ unless $status == 1;
			push @stack, $status;
		} elsif (/^\s*\@\@else\s*$/) {
			return $self->{eh}->error("$source:$line: \@\@else outside of conditional") unless @stack;
			my $status = pop @stack;
			$disabled-- unless $status == 1;
			$status = $status == 0 ? 1 : -1;
			$disabled++ unless $status == 1;
			push @stack, $status;
		} elsif (/^\s*\@\@elif(n?)def\s+([-_A-Za-z0-9:]+)\s*$/) {
			return $self->{eh}->error("$source:$line: \@\@elif$1def outside of conditional") unless @stack;
			my $status = pop @stack;
			$disabled-- unless $status == 1;
			$status = $status == 0 ? (($self->isDefined($2) xor $1) ? 1 : 0) : -1;
			$disabled++ unless $status == 1;
			push @stack, $status;
		} elsif (/^\s*\@\@endif\s*$/) {
			return $self->{eh}->error("$source:$line: \@\@endif outside of conditional") unless @stack;
			my $status = pop @stack;
			$disabled-- unless $status == 1;
		} elsif (/^\s*\@\@define\s+([-_A-Za-z0-9:]+)\s*(.*)?\s*$/) {
			$self->define($1, $self->interpolate($2, "$source:$line", $delayed)) unless $disabled;
		} elsif (/^\s*\@\@undef\s+([-_A-Za-z0-9:]+)\s*$/) {
			$self->define($1, undef) unless $disabled;
		} elsif (/^(\s*)\@\@include\s+([^\s]+)\s*$/) {
			unless ($disabled) {
				my (undef, $dir, undef) = File::Spec->splitpath($source);
				my $file = $self->interpolate($2, "$source:$line", 0);
				$self->include(File::Spec->rel2abs($file, $dir), *OUT, "$indent$1", $delayed) or return 0;
			}
		} elsif (/^\s*\@\@error\s+(.*)\s*$/) {
			return $self->{eh}->error("$source:$line: $1") unless $disabled;
		} elsif (/^\s*\@\@#/) {
			# comment
		} elsif (/\@\@(?!{([^}]+)})/) {
			return $self->{eh}->error("$source:$line: Macro syntax error");
		} else {
			print OUT $indent . $self->interpolate($_, "$source:$line", $delayed) unless $disabled;
		}
	}
	return $self->{eh}->error("$source: Unterminated conditional") if $disabled;
	return 1;
}

sub define($$$) {
	my ($self, $def, $value) = @_;
	if (defined $value) {
		$self->{defines}->{$def} = $value;
	} else {
		delete $self->{defines}->{$def};
	}
}

sub isDefined($$) {
	my ($self, $def) = @_;
	return exists $self->{defines}->{$def} || grep($_ eq $def, @{$self->{conf}->{targets}});
}

sub interpolate($$$$) {
	my ($self, $text, $loc, $delayed) = @_;
	$text =~ s/\@\@{([^}]+)}/$self->replace($1, $loc, $delayed)/eg;
	return $text;
}

sub replace($$$$) {
	my ($self, $var, $loc, $delayed) = @_;
	if (defined($self->{defines}->{$var})) {
		return $self->{defines}->{$var};
	} elsif ($var eq 'SUFFIX') {
		$self->{further} = 1 if $delayed;
		return $delayed ? "\@\@{$var}" : $self->{conf}->{suffix};
	} elsif ($var eq '_SUFFIX') {
		$self->{further} = 1 if $delayed;
		return $delayed ? "\@\@{$var}" : $self->{conf}->{_suffix};
	} elsif ($var eq 'USUFFIX') {
		$self->{further} = 1 if $delayed;
		return $delayed ? "\@\@{$var}" : $self->{conf}->{usuffix};
	} elsif ($var eq 'PREFIX') {
		$self->{further} = 1 if $delayed;
		return $delayed ? "\@\@{$var}" : $self->{conf}->{prefix};
	} elsif ($var =~ /^TIME:(.*)$/) {
		return POSIX::strftime $1, @{$self->{conf}->{starttime}};
	} elsif ($var =~ /^FILES:(.*)$/) {
		return $self->interpolate($self->{conf}->{fh}->getFileSummary($1), $loc, $delayed);
	} elsif ($var =~ /^CONF:(.*)$/) {
		unless (exists $self->{conf}->{$1}) {
			$self->{eh}->error("$loc: Unknown configuration parameter $1");
			return "\@\@{$var}";
		}
		return $self->{conf}->{$1};
	} elsif ($var =~ /^(ABS|REL):(.*)$/) {
		my $dir = $self->{conf}->{shipping}->folderDir($2);
		unless ($dir) {
			$self->{eh}->error("$loc: Unknown directory $2");
			return "\@\@{$var}";
		}
		$dir = "\@\@{PREFIX}/$dir" if $1 eq 'ABS';
		return $self->interpolate($dir, $loc, $delayed);
	} else {
		$self->{eh}->error("$loc: Undefined substitution $var");
		return "\@\@{$var}";
	}
}

1;
