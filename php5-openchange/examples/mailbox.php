<?php
include('./test-helpers.php');

$dbPath = "/home/jag/.openchange/profiles.ldb";
$profileName = "u2";
$draftFolderId =  "0x1a00000000000001";
$inexistentFolderId = '0xFFFF0d0000000001';
$mailboxName = 'OpenChange Mailbox: u2';
$inboxName = 'INBOX';
$calendarName = "Personal Calendar (c)";
$tasksName = "Personal Calendar (t)";
$contactsName = "Personal Address Book";

$mapi = new MAPIProfileDB($dbPath);
ok($mapi, "Open MAPIProfileDB with path $dbPath");

$mapiProfile = $mapi->getProfile($profileName);
ok($mapiProfile, "Get profile $profileName");

$session = $mapiProfile->logon();
ok($session, "Logon with profile $profileName");


$mailbox = $session->mailbox();
ok($mailbox, "Get default mailbox");
is($mailbox->getName(), $mailboxName, "Get mailbox name");

$inbox = $mailbox->inbox();
ok($inbox, "Get inbox folder");
is($inbox->getFolderType(), "IPF.Note", "Get inbox folder type");
is($inbox->getName(), $inboxName, "Get inbox folder name");
unset($inbox);

$calendar = $mailbox->calendar();
ok($calendar, "Get calendar folder");
is($calendar->getFolderType(), "IPF.Appointment", "Get calendar folder type");
is($calendar->getName(), $calendarName, "Get calendar folder name");
unset($calendar);

$tasks = $mailbox->tasks();
ok($tasks, "Get tasks folder");
is($tasks->getFolderType(), "IPF.Task", "Get tasks folder type");
is($tasks->getName(), $tasksName, "Get tasks folder name");
unset($tasks);

$contacts = $mailbox->contacts();
ok($contacts, "Get contacts folder");
is($contacts->getFolderType(), "IPF.Contact", "Get contacts folder type");
is($contacts->getName(), $contactsName, "Get contacts folder name");
unset($contactt);

$drafts = $mailbox->openFolder($draftFolderId, "IPF.Note");
ok($drafts, "Open by ID draft folder");
unset($drafts);

ok(is_null($mailbox->openFolder($inexistentFolderId, "IPF.Note")), "Open folder with inexistent ID returns NULL");

unset($mailbox);
unset($session);
unset($profile);
unset($mapi);

endTestSuite("mailbox.php");
?>
