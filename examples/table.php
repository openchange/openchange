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

echo "CONTACTS\n\n";
$contacts = $mailbox->contacts();
echo "Contacts item type " . $contacts->getFolderType() . "\n";

echo "Contacts getMessageTable \n";
$table1 =  $contacts->getMessageTable();
echo "Get two messages\n";
var_dump($table1->getMessages(2));
echo "END end of messages\n";

echo "Get parent folder\n";
$parentFolder =   $table1->getParentFolder();
var_dump($parentFolder);
echo "Parent folder ID ". $parentFolder->getID() .  "\n";


echo "Contacts getMessageTable with properties PidLidFileUnder, PidLidEmail1EmailAddress \n";
$table2 =  $contacts->getMessageTable(PidLidFileUnder,
				      PidLidEmail1EmailAddress,
				      PidTagEmailAddress,
				      PidTagCompanyName,
				      PidTagDisplayName,
				      PidTagGivenName);

echo "Get 4 summary contacts\n";
var_dump($table2->summary(4));

echo "Get 1 summary contact\n";
var_dump($table2->summary(1));

echo "Get remaining summary contact\n";
var_dump($table2->summary());

?>
