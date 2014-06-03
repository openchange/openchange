<?php
include('./test-helpers.php');
# System dependent variables
  $dbPath      = "/home/jag/.openchange/profiles.ldb";
  $profileName = "u2";
# END system dependent variables

$mapi = new MAPIProfileDB($dbPath);
ok($mapi, "Opened MAPIProfileDB with path $dbPath");

$mapiProfile = $mapi->getProfile($profileName);
ok ($mapiProfile, "Opened profile $profileName");

$session = $mapiProfile->logon();
ok($session, "Logon with profile $profileName");

$mailbox = $session->mailbox();
ok ($mailbox, "Get default mailbox");
echo "Mailbox name "  . $mailbox->getName() . "\n";


$contacts = $mailbox->contacts();
ok($contacts, "Opened contact folders");
is($contacts->getFolderType(), "IPF.Contact", "Check contact folder type");
$contactsId = $contacts->getID();

$table1 =  $contacts->getMessageTable();
ok($table1, "Get a message table from contacts folder");

$twoMessages = $table1->getMessages(2);
is(count($twoMessages), 2, "Check the retrieval of two messages from the table");
is(get_class($twoMessages[0]), 'MAPIContact', 'Check that first retrieved value is a MAPIContact');
is(get_class($twoMessages[1]), 'MAPIContact', 'Check that second retrieved value is a MAPIContact');
foreach($twoMessages as $msg) {
     unset($msg);
}
unset($twoMessages);

$remainingMessages = $table1->getMessages();
ok(count($remainingMessages)>0, "Check the retrieval of remainig from the table");
is(get_class($remainingMessages[0]), 'MAPIContact', 'Check that we have retrieved a MAPIContact');
foreach($remainingMessages as $msg) {
     unset($msg);
}
unset($remainingMessages);


$parentFolder =   $table1->getParentFolder();
ok($parentFolder, "Getting parent folder");
is($parentFolder->getID(), $contactsId, "Checking that ID of parent fodler is the correct one");

echo "Contacts getMessageTable with properties PidLidFileUnder, PidLidEmail1EmailAddress \n";
$table2 =  $contacts->getMessageTable(PidLidEmail1EmailAddress,
				      PidTagDisplayName);
ok($table2, "Contacts getMessageTable with properties PidLidEmail1EmailAddress and PidTagDisplayName");

$oneSummaryContact = $table2->summary(1);
is(count($oneSummaryContact), 1, "Get one summary contact");
ok($oneSummaryContact[0].PidLidEmail1EmailAddress, "Checking PidLidEmail1EmailAddress attribute for contact");

$remainingSummary = $table2->summary();
ok(count($remainingSummary)>0, "Getting remaining sumamries");

foreach ($oneSummaryContact as $msg) {
	unset($msg);
}
foreach ($remainingSummary as $msg) {
	unset($msg);
}

endTestSuite("message-table.php");
?>
