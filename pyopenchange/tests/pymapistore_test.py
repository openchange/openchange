#!/usr/bin/python

from openchange import mapistore

mst = mapistore.MAPIStore()

#ret = mst.set_parm("mapiproxy:openchangedb", "foo")
#print ret

mst.initialize("u1")

print mst.capabilities()
u1_inbox_ctx = mst.add_context("sogo://u1:u1@mail/folderDrafts")
