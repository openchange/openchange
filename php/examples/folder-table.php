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

$mapi = new MAPIProfileDB($dbPath);
ok($mapi, "MAPIProfileDB for $dbPath");

$mapiProfile = $mapi->getProfile();
ok($mapiProfile, "Profile $profile");

$session = $mapiProfile->logon();
ok($session, "Logon for profile $profile");

$mailbox = $session->mailbox();
ok($mailbox, "Get default mailbox");


$inbox = $mailbox->inbox();
ok($inbox,"Get inbox");
diag("Inbox item type " . $inbox->getFolderType());
$inboxId = $inbox->getID();

$table1 = $inbox->getFolderTable();
ok($table1, "Get folder table for inbox");


$parentFolder =   $table1->getParentFolder();
ok($parentFolder, "Get parent folder for table");
is($parentFolder->getID(), $inboxId, "Checking by ID that parent is the inbox");
ok(($table1->count() > 0), "Check that table is not empty");

# XXX continue whern we have folders..

$folders = $table1->getFolders();
var_dump($folders);
echo "First folder ID " . $folders[0]->getID() . "\n";

echo "Get othe folder table\n";
$table2 = $inbox->getFolderTable();
var_dump($table2);
echo "Folder table summary\n";
var_dump($table2->summary());

unset($table2);
foreach ($folders as $one_folder) {
	unset($one_folder);
}
unset($folders);
unset($table1);
unset($info);
unset($parentFolder);
unset($mailbox);
unset($session);
unset($mapiProfile);
unset($mapi);

endTestSuite("folder-table.php");

?>
