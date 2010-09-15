#!/usr/bin/python

import sys

sys.path.append("python")

import os
import openchange
import openchange.mapistore as mapistore
from openchange import mapi

os.mkdir("/tmp/mapistore")

mapistore.set_mapping_path("/tmp/mapistore")
MAPIStore = mapistore.mapistore()
ctx_id = MAPIStore.add_context("sogo://openchange:openchange@mail/INBOX")

count = MAPIStore.get_folder_count(ctx_id, 0x0000000000160001, 
                                   mapistore.MAPISTORE_MESSAGE)

print "Number of messages in INBOX: %d" % (count)

MAPIStore.del_context(ctx_id)
