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

#$inbox = $mailbox->inbox();
#echo "Inbox item type " . $inbox->getFolderType() . "\n";
#
#$calendar = $mailbox->calendar();
#echo "Calendar item type " . $calendar->getFolderType() . "\n";


#$tasks = $mailbox->tasks();
#echo "Tasks item type " . $tasks->getFolderType() . "\n";
#$taskId = '0x2506130000000001';
#$task = $tasks->openMessage($taskId);
#echo "Task status: "  . var_dump($task->TaskStartDate);
#
$contacts = $mailbox->contacts();
echo "Contacts item type " . $contacts->getFolderType() . "\n";
$messageId = '0xA4010E0000000001';
$message = $contacts->openMessage($messageId);
echo var_dump($message->FileUnder);
echo var_dump($message->Email1OriginalDisplayName);


echo "Contacts getMessageTable: ";
$table =  $contacts->getMessageTable();
echo var_dump($table) . "\n";
echo "Contacts summary\n" .  var_dump($table->summary()) . "\n";


#echo "Change Email1OriginalDisplayName \n";
#if ($message->Email1OriginalDisplayName == "changed@a.org") {
#   $message->Email1OriginalDisplayName = "jkerihuel@zentyal.com";
#} else if ($message->Email1OriginalDisplayName == "jkerihuel@zentyal.com") {
#  $message->Email1OriginalDisplayName = "changed@a.org";
#}
#
#echo "Value:\n ";
#echo var_dump($message->Email1OriginalDisplayName) . "\n";
#
#echo "Save changes \n";
#$message->save($mailbox);
#
#echo "Check after save: " . $message->Email1OriginalDisplayName . "\n";

#$tasks = $mailbox->tasks();
#echo "Tasks item type " . $tasks->getFolderType() . "\n";

?>
