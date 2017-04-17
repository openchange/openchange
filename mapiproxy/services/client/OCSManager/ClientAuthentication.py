#!/usr/bin/env python2.7

import os
import hashlib
from lxml import etree

from base64 import urlsafe_b64encode as encode
from base64 import urlsafe_b64decode as decode

"""
OCSManager Client Authentication documentation
"""

class ClientAuthentication(object):
    """Authentication class documentation. Prepare XML request
    payloads and analyze response payloads.
    """

    def __init__(self):
        self.username = None
        self.password = None
        self.salt = None

    def _check_document(self, payload):
        """Perform common preliminary XML checks on payload and return
        lxml object."""
        try:
            xmlData = etree.XML(payload)
        except etree.XMLSyntaxError:
            return (True, 'Invalid document')

        if xmlData.tag != "ocsmanager": return (True, 'Incorrect root element %s' % xmlData.tag)

        return (False, xmlData)

    def setTokenPayload(self, login=None):
        """Prepare the payload for token authentication phase.
        """
        root = etree.Element('ocsmanager')
        login_element = etree.SubElement(root, "login")
        login_element.text = login
        return etree.tostring(root, xml_declaration=True, encoding="utf-8")

    def getTokenPayload(self, payload=None):
        """Extract content from the token payload returned by the server.
        """
        (error, xmlData) = self._check_document(payload)
        if error is True: return (error, xmlData)

        d = {}
        token = xmlData.find('token')
        if token is None: return (True, 'No token parameter received')
        if not 'type' in token.attrib: return (True, 'No type attribute in token parameter')
        if token.attrib['type'] != 'salt': 
            return (True, 'Expected type=salt for attribute, got type=%s' % token.attrib['type'])
        d["token"] = token.text

        salt = xmlData.find('salt')
        if salt is None: return (True, 'No salt parameter received')
        d["salt"] = salt.text

        ttl = xmlData.find('ttl')
        if ttl is None: return (True, 'No TTL parameter received')
        d["ttl"] = int(ttl.text)

        return (False, d)

    def getTokenLogin(self, payload=None):
        """Extract token login from login response payload.
        """
        (error, xmlData) = self._check_document(payload)
        if error is True: return (error, xmlData)

        token = xmlData.find('token')
        if token is None: return (True, 'No token parameter received')
        if token.text is None: return (True, 'Empty token received')

        return (False, token.text)

    def setLoginPayload(self, d=None):
        """Prepare the payload for the login authentication stage.
        """

        salt = decode(d["salt"])
        salt_token = decode(d["token"])

        if d["encryption"] == "plain":
            h = hashlib.sha1(d['password'])
            h.update(salt)
            sshaPassword = "{SSHA}" + encode(h.digest() + salt)
        elif d["encryption"] == "ssha":
            sshaPassword = d['password']
        else:
            return (True, 'Unsupported encryption scheme: %s' % d["encryption"])

        payload = "%s:%s:%s" % (d["username"], sshaPassword, salt_token)
        h = hashlib.sha1(payload)
        h.update(salt_token)
        token = h.hexdigest()

        root = etree.Element('ocsmanager')
        token_element = etree.SubElement(root, 'token')
        token_element.text = token
        return (False, etree.tostring(root, xml_declaration=True, encoding="utf-8"))
