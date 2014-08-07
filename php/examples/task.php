<?php
/*
   OpenChange PHP bindings examples

   Copyright (C) 2013-2014 Javier Amor Garcia

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

include('./test-helpers.php');
include('./config.php');

$PidNameKeywords = 0xa064101f;

function dumpTask($message) {
  $PidNameKeywords = 0xa064101f;
  echo "\n";
  echo "PidTagBody: " . $message->get(PidTagBody) . "\n";
  echo "PidLidTaskStatus: " . $message->get(PidLidTaskStatus) . "\n";
  echo "PidLidTaskComplete: " . ($message->get(PidLidTaskComplete) ? 'true' : 'false') . "\n";
  echo "PidLidTaskDateCompleted: " . $message->get(PidLidTaskDateCompleted) . "\n";
  echo "PidLidPercentComplete:" . $message->get(PidLidPercentComplete) . "\n";
  echo "PidNameKeywords" . var_dump($message->get($PidNameKeywords)) . "\n";
}

$folder = 'tasks';

# values to be set
$tagBody1 = "body12";
$tagComplete = true;
$status = 2;
$dateFinished = 1380204166;
$percent = 0.12;
$keywords = array('key1', 'key2');

# values for undo
$undoTagBody1 = "notSet";
$undoTagComplete = false;
$undoStatus = 0;
$undoDateFinished = 1380204166;
$undoPercent = 0; # LONG NOT FLOAT!
$undoKeywords = array();

$mapi = new MAPIProfileDB($dbPath);
ok($mapi, "MAPIProfileDB open");
is($mapi->path(), $dbPath  , "MAPI DB correct path");

#$mapi->debug(true, 10);

$mapiProfile = $mapi->getProfile($profileName);
$session = $mapiProfile->logon();
$mailbox = $session->mailbox();
$ocFolder = $mailbox->$folder();

$message = $ocFolder->openMessage($taskId, MAPIMessage::RW);
ok($message, "Appointment $taskId opened in RW mode");
is($message->getID(), $taskId, "Check opened message Id (must be $taskId)");
dumpTask($message);


diag("Changing PidTagBody to '$tagBody1'");
diag("Changing PidLidTaskComplete to '$tagComplete'");
diag("Changing PidLidTaskComplete to '$dateFinished'");
diag("Changing PidLidTaskStatus to '$status'");
echo "\n";
$message->set(PidTagBody, $tagBody1, PidLidTaskStatus, $status, PidLidTaskDateCompleted, $dateFinished, PidLidTaskComplete, $tagComplete);
echo "\n";
diag("Changing PidLidPercentComplete to $percent\n");
$message->set(PidLidPercentComplete, $percent);
$message->set($PidNameKeywords, $keywords);

$message->save();
unset($message);

diag("--After saving changes:");
$message = $ocFolder->openMessage($taskId, MAPIMessage::RW);
ok($message, "Reopen message in RW mode after saving changes");

is($message->get(PidTagBody), $tagBody1, "Checking change in PidTagBody");
is($message->get(PidLidTaskStatus), $status, "Checking change in PidLidTaskStatus");
is($message->get(PidLidTaskComplete), $tagComplete, "Checking change in PidLidTaskComplete");
echo "PidLidTaskDateCompleted: " . $message->get(PidLidTaskDateCompleted) . " NOT WORKING PROPERLY. USING WORKAROUND IN ROUNDCUBE " . "\n";
is($message->get(PidLidPercentComplete), $percent, "Checking change in PidLidPercentComplete");
var_dump($message->get($PidNameKeywords));

diag("-- Reverting changes");

$message->set(PidTagBody, $undoTagBody1, PidLidTaskStatus, $undoStatus, PidLidTaskDateCompleted, $undoDateFinished, PidLidTaskComplete, $undoTagComplete);
$message->set(PidLidPercentComplete, $undoPercent);
$message->set($PidNameKeywords, $undoKeywords);
$message->save();
unset($message);

endTestSuite("task.php");
?>
