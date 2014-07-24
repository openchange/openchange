<?php
include('./test-helpers.php');

# environment settings
$pathDB = "/home/jag/.openchange/profiles.ldb";
$profileName = 'new15';
$username='u2';
$password = 'a';
$domain = 'zentyal-domain';
$realm = 'zentyal-domain.lan';
$server =  '192.168.56.51';

$mapiDB = new MAPIProfileDB($pathDB);
ok($mapiDB, "MAPIProfileDB opened for $pathDB");

$profile = $mapiDB->createAndGetProfile($profileName, $username, $password, $domain, $realm, $server);
ok($profile, "createAndGet profile for $profileName");

$session = $profile->logon();
ok($session, "Check logon with profile $profileName");

endTestSuite("create-get-profile.php");
?>