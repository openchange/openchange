<?php
$dbPath = "/home/jag/.openchange/profiles.ldb";
$profileName = "u2";
$draftFolderId =  "0x3d00000000000001";
$inexistentFolderId = '0xFFFF0d0000000001';


$mapi = new MAPIProfileDB($dbPath);
echo "MAPI DB path: '", $mapi->path(), "'\n";


echo "Profile test\n";
$mapiProfile = $mapi->getProfile($profileName);


echo "Logon test profile\n";
$session = $mapiProfile->logon();

echo "Get mailbox\n";

$mailbox = $session->mailbox();
echo "Mailbox name "  . $mailbox->getName() . "\n";


$inbox = $mailbox->inbox();
echo "Inbox folder " . $inbox->getName() . "/" . $inbox->getFolderType() . "/" . $inbox->getID() .  "\n";

$calendar = $mailbox->calendar();
echo "Calendar folder " . $calendar->getName() . "/" . $calendar->getFolderType() . "/" . $calendar->getID() . "\n";

$tasks = $mailbox->tasks();
echo "Tasks folder " . $tasks->getName() . "/" . $tasks->getFolderType() . "/" . $tasks->getID() .  "\n";

$contacts = $mailbox->contacts();
echo "Contacts folder " . $contacts->getName() . "/" . $contacts->getFolderType() . "/" . $contacts->getID() . "\n";


#$drafts = $mailbox->openFolder($draftFolderId, "IPF.Note");
#echo "Open by ID draft folder " . $drafts->getName() . "/" . $drafts->getFolderType() . "/" . $drafts->getID() . "\n";


echo "Open folder with bad ID \n";
var_dump($mailbox->openFolder($inexistentFolderId, "IPF.Note"));
echo "\n";





?>
