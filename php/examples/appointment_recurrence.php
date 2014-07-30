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

diag("Test disabled for now");
exit(0);

# System dependent varialbes:
$path = "/home/jag/.openchange/profiles.ldb";
$profileName = 'miky';
$eventId = "0x3A08010000000001";

# END system dependent variables




$mapi = new MAPIProfileDB($path);
echo "=> MAPI Profile Database Path: '", $mapi->path(), "'\n";
$mapiProfile = $mapi->getProfile($profileName);
$session = $mapiProfile->logon();
$mailbox = $session->mailbox();


$inbox = $mailbox->inbox();
echo "=> Opening calensdar Folder\n";
$calendar = $mailbox->calendar();

$event = $calendar->openMessage($eventId);
$recur = $event->getRecurrence(PidLidAppointmentRecur);

var_dump($recur);

?>