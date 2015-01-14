import sys
import re

# This only works for 1 mailbox and 1 company
MAILBOX_ID = 1
OU_ID = 1
MAILBOX_FID = 720575940379279361

class BaseDict(object):
    def __init__(self, values):
        self.values = values

    def to_sql(self):
        raise NotImplementedError("to_sql method")

    def properties(self, table_name):
        sql = [("INSERT %s VALUES (LAST_INSERT_ID(), '%s', '%s');" %
                   (table_name, name, value))
               for name, value in self.values.iteritems()
               if name.startswith('Pid') and not isinstance(value, list)]
        sql2 = [("INSERT %s VALUES (LAST_INSERT_ID(), '%s', '%s');" %
                   (table_name, name, v))
               for name, value in self.values.iteritems()
               if isinstance(value, list)
               for v in value]
        return "\n".join(sql + sql2)

    def __str__(self):
        return self.to_sql()

    def pop(self, name, varchar=False):
        if name in self.values:
            ret = self.values.pop(name)
            if varchar:
                return "'%s'" % ret
            return ret
        else:
            return 'NULL'

    def pop_folder_id(self, field='PidTagParentFolderId'):
        pfid = self.pop(field)
        if pfid == MAILBOX_FID or pfid == 'NULL':
            return 'NULL'
        else:
            return ("(SELECT id FROM (SELECT id FROM folders WHERE folder_id = %s) as t)" % pfid)


class Message(BaseDict):
    def to_sql(self):
        message_id = self.pop('PidTagMessageId')
        message_type = self.pop('objectClass', varchar=True)
        folder_id = self.pop_folder_id()
        normalized_subject = self.pop('PidTagNormalizedSubject', varchar=True)

        sql = ("INSERT INTO messages VALUES (0, %s, %s, %s, %s, %s, %s);\n" %
               (OU_ID, message_id, message_type, folder_id, MAILBOX_ID,
                normalized_subject))

        return sql + self.properties('messages_properties')


class Folder(BaseDict):
    def is_mailbox(self):
        return ('PidTagDisplayName' in self.values and
                self.values['PidTagDisplayName'].startswith('OpenChange Mailbox'))

    def to_sql(self):
        if self.is_mailbox():
            return ''

        folder_id = self.pop('PidTagFolderId')
        folder_class = self.pop('objectClass', varchar=True)
        folder_type = self.pop('FolderType')
        system_idx = self.pop('SystemIdx')
        mapi_uri = self.pop('MAPIStoreURI', varchar=True)
        parent_folder = self.pop_folder_id()
        mailbox_id = folder_class[1:13] == 'systemfolder' and MAILBOX_ID or 'NULL'

        sql = ("INSERT INTO folders VALUES (0, %s, %s, %s, %s, %s, %s, %s, %s);\n" %
               (OU_ID, folder_id, folder_class, mailbox_id, parent_folder,
                folder_type, system_idx, mapi_uri))

        return sql + self.properties('folders_properties')


valid_classes = ['systemfolder', 'publicfolder', 'paiMessage', 'systemMessage']

def sort_folders(folders):
    ret = []
    for folder in list(folders):
        if ('PidTagParentFolderId' not in folder.values or
               folder.values['PidTagParentFolderId'] == MAILBOX_ID):
            ret.append(folder)
            folders.remove(folder)
    while folders:
        for folder in list(folders):
            folder_id = folder.values['PidTagParentFolderId']
            added = False
            for f in ret:
                if f.values['PidTagFolderId'] == folder_id:
                    added = True
                    break
            if added:
                ret.append(folder)
                folders.remove(folder)
    return ret

def parse_file(file_path):
    with open(file_path, 'r') as f:
        contents = f.read()
    folders = []
    messages = []
    values = {}
    key, value = '', ''
    for l in contents.split('\n'):
        try:
            if len(l) == 0:
                if 'objectClass' in values:
                    if values['objectClass'] in ('systemfolder', 'publicfolder'):
                        folders.append(Folder(values))
                    elif values['objectClass'] in ('paiMessage', 'systemMessage'):
                        messages.append(Message(values))
                values = {}
            elif l[0] == ' ':
                value = l.strip()
                values[key] += value
            elif l[0] != '#':
                splitted = l.split(':')
                key, value = splitted[0].strip(), ':'.join(splitted[1:]).strip()
                if key == 'objectClass' and value not in valid_classes:
                    continue
                if key in values:
                    if isinstance(values[key], list):
                        values[key].append(value)
                    else:
                        values[key] = [values[key], value]
                else:
                    values[key] = value
        except Exception as ex:
            print "Error for line %s: %r" % (l, ex)
    return sort_folders(folders) + messages

# ----------------------------------------------------------------------------

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print "Use %s input.ldif [output.sql]" % (sys.executable,)
        sys.exit(0)

    f = len(sys.argv) == 3 and open(sys.argv[2], 'a') or sys.stdout
    entries = parse_file(sys.argv[1])
    for entry in entries:
        print >>f, entry
        print >>f

