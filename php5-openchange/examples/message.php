<?php
# System dependent varialbes:
$path = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
$contactMessageId = '0x7500000000000001';
$roContactId = $contactMessageId;
# END system dependent variables


function dumpContact($contact) {
  echo "Contact ID " . $contact->getID() . "\n";
  echo "PidLidEmail1EmailAddress: " . $contact->get(PidLidEmail1EmailAddress) . "\n";
  echo "PidTagEmailAddress: " . $contact->get(PidTagEmailAddress) . "\n";
  echo "PidTagCompanyName: " . $contact->get(PidTagCompanyName) . "\n";
  echo "PidTagDisplayName: " . $contact->get(PidTagDisplayName) . "\n";
  echo "PidTagGivenName: " . $contact->get(PidTagGivenName) . "\n";
  echo "PidLidBusyStatus: " . $contact->get(PidLidBusyStatus) . "\n";
  echo "PidLidWorkAddressStreet: " .  $contact->get(PidLidWorkAddressStreet) . "\n";

  echo "\n";
}


$mapi = new MAPIProfileDB($path);
echo "=> MAPI Profile Database Path: '", $mapi->path(), "'\n";
$mapiProfile = $mapi->getProfile($profileName);
$session = $mapiProfile->logon();
$mailbox = $session->mailbox();


$inbox = $mailbox->inbox();
echo "Inbox item type " . $inbox->getFolderType() . "\n";

echo "=> Opening Contact Folder\n";
$contacts = $mailbox->contacts();


echo "Open RO message and show its properties\n";
$roContact = $contacts->openMessage($contactMessageId, MAPIMessage::RO);
dumpContact($roContact);
$roContact->dump();

$message = $contacts->openMessage($contactMessageId,  MAPIMessage::RW);
echo "=> Message with message ID " . $message->getID() . " opened\n";
echo "Body content format: " . $message->getBodyContentFormat() . "\n";

echo "Contact just after oepning it\n";
dumpContact($message);

echo "Check one-argument prop set: setting PidTagDisplayName to 'juju' and PidTagDescription to 'descChanged'\n";
$message->set(PidTagDisplayName, 'juju');
$message->set(PidTagBody, 'descChanged');
echo "Set PidLidBusyStatus to 1\n";
$message->set(PidLidBusyStatus, 1);

print "[1] PidLidWorkAddressStreet: " . $message->get(PidLidWorkAddressStreet);
$message->set(PidLidWorkAddressStreet, "New Before street");

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
echo "Dump after reopening after save\n";
dumpContact($message);
echo "=> Message with message ID " . $message->getID() . " opened\n";

echo "Set PidLidBusyStatus to 2\n";
$message->set(PidLidBusyStatus, 2);

print "[2] PidLidWorkAddressStreet: " . $message->get(PidLidWorkAddressStreet);
$message->set(PidLidWorkAddressStreet, "New After street");

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


echo "=> SaveChangesMessage \n";
$message->save();

echo "Dump after second save\n";
print "PidLidWorkAddressStreet: " . $message->get(PidLidWorkAddressStreet);
dumpContact($message);

unset($message);

?>
