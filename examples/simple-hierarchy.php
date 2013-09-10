<?php
echo "Create ProfileDB\n";
$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");

echo "Get default profile\n";
$mapiProfile = $mapi->getProfile();



echo "Logon default profile\n";
$session = $mapiProfile->logon();



echo "Get mailbox\n";
$mailbox = $session->mailbox();
echo "Mailbox name "  . $mailbox->getName() . "\n";



#
echo "Get CONTACTS folder\n\n";
$contacts = $mailbox->contacts();
echo "contacts->getID -> " . $contacts->getID() . "\n";

#unset($mapi);

#echo "Get message\n";
#$messageId = '0xA4010E0000000001';
#$message = $contacts->openMessage($messageId);
#echo var_dump($message->FileUnder);
#
?>
