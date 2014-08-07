<?php
/*
   OpenChange PHP bindings examples

   Configuration file for examples. Modify it to match your system.

   Copyright (C) 2013-2014 Javier Amor Garcia

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

$dbPath = "/home/jag/.openchange/profiles.ldb";
$profileName = 'u2';
$username='u2';
$password = 'a';
$domain = 'zentyal-domain';
$realm = 'zentyal-domain.lan';
$server =  '192.168.56.51';

$appointmentId = "0x2500000000000001";
$eventId = "0x3A08010000000001";
$draftFolderId =  "0x1a00000000000001";
$inexistentFolderId = '0xFFFF0d0000000001';
$contactMessageId = '0x7500000000000001';
$roContactId = $contactMessageId;
$inexistentContactId = '0xA4010E0000ABCDEF';
$messageId = '0x7500000000000001';
$inexistentMesageId = '0x7500000CAAA00001';
$taskId = "0x3500000000000001";

$mailboxName = 'OpenChange Mailbox: u2';
?>
