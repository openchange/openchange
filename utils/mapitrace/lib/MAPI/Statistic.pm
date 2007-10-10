##############################################
# EcDoRpc Statistic package for mapitrace tool
# Copyright Julien Kerihuel 2007
# <j.kerihuel@openchange.org>
#
# released under the GNU GPL v3 or later
#
package MAPI::Statistic;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw();
use vars qw($AUTOLOAD $VERSION);
$VERSION = '0.01';

use strict;
use Data::Dumper;

#######################################################
# INIT PART
#######################################################

#
# Initiate the statistic object
#
sub new
{
    my $_self = shift;
    my ($scenario) = shift;
    my (@files) = @_;

    my $self = {};

    $self->{SCENARIO} = {};
    $self->{SCENARIO}->{name} = $scenario;
    $self->{SCENARIO}->{total} = $#files + 1;

    $self->{SCENARIO}->{BRACES}->{total} = 0;
    $self->{SCENARIO}->{BRACES}->{failure} = 0;

    $self->{SCENARIO}->{REQUEST}->{total} = 0;
    $self->{SCENARIO}->{REQUEST}->{success} = 0;
    $self->{SCENARIO}->{REQUEST}->{failure} = 0;

    $self->{SCENARIO}->{RESPONSE}->{total} = 0;
    $self->{SCENARIO}->{RESPONSE}->{success} = 0;
    $self->{SCENARIO}->{RESPONSE}->{failure} = 0;

    count_total_packets_type($self, @files);

    $self->{SCENARIO}->{CALLS} = {};

    bless($self);

    return $self;
}

#######################################################
# COUNT FUNCTION PART
#######################################################

#
# Count the full number of requests and replies
#
sub count_total_packets_type
{
    my $self = shift;
    my (@files) = @_;

    foreach (@files) {
	$self->{SCENARIO}->{REQUEST}->{total}++ if ($_ =~ /in/);
	$self->{SCENARIO}->{RESPONSE}->{total}++ if ($_ =~ /out/);
    }
}

#
#
#
sub increment_brace_counter
{
    my $self = shift;

    $self->{SCENARIO}->{BRACES}->{total}++;
}

sub increment_brace_error_counter
{
    my $self = shift;

    $self->{SCENARIO}->{BRACES}->{failure}++;
}

#
# 
#
sub count_packet_inc
{
    my $self = shift;
    my $inout = shift;
    my $state = shift;

    return -1 if (!$state || !$inout);
    $self->{SCENARIO}->{REQUEST}->{$state}++ if ($inout eq "in");
    $self->{SCENARIO}->{RESPONSE}->{$state}++ if ($inout eq "out");
}

#
# Accessors
#
sub success_request_counter
{
    my $self = shift;
    $self->increment_counter("REQUEST", "success");
}

sub failure_request_counter
{
    my $self = shift;
    $self->increment_counter("REQUEST", "failure");
}

sub success_response_counter
{
    my $self = shift;
    $self->increment_counter("RESPONSE", "success");
}

sub failure_response_counter
{
    my $self = shift;
    $self->increment_counter("RESPONSE", "failure");
}

sub increment_counter
{
    my $self = shift;
    my $type = shift;
    my $case = shift;

    $self->{SCENARIO}->{$type}->{$case}++;
}

#######################################################
# DUMP PART
#######################################################

sub percent
{
    my $obj = shift;
    my $type = shift;

    return ($obj->{$type}->{total} / $obj->{total}) * 100;
}

sub scenario
{
    my $self = shift;

    my $request_percent = percent($self->{SCENARIO}, "REQUEST");
    my $response_percent = percent($self->{SCENARIO}, "RESPONSE");

    print "+-----[ Scenario: " . $self->{SCENARIO}->{name} . " ]-----+\n";
    print "[statistic]: We have " . $self->{SCENARIO}->{total} . " packets ";
    print "(request: " . $request_percent . "%, ";
    print "response: " . $response_percent . "%)\n";

    my $success_req_rate = ($self->{SCENARIO}->{REQUEST}->{success} / $self->{SCENARIO}->{REQUEST}->{total}) * 100;
    my $success_repl_rate = ($self->{SCENARIO}->{RESPONSE}->{success} / $self->{SCENARIO}->{RESPONSE}->{total}) * 100;
    print "[statistic]: request success rate: " . $success_req_rate . "\n";
    print "[statistic]: response success rate: " . $success_repl_rate . "\n";

    my $braces_count = $self->{SCENARIO}->{BRACES}->{total};
    my $braces_failure = $self->{SCENARIO}->{BRACES}->{failure};
    if ($braces_failure) {
	print "[statistic]: success brace analysis rate: " . ((1 - ($braces_failure/$braces_count)) * 100) . "\n";
	print "[statistic]: braces skipped: " . $braces_failure . "/" . $braces_count . "\n";
    } else {
	print "[statistic]: success brace analysis rate: 100%\n";	
	print "[statistic]: no braces skipped\n";
    }
    print "+--------------------------------------------------------------+\n";
    
}

sub dump
{
    my $self = shift;
}


1;
