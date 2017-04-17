#!/usr/bin/env python2.7

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

    def setNewMailPayload(self, tokenLogin=None, newmail=None):
        """Prepare newmail payload.
        """
        if tokenLogin is None: return (True, 'User not authenticated')
        if newmail is None: return (True, 'Missing newmail arguments')

        # Sanity checks on newmail dictionary
        if not "backend" in newmail: return (True, 'Missing backend parameter')
        if not "username" in newmail: return (True, 'Missing username parameter')
        if not "folder" in newmail: return (True, 'Missing folder parameter')
        if not "msgid" in newmail: return (True, 'Missing msgid parameter')

        root = etree.Element('ocsmanager')
        token = etree.SubElement(root, "token")
        token.text = tokenLogin
        
        notification = etree.SubElement(root, "notification", category="newmail")

        backend = etree.SubElement(notification, "backend")
        backend.text = newmail['backend']

        username = etree.SubElement(notification, "username")
        username.text = newmail['username']

        folder = etree.SubElement(notification, "folder")
        folder.text = newmail['folder']

        messageID = etree.SubElement(notification, "messageID")
        messageID.text = newmail['msgid']

        return (False, etree.tostring(root, xml_declaration=True, encoding="utf-8"))
