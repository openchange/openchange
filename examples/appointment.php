<?php
$id = "0x2F00000000000001";


$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");
echo "MAPI DB path: '", $mapi->path(), "'\n";

#$mapi->debug(true, 10);

echo "Profile test\n";
$mapiProfile = $mapi->getProfile('test');


echo "Logon test profile\n";
$session = $mapiProfile->logon();

echo "Get mailbox\n";

$mailbox = $session->mailbox();
echo "Mailbox name "  . $mailbox->getName() . "\n";


$calendar = $mailbox->calendar();
echo "Calendar folder " . $calendar->getName() . "/" . $calendar->getFolderType() .  "\n";


$message = $calendar->openMessage($id, 1);

# PidLidAppointmentCounterProposal (bolean)  0x8257000b
# PidLidAppointmentDuration (long) 0x82130003
# PidLidAppointmentProposedEndWhole (date)
# PidLidAppointmentRecur (binary)
echo "Message " . $id .  " properties: " . var_dump($message->get(PidLidAppointmentCounterProposal, PidLidAppointmentDuration)) . "\n";
echo "Changing properties and saving\n";
#$message->set(PidLidAppointmentCounterProposal, true, PidLidAppointmentDuration, 45);
$message->set(PidLidAppointmentDuration, 77);
$message->save();

return 0;


echo "New properties of message: " . var_dump($message->get(PidLidAppointmentCounterProposal, PidLidAppointmentDuration)) . "\n";
echo "Changing and saving again\n";
#$message->set(PidLidAppointmentCounterProposal, false, PidLidAppointmentDuration, 110);
$message->set(PidLidAppointmentDuration, 999);
$message->save();

echo "Properties after last change: " . var_dump($message->get(PidLidAppointmentCounterProposal, PidLidAppointmentDuration)) . "\n";


# workaround against shutdown problem
unset($message);
#unset($calendar);
#unset($mailbox);
#unset($session);
#unset($mapiProfile);
#unset($mapi);


?>
