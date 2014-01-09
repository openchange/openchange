<?php
include('./test-helpers.php');

$dbPath = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
$id = "0x2500000000000001";

# values to be set
$tagBody1 = "body1";
$tagBody2 = "body2";
$unixtime1 = 1380204166;
$unixtime2 = $unixtime1 + 100;


$mapi = new MAPIProfileDB($dbPath);
ok($mapi, "MAPIProfileDB open");
is($mapi->path(), $dbPath  , "MAPI DB correct path");

#$mapi->debug(true, 10);

$mapiProfile = $mapi->getProfile($profileName);
ok($mapiProfile, "Get profile $profileName");

$session = $mapiProfile->logon();
ok ($session, "Profile logon");

$mailbox = $session->mailbox();
ok ($mailbox, "Get default mailbox");
diag("Mailbox name "  . $mailbox->getName());

$calendar = $mailbox->calendar();
diag("Calendar folder " . $calendar->getName() . "/" . $calendar->getFolderType());
ok($calendar, "Get calendar");


$message = $calendar->openMessage($id, MAPIMessage::RW);
ok($message, "Appointment $id opened in RW mode");
is($message->getID(), $id, "Check opened message Id (msut be $id)");

diag("Changing PidTagBody to '$tagBody1'");
$message->set(PidTagBody, $tagBody1);

diag("Changing  PidLidAppointmentEndWhole to $unixtime1 (09/26/2013 @ 10:02am in EST.)");
$message->set(PidLidAppointmentEndWhole, $unixtime1);

$message->save();
unset($message);

diag("--After saving changes:");
$message = $calendar->openMessage($id, MAPIMessage::RW);
ok($message, "Reopen message in RW mode after saving changes");
is($message->get(PidTagBody), $tagBody1, "Checking that PidTagBody has correctly been changed");
is($message->get(PidLidAppointmentEndWhole), $unixtime1, "Checking that PidLidAppointmentEndWhole has correctly been changed");


diag("Changing PidTagBody again to $tagBody2");
$message->set(PidTagBody, $tagBody2);
diag("Changing  PidLidAppointmentEndWhole to 100 seconds more");
$message->set(PidLidAppointmentEndWhole, $unixtime2);
$message->save();
unset($message);

diag("--After last changes:");
$message = $calendar->openMessage($id, MAPIMessage::RO);
ok($message, "Reopen message in RO mode after saving the last changes");
is($message->get(PidTagBody), $tagBody2, "Checking that PidTagBody has changed again");
is($message->get(PidLidAppointmentEndWhole), $unixtime2, "Checking that PidLidAppointmentEndWhole has changed again");

# workaround against shutdown problem
unset($message);
#unset($calendar);
#unset($mailbox);
#unset($session);
#unset($mapiProfile);
#unset($mapi);

endTestSuite("appointment.php");

?>
