#!/usr/bin/env python2.7

# NOTE:
#
# For this test, we are running the whole environment on a different
# server: imap, ldap, postgresql, postfix, openchange, samba and sogo.
# We have configured SOGo backend locally and adjusted defaults to
# connect to this remote server.
#
# Finally we are accessing the openchange.ldb file through sshfs and
# map it at the top of the checkout within private folder:
# sshfs openchange@ip_addr:/usr/local/samba/private private
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
MAPICtx = MAPIStore.add_context("sogo://Administrator:Administrator@inbox/", "Administrator")
Inbox = MAPICtx.open()
identifier = MAPICtx.add_subscription("sogo://Administrator:Administrator@inbox/", False, 0x2)
while 1:
    time.sleep(1)
    MAPICtx.get_notifications()
    
time.sleep(15)
MAPICtx.delete_subscription("sogo://Administrator:Administrator@inbox/", False, 0x2, identifier)

#Calendar = MAPIStore.add_context("sogo://Administator:Administrator@inbox/", "Administrator").open()
#time.sleep(5)
#print Calendar.folder_count
#time.sleep(5)
#MAPICtx = MAPIStore.add_context("sogo://Administrator:Administrator@inbox/", "GoodAdmin")
Inbox.create_folder(name="Test")
time.sleep(15)
NewCalender = MAPICtx.open()

print "[I] We have %d sub folders, %d messages and %d fai messages  within %s" % (Inbox.folder_count, 
                                                                                  Inbox.message_count, 
                                                                                  Inbox.fai_message_count,
                                                                                  hex(Inbox.fid))
MAPIStore.delete_context(MAPICtx)

