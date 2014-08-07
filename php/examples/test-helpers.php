<?php
/*
   OpenChange PHP bindings examples

   Copyright (C) 2013-2014 Javier Amor Garcia

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

function __testOk($msg) {
	    echo "OK: " . $msg . "\n";
}

function __testFailed($msg) {
	   echo("Failed: " . $msg . "\n");
	   exit(1);
}

function ok($expr, $msg) {
	 if ($expr) {
	    __testOk($msg);
         } else {
	   __testFailed($msg);
         }
}

function is($a, $b, $msg) {
	 if (! $msg) {
	    $msg = "$a == $b";
	 }
	 if ($a == $b) {
	    __testOk($msg);
	 } else {
	   __testFailed("$msg (expected $b but get $a)");
	 }
}

function diag($msg) {
	 echo "# $msg\n";
}

function endTestSuite($msg) {
    diag("END test suite: $msg");
}

?>