<?php
include('./test-helpers.php');

# environment settings
$pathDB = "/home/jag/.openchange/profiles.ldb";
$profileName = 'new15';
$username='u13';
$password = 'a';
$domain = 'example42';
$realm = 'example42.com';
$server =  '192.168.56.42';

$mapiDB = new MAPIProfileDB($pathDB);
ok($mapiDB, "MAPIProfileDB opened for $pathDB");

$profile = $mapiDB->createAndGetProfile($profileName, $username, $password, $domain, $realm, $server);
ok($profile, "createAndGet profile for $profileName");

$session = $profile->logon();
ok($session, "Check logon with profile $profileName");

?>