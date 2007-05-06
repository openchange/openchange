##################################################
# EcDoRpc GraphViz Dump package for mapitrace tool
# Copyright Julien Kerihuel 2007
# <j.kerihuel@openchange.org>
#
# released under the GNU GPL
#

package MAPI::Graph;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw();
use vars qw($AUTOLOAD $VERSION);
$VERSION = '0.01';

use strict;
use GraphViz;
use Data::Dumper;

#
# Init the MAPI::Graph object
#
sub new
{
    my $_self = shift;
    my $output_file = shift;
    my $highlight = shift;

    my $self = {};

    $self->{graph} = GraphViz->new(node => {shape => 'box'});
    $self->{nodelist} = {};
    $self->{output_file} = $output_file;
    $self->{highlight} = $highlight;

    bless($self);
    return $self;
}

#
# Render the graph
#
sub render
{
    my $self = shift;

    print "Dumping graph in ." . $self->{output_file} . "\n";
    $self->{graph}->as_png($self->{output_file});
}

#
# Init a new node in the general nodelist hash
# A node is a set of queue each of them composed of children
#
sub node_init
{
    my $self = shift;
    my $node_id = shift;

    my $nodelist = $self->{nodelist};

    print "[node_init]: Adding node: $node_id\n";
    $nodelist->{$node_id} = {};
    $nodelist->{$node_id}->{children} = {};
    $nodelist->{$node_id}->{count} = 1;
    @{$nodelist->{$node_id}->{children}->{1}} = ();
}

#
# If a release occur on a node we need to increment
# the depth so we can start a new hierarchy
#
sub add_depth
{
    my $self = shift;
    my $node_id = shift;

    my $node = $self->{nodelist}->{$node_id};

    $node->{count} += 1;
    my $count =  $node->{count};

    $node->{children}->{$count} = ();
    
}

#
# Get the last child id for the given node_id
#
sub get_last_nodelist_child
{
    my $self = shift;
    my $node_id = shift;

    my $node = $self->{nodelist}->{$node_id};

    return 0 if (!$node);

    my $count = $node->{count};
    my $child_id = @{$node->{children}->{$count}};

    # If the node doesn't have children return the parent one
    return $node_id if (!$child_id);

    # Otherwise return the last child
    my @hierarchy = @{$node->{children}->{$count}};
    $child_id -= 1;
    my $last_child = $hierarchy[$child_id];
    return $last_child;
}

#
# add a child to an existing node in the nodelist hash
#
sub nodelist_add_child
{
    my $self = shift;
    my $node_id = shift;

    # get the node in the hash
    my $node = $self->{nodelist}->{$node_id};
    # get the number of hierarchy array
    my $count = $node->{count};
    # get the number of children for the given hierarchy
    my $child_count = @{$node->{children}->{$count}};
    # we increment the child counter
    $child_count += 1;
    my $child_name = "$node_id:$count:$child_count";

    print "Adding node: $child_name\n";
    push(@{$node->{children}->{$count}}, $child_name);

    return $child_name;
}

#
# add a node in the general nodelist hash
# it can be a node or the child of an existing node
#
sub nodelist_add
{
    my $self = shift;
    my $node_id = shift;

    my $nodelist = $self->{nodelist};

    # Add the node directly if not referenced in the nodelist hash
    if (!exists $nodelist->{$node_id}) {
	$self->node_init($node_id);
	return $node_id;
    }

    # If the node already exists, add a child for the correct one
    my $last_child = $self->nodelist_add_child($node_id);
    return $last_child;
}

#
# Add edge between two nodes
#
sub add_edge
{
    my $self = shift;
    my $parent_handle = shift;
    my $self_handle = shift;

    my $g = $self->{graph};

    # If self_handle exists add an edge between 
    # the last parent's child and the self_handle
    my $last_child = $self->get_last_nodelist_child($parent_handle);

    if ($self_handle) {
	print "[add_edge] $last_child -> $self_handle\n";
	$g->add_edge($last_child, $self_handle);
    } else {
	# if this is a root element we shouldn't add edge
	if ($parent_handle ne $last_child) {
	    print "[add_edge] $parent_handle -> $last_child\n";
	    $g->add_edge($parent_handle, $last_child);
	} else {
	    print "[add_edge]: no edge for $parent_handle\n";
	}
    }
}

#
# Add a node to the graph
#

sub add_node
{
    my $self = shift;
    my $mapi_call = shift;
    my $key = shift;

    my $g = $self->{graph};

    my $parent_handle = $self->get_parent_handle($mapi_call, $key);
    my $self_handle = $self->get_self_handle($mapi_call, $key);

    # If we do not have a self handle
    if (!$self_handle) {
	my $last_child = $self->get_last_nodelist_child($parent_handle);
	my $node_id = $self->nodelist_add($parent_handle);
	my $label = $self->get_node_label($mapi_call, $key);
	
	if ($self->{highlight} && !$self->node_highlight($label)) {
	    print "############ HERE ##############\n";
	    $g->add_node($node_id, label => $label,
			 style => "filled",
			 color => "green");	    
	} else {
	    $g->add_node($node_id, label => $label);
	}

	# If there is already a child, link against it
	if ($last_child && $last_child ne $parent_handle) {
	    $g->add_edge($last_child => $node_id);
	    
	} else {
	    # Otherwise, just link it to the parent node
	    $self->add_edge($parent_handle);
	}

    }

    # If we do have a self handle
    if ($self_handle) {
	my $node_id = $self->nodelist_add($self_handle);
	my $label = $self->get_node_label($mapi_call, $key);

	if ($self->{highlight} && !$self->node_highlight($label)) {
	    print "############ HERE ##############\n";
	    $g->add_node($node_id, label => $label,
			 style => "filled",
			 color => "green");	    
	} else {
	    $g->add_node($node_id, label => $label);
	}
	$self->add_edge($parent_handle, $node_id);
    }
}

sub get_node_label
{
    my $self = shift;
    my $mapi_call = shift;
    my $key = shift;

    my $in = $mapi_call->{calls}->{REQ}->{$key};
    my $label = '';

    $label = sprintf "%s (%s)", $in->{opname}, $in->{opnum};

    return $label;
}

sub node_highlight
{
    my $self = shift;
    my $label = shift;

    return -1 if !$self->{highlight};

    my $label2 = substr($label, 0, length($self->{highlight}));

    if ($self->{highlight} eq $label2) {
	print "WE CAN HIGHLIGHT THE CALL\n";
	return 0;
    }
    return -1;
}

#########################################
# HANDLE CONVENIENT FUNCTIONS
#########################################

#
# get the parent handle (handle_idx field)
#
sub get_parent_handle
{
    my $self = shift;
    my $mapi_call = shift;
    my $key = shift;

    my $in = $mapi_call->{calls}->{REQ}->{$key};
    my @handles = @{$mapi_call->{handles_out}};

    my $parent_handle = $in->{handle_idx};
    return $handles[hex($parent_handle)];
}

#
# get the self handle (self_handle_idx field)
#
sub get_self_handle
{
    my $self = shift;
    my $mapi_call = shift;
    my $key = shift;

    my $in = $mapi_call->{calls}->{REQ}->{$key};
    my @handles = @{$mapi_call->{handles_out}};

    return 0 if (!exists $in->{self_handle_idx});

    my $self_handle = $in->{self_handle_idx};

    return $handles[hex($self_handle)];
}
