<?php
$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");
echo "MAPI DB path: '", $mapi->path(), "'\n";


echo "Profile test\n";
$mapiProfile = $mapi->getProfile('test');


echo "Logon test profile\n";
$session = $mapiProfile->logon();

echo "Get mailbox\n";

$mailbox = $session->mailbox();
echo "Mailbox name "  . $mailbox->getName() . "\n";


$inbox = $mailbox->inbox();
echo "Inbox folder " . $inbox->getName() . "/" . $inbox->getFolderType() .  "\n";

$calendar = $mailbox->calendar();
echo "Calendar folder " . $calendar->getName() . "/" . $calendar->getFolderType() .  "\n";

$tasks = $mailbox->tasks();
echo "Tasks folder " . $tasks->getName() . "/" . $tasks->getFolderType() .  "\n";

$contacts = $mailbox->contacts();
echo "Contacts folder " . $contacts->getName() . "/" . $contacts->getFolderType() .  "\n";

$mapiId =    '0x8a010d0000000001'; # drafts folder
$drafts = $mailbox->openFolder($mapiId, "IPF.Note");
echo "Open by ID draft folder " . $drafts->getName() . "/" . $drafts->getFolderType() .  "\n";

$badMapiId = '0xFFFF0d0000000001'; # nothing folder
echo "Open folder with bad ID \n";
var_dump($mailbox->openFolder($badMapiId, "IPF.Note"));
echo "\n";

?>
