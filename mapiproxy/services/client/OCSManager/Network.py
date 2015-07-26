#!/usr/bin/env python2.7

import os
import tempfile
import StringIO
import pycurl
import urllib
from lxml import etree
from OCSManager import ClientAuthentication
from OCSManager import ClientNotification

"""
OCSManager Network documentation
"""

class Network(object):
    """Network class documentation. Perform authentication and network
    operation on OpenChange Service Manager.
    """

    def __init__(self, host=None, port=None, verbose=False):
        """Initialize OCSManager Network instance.
        """

        if host is None: return None
        if port is None: return None

        self.auth = ClientAuthentication.ClientAuthentication()
        self.curl = pycurl.Curl()
        self.host = host
        self.port = port
        self.verbose = verbose
        self.token = None
        self.base_uri = 'http://%s:%s' % (host, port)
        (self.fp,self.cookie) = tempfile.mkstemp()

    def __del__(self):
        self.curl.close()
        

    def __make_url(self, service=None):
        """Generate the URL to connect to."""
        if service is None: return None
        return "%s/%s" % (self.base_uri, service)

    def __get_error(self, data):
        """Retrieve error code and associated string if available.
        """
        if data is None: return (None, None)

        try:
            dataXML = etree.XML(data)
        except etree.XMLSyntaxError:
            return (None, None)

        error = dataXML.find('error')
        if error is None: return (None, None)

        if not "code" in error.attrib: return (None, None)
        if error.text is None: return (None, None)
        code = error.attrib["code"]
        string = error.text
        return (code, string)

    def __make_request(self, url=None, postfields=None, headers=None, data=None):
        """Build common part of the PyCurl request.
        """
        if data is None: return (True, 'Invalid StringIO buffer')
        if url is None: return (True, 'Missing URL')

        if self.verbose is True: print postfields

        self.curl.setopt(pycurl.URL, url)
        if headers is not None:
            self.curl.setopt(pycurl.HTTPHEADER, headers)
        else:
            self.curl.setopt(pycurl.HTTPHEADER, ['Content-Type: text/xml'])
        self.curl.setopt(pycurl.FOLLOWLOCATION, 1)
        self.curl.setopt(pycurl.COOKIEFILE, self.cookie)
        self.curl.setopt(pycurl.COOKIEJAR, self.cookie)
        self.curl.setopt(pycurl.WRITEFUNCTION, data.write)
        self.curl.setopt(pycurl.VERBOSE, self.verbose)
        return (False, None)

    def __exec_request(self, data=None):
        """Execute the request and perform HTTP error code and error
        XML payload sanity checks. Return response payload on success"""
        try:
            self.curl.perform()
        except pycurl.error, e:
            return (True, "(code=%s): %s" % (e.args[0], e.args[1]))

        if self.verbose is True: print data.getvalue()

        if self.curl.getinfo(self.curl.HTTP_CODE) != 200:
            return (True, self.curl.getinfo(self.curl.HTTP_CODE))

        (code, string) = self.__get_error(data.getvalue())
        if code is not None and int(code) != 200:
            return (True, "[%s]: %s" % (code, string))
        return (False, data.getvalue())
        

    def __put(self, url=None, postfields=None, headers=None):
        """Make a HTTP PUT request.
        """
        if postfields is None: return (True, 'Empty PUT payload')

        data = StringIO.StringIO()
        (error, msg) = self.__make_request(url, postfields, headers, data)
        if error is True: return (error, msg)
        self.curl.setopt(pycurl.PUT, 1)
        self.curl.setopt(pycurl.INFILESIZE, len(postfields))
        self.curl.setopt(pycurl.READFUNCTION, StringIO.StringIO(postfields).read)
        return self.__exec_request(data)


    def __post(self, url=None, postfields=None, headers=None):
        """Make a HTTP POST request.
        """
        if postfields is None: return (True, 'Empty POST payload')

        data = StringIO.StringIO()
        (error, msg) = self.__make_request(url, postfields, headers, data)
        if error is True: return (error, msg)
        self.curl.setopt(pycurl.POSTFIELDS, postfields)
        self.curl.setopt(pycurl.POST, 1)
        return self.__exec_request(data)

    def __login_token(self, login):
        """First stage of the authentication process.
        """
        postfields = self.auth.setTokenPayload(login)
        url = self.__make_url('authenticate/token')
        return self.__post(url, postfields, None)

    def __login(self, d):
        """Second stage of the authentication process.
        """
        (error, payload) = self.auth.setLoginPayload(d)
        if error is True: return (error, payload)

        url = self.__make_url('authenticate/login')
        return self.__post(url, payload, None)

    def login(self, username=None, password=None, encryption=None):
        """Log onto OCSManager service. Returns (False, None) tupple on
        success, otherwise (True, reason)
        """
        if username is None: return (True, 'Missing username parameter')
        if password is None: return (True, 'Missing password parameter')
        if encryption is None: return (True, 'Missing encryption parameter')

        (error, data) = self.__login_token(username)
        if error is True: return (error, data)

        # Retrieve parameters from reply
        (error, d) = self.auth.getTokenPayload(data)
        if error is True: return (error, d)

        d["username"] = username
        d["password"] = password
        d["encryption"] = encryption

        (error, data) = self.__login(d)
        if error is True: return (error, data)

        (error,tokenLogin) = self.auth.getTokenLogin(data)
        if error is True: return (error, data)
        self.token = tokenLogin

        return (False, None)
        
    def newmail(self, newmail):
        """Sends a newmail notification to OCSManager service.
        """
        
        if self.token is None: return (True, 'User not authenticated')

        notif = ClientNotification.ClientNotification()
        (error, payload) = notif.setNewMailPayload(self.token, newmail)
        if error is True: return (error, payload)

        url = self.__make_url('notification/newmail')
        return self.__put(url, payload, None)
        
        
