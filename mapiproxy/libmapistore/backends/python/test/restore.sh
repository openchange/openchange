#!/bin/bash

mysql_pass=`cat /var/lib/zentyal/conf/zentyal-mysql.passwd`

for db in sogo openchange;
do
	mysql --user=root --password=$mysql_pass $db < $db.sql
done

