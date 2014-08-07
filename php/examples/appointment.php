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

include('./test-helpers.php');
include('./config.php');

# values to be set
$tagBody1 = "body1";
$tagBody2 = "body2";
$unixtime1 = 1380204166;
$unixtime2 = $unixtime1 + 100;


$mapi = new MAPIProfileDB($dbPath);
ok($mapi, "MAPIProfileDB open");
is($mapi->path(), $dbPath  , "MAPI DB correct path");

#$mapi->debug(true, 10);

$mapiProfile = $mapi->getProfile($profileName);
ok($mapiProfile, "Get profile $profileName");

$session = $mapiProfile->logon();
ok ($session, "Profile logon");

$mailbox = $session->mailbox();
ok ($mailbox, "Get default mailbox");
diag("Mailbox name "  . $mailbox->getName());

$calendar = $mailbox->calendar();
diag("Calendar folder " . $calendar->getName() . "/" . $calendar->getFolderType());
ok($calendar, "Get calendar");


$message = $calendar->openMessage($appointmentId, MAPIMessage::RW);
ok($message, "Appointment $appointmentId opened in RW mode");
is($message->getID(), $appointmentId, "Check opened message Id (msut be $appointment_id)");

diag("Changing PidTagBody to '$tagBody1'");
$message->set(PidTagBody, $tagBody1);

diag("Changing  PidLidAppointmentEndWhole to $unixtime1 (09/26/2013 @ 10:02am in EST.)");
$message->set(PidLidAppointmentEndWhole, $unixtime1);

$message->save();
unset($message);

diag("--After saving changes:");
$message = $calendar->openMessage($appointmentId, MAPIMessage::RW);
ok($message, "Reopen message in RW mode after saving changes");
is($message->get(PidTagBody), $tagBody1, "Checking that PidTagBody has correctly been changed");
is($message->get(PidLidAppointmentEndWhole), $unixtime1, "Checking that PidLidAppointmentEndWhole has correctly been changed");


diag("Changing PidTagBody again to $tagBody2");
$message->set(PidTagBody, $tagBody2);
diag("Changing  PidLidAppointmentEndWhole to 100 seconds more");
$message->set(PidLidAppointmentEndWhole, $unixtime2);
$message->save();
unset($message);

diag("--After last changes:");
$message = $calendar->openMessage($appointmentId, MAPIMessage::RO);
ok($message, "Reopen message in RO mode after saving the last changes");
is($message->get(PidTagBody), $tagBody2, "Checking that PidTagBody has changed again");
is($message->get(PidLidAppointmentEndWhole), $unixtime2, "Checking that PidLidAppointmentEndWhole has changed again");

# workaround against shutdown problem
unset($message);
#unset($calendar);
#unset($mailbox);
#unset($session);
#unset($mapiProfile);
#unset($mapi);

endTestSuite("appointment.php");

?>
