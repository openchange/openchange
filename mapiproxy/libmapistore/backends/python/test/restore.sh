#!/bin/bash

for db in sogo openchange;
do
	mysql --user=root --password=SEak28Bx $db < $db.sql
done

