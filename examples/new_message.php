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

$ids = array();

echo "Create new contact\n";
$newContact = $contacts->createMessage(PidLidEmail1EmailAddress, 'first_new@contacts.org', PidTagCompanyName, 'to_delete_org', PidTagDisplayName, 'first');
$ids[] = $newContact->getID();
var_dump($newContact);
print "New contact ID " . $newContact->getID() . "\n";
unset($newContact);

echo "Create a second contact\n";
$newContact = $contacts->createMessage(PidLidEmail1EmailAddress, 'second_new@contacts.org', PidTagCompanyName, 'to_delete_org', PidTagDisplayName, 'second');
$ids[] = $newContact->getID();
var_dump($newContact);
print "New contact ID " . $newContact->getID() . "\n";
unset($newContact);

echo "Opening created messages\n";
foreach ($ids as $newId) {
    $retrieved = $contacts->openMessage($newId);
    var_dump($retrieved);
    unset($retrieved);
}

echo "Delete created messages\n";
call_user_func_array(array($contacts, 'deleteMessages'), $ids);


echo "Tried to open removed messages\n";
foreach ($ids as $newId) {
    $retrieved = $contacts->openMessage($newId);
    var_dump($retrieved);
    unset($retrieved);
}


?>
