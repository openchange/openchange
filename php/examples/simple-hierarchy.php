<?php
# environment settings
$pathDB = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
$messageId = '0x7500000000000001';
# END


echo "Create ProfileDB\n";
$mapi = new MAPIProfileDB($pathDB);

echo "Get inexistent profile\n";
$inexistent = $mapi->getProfile("idonotexist_232");
var_dump($inexistent);

echo "Get default profile\n";
$mapiProfile = $mapi->getProfile($profileName);

echo "Logon default profile\n";
$session = $mapiProfile->logon();

echo "Get mailbox\n";
$mailbox = $session->mailbox();
echo "Mailbox name "  . $mailbox->getName() . "\n";

echo "Get CONTACTS folder\n\n";
$contacts = $mailbox->contacts();
echo "contacts->getID -> " . $contacts->getID() . "\n";

echo "Get message table\n";
$messageTable = $contacts->getMessageTable();
var_dump($messageTable);

echo "Get message\n";
$message = $contacts->openMessage($messageId);
var_dump($message);

#unset($mapi); # do not work..
#unset($profile);
#unset($session);
#unset($mailbox);
#unset($contacts);
#unset($messages);
?>
