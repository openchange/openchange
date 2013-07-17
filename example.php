<?php

$mapi = new MAPI("/home/jag/.openchange/profiles.ldb");

$allProfiles = $mapi->profiles();
var_dump($allProfiles);
$profile = $mapi->dump_profile('test');
var_dump($profile);

$profile = $mapi->dump_profile();
var_dump($profile);
?>