#!perl  # -*- tab-width: 4; fill-column: 80 -*-

############################################################ 
#
# Base class for files that need to be downlaoded
# from various locations using HTTP::REQUEST instead of 
# the much easier FILE::FETCH and dumps it on disk
# 
############################################################ 

use strict;
use warnings;

package FileDownloader;

use LWP;
use File::Spec;
use File::Path;
use HTTP::Request;

sub new {
    my $proto = shift;
	my $class = ref($proto) || $proto;

	# Declare self
    my $self  = {}; 

	# Setup the user agent
	$self->{_user_agent} =LWP::UserAgent->new();

	# bcd-portal responses
	$self->{_response} = undef; 

	# maximum number of download attempts
	$self->{_max_attempts} = 5;

	# delay before retry, in seconds
	$self->{_retry_interval} = 10;
	
	$self->{_ready} = 0;

	bless ($self, $class);

    return $self;
}

sub setUAVersion {
	my $self = shift;
	my $UA_version = shift;
	
	$self->{_user_agent}->agent("Opera/9.80 (Windows NT 5.1; U; en) Presto/2.2.15 Version/".$UA_version);
	
	$self->{_ready} = 1;
}

############################################################ 
#
# Makes a request and gets the content from the server
#
############################################################ 

sub getFile
{
	my $self = shift;
	my $dest_path = shift;
	my $file_name = shift;
	my $retries = 0;
	
	if (!$self->{_ready})
	{
		return 0;
	}
	
	#Checks if both destination path and file name is set
	if (!defined($dest_path) || $dest_path eq "" )
	{
		return 0;
	}
	if (!defined($file_name) || $file_name eq "" )
	{
		return 0;
	}
	
	while ($retries < $self->{_max_attempts})
	{
		if ($retries > 0)
		{
			warn "Failed to download ".$file_name.", retrying in ".$self->{_retry_interval}." seconds...";
			sleep $self->{_retry_interval};
		}

		# Build the request
		my $req = new HTTP::Request(GET=>$self->{_url});

		# Send the request
		my $res = $self->{_user_agent}->request($req);

		# Check if the request worked
		if ($res->is_success) 
		{
			$self->{_response} = $res->content;
	
			# Looks for a string to see if we have the right file
			if ($self->checkFile()) 
			{
				# Saves the response to the given path
				$self->saveresponse(File::Spec->catfile($dest_path, $file_name));
				return 1;
			}
		}
		$retries++;
	}
	
	return 0;
}

############################################################ 
#
# Saves the response from the server to disk
#
############################################################ 

sub saveresponse
{
	my $self = shift;
	my $save_file = shift;		# Relative or absolute path to the file to save to
	
	# Make sure there is a save dir otherwise error and there is a response
	if (defined($save_file) && $save_file ne "" && defined($self->{_response}))
	{
		# Save the response to disk
		if (open(BCD_OUTPUT, '>', $save_file))
		{
			binmode BCD_OUTPUT;
		
			print BCD_OUTPUT $self->{_response};
			close(BCD_OUTPUT);
	
			return 1;
		}
	}	
	return 0;
}

############################################################ 
#
# Condition to check that the file was downloaded ok
#
############################################################ 
	
sub checkFile {
	
	my $self = shift;
	
	return 1;
}
	
1; # To make sure the use and request tests are passed
