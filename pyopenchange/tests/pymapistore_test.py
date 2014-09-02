#!/usr/bin/python

import optparse
import os,sys

from openchange import mapistore

parser = optparse.OptionParser(usage="%prog --username=<username> [options]", version="%prog 1.0")
parser.add_option("--username", type="string", help="Set the username to use with mapistore")
parser.add_option("--mapping-path", type="string", help="Specify mapistore mapping path")
parser.add_option("--openchangedb", type="string", help="Specify openchangedb URL")

group = optparse.OptionGroup(parser, "Debug Options")
group.add_option("-d", "--debuglevel", type="int", dest="debuglevel", default=0)
parser.add_option_group(group)

opts,args = parser.parse_args()

if not opts.username:
    print "[ERR] --username option required"
    parser.print_help()
    sys.exit(1)

if opts.mapping_path:
    mapistore.set_mapping_path(opts.mapping_path)

mstore = mapistore.MAPIStore()

if opts.debuglevel is not None:
    mstore.debuglevel = opts.debuglevel

if opts.openchangedb is not None:
    mstore.set_parm("mapiproxy:openchangedb", opts.openchangedb)

mstore.dump()

print "[PYMAPISTORE] Initialise MAPIStore for user " + opts.username

mstore.initialize(opts.username)
print

print '[PYMAPISTORE] List backends:'
print

backends = mstore.list_backends()
for i in backends:
	print i
print

'[PYMAPISTORE] List subfolders and messages for each context'
print

cap_list = mstore.capabilities()
for cap in cap_list:
	name = cap['name']
	if name == 'Fallback': 
		continue
	
	if name:
		print '[CONTEXT]: ' + name
	
	uri = cap['url']
	ctx = mstore.add_context(uri)
	root_fld = ctx.open()
	
	subfld_count = root_fld.get_child_count(1) 
	print '[NUMBER OF SUBFOLDERS]: ' + repr(subfld_count)
	if subfld_count > 0:
		for fld in root_fld.get_child_folders():
			print '[SUBFOLDER]: ' + fld.get_uri()
	msg_count = root_fld.get_child_count(2)
	print '[NUMBER OF MESSAGES]: ' + repr(msg_count)
	if msg_count > 0:
		for msg in root_fld.get_child_messages():
			print '[MESSAGE]: ' + msg.get_uri()
	print
