<?php
$mapi = new MAPIProfileDB("/home/jkerihuel/.openchange/profiles.ldb");
echo "=> MAPI Profile Database Path: '", $mapi->path(), "'\n";
$mapiProfile = $mapi->getProfile();
$session = $mapiProfile->logon();
$mailbox = $session->mailbox();


$inbox = $mailbox->inbox();
echo "Inbox item type " . $inbox->getFolderType() . "\n";

echo "=> Opening Contact Folder\n";
$contacts = $mailbox->contacts();

$messageId = '0x4B02130000000001';
$message = $contacts->openMessage($messageId);
echo "=> Message with message ID " . $message->getID() . " opened\n";

$email = $message->get(PidLidEmail1EmailAddress);
if ($email == "changed@a.org") {
   echo "=> [1] Set PidLidEmail1EmailAddress to jkerihuel@zentyal.com\n";
   $message->set(PidLidEmail1EmailAddress, "jkerihuel@zentyal.com");
} else if ($email == "jkerihuel@zentyal.com") {
   echo "=> [2] Set PidLidEmail1EmailAddress to changed@a.org\n";
   $message->set(PidLidEmail1EmailAddress, "changed@a.org");
} else {
  echo "Not expected value\n";
}

echo "=> SaveChangesMessage \n";
$message->save($mailbox);
unset($message);

echo "===================================================\n";

$messageId = '0x4B02130000000001';
$message = $contacts->openMessage($messageId);
echo "=> Message with message ID " . $message->getID() . " opened\n";

$email = $message->get(PidLidEmail1EmailAddress);
if ($email == "changed@a.org") {
   echo "=> [1] Set PidLidEmail1EmailAddress to jkerihuel@zentyal.com\n";
   $message->set(PidLidEmail1EmailAddress, "jkerihuel@zentyal.com");
} else if ($email == "jkerihuel@zentyal.com") {
   echo "=> [2] Set PidLidEmail1EmailAddress to changed@a.org\n";
   $message->set(PidLidEmail1EmailAddress, "changed@a.org");
} else {
  echo "Not expected value\n";
}

echo "=> SaveChangesMessage \n";
$message->save($mailbox);
unset($message);


?>
