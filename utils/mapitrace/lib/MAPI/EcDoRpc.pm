#############################################
# EcDoRpc package for mapitrace tool
# Copyright Julien Kerihuel 2007
# <j.kerihuel@openchange.org>
#
# released under the GNU GPL v3 or later
#
package MAPI::EcDoRpc;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw();
use vars qw($AUTOLOAD $VERSION);
$VERSION = '0.03';

use strict;
use File::Basename;
use MAPI::Dump_EcDoRpc;
use MAPI::Graph;
use MAPI::Regression;
use MAPI::Statistic;

use Data::Dumper;

#######################################################
# CONVENIENT SORT FUNCTIONS
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

sub sort_packets
{
    my $garbage = 0;

    my @file1 = split(/\_/, get_filename($garbage, $a));
    my @file2 = split(/\_/, get_filename($garbage, $b));

    hex($file1[0]) <=> hex($file2[0]);
}

#######################################################
# CONVENIENT GET FUNCTIONS
#######################################################

#
# Dump the filename from a pathname
#
sub get_filename
{
    my $_self = shift;
    my $pathname = shift;

    my @parts = split(/\//, $pathname);
    return $parts[$#parts];
}



#######################################################
# INIT PART
#######################################################

#
# index packets by ascending numeric order
# brace packets {in,out}
# ndrdump files in the output field
#

sub new
{
    my $_self = shift;
    my $path = shift;
    my $outputdir = shift;
    my $verbose = shift;
    my $counter = -1;

    my $self = {};
    $self->{CONF} = {};
    $self->{CONF}->{stdout} = 1;
    $self->{CONF}->{outputdir} = $outputdir;
    $self->{CONF}->{path} = $path;
    $self->{CONF}->{verbose} = $verbose;
    $self->{CONF}->{name} = $_self->get_filename($path);

    $self->{ECDORPC} = {};
    $self->{ECDORPC}->{braces_count} = 0;
    $self->{ECDORPC}->{braces} = {};
    
    $self->{ECDORPC_ERROR} = {};
    $self->{ECDORPC_ERROR}->{in} = {};
    $self->{ECDORPC_ERROR}->{out} = {};

    ############# Should disappear ################
    $self->{BRACES_ERROR} = {};
    $self->{SKIPPED_FILES} = {};

    $self->{SKIPPED_FILES}->{REQ} = {};
    $self->{SKIPPED_FILES}->{REQ}->{count} = 0;
    $self->{SKIPPED_FILES}->{REQ}->{files} = {};

    $self->{SKIPPED_FILES}->{REPL} = {};
    $self->{SKIPPED_FILES}->{REPL}->{count} = 0;
    $self->{SKIPPED_FILES}->{REPL}->{files} = {};
    ############# /Should disappear ################

    my @files =  <$path/*_Mapi_EcDoRpc>;
    my @ndrdump_files = sort sort_packets @files;

    $self->{STATISTIC} = MAPI::Statistic->new($_self->get_filename($path), @ndrdump_files);

    my $in_filename = 0;
    my $out_filename = 0;
    foreach(@ndrdump_files) {
	chomp($_);
	if ($_ =~ /in/) {
	    if ($in_filename) {
		print "[1] Skipping $in_filename\n" if $self->{CONF}->{verbose};
		$in_filename = $_;
		add_skipped_file($self, $in_filename, "REQ");
	    } else {
		$in_filename = $_ if (!$in_filename);
	    }
	}
	if ($_ =~ /out/) {
	    if (!$in_filename) {
		print "[2] Skipping $_\n" if $self->{CONF}->{verbose};
		add_skipped_file($self, $_, "REPL");
	    } else {
		$out_filename = $_;
		$counter++;
		add_brace($self, $counter, $in_filename, $out_filename);
		$out_filename = $in_filename = 0;
	    }
	}
	$self->{ECDORPC}->{braces_count} = $counter;
    }

    bless($self);

    return $self;
}

#######################################################
# ERROR RELATED FUNCTIONS
#######################################################
sub add_bad_packet
{
    my $self = shift;
    my $inout = shift;
    my $filename = shift;
    my $output = shift;

    $self->{STATISTIC}->failure_request_counter() if ($inout eq "in");
    $self->{STATISTIC}->failure_response_counter() if ($inout eq "out");

    $self->{ECDORPC_ERROR}->{$inout}->{$filename} = {};
    my $packet = $self->{ECDORPC_ERROR}->{$inout}->{$filename};

    $packet->{output} = $output;
}

sub error_report
{
    my $self = shift;
    my $inout = shift;

    if ($inout ne "in" && $inout ne "out") {
	print "[mapitrace]: Wrong parameter, must be in or out\n";
	return -1;
    }

    
    print "\n[$inout" . "put error_report]:\n\n";
    foreach my $key (sort sort_packets keys (%{$self->{ECDORPC_ERROR}->{$inout}})) {
	print $key . "\n";
    }
}

#######################################################
# NDRDUMP + BRACES + STORAGE OPERATIONS
#######################################################

#
# Initialize and add a brace to the hash
#
sub add_brace
{
    my $self = shift;
    my $idx = shift;
    my $in_filename = shift;
    my $out_filename = shift;

    $self->{ECDORPC}->{braces}->{$idx} = {};
    $self->{ECDORPC}->{braces}->{$idx}->{in} = {};
    $self->{ECDORPC}->{braces}->{$idx}->{out} = {};
    
    $self->{ECDORPC}->{braces}->{$idx}->{in}->{filename} = $in_filename;
    $self->{ECDORPC}->{braces}->{$idx}->{in}->{output} = '';
    
    $self->{ECDORPC}->{braces}->{$idx}->{out}->{filename} = $out_filename;
    $self->{ECDORPC}->{braces}->{$idx}->{out}->{output} = '';

    $self->{STATISTIC}->increment_brace_counter();
}

#
# Store the error brace in the BRACES_ERROR hash and
# Delete the brace from the ECDORPC hash
#
sub delete_brace
{
    my $self = shift;
    my $idx = shift;

    $self->{BRACES_ERROR}->{$idx} = {};
    $self->{BRACES_ERROR}->{$idx} = $self->{ECDORPC}->{braces}->{$idx};

    $self->{ECDORPC}->{braces_count}--;
    delete($self->{ECDORPC}->{braces}->{$idx});

    $self->{STATISTIC}->increment_brace_error_counter();
}

#
# ndrdump a brace and check in the output
# if we have an error (segfault)
#
sub ndrdump_brace
{
    my $self = shift;
    my $idx = shift;
    my $inout = shift;

    my $filename = $self->{ECDORPC}->{braces}->{$idx}->{$inout}->{filename};
    my $output = `ndrdump -l libmapi.so exchange_emsmdb 0x2 $inout $filename`;

    $self->{ECDORPC}->{braces}->{$idx}->{$inout}->{output} = $output;    
    if ($output =~ /INTERNAL ERROR:/) {
	$self->add_bad_packet($inout, $filename, $output);
	$self->{ECDORPC}->{braces}->{$idx}->{error} = $inout;
	return $idx;
    }

    $self->{STATISTIC}->success_request_counter() if ($inout eq "in");
    $self->{STATISTIC}->success_response_counter() if ($inout eq "out");

    return 0;
}

#
# ndrdump the files in the list and clean the hash
# if incorrect braces are found
#
sub ndrdump
{
    my $self = shift;
    my @invalid_braces = ();
    my $idx;

    foreach my $key (sort asc_num (keys(%{$self->{ECDORPC}->{braces}}))) {
	push(@invalid_braces, $idx) if ($idx = ndrdump_brace($self, $key, "in"));
	push(@invalid_braces, $idx) if ($idx = ndrdump_brace($self, $key, "out"));
    }

#    $self->{STATISTIC}->{}

    foreach(@invalid_braces) {
	delete_brace($self, $_);
    } 

#    MAPI::Dump_EcDoRpc->dump_error_brace($self);
}

############################################################
# ANALYZE PART
############################################################

#
# Retrieve the handles array (in,out) from a given packet
#
sub get_pkt_handles
{
    my $self = shift;
    my $mapi_call = shift;
    my $inout = shift;
    my $output = shift;
    my @lines = split(/\n/, $output);
    my $count_marker = 0;

    foreach(@lines) {
	if ($count_marker) {
	    $_ =~ /: (\w+) \(/i;
	    push(@{$mapi_call->{handles_in}},  $1) if ($inout eq "REQ");
	    push(@{$mapi_call->{handles_out}}, $1) if ($inout eq "REPL");
	    $count_marker--;
	}
	
	if (!$count_marker && $_ =~ /\(handles\)/) {
	    $_ =~/number=(\d+)/i;
	    $count_marker = $1;
	}
    }
}

#
# Reset mapi_call markers
#
sub mapi_call_reset_markers
{
    my $mapi_call = shift;

    $mapi_call->{marker}->{opnum} = 0;
    $mapi_call->{marker}->{opname} = 0;
    $mapi_call->{marker}->{handle_idx} = 0;
    $mapi_call->{marker}->{sub_handles} = 0;
    $mapi_call->{marker}->{store} = 0;
}

#
# Initialize the mapi call parsing structure
#

sub init_mapi_call
{
    my $mapi_call = {};

    # Initialize markers
    $mapi_call->{marker} = {};
    mapi_call_reset_markers($mapi_call);
    $mapi_call->{marker}->{call_idx} = -1;

    # Initialize the call serialization hash
    $mapi_call->{calls} = {};

    # Initialize handles
    $mapi_call->{handles_in} = ();
    $mapi_call->{handles_out} = ();

    bless($mapi_call);
    return ($mapi_call);
}

#
# Get the start of a mapi call
#

sub get_mapi_call_start
{
    my $mapi_call = shift;
    my $line = shift;
    my $inout = shift;

    if ($line =~ /EcDoRpc_MAPI_$inout$/) {
	mapi_call_reset_markers($mapi_call);
	$mapi_call->{marker}->{opnum} = 1;
	$mapi_call->{marker}->{call_idx}++;
    }
}

#
# Get mapi call opnum
#
sub get_mapi_call_opnum
{
    my $mapi_call = shift;
    my $line = shift;
    my $inout = shift;

    if ($mapi_call->{marker}->{opnum} && $line =~ /opnum\s+:/) {
	$line =~ /(0x\w{2})/i;
	
	my $idx = $mapi_call->{marker}->{call_idx};
	$mapi_call->{calls}->{$inout}->{$idx}->{opnum} = $1;

	$mapi_call->{marker}->{opnum} = 0;
	$mapi_call->{marker}->{handle_idx} = 1;
	$mapi_call->{marker}->{retval} = 1;
    }
}

#
# Get mapi call opname
#
sub get_mapi_call_opname
{
    my $mapi_call = shift;
    my $line = shift;
    my $inout = shift;

    if ($mapi_call->{marker}->{opname} && $line =~ /mapi_\w+: struct/) {
	$line =~ /mapi_(\w+):/i;

	my $idx = $mapi_call->{marker}->{call_idx};
	$mapi_call->{calls}->{$inout}->{$idx}->{opname} = $1;

	$mapi_call->{marker}->{opname} = 0;
	$mapi_call->{marker}->{sub_handles} = 1;
    }
}

#
# Get mapi call error code
#
sub get_mapi_call_retval
{
    my $mapi_call = shift;
    my $line = shift;
    my $inout = shift;

    if ($mapi_call->{marker}->{retval} && $line =~ /error_code/) {
	$line =~ /error_code\s+:\s+(\w+)/i;
	my $idx = $mapi_call->{marker}->{call_idx};
	$mapi_call->{calls}->{$inout}->{$idx}->{retval} = $1;
	$mapi_call->{marker}->{retval} = 0;
    }
}

#
# Get mapi call handle idx
#
sub get_mapi_call_handle_idx
{
    my $mapi_call = shift;
    my $line = shift;
    my $inout = shift;

    if ($mapi_call->{marker}->{handle_idx} && $line =~ /handle_idx\s+:/) {
	$line =~ /(0x\w{2})/i;
	
	my $idx = $mapi_call->{marker}->{call_idx};
	$mapi_call->{calls}->{$inout}->{$idx}->{handle_idx} = $1;

	$mapi_call->{marker}->{handle_idx} = 0;
	$mapi_call->{marker}->{opname} = 1;
    }
}

#
# Get sub handles idx within mapi_call specific dump
#
sub get_mapi_call_sub_handle_idx
{
    my $mapi_call = shift;
    my $line = shift;
    my $inout = shift;

    if ($mapi_call->{marker}->{sub_handles} && $line =~ /handle_idx/) {
	$line =~ /(0x\w{2})/i;
	
	my $idx = $mapi_call->{marker}->{call_idx};
	$mapi_call->{calls}->{$inout}->{$idx}->{self_handle_idx} = $1;
    }
}

#
# Retrieve MAPI calls for a given packet:
# - opnum
# - handle_idx
# - opname
# - sub handles
#
sub get_mapi_calls
{
    my $self = shift;
    my $mapi_call = shift;
    my $inout = shift;

    # Reset the mapi call counter
    $mapi_call->{marker}->{call_idx} = -1;

    my @lines = split(/\n/, $self->{output});
    foreach my $line (@lines) {
	chomp($line);

	get_mapi_call_start($mapi_call, $line, $inout);
	get_mapi_call_opnum($mapi_call, $line, $inout);
	get_mapi_call_handle_idx($mapi_call, $line, $inout);
	get_mapi_call_retval($mapi_call, $line, $inout);
	get_mapi_call_opname($mapi_call, $line, $inout);
	get_mapi_call_sub_handle_idx($mapi_call, $line, $inout);
    }    

    # We do not need anymore the markers
    delete($mapi_call->{marker});
}

#
# When checking mapi calls number consistency between in and out,
# we have to remove Release calls count from request cause Release
# doesn't produce any call in out (until now).
#
sub get_mapi_calls_request_count
{
    my $_self = shift;
    my $in = shift;

    my $counter = keys(%{$in});

    foreach my $key (keys(%{$in})) {
	if (hex($in->{$key}->{opnum}) == 0x1) {
	    $counter--;
	}
    }
    return $counter;
}

#
# Get the call index for a given call in the response
# This hack fixes the Release issue
# Note: MAPI serialize calls in such way we can predict 
# the response call index with reasonable confidence
#
sub get_call_index
{
    my $self = shift;
    my $mapi_call = shift;
    my $call_idx = shift;

    my $in = $mapi_call->{calls}->{REQ};
    
    return $call_idx if (keys(%{$in}) == $self->get_mapi_calls_request_count($in));

    # If count mismatches case

    return $call_idx if ($call_idx == 0);

    for (my $idx = 0; $idx <= $call_idx; $idx++) {
	$call_idx-- if (hex($in->{$idx}->{opnum}) == 0x1);
    }
    return $call_idx;
}

#
# Analyze EcDoRpc braces output and extract the information
# we need to create the object hierarchy
#

sub analyze
{
    my $self = shift;
    my $trace_opt = shift;

    foreach my $key (sort asc_num (keys(%{$self->{ECDORPC}->{braces}}))) {
	my $in = $self->{ECDORPC}->{braces}->{$key}->{in};
	my $out = $self->{ECDORPC}->{braces}->{$key}->{out};

	MAPI::Dump_EcDoRpc->dump_brace_start($in, $out) if ($trace_opt);

	$self->{ECDORPC}->{braces}->{$key}->{mapi_calls} = init_mapi_call();
	my $mapi_call = $self->{ECDORPC}->{braces}->{$key}->{mapi_calls};

	get_mapi_calls($in,  $mapi_call, "REQ");
	get_mapi_calls($out, $mapi_call, "REPL");

	# Add the request/response handle array in the element hash
	$self->get_pkt_handles($mapi_call, "REQ", $in->{output});
	$self->get_pkt_handles($mapi_call, "REPL", $out->{output});

	$self->get_mapi_call_hierarchy($in, $out, $mapi_call, $key);
	if ($trace_opt) {
	    if (MAPI::Dump_EcDoRpc->dump_mapi_calls($mapi_call)) {
		$self->delete_brace($key);
	    } else {
		MAPI::Dump_EcDoRpc->dump_brace_handles($mapi_call);
		MAPI::Dump_EcDoRpc->dump_brace_end();
	      }
	}
    }
}

############################################################
# ANALYZE PART FOR HANDLES
############################################################

#
# Get mapi_call hierarchy within a serialized request
# 
# Explanations:
# * If the call has a handle_idx set to 0xffffffff but this handle_idx is
#   already referenced by a previous call in its self_handle_idx field, then
#   the call is a child of the one with the set_handle_idx
#
# * In MAPI Semantic, a self handle is unique and can't be set twice
#
sub get_mapi_call_hierarchy
{
    my $self = shift;
    my $request = shift;
    my $response = shift;
    my $mapi_call = shift;
    my $key = shift;

    ## WARNING: This is the nasty hack that should be removed 
    if (!defined $mapi_call->{handles_in} || !defined $mapi_call->{handles_out}) {
	$self->delete_brace($key);
	return ;
    }
    
    my @handles_in = @{$mapi_call->{handles_in}};
    my @handles_out = @{$mapi_call->{handles_out}};

    my $self_handles = {};
    my $in = $mapi_call->{calls}->{REQ};

     # If we don't have multi calls, no need to go further
    return if (keys(%{$in}) == 1);

    # Retrieve calls with self_handle
    foreach my $key (sort pkt_number keys(%{$in})) {
	if ($in->{$key}->{self_handle_idx}) {
	    my $self_handle_idx = $in->{$key}->{self_handle_idx};
	    $self_handles->{$self_handle_idx} = $key;
	    $in->{$key}->{children} = ();
	}
    }

    # If we don't have any self handles, no need to go further
    return if (keys(%{$in}) == 0);

    # We now retrieve calls again
    # check for handle_idx referring handles value 0xffffffff
    # and check if their handle idx is already indexed in %self_handles
    # which means it is a child of this call
    foreach my $key (sort pkt_number keys(%{$in})) {
	if ($handles_in[hex($in->{$key}->{handle_idx})] &&
	    $handles_in[hex($in->{$key}->{handle_idx})] eq "0xffffffff") {
	    my $call_handle_idx = $in->{$key}->{handle_idx};
	    if (exists $self_handles->{$call_handle_idx}) {
		my $parent_call_idx = $self_handles->{$call_handle_idx};
		$in->{$key}->{parent} = $self_handles->{$call_handle_idx};
		push(@{$in->{$parent_call_idx}->{children}}, $key);
	    }
	}
    }
}

############################################################
# WRAPPER FUNCTIONS ON THE GRAPHVIZ MODULE
############################################################

#
# process the call hierarchy and prepare the nodes for graph output
#
sub graph_calls
{
    my $self = shift;
    my $g = shift;
    my $brace_key = shift;

    my $brace = $self->{ECDORPC}->{braces}->{$brace_key};
    my $mapi_call = $brace->{mapi_calls};

    return if (!exists $mapi_call->{handles_out});

    # Get the request and response handles
    my @handles_out = @{$mapi_call->{handles_out}};

    # Get the request mapi_call serialization hash
    my $in = $mapi_call->{calls}->{REQ};

    # Check if we are in a serialized call
    #    if (@handles_out == 1) {
    #	$g->add_node($mapi_call, 0);
    #	return ;
    #    }
 

    # Process the serialized
    # We first check for self_handle packets only
    # Next we process self_handle packets with children
    # Finally we check normal calls
    foreach my $key (sort asc_num (keys(%{$in}))) {
	if ($in->{$key}->{self_handle_idx}) {
	    $g->add_node($mapi_call, $key);
	    if (exists $in->{$key}->{children}) {
		foreach my $call_idx (@{$in->{$key}->{children}}) {
		    $g->add_node($mapi_call, $call_idx);
		}
	    }
	} else {
	    $g->add_node($mapi_call, $key);
	}
    }
}

sub graph
{
    my $self = shift;
    my $dir = shift;
    my $highlight = shift;
    my $png  = $dir . "/" . $self->get_filename($self->{CONF}->{path}) . ".png";

    # mkdir output directory if it doesn't exist
    mkdir $dir;

#    my $g = MAPI::Graph_EcDoRpc->new($png);
    my $g = MAPI::Graph->new($png, $highlight);

    # Dump the hierarchy
    $self->graph_calls($g, $_) foreach (sort asc_num 
					(keys(%{$self->{ECDORPC}->{braces}})));

    $g->render();
}

############################################################
# WRAPPER FUNCTIONS ON THE STATS MODULE
############################################################

#
# Add skipped files
#
sub add_skipped_file
{
    my $self = shift;
    my $filename = shift;
    my $inout = shift;

    my $count = $self->{SKIPPED_FILES}->{$inout}->{count};
    my $files = $self->{SKIPPED_FILES}->{$inout}->{files};

    # Initiate and add the new entry
    $files->{$count} = {};
    $files->{$count}->{filename} = $filename;

    # Increment the counter
    $self->{SKIPPED_FILES}->{$inout}->{count}++;
}

#
#
#
sub stats
{
    my $self = shift;

    $self->{STATISTIC}->scenario();

#    print Dumper($self->{SKIPPED_FILES});
}

############################################################
# SEARCH FUNCTIONS PART
############################################################

#
# Trace a call through the scenario
#
sub search_call
{
    my $self = shift;
    my $callname = shift;
    my $dump_opt = shift;
    my $inout = shift;

    my $search = {};
    my $count = 0;
    my @results = ();

    my $find_marker = 0;

    # We loop through mapi calls REQ calls and search for the given call
    foreach my $brace_key (sort pkt_number keys(%{$self->{ECDORPC}->{braces}})) {
	my $brace = $self->{ECDORPC}->{braces}->{$brace_key};

	foreach my $call_key (sort pkt_number keys(%{$brace->{mapi_calls}->{calls}->{REQ}})) {
	    my $call = $brace->{mapi_calls}->{calls}->{REQ}->{$call_key};

	    if ($call->{opname} && $call->{opname} eq $callname) {
		$find_marker = 1;
		$search->{$count}->{brace_key} = $brace_key;
		$count++;
		print $brace->{$inout}->{output} if ($dump_opt && $inout);
		push(@results, $call->{opname} . ": " . $self->get_filename($brace->{in}->{filename}) . " / " . $self->get_filename($brace->{out}->{filename}) . "\n");
	    }
	}
    }

    if ($#results >= 0) {
	print "[X] Call found in [ " . $self->{CONF}->{name} . " ]:\n";
	print "\t$_" foreach (@results);
    } else {
	print "[O] No call found in [ " . $self->{CONF}->{name} . " ]\n";
    }
    # We are now looking for the call in problematic packets
    
}

1;
