#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys

sys.path.append("python")

import os
import openchange
import openchange.mapistoredb as mapistoredb
import openchange.mapistore as mapistore
from openchange import mapi

os.mkdir("/tmp/mapistoredb");

print "[Step 1]. Initializing mapistore database"
print "========================================="

MAPIStoreDB = mapistoredb.mapistoredb("/tmp/mapistoredb")
print ""

print "[Step 2]. Provisioning mapistore database"
print "========================================="
ret = MAPIStoreDB.provision()
if (ret == 0):
    print "Provisioning: SUCCESS"
else:
    print "Provisioning: FAILURE"
print ""

print "[Step 3]. Modify and dump configuration"
print "======================================="

MAPIStoreDB.netbiosname = "server"
MAPIStoreDB.firstorg = "OpenChange Project"
MAPIStoreDB.firstou = "OpenChange Development Unit"

MAPIStoreDB.dump_configuration()
print ""

print "[Step 4]. Testing API parts"
print "==========================="

print "A. Testing NetBIOS name"
print "-----------------------"
print "* NetBIOS name: %s" %MAPIStoreDB.netbiosname
print ""

print "B. Testing First OU"
print "-------------------"
print "* FirstOU: %s" %MAPIStoreDB.firstou
print ""

print "C. Testing First Organisation"
print "-----------------------------"
print "* First Organisation: %s" %MAPIStoreDB.firstorg
print ""
