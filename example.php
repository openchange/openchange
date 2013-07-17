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
?>