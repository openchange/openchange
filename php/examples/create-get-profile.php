<?php
/*
   OpenChange PHP bindings examples

   Copyright (C) 2013-2014 Zentyal S.L.

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

include('./test-helpers.php');

# environment settings
$pathDB = "/home/jag/.openchange/profiles.ldb";
$profileName = 'new15';
$username='u2';
$password = 'a';
$domain = 'zentyal-domain';
$realm = 'zentyal-domain.lan';
$server =  '192.168.56.51';

$mapiDB = new MAPIProfileDB($pathDB);
ok($mapiDB, "MAPIProfileDB opened for $pathDB");

$profile = $mapiDB->createAndGetProfile($profileName, $username, $password, $domain, $realm, $server);
ok($profile, "createAndGet profile for $profileName");

$session = $profile->logon();
ok($session, "Check logon with profile $profileName");

endTestSuite("create-get-profile.php");
?>