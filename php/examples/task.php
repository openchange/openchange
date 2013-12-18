<?php
include('./test-helpers.php');

$dbPath = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
#$id = "0xC1E000000000001";
$id = "0x3500000000000001";
$folder = 'tasks';

# values to be set
$tagBody1 = "body12";
$tagComplete = true;
$status = 2;
$dateFinished = 1380204166;
$percent = 0.12;


$mapi = new MAPIProfileDB($dbPath);
ok($mapi, "MAPIProfileDB open");
is($mapi->path(), $dbPath  , "MAPI DB correct path");

#$mapi->debug(true, 10);

$mapiProfile = $mapi->getProfile($profileName);
$session = $mapiProfile->logon();
$mailbox = $session->mailbox();
$ocFolder = $mailbox->$folder();

$message = $ocFolder->openMessage($id, MAPIMessage::RW);
ok($message, "Appointment $id opened in RW mode");
is($message->getID(), $id, "Check opened message Id (must be $id)");

var_dump($message->get(PidTagBody));

var_dump($message->get(PidLidTaskComplete));

var_dump($message->get(PidLidTaskDateCompleted));
echo "\nPidLidPercentComplete:" . $message->get(PidLidPercentComplete) . "\n";
diag("Changing PidTagBody to '$tagBody1'");
diag("Changing PidLidTaskComplete to '$tagComplete'");
diag("Changing PidLidTaskComplete to '$dateFinished'");
diag("Changing PidLidTaskStatus to '$status'");
echo "\n";
$message->set(PidTagBody, $tagBody1, PidLidTaskStatus, $status, PidLidTaskDateCompleted, $dateFinished, PidLidTaskComplete, $tagComplete);
echo "\n";
diag("Changing PidLidPercentComplete to $percent\n");
$message->set(PidLidPercentComplete, $percent);

$message->save();
unset($message);

diag("--After saving changes:");
$message = $ocFolder->openMessage($id, MAPIMessage::RW);
ok($message, "Reopen message in RW mode after saving changes");
var_dump($message->get(PidTagBody));
var_dump($message->get(PidLidTaskComplete));
var_dump($message->get(PidLidTaskDateCompleted));
var_dump($message->get(PidLidTaskStatus));
echo "\nPidLidPercentComplete:" . $message->get(PidLidPercentComplete) . "\n";
diag("-- Reverting changes");


$message->set(PidLidPercentComplete, 0.0);
$message->save();
unset($message);

endTestSuite("task.php");
?>
