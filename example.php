<?php
$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");

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

# echo "Folders default profile\n";
# $folders = $session->folders();
# var_dump($folders);



echo "Fetchmail\n";
$messages = $session->fetchmail();
var_dump($messages);


 echo "Destroy objecs\n";
 unset($mapiProfile);
# $mapiProfile->__destruct();
$mapi->__destruct();
echo "After destroy\n";


echo "Create and destroy idle MAPIDb\n";
$mapi = new MAPIProfileDB("/dfdf");
$mapi->__destruct();
?>
