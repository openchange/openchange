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
$mapi = new MAPIProfileDB($path);
ok($mapi, "Opened MAPIProfile with path $path");
$mapiProfile = $mapi->getProfile($profileName);
ok($mapiProfile, "Get profile $profileName");
$session = $mapiProfile->logon();
ok($session, "Logon with profile $profileName");
$mailbox = $session->mailbox();
ok($mailbox, "Default mailbox");
$contacts = $mailbox->contacts();
ok($contacts, "Open contacts folder");

$ids = array();

$newContact = $contacts->createMessage(PidLidEmail1EmailAddress, 'first_new@contacts.org', PidTagCompanyName, 'to_delete_org', PidTagDisplayName, 'first');
ok($newContact, "Created new contact");
is(get_class($newContact), "MAPIContact", "Check returned new class type");
$ids[] = $newContact->getID();
unset($newContact);

$newContact = $contacts->createMessage(PidLidEmail1EmailAddress, 'second_new@contacts.org', PidTagCompanyName, 'to_delete_org', PidTagDisplayName, 'second');
ok($newContact, "Created a second new contact");
is(get_class($newContact), "MAPIContact", "Check returned new class type for second new contact");
$ids[] = $newContact->getID();
unset($newContact);

foreach ($ids as $newId) {
    $retrieved = $contacts->openMessage($newId);
    ok($retrieved, "Check that we can retrieve a new created contact by its ID=$newId");
    is(get_class($retrieved), "MAPIContact", "Check class type for retrieved contact");
    is($retrieved->getID(), $newId, "Check ID for retrieved contact");
    unset($retrieved);
}

diag ("Delete created contacts");
call_user_func_array(array($contacts, 'deleteMessages'), $ids);

foreach ($ids as $newId) {
    $retrieved = $contacts->openMessage($newId);
    ok(is_null($retrieved), "Check that removed contact $newId has been removed");
}

diag("Try to delete inexistent contact. XXX does not return error because SOGO limitation");
$contacts->deleteMessages($inexistentContactId);

endTestSuite("new_message.php");

?>
