<?php
function __testOk($msg) {
	    echo "OK: " . $msg . "\n";
}

function __testFailed($msg) {
	   exit("Failed: " . $msg . "\n");
}

function ok($expr, $msg) {
	 if ($expr) {
	    __testOk($msg);
         } else {
	   __testFailed($msg);
         }
}

function is($a, $b, $msg) {
	 if (! $msg) {
	    $msg = "$a == $b";
	 }
	 if ($a == $b) {
	    __testOk($msg);
	 } else {
	   __testFailed("$msg (expected $b but get $a)");
	 }
}

?>