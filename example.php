<?php

$mapi = new MAPIProfileDB("/home/jag/.openchange/profiles.ldb");

$allProfiles = $mapi->profiles();
var_dump($allProfiles);

echo "Profile test\n";
$profile = $mapi->getProfile('test');
var_dump($profile);
$mapiProfile = new MAPIProfile($profile);
var_dump($mapiProfile);
$profileDump = $mapiProfile->dump();
var_dump($profileDump);

echo "Default profile\n";
$profile = $mapi->getProfile();
var_dump($profile);
$mapiProfile = new MAPIProfile($profile);
var_dump($mapiProfile);
$profileDump = $mapiProfile->dump();
var_dump($profileDump);


echo "Folders default profile\n";
$folders = $mapi->folders();
var_dump($folders);

# echo "Folders by profile name\n";
# $folders = $mapi->folders('test');
# var_dump($folders);

# echo "Fetchmail\n";
# $mapi->fetchmail();


 echo "Destroy objecs\n";
 unset($mapiProfile);
# $mapiProfile->__destruct();
$mapi->__destruct();
echo "After destroy\n";


# echo "Create and destroy idle MAPI\n";
# $mapi = new MAPIProfileDB("/dfdf");
# $mapi->__destruct();
?>
