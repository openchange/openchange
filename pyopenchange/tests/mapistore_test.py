#!/usr/bin/python

import os
import openchange
import openchange.mapistore as mapistore

os.mkdir("/tmp/mapistore")
mapistore.set_mapping_path("/tmp/mapistore")
MAPIStore = mapistore.mapistore()
ctx_id = MAPIStore.add_context("fsocpf:///tmp/mapistore/mapistore_test")
ctx_id2 = MAPIStore.add_context("fsocpf:///tmp/mapistore/mapistore_test2")
print "ctx_id = %d ctx_id2 = %d" % (ctx_id, ctx_id2)

ctx_id3 = MAPIStore.search_context_by_uri("/tmp/mapistore/mapistore_test")
print "ctx_id = %d and ctx_id3 = %d should be the same" % (ctx_id, ctx_id3)

MAPIStore.del_context(ctx_id3)
MAPIStore.del_context(ctx_id2)

