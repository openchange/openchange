<?php
$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");
echo "MAPI DB path: '", $mapi->path(), "'\n";

echo "Default profile\n";
$mapiProfile = $mapi->getProfile();

echo "Logon default profile\n";
$session = $mapiProfile->logon();

echo "Get mailbox\n";
$mailbox = $session->mailbox();


$inbox = $mailbox->inbox();
echo "Inbox item type " . $inbox->getFolderType() . "\n";


echo "CONTACTS\n\n";
$contacts = $mailbox->contacts();
echo "Contacts item type " . $contacts->getFolderType() . "\n";

$incorrectMessageId = '0xFFFF0E0000500001';
print "Get inexistent message: " .  var_dump($contacts->openMessage($incorrectMessageId)) . "\n";


$messageId = '0xA4010E0000000001';
$message = $contacts->openMessage($messageId);
echo "Getting message PidLidFileUnder\n";
echo $message->get(PidLidFileUnder) . "\n";
echo "Getting at the same time PidLidFileUnder and PidLidEmail1OriginalDisplayNamen";
$propValues = $message->get(PidLidFileUnder, PidLidEmail1OriginalDisplayName);
var_dump($propValues);


echo "Getting inexistent property\n";
echo $message->get(11) . "\n";
echo "Getting list of inexistent properties at the same time\n";
$propValues = $message->get(30, 456, 0, -1);
var_dump($propValues);

return;

echo "Change Email1OriginalDisplayName \n";
if ($message->Email1OriginalDisplayName == "changed@a.org") {
   $message->Email1OriginalDisplayName = "jkerihuel@zentyal.com";
} else if ($message->Email1OriginalDisplayName == "jkerihuel@zentyal.com") {
  $message->Email1OriginalDisplayName = "changed@a.org";
}

echo "Value:\n ";
echo var_dump($message->Email1OriginalDisplayName) . "\n";

echo "Save changes \n";
$message->save($mailbox);

echo "Check after save: " . $message->Email1OriginalDisplayName . "\n";

echo "END SAVE\n\n";

?>
