package Opera::INI;
use strict; use warnings;

#eval {
    #use threads::shared;
#};

#if (@!) {
#    use Opera::INI::fakethreads;
#}

use overload q("") => \&toString;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {}; #&share( {} );

    bless ($self, $class);

    my %options = @_;

    $self->{preservecase} = defined $options{preservecase};
    $self->read( $options{file} ) if defined $options{file};

    return $self;
}

sub read {
    my $self = shift;
    my $filename = shift;
    my $preservecase = shift;

    if (! defined $self->{ini} ) {
        $self->{ini} = {}; #&share( {} );
    }

    my ($l, $currentstruct) = ("","");
    
    open(FILE,$filename) || $self->error("Really needed that INI, but couldn't find it.");
    
    while ($l = <FILE>) {
        if ($l =~ /^\[(.*)\]/) {
            $currentstruct = ($self->{preservecase}) ? $1 : lc($1);
        } else {
            next if $l =~ /^#/;

            if ($l =~ /^\s*(.*?)\s*=\s*(.*)\n/) {

                if (!defined $self->{ini}->{$currentstruct}) {
                    $self->{ini}->{$currentstruct} = {}; #&share( {} );
                }                

                if ($self->{preservecase}) {
                    ${ $self->{ini}->{$currentstruct} }{$1} = $2;
                }
                else {
                    ${ $self->{ini}->{$currentstruct} }{lc($1)} = $2;
                }
            }
        }	
    }
    close(FILE);
}

## Writes the modified ini file to disk

sub write {
    my $self = shift;
    my ($filename) = (@_);

    open(FILE, '>', $filename) || $self->error("Really needed to write a new INI, but wasn't allowed to do that");
    
    foreach my $i (keys %{$self->{ini}}) {
        print FILE "[$i]\n" unless ($i=~/^\s*$/);
        my %hash = %{$self->{ini}->{$i}};
        foreach my $a (keys %hash) {
            print FILE "$a=$hash{$a}\n" unless ($i=~/^\s*$/);
        }
        print FILE "\n";
    }
    close(FILE);
}

sub writeToISM {
	(my $self, my $ISM_writer) = @_;
	return $ISM_writer->AddIniSettings($self->{ini});
}

## Writes the modified ini file to a string and returns it



sub toString {
    my $self = shift;
    
    my $string = ""; 
    foreach my $i (keys %{$self->{ini}}) {
        $string .= "[$i]\n";
        my %hash = %{$self->{ini}->{$i}};
        foreach my $a (keys %hash) {
            $string .= "$a=$hash{$a}\n";
        }
        $string .= "\n";
    }
    
    return $string;
}


## Sets a variable (whether it exists or not - hash tables
## Returns the modified hash table. Access like this:
## %ini = setIniVariable("struct","setting","value",%ini);

sub set {
    my $self = shift;
    my ($struct, $var, $value) = (@_);

    $struct = ($self->{preservecase}) ? $struct : lc($struct);
    $var = ($self->{preservecase}) ? $var : lc($var);

    ${$self->{ini}->{$struct}}{$var} = $value;
}

sub get {
    my $self = shift;
    my ($struct, $var) = @_;

    if (! defined $struct and ! defined $var) {
        die "FFFFFFFFFOOOOOOOOOOOOOOOOOOOBBBBBBBBBBBAAAAAAAARRRRRR!!!!\n";
    }
    else {
        #warn $struct ." - ". $var;
    }

    $struct = ($self->{preservecase}) ? $struct : lc($struct);
    $var = ($self->{preservecase}) ? $var : lc($var);

    return ${$self->{ini}->{$struct}}{$var};
}


sub getSection {
    my $self = shift;
    my $section_name = shift;

    if (defined $self->{ini}->{$section_name}) {
        return %{$self->{ini}->{$section_name}};
    }
    else {
        return undef;
    }
}


## The mandatory error method	

sub error {
    my $self = shift;
    my $msg = shift;
    print $msg."\n";
    exit;
}

1;
__END__

=head1 NAME

Opera::INI - Perl extension for reading INI-files.

=head1 SYNOPSIS

  use Opera::INI;

  my $ini = Opera::INI::->new( file => 'config.ini' );
  $ini->read( 'config.ini' );
  $ini->get('section', 'key');

  my %settings = $ini->getSection('section');

=head1 DESCRIPTION

It ain't pretty, but it works.


=head1 METHODS



=head2 new( [file =E<gt> FILENAME, preservecase => (1 | undef)] )

Creates a new instance.

=head3 OPTIONS

  file => FILENAME            - Read the specified ini file.


  preservecase => (1 | undef) - If set to 1 then case is preserved on
                                section names and keys. If it isn't
                                specified (or set to undef), then all
                                section names and keys are converted
                                to lowercase.





=head2 read( FILENAME )

Reads the specified ini-file.



=head2 get( SECTION, KEY )

Retrieves the ini data from the specified section and key.



=head2 getSection( SECTION )

Returns a hash containing all key/value pairs in the specified section.



=head2 EXPORT

None by default.


=head1 AUTHOR

Stephan B. Nedregaard, E<lt>stephan@opera.com<gt>
Vetle Roeim, E<lt>vetler@opera.com<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2004 by Opera Software ASA

=cut
