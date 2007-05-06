#!/usr/bin/perl -w
## regress.pl for  in /home/texane/wip/openchange/svn/MAILOOK.last/private/regress
## 
## Made by texane
## Login   <texane@gmail.com>
## 
## Started on  Tue Mar  6 11:51:25 2007 texane
## Last update Fri Apr  6 00:02:12 2007 texane
##


# todos
# -> generate an output accordingly to the base file
# -> generate an output accordingly to the reference file


use XML::Simple;
use strict;


# configuration


# units
sub units_new
{
    my $filename = $_[0];

    # load units.xml file
    my %units = ('file' => $filename,
		 'xml' => XMLin($filename, ForceArray => 1, KeepRoot => 1)
		 );
    return \%units;
}

sub units_report
{
    my $units = $_[0]->{'xml'}{'units'}[0]{'unit'};

    print "\n";
    print "/* unit tests\n";
    print " */\n";

    # all units
    foreach my $u (@$units)
    {
	print "<" . $u->{'name'}[0] . ">\n";

	# all tests
	foreach my $t (@{$u->{'tests'}[0]{'test'}})
	{
	    # test passed?
	    my $pass = '[x]';
	    $pass = '[!]' unless $t->{'pass'}[0] =~ /true/;

	    # previous test passed (or new one)
	    my $prev_pass = '[ ]';
	    if (exists($t->{'prev_pass'}[0]))
	    {
		$prev_pass = '[x]';
		$prev_pass = '[!]' unless $t->{'prev_pass'}[0] =~ /true/;
	    }

	    # report
	    my $spaces = ' ' x (40 - length($t->{'id'}[0]));
	    my $report = "   <" . $t->{'id'}[0] . ">$spaces $prev_pass -> $pass";
	    ($report .= '   /* ' . $t->{'file'}[0] . ',' . $t->{'line'}[0] .  ': ' . $t->{'reason'}[0] . ' */') unless ($pass eq '[x]');
	    print "$report\n";
	}
	print "\n";
    }

}


# regress main type
sub regress_new
{
    my %regress = ( 'info' => info_new(),
		    'profile' => undef,
		    'ixml' => undef,
		    'oxml' => undef,
		    'units' => undef
		   );

    # parse cmdline
    regress_process_cmdline(\%regress);
    $regress{'units'} = units_new($regress{'ixml'});

    return \%regress;
}

sub regress_process_cmdline
{
    my $regress = $_[0];

    # defaults
    $regress->{'ixml'} = 'units.xml';
    $regress->{'profile'} = 'test';

    # process
    my $cn = $#ARGV + 1;
    my $s = '';
    my $val = '';
    for (my $i = 0; $i < $cn; ++$i)
    {
	$s = $ARGV[$i++];
	help() && exit(-1) if ($i == $cn);
	$val = $ARGV[$i];
	if ($s eq '-p')
	{
	    $regress->{'profile'} = $val;
	}
	elsif ($s eq '-i')
	{
	    $regress->{'ixml'} = $val;
	}
	elsif ($s eq '-o')
	{
	    $regress->{'oxml'} = $val;
	}
	else
	{
	    help() && exit(-1);
	}
    }

    # checks
    help() && exit(-1) unless defined($regress->{'profile'});
    help() && exit(-1) unless defined($regress->{'ixml'});

    # more checks
    print "[!] invalid -ixml file\n" && exit(-1) unless (-e $regress->{'ixml'});
}

sub regress_report
{
    my $regress = $_[0];

    print "\n";
    print '-' x 60;
    print "\n";
    print '-' x 60;
    print "\n";
    print "/* regress run options\n";
    print " */\n";
    my $spaces = ' ' x (20 - length('<profile>'));
    print "<profile>$spaces" . $regress->{'profile'} . "\n" if defined($regress->{'profile'});
    $spaces = ' ' x (20 - length('<oxml>'));
    print "<oxml>$spaces" . $regress->{'oxml'} . "\n" if defined($regress->{'oxml'});
    $spaces = ' ' x (20 - length('<ixml>'));
    print "<ixml>$spaces" . $regress->{'ixml'} . "\n" if defined($regress->{'ixml'});

    info_report($regress->{'info'});
    units_report($regress->{'units'});

    regress_do_oxml($regress) if defined($regress->{'oxml'});
}

sub update_test_byid
{
    # given an id, update info
    # in the reference test node

    my ($tests, $id, $desc, $pass, $file, $line, $reason) = @_;

    # find the test node by id
    foreach my $t (@$tests)
    {
	# update the information
	if ($t->{'id'}[0] eq $id)
	{
	    # save the previous ones
	    $t->{'prev_desc'}[0] = $t->{'desc'}[0];
	    $t->{'prev_pass'}[0] = $t->{'pass'}[0];
	    $t->{'prev_file'}[0] = $t->{'file'}[0];
	    $t->{'prev_line'}[0] = $t->{'line'}[0];
	    $t->{'prev_reason'}[0] = $t->{'reason'}[0];

	    # store the new ones
 	    $t->{'desc'}[0] = $desc;
 	    $t->{'pass'}[0] = $pass;
 	    $t->{'file'}[0] = $file;
 	    $t->{'line'}[0] = $line;
 	    $t->{'reason'}[0] = $reason;

	    return ;
	}
    }

    # vivify
    my $i = $#{$tests} + 1;
    ${$tests}[0] = () if ($i eq 0);

    # not found, create new test
    ${$tests}[$i] = ({ 'id' => [ $id ],
		       'desc' => [ $desc ],
		       'pass' => [ $pass ],
		       'file' => [ $file ],
		       'line' => [ $line ],
		       'reason' => [ $reason ]
		       });
}

sub regress_do_oxml
{
    my $regress = $_[0];

    open my $f_out, '>', $regress->{'oxml'} or die('cannot open ' . $regress->{'oxml'});
    my $xml = $regress->{'units'}{'xml'};
    XMLout($xml, RootName => undef, OutputFile => $f_out );
    close($f_out);
}


sub regress_do
{
    my $regress = $_[0];
    my $units = $regress->{'units'}{'xml'}{'units'}[0]{'unit'};

    # all units
    foreach my $u (@$units)
    {
	my $oc_test_out='/tmp/.oc.regress';

	# delete the file if exists
	unlink($oc_test_out) if ( -e $oc_test_out );

	# execute smbtorture
	my $cmdline = 'smbtorture -U% ncacn_ip_tcp:192.168.0.0[]';
	$cmdline .= " --option=\"openchange:oc_test_out=$oc_test_out\"";
	$cmdline .= ' --option="mapi:profile_store=/tmp/profiles.ldb"';
	$cmdline .= ' --option="mapi:profile=' . $regress->{'profile'} . '"';
# fixme: $cmdline .= ' ' . $u->{'args'}[0];
	$cmdline .= ' ' . $u->{'name'}[0];
	$cmdline .= ';';
	system($cmdline);

	# load xml tree
	next unless ( -e $oc_test_out );
	my $xml = XMLin($oc_test_out, ForceArray => 1, KeepRoot => 1);

	# check tree
	next unless ( $xml && exists($xml->{'tests'}) );

	foreach my $t (@{$xml->{'tests'}[0]{'test'}})
	{
	    my $id = $t->{'id'}[0];
	    my $desc = $t->{'desc'}[0];
	    my $pass = $t->{'pass'}[0];
	    my $file = $t->{'file'}[0];
	    my $line = $t->{'line'}[0];
	    my $reason = $t->{'reason'}[0];

	    # create the key if doesn't exist
	    $u->{'tests'}[0]{'test'} = [] if (!exists($u->{'tests'}[0]{'test'}));

	    # update previous results
 	    update_test_byid($u->{'tests'}[0]{'test'}, $id, $desc, $pass, $file, $line, $reason);
	}
    }
    
}


# system related info
sub info_new
{
    my %info = ( 'plateform' => 'unknown',
		 'user' => 'unknown'
		 );
    info_collect(\%info);
    return \%info;
}

sub info_report
{
    my $info = $_[0];

    print "\n";
    print "/* environment\n";
    print " */\n";
    my $spaces = ' ' x (20 - length('<plateform>'));
    print "<plateform>$spaces". $info->{'plateform'} . "\n";
    $spaces = ' ' x (20 - length('<user>'));
    print "<user>$spaces". $info->{'user'} . "\n";
    print "\n";
}

sub info_collect
{
    my $info = $_[0];

    # collect plateform info
    my $out = `uname -mp`;
    chomp($out);
    $info->{'plateform'} = $out;
    $info->{'user'} = $ENV{'USER'};
}


# help
sub help
{
    print "[?] openchange regression script\n";
    print "    usage: ./regress.pl\n";
    print "           -p <profile>  # specify the profile to use (required) \n";
    print "           -i <filename> # xml reference input file\n";
    print "           -o <filename> # xml output file\n";
}


# entry point
sub main
{
    my $regress = regress_new();
    regress_do($regress);
    regress_report($regress);
}

main();
