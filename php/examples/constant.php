<?php
include('./test-helpers.php');

is(PidLidEmail1OriginalDisplayName,  2156134431, "PidLidEmail1OriginalDisplayName constant");
is(PidTagBody, 268435487, "PidTagBody constant");
is(MAPIMessage::RW, 1, "Class constant MAPIMessage::RW");

endTestSuite("constant.php");
?>