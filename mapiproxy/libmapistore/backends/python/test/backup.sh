#!/bin/bash

mysql_pass=`cat /var/lib/zentyal/conf/zentyal-mysql.passwd`

for db in sogo openchange;
do
	mysqldump -u root -p$mysql_pass --opt --skip-extended-insert --add-drop-database --databases $db > $db.sql.new
	grep -v -e "-- Dump completed" $db.sql.new > $db.sql
	rm $db.sql.new
done

