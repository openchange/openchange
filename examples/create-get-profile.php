<?php
# environment settings
$pathDB = "/home/jag/.openchange/profiles.ldb";
$profileName = 'new15';
$username='u10';
$password = 'a';
$domain = 'example42';
$realm = 'example42.com';
$server =  '192.168.56.42';

$mapiDB = new MAPIProfileDB($pathDB);
$profile = $mapiDB->createAndGetProfile($profileName, $username, $password, $domain, $realm, $server);
var_dump($profile);


?>