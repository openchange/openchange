<?php
# System dependent varialbes:
$path = "/home/jag/.openchange/profiles.ldb";
$profileName = 'test';
$contactMessageId = '0xA4010E0000000001';
# END system dependent vatriables


$mapi = new MAPIProfileDB($path);
echo "=> MAPI Profile Database Path: '", $mapi->path(), "'\n";
$mapiProfile = $mapi->getProfile($profileName);
$session = $mapiProfile->logon();
$mailbox = $session->mailbox();


$inbox = $mailbox->inbox();
echo "Inbox item type " . $inbox->getFolderType() . "\n";

echo "=> Opening Contact Folder\n";
$contacts = $mailbox->contacts();

$roContactId = "A5010E0000000001";
$roContact = $contacts->openMessage($contactMessageId, 0);

echo "get PidTagGivenName\n";
$retGet = $roContact->get(PidTagGivenName);
var_dump($retGet);

echo "get various properties at the same time\n";
$retMultipleGet = $roContact->get(PidLidEmail1EmailAddress,
				  PidTagEmailAddress,
				  PidTagCompanyName,
				  PidTagDisplayName,
				  PidTagGivenName);
var_dump($retMultipleGet);


$message = $contacts->openMessage($contactMessageId, 1);
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
$message->save();
#unset($message); # XXX does not work! probably for destructor

echo "===================================================\n";


$message = $contacts->openMessage($contactMessageId, 1);
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
$message->save();
unset($message);

echo "=================\n";
echo "Create new contact\n";
$newContact = $contacts->createMessage(PidLidEmail1EmailAddress, 'new@contacts.org');
var_dump($newContact);

print "New contact ID " . $newContact->getID() . "\n";
unset($newContact);
?>
