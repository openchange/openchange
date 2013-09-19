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



echo "Check one-argument prop set: setting PidTagDisplayName to 'juju' and PidTagDescription to 'descChanged'\n";
$message->set(PidTagDisplayName, 'juju');
echo "PidTagDisplayName value after set: " . $message->get(PidTagDisplayName) . "\n";
$message->set(PidTagBody, 'descChanged');
echo "PidTagBody value after set: " . $message->get(PidTagBody) . "\n";

echo "Check set of binary field\n";
$binaryValue1 = array(5, 55, 255);
$message->set(PidLidContactLinkedGlobalAddressListEntryId, $binaryValue1);
echo "Binary value: " . $message->get(PidLidContactLinkedGlobalAddressListEntryId) . "\n";


echo "Set PidLidContactCharacterSet to 33";
$message->set(PidLidContactCharacterSet, 33);


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
echo "Get contact link: " . $message->get(PidLidContactCharacterSet) . "\n";


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
echo "Setting contact link to 14\n";
$message->set(PidLidContactCharacterSet, 14);
echo "Set of binary field to all zeros\n";
$binaryValue2 = array(0, 0, 0);
$message->set(PidLidContactLinkedGlobalAddressListEntryId, $binaryValue2);
echo "Binary value: " . $message->get(PidLidContactLinkedGlobalAddressListEntryId) . "\n";



echo "=> SaveChangesMessage \n";
$message->save();
unset($message);

?>
