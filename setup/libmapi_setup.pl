#!/usr/bin/perl -w

use strict;

#
# Create smb.conf file if it doesn't exist
# Ask the user for the profiles database location
# Add the profile_store path to smb.conf file
# Ask for a profile name
# Codepage, language, method setup
# Ask for the exchange server ip address
# Ask for the username
# Ask for the password
# Run the nspi profile test

my $dbmarker = 0;

#
# inititalize the setup hash
#
sub init
{
    my $self = {};

    $self->{SECTION} = 0;
    $self->{SUBSECTION} = 0;
    $self->{PROFILE} = {};
    $self->{PROFILE}->{conf} = {};
    $self->{PROFILE}->{input_locale} = {};
    $self->{PROFILE}->{connection} = {};

    bless($self);

    return $self;
}

sub title
{
    my $self = shift;
    my $label = shift;

    chomp($label);

    $self->{SECTION}++;
    $self->{SUBSECTION} = 1;

    my $title = "[" . $self->{SECTION} . "] Section: " . $label;

    print "\n\n" . $title . "\n";
    print "=" x length($title);
    print "\n";
}

sub question
{
    my $self = shift;
    my $label = shift;
    my $dflt = shift;

    chomp($label);
    chomp($dflt);

    print "[" . $self->{SECTION} . "." . $self->{SUBSECTION} . "] " . $label . " [" . $dflt . "]: ";
    my $answer = <STDIN>;
    chomp($answer);
    $answer = $dflt if ($answer eq "");

    $self->{SUBSECTION}++;

    return $answer;
}

#
# Search a profile in the database
#
sub profile_search
{
    my $self = shift;

    my $command = "ldbsearch -H " . $self->{PROFILE}->{conf}->{profpath};
    $command .= " -s sub CN=" . $self->{PROFILE}->{conf}->{profname};

    my $output = `$command`;

    if ($output =~ /dn:/) {
	print "[-] The profile already exists\n";
	exit 1;
    } else {
	print "[+] Profile CN=" . $self->{PROFILE}->{conf}->{profname} . "\n";
	print "    will be added to the database\n";
    }
}

#
# Set up the profile database path
#
sub conf
{
    my $self = shift;
    
    $self->{PROFILE}->{conf}->{path} = $self->question("Profile database absolute path", "/tmp");
    $self->{PROFILE}->{conf}->{filename} = $self->question("Profile database name", "profiles.ldb");
    $self->{PROFILE}->{conf}->{profname} = $self->question("Profile to create", "default");

    $self->{PROFILE}->{conf}->{profpath} = $self->{PROFILE}->{conf}->{path} . "/" . $self->{PROFILE}->{conf}->{filename};

    if ( -e $self->{PROFILE}->{conf}->{profpath}) {

	$self->profile_search();
	print "[+] The profile database specified already exists.\n";
	print "    The provision step will be skipped\n";
	$dbmarker = 1;
    }

}

#
# Setup input locale data
#

sub input_locale
{
    my $self = shift;

    $self->{PROFILE}->{input_locale}->{codepage} = 0x4e4;
    $self->{PROFILE}->{input_locale}->{language} = 0x40c;
    $self->{PROFILE}->{input_locale}->{method} = 0x409;
    
}

#
# Connection information
#
sub conn_info
{
    my $self = shift;

    $self->{PROFILE}->{connection}->{username} = $self->question("Exchange account username", "");
    $self->{PROFILE}->{connection}->{password} = $self->question("Exchange account password", "");
    $self->{PROFILE}->{connection}->{server} = $self->question("Exchange server address", "");
}

#
# Provision a blank profiles database
#
sub provision
{
    my $self = shift;
    
# If the database file already exist, do not overwrite it

    my $command = "./setup/oc_profiles --profpath=" . $self->{PROFILE}->{conf}->{path};
    $command .= " --profdb=" . $self->{PROFILE}->{conf}->{filename};
    print `$command`;
}

#
# Launch OPENCHANGE-NSPI-PROFILE torture test
#
sub create_profile
{
    my $self = shift;
    
    # setup cmdline credentials
    my $command = "smbtorture -U" . $self->{PROFILE}->{connection}->{username};
    $command .= "%" . $self->{PROFILE}->{connection}->{password};
    # setup profiles options
    $command .= " --option=\"mapi:profile_store=" . $self->{PROFILE}->{conf}->{profpath} . "\" ";
    $command .= " --option=\"mapi:profile=" . $self->{PROFILE}->{conf}->{profname} . "\"";
    $command .= " --option=\"mapi:codepage=" . $self->{PROFILE}->{input_locale}->{codepage} . "\"";
    $command .= " --option=\"mapi:language=" . $self->{PROFILE}->{input_locale}->{language} . "\"";
    $command .= " --option=\"mapi:method=" . $self->{PROFILE}->{input_locale}->{method} . "\"";
    $command .= " ncacn_ip_tcp:" . $self->{PROFILE}->{connection}->{server} . "[] OPENCHANGE-NSPI-PROFILE";
    print `$command`;
}

my $self = init();

$self->title("Profile Database Setup");
$self->conf();
$self->title("Input Locale");
$self->input_locale();
$self->title("Connection Information");
$self->conn_info();
$self->provision() if (!$dbmarker);
$self->create_profile();

