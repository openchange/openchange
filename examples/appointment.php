<?php
$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");
echo "MAPI DB path: '", $mapi->path(), "'\n";


echo "Profile test\n";
$mapiProfile = $mapi->getProfile('test');


echo "Logon test profile\n";
$session = $mapiProfile->logon();

echo "Get mailbox\n";

$mailbox = $session->mailbox();
echo "Mailbox name "  . $mailbox->getName() . "\n";


$calendar = $mailbox->calendar();
echo "Calendar folder " . $calendar->getName() . "/" . $calendar->getFolderType() .  "\n";

$id = "0x1307130000000001";
$message = $calendar->openMessage($id);
print "Message\n";
var_dump($message);


# workaround against shutdown problem
#unset($message);
#unset($calendar);
#unset($mailbox);
#unset($session);
#unset($mapiProfile);
#unset($mapi);


?>
