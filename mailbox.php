<?php
$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");
echo "MAPI DB path: '", $mapi->path(), "'\n";

$allProfiles = $mapi->profiles();
var_dump($allProfiles);

echo "Profile test\n";
$mapiProfile = $mapi->getProfile('test');
var_dump($mapiProfile);
$profileDump = $mapiProfile->dump();
var_dump($profileDump);

echo "Default profile\n";
$mapiProfile = $mapi->getProfile();
var_dump($mapiProfile);
$profileDump = $mapiProfile->dump();
var_dump($profileDump);

echo "Logon default profile\n";
$session = $mapiProfile->logon();
var_dump($session);

echo "Get mailbox\n";

$mailbox = $session->mailbox();
var_dump($mailbox);
echo "Mailbox name "  . $mailbox->getName() . "\n";

$inbox = $mailbox->inbox();
echo "Inbox item type " . $inbox->getFolderType() . "\n";

$calendar = $mailbox->calendar();
echo "Calendar item type " . $calendar->getFolderType() . "\n";

$contacts = $mailbox->contacts();
echo "Contacts item type " . $contacts->getFolderType() . "\n";
$messageId = '0xA4010E0000000001';
$message = $contacts->openMessage($messageId);
echo var_dump($message->FileUnder);
echo var_dump($message->Email1OriginalDisplayName);

$tasks = $mailbox->tasks();
echo "Tasks item type " . $tasks->getFolderType() . "\n";

?>
