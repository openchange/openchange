#!/usr/bin/python

import os,sys

root = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.sep.join((root, '..', '..')))

class BackendObject(object):

    name = "sample"
    description = "Sample backend"
    namespace = "sample://"

    def __init__(self):
        print '[PYTHON]: backend class __init__'
        return

    def init(self):
        """ Initialize sample backend
        """
        print '[PYTHON]: backend.init: init()'
        return 0


    def create_context(self, uri):
        """ Create a context.
        """
        context = {}
        context['URI'] = uri

        print '[PYTHON]: backend.create_context: uri = %s' % uri
        return (0, context)

    def get_root_folder(self, folderID):
        return (0, None)
