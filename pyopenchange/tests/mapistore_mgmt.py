#!/usr/bin/env python2.7

# NOTE:
#
# For this test, we are running the whole environment on a different
# server: imap, ldap, postgresql, postfix, openchange, samba and sogo.
# We have configured SOGo backend locally and adjusted defaults to
# connect to this remote server.
#
# Finally we are accessing the openchange.ldb file through sshfs and
# map it to expected /usr/local/samba/private folder.
# sshfs openchange@ip_addr:/usr/local/samba/private /usr/local/samba/private
# We have also adjusted the permissions to allow openchange user to
# read/write openchange.ldb file remotely.
#
# Do not forget to run memcached with the user account running the
# script.
#

import os
import sys
import time

sys.path.append("python")

import openchange.mapistore as mapistore

dirname = "/usr/local/samba/private/mapistore"
if not os.path.exists(dirname):
    os.mkdir("/usr/local/samba/private/mapistore")

mapistore.set_mapping_path(dirname)
MAPIStore = mapistore.mapistore(syspath="/usr/local/samba/private")
mgmt = MAPIStore.management()
#while 1:
#    time.sleep(5)
#    d = mgmt.registered_users("SOGo", "Administrator")
#    print d
print "Is SOGo backend registered: %s" % mgmt.registered_backend("SOGo")
print "Is NonExistent backend registered: %s" % mgmt.registered_backend("NonExistent")
print "Registered message: %s" % mgmt.registered_message("SOGo", "Administrator", "Administrator", "inbox", "61")
print "Registered message: %s" % mgmt.registered_message("SOGo", "Administrator", "Administrator", "inbox", "74")

mgmt.existing_users("SOGo", "Administrator", "inbox")
