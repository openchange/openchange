# -*- coding: utf-8 -*-
#
# Copyright (C) 2012-2014  Julien Kerihuel <jkerihuel@openchange.org>
#                          Jean Raby <jraby@inverse.ca>
#                          Enrique J. Hernández <ejhernandez@zentyal.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
"""
This module provides an implementation of the HTTP autodiscover protocol.
"""
from cStringIO import StringIO
from time import time, strftime, localtime
import traceback
import urllib
import uuid
from xml.etree.ElementTree import Element, ElementTree, tostring

from pylons import request, response
from pylons import config
from pylons.decorators.rest import restrict

import ldb
from ocsmanager.lib.base import BaseController


REQUEST_XMLNS = ("http://schemas.microsoft.com/exchange/autodiscover/outlook"
                 "/requestschema/2006")
RESPONSE_XMLNS = ("http://schemas.microsoft.com/exchange/autodiscover"
                  "/responseschema/2006")
RESPONSEA_XMLNS = ("http://schemas.microsoft.com/exchange/autodiscover/outlook"
                   "/responseschema/2006a")

# NOTE: the use of this module requires proper configuration of either SCP or
# the SRV field for "_Autodiscover._tcp" in the name service. See MS-OXDISCO.


class AutodiscoverHandler(object):
    """This class parses the XML request, interprets it, find the requested
    answers and spills back an appropriate XML response.

    """

    def __init__(self, req, env):
        self.environ = env
        self.request = None
        self._parse_request(req.body)

        self.http_server_name = None
        server_env_names = iter(["HTTP_X_FORWARDED_HOST",
                                 "HTTP_X_FORWARDED_SERVER",
                                 "HTTP_HOST"])
        try:
            while self.http_server_name is None:
                env_name = server_env_names.next()
                if env_name in self.environ:
                    self.http_server_name \
                        = (self.environ[env_name].split(":"))[0]
        except StopIteration:
            pass

    def _parse_request(self, body):
        if body is not None and len(body) > 0:
            body = urllib.unquote_plus(body)
            tree = ElementTree(file=StringIO(body))
            xrq = tree.find("{%s}Request" % REQUEST_XMLNS)
            if xrq is not None:
                adrq = {}
                for child in xrq.getchildren():
                    nameidx = child.tag.find("}") + 1
                    tagname = child.tag[nameidx:]
                    adrq[tagname] = child.text
                self.request = adrq

    def _append_elements(self, top_element, tree_dict):
        for (tag, value) in tree_dict.iteritems():
            new_element = Element(tag)
            if isinstance(value, dict):
                self._append_elements(new_element, value)
            else:
                new_element.text = value
            top_element.append(new_element)

    def _fetch_user_ldb_record(self):
        samdb = config["samba"]["samdb_ldb"]

        # fetch user data
        if "LegacyDN" in self.request:
            ldb_filter = ("(legacyExchangeDN=%s)"
                          % self.request["LegacyDN"])
        elif "EMailAddress" in self.request:
            ldb_filter = ("(&(objectClass=user)(mail=%s))"
                          % self.request["EMailAddress"])
        # TODO: handle missing parameter

        base_dn = "CN=Users,%s" % config["samba"]["domaindn"]
        res = samdb.search(base=base_dn, scope=ldb.SCOPE_SUBTREE,
                           expression=ldb_filter, attrs=["*"])
        if len(res) == 1:
            ldb_record = res[0]
        else:
            ldb_record = None

        # TODO: handle invalid nbr of records
        return ldb_record

    def _fill_deployment_id(self, record):
        samdb = config["samba"]["samdb_ldb"]

        # fetch first org data
        base_dn = config["samba"]["firstorgdn"]
        res = samdb.search(base=base_dn, scope=ldb.SCOPE_BASE, attrs=["*"])
        if len(res) == 1:
            ldb_record = res[0]
            if "objectGUID" in ldb_record:
                guid = uuid.UUID(bytes=ldb_record["objectGUID"][0])
                record["DeploymentId"] = str(guid)

        # TODO: handle invalid nbr of records

    def _get_user_record(self, ldb_record):
        record = None

        if "samba" in config:
            record = {}
            if "displayName" in ldb_record:
                record["DisplayName"] = ldb_record["displayName"][0]
            if "legacyExchangeDN" in ldb_record:
                record["LegacyDN"] = ldb_record["legacyExchangeDN"][0]
            if "mail" in ldb_record:
                record["AutoDiscoverSMTPAddress"] = ldb_record["mail"][0]
            self._fill_deployment_id(record)
        else:
            raise RuntimeError("samba config not loaded")

        return record

    def _fetch_mdb_dn(self, user_ldb_record):
        mdb_dn = None

        if "homeMDB" in user_ldb_record:
            base_dn = user_ldb_record["homeMDB"][0]

            samdb = config["samba"]["samdb_ldb"]
            res = samdb.search(base=base_dn, scope=ldb.SCOPE_BASE, attrs=["*"])
            if len(res) == 1:
                ldb_record = res[0]
                if "legacyExchangeDN" in ldb_record:
                    mdb_dn = ldb_record["legacyExchangeDN"][0]
            # TODO: handle invalid nbr of records

        return mdb_dn

    def _append_user_found_response(self, resp_element, ldb_record):
        #TODO: check user_record
        response_tree = {"User": self._get_user_record(ldb_record)}
        self._append_elements(resp_element, response_tree)

        account_element = Element("Account")
        resp_element.append(account_element)

        self._append_elements(account_element, {"AccountType": "email",
                                                "Action": "settings"})

        mdb_dn = self._fetch_mdb_dn(ldb_record)

        """
        EXCH: The Protocol element contains information that the Autodiscover
        client can use to communicate with the mailbox via a remote procedure
        call (RPC). For details, see [MS- OXCRPC]. The AuthPackage element,
        the ServerVersion element, or the ServerDN element can be used. """
        prot_element = Element("Protocol")
        account_element.append(prot_element)
        response_tree = \
            {"Type": "EXCH",
             "ServerDN": config["samba"]["legacyserverdn"],
             "ServerVersion": "720082AD",  # TODO: that is from ex2010
             "MdbDN": mdb_dn,
             "Server": self.http_server_name,
             "ASUrl": "https://%s/ews/as"
                      % self.http_server_name,  # availability
             "OOFUrl": "https://%s/ews/oof"
                       % self.http_server_name,  # out-of-office
             "OABUrl": "https://%s/ews/oab"
                       % self.http_server_name,  # offline address book
             }
        self._append_elements(prot_element, response_tree)

        """
        EXPR: The Protocol element contains information that the Autodiscover
        client can use to communicate when outside the firewall, including
        RPC/HTTP connections. For details, see [MS- RPCH]. The AccountType
        element MUST be set to email. The AuthPackage element or the
        ServerVersion element can be used.
        """
        prot_element = Element("Protocol")
        account_element.append(prot_element)

        rpcproxy_server_name = self.http_server_name
        autodiscover_rpcproxy_conf = config['ocsmanager']['autodiscover:rpcproxy']
        if autodiscover_rpcproxy_conf['external_hostname'] != '__none__':  # Default value
            rpcproxy_server_name = autodiscover_rpcproxy_conf['external_hostname']

        if autodiscover_rpcproxy_conf['ssl']:
            ssl_opts = {"SSL": "On",
                        # This option can be implemented for increased security
                        "CertPrincipalName": "none"}
        else:
            ssl_opts = {"SSL": "Off"}

        response_tree = {"Type": "EXPR",
                         "Server": rpcproxy_server_name,
                         "AuthPackage": "Ntlm",
                         "ASUrl": "https://%s/ews/as"
                                  % rpcproxy_server_name,  # availability
                         "OOFUrl": "https://%s/ews/oof"
                                   % rpcproxy_server_name,  # out-of-office
                         "OABUrl": "https://%s/ews/oab"
                                   % rpcproxy_server_name}  # offline address book

        response_tree.update(ssl_opts)
        self._append_elements(prot_element, response_tree)

        """
        WEB: The Protocol element contains settings the client can use to
        connect via a Web browser. The AccountType element MUST be set to
        email. """
        # TODO: implement the above. OWA = SOGo?

    def _append_error(self, resp_element, error_code):
        error_messages = {500: "Unknown e-mail address",
                          501: "No configuration information available"
                               " for e-mail address",
                          600: "Invalid request",
                          601: "Configuration information not available",
                          602: "Error in configuration information for"
                               " e-mail address",
                          603: "An internal error occurred",
                          0: "An unknown error occurred"}

        seconds_as_float = time()
        seconds_as_int = int(seconds_as_float)
        millisecs = int((seconds_as_float - seconds_as_int) * 1000000)
        time_val = "%s.%d" % (strftime("%2H:%2M:%2S",
                                       localtime(seconds_as_int)),
                              millisecs)

        server_id = abs(hash(self.http_server_name))
        error_element = Element("Error", {"Time": time_val,
                                          "Id": str(server_id)})
        resp_element.append(error_element)

        if error_code in error_messages:
            message = error_messages[error_code]
        else:
            message = error_messages[0]

        response_tree = {"ErrorCode": str(error_code),
                         "Message": message}
        self._append_elements(error_element, response_tree)
        error_element.append(Element("DebugData"))

    def response(self):
        """The method the actually performs the class's parsing and
        interpretation work.

        Returns a fully formatted XML response.
        """

        ## TODO:
        # error handling when self.request is none
        # error handling when samba_ldb is not available

        top_element = Element("Autodiscover", {"xmlns": RESPONSE_XMLNS})
        resp_element = Element("Response")
        top_element.append(resp_element)

        if self.request is not None:
            ldb_record = self._fetch_user_ldb_record()

            if ldb_record is None:
                self._append_error(resp_element, 500)
            else:
                resp_element.set("xmlns", RESPONSEA_XMLNS)
                self._append_user_found_response(resp_element, ldb_record)
        else:
            self._append_error(resp_element, 600)

        body = ("<?xml version='1.0' encoding='utf-8'?>\n"
                + tostring(top_element, "utf-8"))
        return body


class AutodiscoverController(BaseController):
    """The constroller class for Autodiscover requests."""

    @restrict('POST', 'GET')
    def autodiscover(self, **kwargs):
        #TODO: authentication
        try:
            if "environ" in kwargs:
                environ = kwargs["environ"]
            else:
                environ = None

            rqh = AutodiscoverHandler(request, environ)
            response.headers["content-type"] = "application/xml"
            body = rqh.response()
        except:
            response.status = 500
            response.headers["content-type"] = "text/plain"
            # TODO: disable the showing of exception in prod
            body = "An exception occurred:\n" + traceback.format_exc()

        return body
