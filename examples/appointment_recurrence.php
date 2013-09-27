<?php
$dbPath = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
$id = "0x2500000000000001";


$mapi = new MAPIProfileDB($dbPath);
echo "MAPI DB path: '", $mapi->path(), "'\n";



echo "Profile test\n";
$mapiProfile = $mapi->getProfile($profileName);


echo "Logon test profile\n";
$session = $mapiProfile->logon();

echo "Get mailbox\n";

$mailbox = $session->mailbox();
echo "Mailbox name "  . $mailbox->getName() . "\n";


$calendar = $mailbox->calendar();
echo "Calendar folder " . $calendar->getName() . "/" . $calendar->getFolderType() .  "\n";


$message = $calendar->openMessage($id, 1);

#$toSet = array('StartTimeOffset' => 0);
$toSet = array(
		   'RecurrencePattern' => array(),
                   'StartTimeOffset' => 4,
                   'EndTimeOffset' => 64
         );
#$mapi->debug(true, 10);
$message->setRecurrence($toSet);


$message->save();
unset($message);

$message = $calendar->openMessage($id, 1);
$rec = $message->getRecurrence();
var_dump($rec);
?>