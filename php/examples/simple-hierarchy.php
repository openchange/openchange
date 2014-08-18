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

$mapi = new MAPIProfileDB($pathDB);
ok($mapi, "MAPIProfileDB open for path $pathDB");
is($mapi->path(), $pathDB, "MAPI DB correct path");

$inexistent = $mapi->getProfile("idonotexist_232");
ok(is_null($inexistent), "Checking that trying to get inexistent profile return NULL");

$mapiProfile = $mapi->getProfile($profileName);
ok($mapiProfile, "Getting profile $profileName");

$session = $mapiProfile->logon();
ok($session, "Logon with profile $profileName");

$mailbox = $session->mailbox();
ok($mailbox, "Get default mailbox");
diag("Mailbox name "  . $mailbox->getName());

$contacts = $mailbox->contacts();
ok($contacts, "Get contacts folder");

$messageTable = $contacts->getMessageTable();
ok ($messageTable, "Get message table from contacts folder");
ok ($messageTable->count() > 0, "Checking that message table contains messages");

$message = $contacts->openMessage($messageId);
ok($message, "Get message with ID $messageId");
is($message->getID(), $messageId, "Check opened message Id (must be $messageId)");

#unset($mapi); # do not work..
#unset($profile);
#unset($session);
#unset($mailbox);
#unset($contacts);
#unset($messages);

endTestSuite("simple-hierarchy.php");
?>
