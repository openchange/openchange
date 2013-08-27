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
echo "Inbox item type " . $inbox->getItemType() . "\n";

$calendar = $mailbox->calendar();
echo "Calendar item type " . $calendar->getItemType() . "\n";

$contacts = $mailbox->contacts();
echo "Contacts item type " . $contacts->getItemType() . "\n";

$tasks = $mailbox->tasks();
echo "Tasks item type " . $tasks->getItemType() . "\n";

?>
