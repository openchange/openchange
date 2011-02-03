#!/bin/sh

./libmapi/conf/mparse.pl --parser=mapicodes --outputdir=libmapi/ libmapi/conf/mapi-codes
./libmapi/conf/mparse.pl --parser=codepage_lcid --outputdir=libmapi/ libmapi/conf/codepage-lcid
./libmapi/conf/mparse.pl --parser=pymapi_properties --outputdir=pyopenchange/ libmapi/conf/mapi-properties