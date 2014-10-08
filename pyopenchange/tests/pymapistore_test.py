#!/usr/bin/python

import optparse
import os,sys

from openchange import mapistore

def print_tree(folder, indent):
    print '   '*indent + '[F]: ' + folder.get_properties(['PidTagDisplayName'])['PidTagDisplayName']
    for m in folder.messages:
        try:
            print '   '*(indent+1) + '[m]: ' + m.get_properties(['PidTagDisplayName'])['PidTagDisplayName']
        except:
            print '   '*(indent+1) + '[m]: ' + repr(m.get_properties(['PidTagMid'])['PidTagMid'])
    for f in folder.subfolders:
        print_tree(f, indent+1)
        
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
	print_tree(root_fld, 0)	
	print

# -------------------------------- #
# CREATE, MOVE & DELETE SUBFOLDERS
# -------------------------------- #
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
 
# Display the hierarchy
print "[PYMAPISTORE] INBOX hierarchy:"
print_tree(in_fld, 0)
print
 
# Display and modify folder properties
print '[PYMAPISTORE] Get FOO properties:'
print foo_fld.get_properties();
print
 
print "[PYMAPISTORE] Set FOO 'PidTagContentCount' property:"
print foo_fld.get_properties(['PidTagContentCount']);
print
 
foo_fld.set_properties({'PidTagContentCount': 2L});
 
print foo_fld.get_properties(['PidTagContentCount']);
print
 
# Set back to original value
foo_fld.set_properties({'PidTagContentCount': 0L});
print
 
### Copy folder ###
print '[PYMAPISTORE] Duplicate BLAH (copy into FOO):'
try:
    blah_fld.copy_folder(foo_fld, 'BLAHC', mapistore.NON_RECURSIVE)
except:
    print "[PYMAPISTORE][ERR] Can't copy folder"
    print sys.exc_info()[0]
    print
    
print "[PYMAPISTORE] INBOX hierarchy:"
print_tree(in_fld, 0)
print
 
# Move folder
print '[PYMAPISTORE] Move FOO subfolder to BLAH as FOOM:'
 
try:
	foo_fld.move_folder(blah_fld, 'FOOM')
except:
	print "[PYMAPISTORE][ERR] Can't move folder"
	print sys.exc_info()[0]
	print
 
	print '[PYMAPISTORE][ERR] Delete FOO folder:'
	foo_fld.delete(mapistore.DEL_ALL)
	print
  
print "[PYMAPISTORE] INBOX hierarchy:"
print_tree(in_fld, 0)
print
 
print '[PYMAPISTORE] Display FOOM properties:'
foom_fld = blah_fld.open_folder('sogo://user1:user1@mail/folderINBOX/foldersampleblah/folderFOOM/')
print foom_fld.get_properties();
print
 
# Copy folder
print '[PYMAPISTORE] Copy FOOM into INBOX as FOOC:'
try:
	foom_fld.copy_folder(in_fld, 'FOOC', mapistore.RECURSIVE)
except:
	print "[PYMAPISTORE][ERR] Can't copy folder"
	print sys.exc_info()[0]
	print
    
print "[PYMAPISTORE] INBOX hierarchy:"
print_tree(in_fld, 0)
print
 
print '[PYMAPISTORE] Display FOOC properties:'
fooc_fld = in_fld.open_folder('sogo://user1:user1@mail/folderINBOX/folderFOOC/')
print fooc_fld.get_properties();
print
 
# Clean up
print '[PYMAPISTORE] Delete FOOC folder:'
fooc_fld.delete(mapistore.DEL_ALL)
print
 
print '[PYMAPISTORE] Delete FOOM folder:'
try:
    foom_fld.delete(mapistore.DEL_ALL)
except:
    print '[PYMAPISTORE][ERR] Cannot delete FOOM folder. Use stubbornness'
    foom_fld.delete(mapistore.DEL_ALL)
print

# ----------------------------------- #
# TEST CREATE, MOVE AND COPY MESSAGES
# ----------------------------------- #

# Create message
print '[PYMAPISTORE] Create FOO message in BLAH:'
blah_msg = blah_fld.create_message(mapistore.MESSAGE_GENERIC)
print '[PYMAPISTORE] URI: ' + blah_msg.uri
print
 
print '[PYMAPISTORE] Display message data:'
blah_msg_props = blah_msg.get_properties()
print blah_msg_props
print
 
print '[PYMAPISTORE] Modify message properties:'
blah_msg.set_properties({'PidTagSubject': 'blahmsg', 'PidTagNormalizedSubject': 'blahmsg', 'PidTagBody': 'Hello, World!' })
print
 
print '[PYMAPISTORE] Save message:'
blah_msg.save()
print 
 
print "[PYMAPISTORE] INBOX hierarchy:"
print_tree(in_fld, 0)
print 

print '[PYMAPISTORE] Add Sent context:'
sent_ctx = mstore.add_context('sogo://user1:user1@mail/folderSent')
print

print '[PYMAPISTORE] Open Sent root folder:'
sent_fld = sent_ctx.open()
print

print '[PYMAPISTORE] Sent hierarchy:'
print_tree(sent_fld,0)
print

print '[PYMAPISTORE] Copy child messages to INBOX:'
uri_list = []
for m in sent_fld.messages:
    uri_list.append(m.uri)

sent_fld.copy_messages(uri_list, in_fld)
print
 
print '[PYMAPISTORE] INBOX hierarchy:'
print_tree(in_fld, 0)
print

# Backend returns an invalid pointer for the data associated to 'PidTagInternetMessageId' 
print '[PYMAPISTORE] Modify message properties:'
uri_list = []
for m in in_fld.messages:
    uri_list.append(m.uri)
print

in_msg = in_fld.open_message(uri_list[0], mapistore.OPEN_WRITE)

in_msg.set_properties({'PidTagInternetMessageId': 'FOO'})

print '[PYMAPISTORE] Display message properties:'
in_msg_props = in_msg.get_properties()
print in_msg_props
print

print '[PYMAPISTORE] Move child messages into BLAH:'
in_fld.move_messages(uri_list, blah_fld)
print
 
print '[PYMAPISTORE] INBOX hierarchy:'
print_tree(in_fld, 0)

print '[PYMAPISTORE] Display message data:'
in_msg_data = in_msg.get_data();
print in_msg_data
print

# Clean up
print '[PYMAPISTORE] BLAH child messages (delete them once shown):'
for m in blah_fld.messages:
    blah_fld.delete_message(m.uri, mapistore.PERMANENT_DELETE)
print

print_tree(in_fld, 0)

# ----------------------------------- #
# TEST CREATE AND ITERATE OVER TABLES
# ----------------------------------- #

print '[PYMAPISTORE] Open INBOX folder table'
in_fld_tbl = in_fld.open_table(mapistore.FOLDER_TABLE)
print

print '[PYMAPISTORE] Row count: ' + repr(in_fld_tbl.count)
print

print "[PYMAPISTORE] Set colums to ['PidTagLastModificationTime', 'PidTagFolderId']"
in_fld_tbl.columns = ['PidTagLastModificationTime', 0x67480014]
print

print '[PYMAPISTORE] Iterate over the rows and display its content'
for r in in_fld_tbl.rows:
    print r
printprint

# ------------------------------- #
# TEST ATTACHMENT IMPLIEMENTATION
# ------------------------------- #
dont_fld = in_fld.open_folder('sogo://user1:user1@mail/folderINBOX/folderdonottouch/')
dos_msg = dont_fld.open_message('sogo://user1:user1@mail/folderINBOX/folderdonottouch/2.eml')
att_table = dos_msg.open_attachment_table()
att_table.columns = ['PidTagAttachNumber']
att = dos_msg.open_attachment(att_table.get_row(0)['PidTagAttachNumber'])