#!/usr/bin/python

import os
from lxml import etree

"""
OCSManager Client Notification documentation
"""

class ClientNotification(object):
    """Client Notification class documentation. Prepare XML request
    payloads and analyze response payloads.
    """

    def __init__(self):
        return None

    def setNewMailPayload(self, tokenLogin=None):
        """Prepare newmail payload.
        """
        if tokenLogin is None: return (True, 'User not authenticated')

        root = etree.Element('ocsmanager')
        token = etree.SubElement(root, "token")
        token.text = tokenLogin
        
        return (False, etree.tostring(root, xml_declaration=True, encoding="utf-8"))
