#!/usr/bin/python

from openchange import mapistore

mst = mapistore.MAPIStore()

#ret = mst.set_parm("mapiproxy:openchangedb", "foo")
#print ret

mst.initialize()

print mst.list_contexts_for_user("u1")
notes_ctx_u2 = mst.add_context("sogo://u1:u1@mail/folderINBOX", "u2")
