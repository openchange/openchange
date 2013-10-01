<?php
include('./test-helpers.php');

# System dependent varialbes:
$path = "/home/jag/.openchange/profiles.ldb";
$profileName = 'miky';
$eventId = "0x3A08010000000001";

# END system dependent variables




$mapi = new MAPIProfileDB($path);
echo "=> MAPI Profile Database Path: '", $mapi->path(), "'\n";
$mapiProfile = $mapi->getProfile($profileName);
$session = $mapiProfile->logon();
$mailbox = $session->mailbox();


$inbox = $mailbox->inbox();
echo "=> Opening calensdar Folder\n";
$calendar = $mailbox->calendar();

$event = $calendar->openMessage($eventId);
$recur = $event->getRecurrence(PidLidAppointmentRecur);

var_dump($recur);

?>