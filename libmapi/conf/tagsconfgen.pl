#!/usr/bin/perl -w
## tagsconfgen.pl for tags conf generator
## 
## Made by Pauline KHUN
## Login   p.khun
## Mail   <p.khun@openchange.org>
## 
## Started on  Tue Nov 29 16:23:58 2005 Pauline KHUN
## Last update Tue Dec 20 15:18:49 2005 Pauline KHUN
##

use	strict;

## tagsconfgen.pl script generates a conf file for Nspi_profile torture test:
## - the conf file contains a Property tags per section and thus allows a test for
##   one tag.
## This script can be changed according to the torture test to do.

sub	TAGSCONF	{ return ("proptags.conf"); }
sub	TAGSFILE	{ return ("mapi-properties"); }

my	@line;

open(CONF, ">", TAGSCONF) or die('Cannot open ' . TAGSCONF . "\n");
open(FD, TAGSFILE) or die('Cannot open ' . TAGSFILE . "\n");
while (<FD>)
{
    if ($_ !~ /^\#/)
    {
	@line = split(/\p{IsSpace}/, $_);
	if (defined($line[2]))
	{
	    print CONF "[proptags]\n";
	    print CONF "$line[2] = $line[0]\n";
	}
    }
}
close(FD);
close(CONF);
