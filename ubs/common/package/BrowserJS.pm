#!perl  # -*- tab-width: 4; fill-column: 80 -*-

############################################################ 
#
# This module gets a file from the "http://xml.opera.com/userjs/" using HTTP::REQUEST instead of 
# the much easier FILE::FETCH and dumps it on disk
# 
############################################################ 

use strict;
use warnings;

use lib qw(../);
use lib qw(../package);

package BrowserJS;
use base qw(FileDownloader);

use LWP;
use File::Spec;
use File::Path;
use HTTP::Request;

sub new {
    my $proto = shift;
	my $class = ref($proto) || $proto;
	my %args = @_;
	
	# Call the superclass constructor
	my $self = $class->SUPER::new(%args);

	# Set the main URL to the browser js file
	$self->{_url} = "http://xml.opera.com/userjs/";  

	return $self;
}

############################################################ 
#
# Looks for a string and returns 0 if it is not found
#
############################################################ 
	
sub checkFile {
	
	my $self = shift;
	if (!$self->SUPER::checkFile())
	{
		return 0;
	}
	
	# Test if the line http://www.opera.com/docs/browserjs is in the file.
	# This is a really basic way of testing if it is the right file and might need to be endhanced.
	if ($self->{_response} =~ m/http:\/\/www\.opera\.com\/docs\/browserjs\//) 
    {
		return 1;
    }
	return 0;
}
	
1; # To make sure the use and request tests are passed
