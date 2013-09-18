<?php
# System dependent varialbes:
$path = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
$contactMessageId = '0x7500000000000001';
$roContactId = $contactMessageId;
# END system dependent variables


$mapi = new MAPIProfileDB($path);
echo "=> MAPI Profile Database Path: '", $mapi->path(), "'\n";
$mapiProfile = $mapi->getProfile($profileName);
$session = $mapiProfile->logon();
$mailbox = $session->mailbox();


$inbox = $mailbox->inbox();
echo "Inbox item type " . $inbox->getFolderType() . "\n";

echo "=> Opening Contact Folder\n";
$contacts = $mailbox->contacts();


$roContact = $contacts->openMessage($contactMessageId, MAPIMessage::RO);

echo "get PidTagGivenName\n";
$retGet = $roContact->get(PidTagGivenName);
var_dump($retGet);
unset($retGet);



echo "get various properties at the same time\n";
$retMultipleGet = $roContact->get(PidLidEmail1EmailAddress,
				  PidTagEmailAddress,
				  PidTagCompanyName,
				  PidTagDisplayName,
				  PidTagGivenName);
foreach ($retMultipleGet as $prop) {
	unset($prop);
}
var_dump($retMultipleGet);


$message = $contacts->openMessage($contactMessageId,  MAPIMessage::RW);
echo "=> Message with message ID " . $message->getID() . " opened\n";
echo "PidLidEmail1EmailAddress " .$message->get(PidLidEmail1EmailAddress) . " \n";
echo "Displayname " . $message->get(PidTagDisplayName) . "\n";



echo "Check one-argument prop set: setting PidTagDisplayName to 'juju'\n";
$message->set(PidTagDisplayName, 'juju');
echo "Value after set: " . $message->get(PidTagDisplayName) . "\n";


$email = $message->get(PidLidEmail1EmailAddress);
if ($email == "changed@a.org") {
   echo "=> [1] Set PidLidEmail1EmailAddress to jkerihuel@zentyal.com\n";
   $message->set(PidLidEmail1EmailAddress, "jkeriheuel@zentyal.com", PidTagDisplayName, 'julien');
} else if ($email == "jkerihuel@zentyal.com") {
   echo "=> [2] Set PidLidEmail1EmailAddress to changed@a.org\n";
   $message->set(PidLidEmail1EmailAddress, "changed@a.org", PidTagDisplayName, 'changedD');
} else {
  echo "Not expected value\n";
   $message->set(PidLidEmail1EmailAddress, "jkerihuel@zentyal.com", PidTagDisplayName, 'julien');
}


echo "=> SaveChangesMessage \n";
$message->save();
unset($message);

echo "===================================================\n";


$message = $contacts->openMessage($contactMessageId, 1);
echo "=> Message with message ID " . $message->getID() . " opened\n";
echo "PidLidEmail1EmailAddress " .$message->get(PidLidEmail1EmailAddress) . " \n";
echo "Displayname " . $message->get(PidTagDisplayName) . "\n";

$email = $message->get(PidLidEmail1EmailAddress);
echo "Reverting changes\n";
if ($email == "changed@a.org") {
   echo "=> [1] Set PidLidEmail1EmailAddress to jkerihuel@zentyal.com\n";
   $message->set(PidLidEmail1EmailAddress, "jkerihuel@zentyal.com", PidTagDisplayName, 'julien');
} else if ($email == "jkerihuel@zentyal.com") {
   echo "=> [2] Set PidLidEmail1EmailAddress to changed@a.org\n";
   $message->set(PidLidEmail1EmailAddress, "changed@a.org", PidTagDisplayName, 'changedD');
} else {
  echo "Not expected value\n";
   $message->set(PidLidEmail1EmailAddress, "jkerihuel@zentyal.com", PidTagDisplayName, 'julien');
}



echo "=> SaveChangesMessage \n";
$message->save();
unset($message);

?>
