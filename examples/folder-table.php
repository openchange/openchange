<?php
$dbPath = "/home/jag/.openchange/profiles.ldb";


$mapi = new MAPIProfileDB($dbPath);
echo "MAPI DB path: '", $mapi->path(), "'\n";

echo "Default profile\n";
$mapiProfile = $mapi->getProfile();

echo "Logon default profile\n";
$session = $mapiProfile->logon();

echo "Get mailbox\n";
$mailbox = $session->mailbox();
#var_dump($mailbox);
echo "Mailbox name "  . $mailbox->getName() . "\n";

echo "Get inbox\n";
$inbox = $mailbox->inbox();
echo "Inbox item type " . $inbox->getFolderType() . "\n";



echo "Get folder table\n";
$table1 = $inbox->getFolderTable();
var_dump($table1);

echo "Get parent folder\n";
$parentFolder =   $table1->getParentFolder();
var_dump($parentFolder);
echo "Parent folder ID ". $parentFolder->getID() .  "\n";

echo "Get folders from table\n";
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



?>
