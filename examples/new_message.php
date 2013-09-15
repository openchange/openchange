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

echo "Create new contact\n";
$newContact = $contacts->createMessage(PidLidEmail1EmailAddress, 'new@contacts.org', PidTagCompanyName, 'monossalvakjes', PidTagDisplayName, 'macaco');
var_dump($newContact);

print "New contact ID " . $newContact->getID() . "\n";
unset($newContact);
?>
