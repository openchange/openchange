#!/bin/sh

./libmapi/conf/mparse.pl --parser=mapitags --outputdir=libmapi/ libmapi/conf/mapi-properties
./libmapi/conf/mparse.pl --parser=mapicodes --outputdir=libmapi/ libmapi/conf/mapi-codes
