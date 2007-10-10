#############################################
# EcDoRpc Dump utility for mapitrace tool
# Copyright Julien Kerihuel 2007
# <j.kerihuel@openchange.org>
#
# released under the GNU GPL v3 or later
#
package MAPI::Dump_EcDoRpc;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw();
use vars qw($AUTOLOAD $VERSION);
$VERSION = '0.01';

use strict;
use Data::Dumper;

#######################################################
# convenient sort functions                           #
#######################################################
sub pkt_number 
{
    my ($aa) = $a =~ /(\d+)/;
    my($bb) = $b =~ /(\d+)/;
    ($aa) <=> ($bb);
}

sub asc_num {
   hex($a) <=> hex($b);
}
#######################################################
# BRACE DUMPING FUNCTIONS
#######################################################

sub dump_error_brace
{
    my $_self = shift;
    my $self = shift;

    foreach my $key (sort asc_num (keys(%{$self->{BRACES_ERROR}}))) {
	my $inout = $self->{BRACES_ERROR}->{$key}->{error};
	if ($inout) {
	    print "[mapitrace] error in [$inout]: ";
	    print $self->{BRACES_ERROR}->{$key}->{$inout}->{filename} . "\n";
	}
    }
}

sub dump_brace_start
{
    my $_self = shift;
    my $in = shift;
    my $out = shift;

    print MAPI::EcDoRpc->get_filename($in->{filename});
    print " / ";
    print MAPI::EcDoRpc->get_filename($out->{filename});
    print "\n";
    print "----------------------------------------------\n";
}

sub dump_brace_end
{
    print "\n\n";
}

#######################################################
# DUMP FUNCTIONS FOR THE ANALYZE PART
#######################################################

#
# Dump the handles array from a request or response
#
sub dump_handles
{
    my $_self = shift;
    my $self = shift;
    my @handles = @_;

    print $self->{filename} . " :\n";
    print "handles array (" . ($#handles + 1) . "):\n";
    print "\t$_\n" foreach (@handles);
    print "\n";
}

#
# Provides a dual dump view for the handle array
# both in request and response
#
# MAPI Semantic tells that request and response have the same
# number of handles elements
#
sub dump_brace_handles
{
    my $_self = shift;
    my $mapi_call = shift;

    my @handles_in = @{$mapi_call->{handles_in}};
    my @handles_out = @{$mapi_call->{handles_out}};

    print "handles array (" . ($#handles_in + 1) . "):\n";
    for (my $i = 0; $handles_in[$i]; $i++) {
	printf "\t[%.2x] %s -> %s\n", $i, $handles_in[$i], $handles_out[$i];
    }
}

#
# Print a call
#
#
sub dump_call
{
    my $mapi_call = shift;
    my $key = shift;
    my $call_idx = shift;
    my $space = shift;

    my $in  = $mapi_call->{calls}->{REQ};
    my $out = $mapi_call->{calls}->{REPL};

    my $new_idx;
    
    if ($call_idx) {
	$new_idx = MAPI::EcDoRpc->get_call_index($mapi_call, $call_idx);
	printf "\t%s+-- [0x%.2x] [%s] (opnum = %s) %15s (handle_idx = %s) (child of %s)\n", 
	$space, hex($call_idx), 
	(exists $out->{$new_idx}->{retval}) ? $out->{$new_idx}->{retval}:'',
	$in->{$call_idx}->{opnum}, $in->{$call_idx}->{opname}, 
	$in->{$call_idx}->{handle_idx}, $in->{$key}->{opname};
    } else {
	$new_idx = MAPI::EcDoRpc->get_call_index($mapi_call, $key);
	printf "\t[0x%.2x] [%s] (opnum = %s) %25s (handle_idx = %s) (self_handle_idx = %s)\n", 
	hex($key), 
	(exists $out->{$new_idx}->{retval}) ? $out->{$new_idx}->{retval} : '', 
	$in->{$key}->{opnum}, $in->{$key}->{opname}, 
	$in->{$key}->{handle_idx},
	$in->{$key}->{self_handle_idx};
    }
}

#
# Dump mapi calls
#
# Since calls with self_handles have a smaller index than other calls relying on them
# When a parent value is > than the current idx, we can skip display the call.
# It should have normally been printing earlier when checking for call children
# Return -1 if we have an error
#

sub dump_mapi_calls
{
    my $_self = shift;
    my $mapi_call = shift;

    my $in  = $mapi_call->{calls}->{REQ};
    my $out = $mapi_call->{calls}->{REPL};

    my $in_call_count = MAPI::EcDoRpc->get_mapi_calls_request_count($in);

    if ($in_call_count != keys(%{$out})) {
	print "[mapitrace] error: Oops seems like we have some trouble ...\n";
	print "[mapitrace] error: Mismatch call count between request ($in_call_count) and response (". keys(%{$out})  .")\n";
	return -1;
    }
    
    printf "serialized mapi calls (count = %.2d):\n", $in_call_count;
    foreach my $key (sort asc_num (keys(%{$in}))) {
	if ($in->{$key}->{self_handle_idx}) {
	    # If this self_handle call has a child ;p
	    my $space = "  ";
	    if (exists $in->{$key}->{parent}) {
		$space .= "       ";
	    } else {
		dump_call($mapi_call, $key, 0, 0);
	    }
	    # Check if the call with self_handle_idx has children
	    # The dump will fail if we have Release call.
	    # In order to avoid this problem we use out instead
	    
	    if ($in->{$key}->{children}) {
		foreach my $call_idx (sort asc_num @{$in->{$key}->{children}}) {
		    dump_call($mapi_call, $key, $call_idx, $space);
		}
	    }
	}
	else {
	    if (exists $in->{$key}->{parent} && hex($in->{$key}->{parent}) < hex($key)) {
	    } else {
		printf "\t[0x%.2x] [%s] (opnum = %s) %25s (handle_idx = %s)\n", 
		hex($key),
		(exists $out->{$key}->{retval}) ? $out->{$key}->{retval}: '', 
		$in->{$key}->{opnum}, $in->{$key}->{opname}, $in->{$key}->{handle_idx};	    
	    }
	}
    }
    print "\n";
    return 0;
}

1;
