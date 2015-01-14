#!/usr/bin/perl
# Copyright (C) 2013 Javier Amor Garcia
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2, as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

use strict;
use warnings;

my @phpFiles  = glob("*.php");
my @failed;
foreach my $file (@phpFiles) {
    print "Executing $file..\n";
    system "php -f $file";
    if ($? != 0 ) {
	print "$file FAILED\n";
	push @failed, $file;
    }
}
if (@failed) {
    print scalar(@failed) . " files of " . scalar @phpFiles . " failed:\n";
    foreach my $file (@failed) {
	print "\t$file\n";
    }
} else {
    print "All files ok\n";
}

1;
