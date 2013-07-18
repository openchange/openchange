<?php

$mapi = new MAPI("/home/jag/.openchange/profiles.ldb");

$allProfiles = $mapi->profiles();
var_dump($allProfiles);

echo "Dump profile test\n";
$profile = $mapi->dump_profile('test');
var_dump($profile);

echo "Dump default profile\n";
$profile = $mapi->dump_profile();
var_dump($profile);

echo "Folders default profile\n";
$folders = $mapi->folders();
var_dump($folders);

echo "Folders by profile name\n";
$folders = $mapi->folders('test');
var_dump($folders);


echo "Destroy object\n";
$mapi->__destruct();

echo "Create and destroy idle MAPI\n";
$mapi = new MAPI("/dfdf");
$mapi->__destruct();
?>
