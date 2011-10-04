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

import os
import sys

sys.path.append("python")

import openchange.mapistore as mapistore

dirname = "private/mapistore"
if not os.path.exists(dirname):
    os.mkdir("private/mapistore")

mapistore.set_mapping_path(dirname)
MAPIStore = mapistore.mapistore(syspath="private")
MAPICtx = MAPIStore.add_context("sogo://Administrator:Administrator@inbox/", "Administrator")
MAPIStore.delete_context(MAPICtx)

MAPICtx = MAPIStore.add_context("sogo://Administrator:Administrator@calendar/", "Administrator")
