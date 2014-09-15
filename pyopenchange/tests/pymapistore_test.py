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

# Create MAPIStore Object
mstore = mapistore.MAPIStore()

# Change log_level
if opts.debuglevel is not None:
    mstore.debuglevel = opts.debuglevel

if opts.openchangedb is not None:
    mstore.set_parm("mapiproxy:openchangedb", opts.openchangedb)

# Display info
mstore.dump()

# Initialise MAPIStore
print "[PYMAPISTORE] Initialise MAPIStore for user " + opts.username
mstore.initialize(opts.username)
print

# List backends
print '[PYMAPISTORE] List backends:'
backends = mstore.list_backends()
for i in backends:
	print i
print

# Get available contexts 
print '[PYMAPISTORE] List subfolders and messages for each context:'
 
cap_list = mstore.capabilities()
  
# Iterate over the available contexts and open root folder, subfolders and messages
for cap in cap_list:
	name = cap['name']
	if name:
		print '[CONTEXT]: ' + name
 	
	uri = cap['url']
	try:
		ctx = mstore.add_context(uri)
	except:
		print "[PYMAPISTORE][ERR]: '" + name + "' couldn't be opened"
		print
		continue
 
	root_fld = ctx.open()
 	
	subfld_count = root_fld.get_child_count(mapistore.FOLDER_TABLE) 
	print '[NUMBER OF SUBFOLDERS]: ' + repr(subfld_count)
	if subfld_count > 0:
		for fld in root_fld.get_child_folders():
			print '[SUBFOLDER]: ' + fld.get_uri()
	msg_count = root_fld.get_child_count(mapistore.MESSAGE_TABLE)
	print '[NUMBER OF MESSAGES]: ' + repr(msg_count)
	if msg_count > 0:
		for msg in root_fld.get_child_messages():
			print '[MESSAGE]: ' + msg.get_uri()
	print

# Create, move and delete subfolders
# *PARTICULAR TO MY ENVIRONMENT*
print '[PYMAPISTORE] Add INBOX context:'
in_ctx = mstore.add_context('sogo://user1:user1@mail/folderINBOX')
print

print '[PYMAPISTORE] Open INBOX root folder:'
in_fld = in_ctx.open()
print

print '[PYMAPISTORE] Open BLAH folder:'
blah_fld = in_fld.open_folder('sogo://user1:user1@mail/folderINBOX/foldersampleblah/')
print

print '[PYMAPISTORE] Create FOO subfolder:'
foo_fld = in_fld.create_folder('FOO')
print

# Display and modify folder properties
print '[PYMAPISTORE] Get FOO properties:'
print foo_fld.get_properties();
print

print "[PYMAPISTORE] Set FOO 'PidTagContentCount' property:"
print foo_fld.get_properties(['PidTagContentCount']);

foo_fld.set_properties({'PidTagContentCount': 2L});

print foo_fld.get_properties(['PidTagContentCount']);

# Set back to original value
foo_fld.set_properties({'PidTagContentCount': 0L});
print

# Move folder
print '[PYMAPISTORE] Move FOO subfolder to BLAH:'

try:
	foo_fld.move_folder(blah_fld, 'FOOM')
except:
	print "[PYMAPISTORE][ERR] Can't move folder"
	print sys.exc_info()[0]
	print

	print '[PYMAPISTORE][ERR] Delete FOO folder:'
	foo_fld.delete(mapistore.PERMANENT_DELETE)
	print
 
print "[PYMAPISTORE] INBOX subfolders (FOO shouldn't be one):"
for f in in_fld.get_child_folders():
	print f.get_uri()
print

print "[PYMAPISTORE] BLAH subfolders:"
for f in blah_fld.get_child_folders():
	print f.get_uri()
print

foom_fld = blah_fld.open_folder('sogo://user1:user1@mail/folderINBOX/foldersampleblah/folderFOOM/')

print '[PYMAPISTORE] Display FOOM properties:'
print foom_fld.get_properties();
print


# Clean up
print '[PYMAPISTORE] Delete FOOM:'
foom_fld.delete(mapistore.PERMANENT_DELETE)
print

