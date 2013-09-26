<?php

function showApp($message) {
	echo "Message " . $message->getID() . "  Properties:\n";
#	echo "PidLidAppointmentSubType (boolean):" . $message->get(PidLidAppointmentSubType) . "\n";
#	echo "PidLidAppointmentDuration    (long):" . $message->get(PidLidAppointmentDuration) . "\n";
	echo "PidTagBody    (string):" . $message->get(PidTagBody) . "\n";
	echo "PidLidAppointmentEndWhole   (date):" . $message->get(PidLidAppointmentEndWhole) . "\n";
#	echo "PidLidAppointmentRecur   (binary/especial):" . serialize($message->get(PidLidAppointmentRecur)) . "\n";
}

$dbPath = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
$id = "0x2500000000000001";


$mapi = new MAPIProfileDB($dbPath);
echo "MAPI DB path: '", $mapi->path(), "'\n";

#$mapi->debug(true, 10);

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

# PidLidAppointmentSubType (bolean)  0x8257000b
# PidLidAppointmentDuration (long) 0x82130003
# (date)
# PidLidAppointmentRecur (binary)
showApp($message);

echo "changing PidTagBody to 'bodyChanged'\n";
$message->set(PidTagBody, 'bodyChanged');

echo "Changing  PidLidAppointmentEndWhole to 1380204166 (09/26/2013 @ 10:02am in EST.)\n";
$unixtime = 1380204166;
$message->set(PidLidAppointmentEndWhole, $unixtime);

$message->save();
unset($message);

echo "\n--- After save:\n";
$message = $calendar->openMessage($id, 1);
showApp($message);
echo "Changing and saving again\n";
$message->set(PidTagBody, 'bodyRestored');
echo "Changing  PidLidAppointmentEndWhole to 100 seconds more \n";
$message->set(PidLidAppointmentEndWhole, $unixtime + 100);
$message->save();
unset($message);

echo "\n --After last change: \n";
$message = $calendar->openMessage($id, 1);
showApp($message);

# workaround against shutdown problem
unset($message);
#unset($calendar);
#unset($mailbox);
#unset($session);
#unset($mapiProfile);
#unset($mapi);


?>
