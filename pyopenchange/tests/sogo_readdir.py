#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-

import sys

sys.path.append("python")

import os
import openchange
import openchange.mapistore as mapistore
from openchange import mapi

dirname = "/tmp/mapistore"
if not os.path.exists(dirname):
    os.mkdir("/tmp/mapistore")

mapistore.set_mapping_path("/tmp/mapistore")
MAPIStore = mapistore.mapistore()
ctx_id = MAPIStore.add_context("sogo://openchange:openchange@mail/")

count = MAPIStore.get_folder_count(ctx_id, 0x0000000000160001, 
                                   mapistore.MAPISTORE_FOLDER)

print "Number of folders in INBOX: %d" % (count)

SPropValue = mapi.SPropValue()
SPropValue.add(mapi.PR_PARENT_FID, 0x0000000000160001)
SPropValue.add(mapi.PR_DISPLAY_NAME, "testé")
SPropValue.add(mapi.PR_COMMENT, "test folder")
SPropValue.add(mapi.PR_FOLDER_TYPE, 1)

MAPIStore.mkdir(ctx_id, 0x0000000000160001, 0x00000000001620001, SPropValue)

count = MAPIStore.get_folder_count(ctx_id, 0x0000000000160001, 
                                   mapistore.MAPISTORE_FOLDER)

print "Number of folders in INBOX: %d" % (count)

MAPIStore.del_context(ctx_id)
