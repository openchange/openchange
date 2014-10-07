from openchange import mapistore

mstore = mapistore.MAPIStore()
mstore.initialize('user1')

ctx = mstore.add_context('sample://deadbeef0000001/')
root_fld = ctx.open()

root_uri = root_fld.uri
root_name = root_fld.get_properties(['PidTagDisplayName'])['PidTagDisplayName']

print
print '*** Root folder: ' + root_name
print '*** Root folder URI: ' + root_uri
print

subfolder_count = root_fld.get_child_count(mapistore.FOLDER_TABLE)
message_count = root_fld.get_child_count(mapistore.MESSAGE_TABLE)

print
print "*** Number of folders: " + repr(subfolder_count)
print "*** Number of messages: " + repr(message_count)
print

mid_list = []
for m in root_fld.messages:
       mid_list.append(m.mid)


sample_msg = root_fld.sample_open_message(mid_list[0])
attachment_count = sample_msg.attachment_count
print
print "*** Number of attachments: " + repr(attachment_count)
print

sample_msg.delete_attachment(0)

attachment_count = sample_msg.attachment_count
print
print "*** Number of attachments: " + repr(attachment_count)
print
