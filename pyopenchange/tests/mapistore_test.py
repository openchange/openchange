#!/usr/bin/python

import os
import openchange
import openchange.mapistore as mapistore
from openchange import mapi

os.mkdir("/tmp/mapistore")

mapistore.set_mapping_path("/tmp/mapistore")
MAPIStore = mapistore.mapistore()
ctx_id = MAPIStore.add_context("fsocpf:///tmp/mapistore/0x0000000000010001")

SPropValue = mapi.SPropValue()
SPropValue.add(mapi.PR_PARENT_FID, 0x0000000000010001)
SPropValue.add(mapi.PR_DISPLAY_NAME, "test")
SPropValue.add(mapi.PR_COMMENT, "test folder")
SPropValue.add(mapi.PR_FOLDER_TYPE, 1)

MAPIStore.mkdir(ctx_id, 0x0000000000010001, 0x0000000000020001, SPropValue)
MAPIStore.rmdir(ctx_id, 0x0000000000010001, 0x0000000000020001, mapistore.DEL_FOLDERS)

MAPIStore.del_context(ctx_id)

