#!/usr/bin/python

import os,sys

root = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.sep.join((root, '..', '..')))

class BackendObject(object):

    name = "sample"
    description = "Sample backend"
    namespace = "sample://"

    def __init__(self):
        print '[PYTHON]: %s backend class __init__' % self.name
        return

    def init(self):
        """ Initialize sample backend
        """
        print '[PYTHON]: %s backend.init: init()' % self.name
        return 0

    def list_contexts(self, username):
        """ List context capabilities of this backend.
        """
        print '[PYTHON]: %s backend.list_contexts(): username = %s' % (self.name, username)
        contexts = [{ "inbox", "INBOX", "calendar", "CALENDAR" }]
        return (0, contexts)

    def create_context(self, uri):
        """ Create a context.
        """

        print '[PYTHON]: %s backend.create_context: uri = %s' % (self.name, uri)
        context = ContextObject()

        return (0, context)


class ContextObject(BackendObject):

    mapping = {}

    def __init__(self):
        self.mapping[0xdeadbeef] = "sample://deadbeef"
        self.mapping[0x1] = "sample://deadbeef/subfolder_1"
        print '[PYTHON]: %s context class __init__' % self.name
        return

    def get_path(self, fmid):
        print '[PYTHON]: %s context.get_path' % self.name
        if fmid in self.mapping:
            print '[PYTHON]: %s get_path URI: %s' % (self.name, self.mapping[fmid])
            return self.mapping[fmid]
        else:
            print '[Python]: %s get_path URI: None' % (self.name)
            return None

    def get_root_folder(self, folderID):
        print '[PYTHON]: %s context.get_root_folder' % self.name
        folder = FolderObject(folderID, 0x0)
        return (0, folder)


class FolderObject(ContextObject):

    def __init__(self, folderID, parentFID):
        print '[PYTHON]: %s folder.__init__(fid=%s)' % (self.name, folderID)
        self.parentFID = parentFID;
        self.folderID = folderID;
        return

    def open_folder(self, folderID):
        print '[PYTHON]: %s folder.open_folder(%s)' % (self.name, folderID)

        if folderID in self.mapping:
            print '[PYTHON]: folderID %s found\n' % (folderID)
            return FolderObject(folderID, self.folderID)

        return None
