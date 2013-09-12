<?php
$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");
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

?>
