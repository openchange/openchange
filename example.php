<?php
$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");
echo "MAPI DB path: '", $mapi->path(), "'\n";

$allProfiles = $mapi->profiles();
var_dump($allProfiles);

echo "Profile test\n";
$mapiProfile = $mapi->getProfile('test');
var_dump($mapiProfile);
$profileDump = $mapiProfile->dump();
var_dump($profileDump);

echo "Default profile\n";
$mapiProfile = $mapi->getProfile();
var_dump($mapiProfile);
$profileDump = $mapiProfile->dump();
var_dump($profileDump);

echo "Logon default profile\n";
$session = $mapiProfile->logon();
var_dump($session);

echo "Folders default profile\n";

$folders = $session->folders();
var_dump($folders);

echo "Fetchmail\n";
$messages = $session->fetchmail();
var_dump($messages);

echo "Appointments\n";
$appointments = $session->appointments();
var_dump($appointments);
echo "Appoinments END\n";

echo "Destroy session\n";
$session = 'DESTROY';

echo "Destroy mapiProfile\n";
$mapiProfile = 'DESTROY';

echo "Create and destroy idle MAPIDb\n";
$mapi = new MAPIProfileDB("/dfdf");

?>
