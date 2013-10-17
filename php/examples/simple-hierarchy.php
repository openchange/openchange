<?php
include('./test-helpers.php');

# environment settings
$pathDB = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
$messageId = '0x7500000000000001';
# END

$mapi = new MAPIProfileDB($pathDB);
ok($mapi, "MAPIProfileDB open for path $pathDB");
is($mapi->path(), $pathDB, "MAPI DB correct path");

$inexistent = $mapi->getProfile("idonotexist_232");
ok(is_null($inexistent), "Checking that trying to get inexistent profile return NULL");

$mapiProfile = $mapi->getProfile($profileName);
ok($mapiProfile, "Getting profile $profileName");

$session = $mapiProfile->logon();
ok($session, "Logon with profile $profileName");

$mailbox = $session->mailbox();
ok($mailbox, "Get default mailbox");
diag("Mailbox name "  . $mailbox->getName());

$contacts = $mailbox->contacts();
ok($contacts, "Get contacts folder");

$messageTable = $contacts->getMessageTable();
ok ($messageTable, "Get message table from contacts folder");
ok ($messageTable->count() > 0, "Checking that message table contains messages");

$message = $contacts->openMessage($messageId);
ok($message, "Get message with ID $messageId");
is($message->getID(), $messageId, "Check opened message Id (must be $messageId)");

#unset($mapi); # do not work..
#unset($profile);
#unset($session);
#unset($mailbox);
#unset($contacts);
#unset($messages);

endTestSuite("simple-hierarchy.php");
?>
