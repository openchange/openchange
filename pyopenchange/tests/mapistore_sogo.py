#!/usr/bin/python

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

sys.path.append("python")

import openchange.mapistore as mapistore

dirname = "private/mapistore"
if not os.path.exists(dirname):
    os.mkdir("private/mapistore")

mapistore.set_mapping_path(dirname)
MAPIStore = mapistore.mapistore(syspath="private")
Inbox = MAPIStore.add_context("sogo://Administrator:Administrator@inbox/", "Administrator").open()
print "[I] We have %d sub folders, %d messages and %d fai messages  within %s" % (Inbox.folder_count, 
                                                                                  Inbox.message_count, 
                                                                                  Inbox.fai_message_count,
                                                                                  hex(Inbox.fid))
Inbox.create_folder(name="Test")

MAPICtx = MAPIStore.add_context("sogo://Administrator:Administrator@calendar/", "Administrator")
MAPIStore.delete_context(MAPICtx)

