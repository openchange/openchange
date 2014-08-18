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

$pidTagBody1 = 'body1';
$pidTagDisplayName1 = 'displayName1';
$pidLidWorkAddressStreet1 = 'street1';
$pidLidEmail1EmailAddress1 = 'jk1@example1.org';
$pidTagBody2 = 'body2';
$pidTagDisplayName2 = 'displayName2';
$pidLidWorkAddressStreet2 = 'street2';
$pidLidEmail1EmailAddress2 = 'jk2@example2.org';

$mapi = new MAPIProfileDB($dbPath);
ok($mapi, "MAPIProfileDB opened from $dbPath");

$mapiProfile = $mapi->getProfile($profileName);
ok($mapiProfile, "Profile $profileName");

$session = $mapiProfile->logon();
ok($session, "Logon with $profileName");

$mailbox = $session->mailbox();
ok($mailbox, "Default mailbox for $profileName");

$contacts = $mailbox->contacts();
ok($contacts, "Opening contact folder");

ok(is_null($contacts->openMessage($inexistentMesageId)), "Trying to open a inexistent message return NULL");

$message = $contacts->openMessage($contactMessageId,  MAPIMessage::RW);
ok($message, "Open message RW");
is($message->getID(), $contactMessageId, "Checking opening message ID");
is($message->getBodyContentFormat(), "txt", "Checking body content format");

diag("Setting multiple messages properties to first set");
$message->set(
	PidTagBody, $pidTagBody1,
	PidLidWorkAddressStreet, $pidLidWorkAddressStreet1,
	PidLidEmail1EmailAddress, $pidLidEmail1EmailAddress1
);

diag("Setting one property PidTagDisplayName to first set");
$message->set(PidTagDisplayName, $pidTagDisplayName1);

$message->save();
unset($message);

diag ("- Saving changes");
$message = $contacts->openMessage($contactMessageId, MAPIMessage::RW);
ok($message, "Reopening message in RW mode");

is($message->get(PidTagDisplayName), $pidTagDisplayName1, "Checking PidTagDisplayName1 has changed properly");
$multipleValues = $message->get(PidTagBody, PidLidWorkAddressStreet, PidLidEmail1EmailAddress);
is($multipleValues[PidTagBody], $pidTagBody1, "Checking value as PidTagBody (retrieved in a multiple value get)");
is($multipleValues[PidLidWorkAddressStreet], $pidLidWorkAddressStreet1, "Checking value as PidLidWorkAddressStreet (retrieved in a multiple value get)");
is($multipleValues[PidLidEmail1EmailAddress], $pidLidEmail1EmailAddress1, "Checking value as PidLidEmail1EmailAddress (retrieved in a multiple value get)");

diag("Setting values again");
$message->set(
	PidTagBody , $pidTagBody2,
	PidLidWorkAddressStreet , $pidLidWorkAddressStreet2,
	PidLidEmail1EmailAddress , $pidLidEmail1EmailAddress2
);
$message->set(PidTagDisplayName, $pidTagDisplayName2);

$message->save();
unset($message);

diag("- Saving last changes");

$message = $contacts->openMessage($contactMessageId, MAPIMessage::RO);
ok($message, "Reopening message in RO mode");

is($message->get(PidTagDisplayName), $pidTagDisplayName2, "Checking PidTagDisplayName2 has changed properly");
$multipleValues = $message->get(PidTagBody, PidLidWorkAddressStreet, PidLidEmail1EmailAddress);
is($multipleValues[PidTagBody], $pidTagBody2, "Checking value as PidTagBody (retrieved in a multiple value get)");
is($multipleValues[PidLidWorkAddressStreet], $pidLidWorkAddressStreet2, "Checking value as PidLidWorkAddressStreet (retrieved in a multiple value get)");
is($multipleValues[PidLidEmail1EmailAddress], $pidLidEmail1EmailAddress2, "Checking value as PidLidEmail2EmailAddress (retrieved in a multiple value get)");


unset($message);

endTestSuite("message.php");

?>
