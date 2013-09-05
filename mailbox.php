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

#echo "Destroying mapi db object\n";
#unset($mapi);

echo "Logon default profile\n";
$session = $mapiProfile->logon();
var_dump($session);



echo "Get mailbox\n";

$mailbox = $session->mailbox();
var_dump($mailbox);
echo "Mailbox name "  . $mailbox->getName() . "\n";


#$inbox = $mailbox->inbox();
#echo "Inbox item type " . $inbox->getFolderType() . "\n";

#echo "CALENDAR\n\n";
#
#$calendar = $mailbox->calendar();
#$calendarTable = $calendar->getMessageTable();
#echo "Unset calendar\n";
#$count = $calendarTable->count();
#while ($count >0) {
#      echo "Next 3 summary :" . var_dump($calendarTable->summary(3)) . "\n";
#      $count -= 3;
#}
#
#echo "END CALENDAR\n\n";


#echo "TASKS\n\n";
#$tasks = $mailbox->tasks();
#echo "Tasks item type " . $tasks->getFolderType() . "\n";
#$taskId = '0x2506130000000001';
#$task = $tasks->openMessage($taskId);
#echo "Task status: "  . var_dump($task->TaskStartDate);
#
#echo "END TASKS\n\n";



echo "CONTACTS\n\n";
$contacts = $mailbox->contacts();
echo "Contacts item type " . $contacts->getFolderType() . "\n";
$messageId = '0xA4010E0000000001';
$message = $contacts->openMessage($messageId);
echo var_dump($message->FileUnder);
echo var_dump($message->Email1OriginalDisplayName);


echo "Contacts getMessageTable: ";
$table =  $contacts->getMessageTable();
$table2 = $contacts->getMessageTable();
echo var_dump($table) . "\n";
echo "Contacts summary\n" .  var_dump($table->summary()) . "\n";
$contactMessages = $table->getMessages();
echo "Table2 cotnact messages " . var_dump($contactMessages) . "\n";



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

echo "END CONTACTS\n\n";

echo "\n\nTASKS";
$tasks = $mailbox->tasks();
echo "Tasks item type " . $tasks->getFolderType() . "\n";
$taskTable = $tasks->getMessageTable();
$taskObjects = $taskTable->getMessages();
echo "Messages get from table: " . var_dump($taskObjects) . "\n";
echo "END TASKS\n\n";

?>
