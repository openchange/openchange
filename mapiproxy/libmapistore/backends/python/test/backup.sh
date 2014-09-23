#!/bin/bash

for db in sogo openchange;
do
	mysqldump -u root -pSEak28Bx --opt --skip-extended-insert --add-drop-database --databases $db > $db.sql.new
	grep -v -e "-- Dump completed" $db.sql.new > $db.sql
	rm $db.sql.new
done

