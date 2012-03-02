#!/bin/sh

./libmapi/conf/mparse.pl --parser=mapicodes --outputdir=libmapi/ libmapi/conf/mapi-codes
./libmapi/conf/mparse.pl --parser=codepage_lcid --outputdir=libmapi/ libmapi/conf/codepage-lcid
./libmapi/conf/mparse.pl --parser=mapistore_nameid --outputdir=mapiproxy/libmapistore/ libmapi/conf/mapi-named-properties
./libmapi/conf/mparse.pl --parser=mapistore_namedprops --outputdir=setup/mapistore/ libmapi/conf/mapi-named-properties