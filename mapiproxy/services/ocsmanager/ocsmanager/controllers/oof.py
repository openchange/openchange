import logging
import traceback
import urllib
import base64
import struct
import ldb

from pylons import request, response, session, tmpl_context as c, url
from pylons.controllers.util import abort, redirect
from pylons.decorators.rest import restrict
from pylons import config
from xml.etree.ElementTree import Element, ElementTree, tostring, register_namespace
from cStringIO import StringIO
from time import time, strftime, localtime

from ocsmanager.lib.base import BaseController, render

log = logging.getLogger(__name__)

namespaces = {
    'q': 'http://schemas.xmlsoap.org/soap/envelope/',
    'm': 'http://schemas.microsoft.com/exchange/services/2006/messages',
    't': 'http://schemas.microsoft.com/exchange/services/2006/types',
}

class ServerException(Exception):
    pass

class InvalidRequestException(Exception):
    pass

class AccessDeniedException(Exception):
    pass

class DbException(Exception):
    pass

class OofHandler(object):
    """
    This class parses the XML request, interprets it, find the requested
    answers and spills back an appropriate XML response.
    """

    def __init__(self, env):
        self.http_server_name = None
        self.target = None
        self.username = None
        self.workstation = None

        # Set the auth info
        self.decode_ntlm_auth(env)
        # Set the http_server_name
        server_env_names = iter(["HTTP_X_FORWARDED_SERVER",
                                 "HTTP_X_FORWARDED_HOST",
                                 "HTTP_HOST"])
        try:
            while self.http_server_name == None:
                env_name = server_env_names.next()
                if env_name in env:
                    self.http_server_name = (env[env_name].split(":"))[0]
        except StopIteration:
            pass

    def decode_ntlm_auth(self, env):
        """
        Decode the HTTP_AUTHORIZATION header and extract the target domain,
        the username and the workstation
        """
        header = env['HTTP_AUTHORIZATION']
        if header.startswith('NTLM '):
            blob_b64 = header[5:];
            blob = base64.b64decode(blob_b64)
            (signature, msgtype) = struct.unpack('@ 8s I', blob[0:12])
            if (msgtype == 3):
                (tgt_len, tgt_alloc, tgt_offset) = struct.unpack('@h h I', blob[28:36])
                if tgt_len > 0:
                    self.target = blob[tgt_offset:tgt_offset + tgt_len]
                    self.target = self.target.replace('\0', '')

                (user_len, user_alloc, user_offset) = struct.unpack('@h h I', blob[36:44])
                if user_len > 0:
                    self.username = blob[user_offset:user_offset + user_len]
                    self.username = self.username.replace('\0', '')
                    self.username = self.username.split('@')[0]

                (wks_len, wks_alloc, wks_offset) = struct.unpack('@h h I', blob[44:52])
                if wks_len > 0:
                    self.workstation = blob[wks_offset:wks_offset + wks_len]
                    self.workstation = self.workstation.replace('\0', '')

        if self.username is None:
            raise ServerException('User name not found in request NTLM authorization token')


    def fetch_ldb_record(self, ldb_filter):
        """
        Fetchs a record from LDB
        """
        samdb = config["samba"]["samdb_ldb"]
        base_dn = config["samba"]["domaindn"]
        res = samdb.search(base=base_dn, scope=ldb.SCOPE_SUBTREE,
                           expression=ldb_filter, attrs=["*"])
        if len(res) == 1:
            ldb_record = res[0]
        else:
            raise DbException('Error fetching database entry. Expected one result but got %s' % len(res))

        return ldb_record

    def check_mailbox(self, request_mailbox):
        """
        Checks that the mailbox specified in the request belongs to the user
        who is making the request
        """
        user_ldb_record = self.fetch_ldb_record("(&(objectClass=user)(samAccountName=%s))" % self.username)
        mbox_ldb_record = self.fetch_ldb_record("(&(objectClass=user)(mail=%s))" % request_mailbox)

        user_sid = user_ldb_record['objectSID'][0]
        mbox_sid = mbox_ldb_record['objectSID'][0]

        if user_sid == mbox_sid:
            return True

        samdb = config["samba"]["samdb_ldb"]
        # ID of the user who is making the request
        user_sid = samdb.schema_format_value('objectSid', user_sid)
        # Mailbox ID of the mailbox for which the attempt was made
        mbox_sid = samdb.schema_format_value('objectSid', mbox_sid)
        raise AccessDeniedException('Microsoft.Exchange.Data.Storage.AccessDeniedException: '
                                    'User is not mailbox owner. User = %s, MailboxGuid = %s '
                                    '---> User is not mailbox owner.' % (user_sid, mbox_sid))

    def process(self, request):
        """
        Process SOAP request
        """
        if request.body is not None and len(request.body) > 0:
            body = urllib.unquote_plus(request.body)
            tree = ElementTree(file=StringIO(body))
            envelope = tree.getroot()
            if envelope is None:
                raise InvalidRequestException('Invalid syntax')
            body = envelope.find("q:Body", namespaces = namespaces)
            if body is None:
                raise InvalidRequestException('Invalid syntax')

            soap_req = body.find("m:GetUserOofSettingsRequest", namespaces=namespaces)
            if soap_req is not None:
                return self.process_get_request(soap_req)

            soap_req = body.find("m:SetUserOofSettingsRequest", namespaces=namespaces)
            if soap_req is not None:
                return self.process_set_request(soap_req)

            raise InvalidRequestException('Unknown SOAP request')

        raise InvalidRequestException('No body in request')

    def process_get_request(self, elem):
        # Prepare the response
        envelope_element = Element("{%s}Envelope" % namespaces['q'])

        # Create the header
        header_element = Element("{%s}Header" % namespaces['t'])
        envelope_element.append(header_element)

        ServerVersionInfo = Element("{%s}ServerVersionInfo" % namespaces['t'])
        ServerVersionInfo.set('MajorVersion', '8')
        ServerVersionInfo.set('MinorVersion', '1')
        ServerVersionInfo.set('MajorBuildNumber', '240')
        ServerVersionInfo.set('MinorBuildNumber', '5')
        header_element.append(ServerVersionInfo)

        body_element = Element("{%s}Body" % namespaces['q'])
        envelope_element.append(body_element)

        # Check that the mailbox specified in the request belong to the user
        # who is making the request
        try:
            mailbox_element = elem.find("t:Mailbox", namespaces=namespaces)
            address_element = mailbox_element.find("t:Address", namespaces=namespaces)
            self.check_mailbox(address_element.text)
        except Exception as e:
            fault_element = Element("{%s}Fault" % namespaces['q'])
            body_element.append(fault_element)

            fault_code_element = Element("FaultCode")
            if isinstance(e, AccessDeniedException):
                fault_code_element.text = "Client"
            else:
                fault_code_element.text = "Server"
            fault_element.append(fault_code_element)

            fault_string_element = Element("FaultString")
            fault_string_element.text = e
            fault_element.append(fault_string_element)

            fault_actor_element = Element("FaultActor")
            fault_actor_element.text = self.workstation
            fault_element.append(fault_actor_element)

            fault_detail_element = Element("Detail")
            fault_element.append(fault_detail_element)
            if isinstance(e, AccessDeniedException):
                error_code_element = Element("{%s}ErrorCode" % namespaces['m'])
                error_code_element.text = "-2146233088"
                fault_detail_element.append(error_code_element)

            response_string = "<?xml version='1.0' encoding='utf-8'?>\n"
            response_string += tostring(envelope_element, encoding='utf-8', method='xml')
            return response_string

        # Build the command response
        response_element = Element("{%s}GetUserOofSettingsResponse" % namespaces['m'])
        body_element.append(response_element)

        # TODO Retrieve the OOF status for the mailbox

        # Attach info to response
        response_message_element = Element("ResponseMessage")
        response_message_element.set('ResponseClass', 'Success')
        response_element.append(response_message_element)

        response_code_element = Element("ResponseCode")
        response_code_element.text = "NoError"
        response_message_element.append(response_code_element)

        oof_settings_element = Element("{%s}OofSettings" % namespaces['t'])
        response_element.append(oof_settings_element)

        oof_state_element = Element("OofState")
        oof_state_element.text = "Disabled"
        oof_settings_element.append(oof_state_element)

        external_audience_element = Element("ExternalAudience")
        external_audience_element.text = "All"
        oof_settings_element.append(external_audience_element)

        duration_element = Element("Duration")
        oof_settings_element.append(duration_element)

        StartTime = Element("StartTime")
        StartTime.text = "2014-01-13T18:00:00Z"
        duration_element.append(StartTime)

        EndTime = Element("EndTime")
        EndTime.text = "2014-01-14T18:00:00Z"
        duration_element.append(EndTime)

        InternalReply = Element("InternalReply")
        oof_settings_element.append(InternalReply)

        MessageInternal = Element("Message")
        MessageInternal.text = "I am out of office. This is my internal response"
        InternalReply.append(MessageInternal)

        ExternalReply = Element("ExternalReply")
        oof_settings_element.append(ExternalReply)

        MessageExternal = Element("Message")
        MessageExternal.text = "I am out of office. This is my external response"
        ExternalReply.append(MessageExternal)

        AllowExternalOof = Element("AllowExternalOof")
        AllowExternalOof.text = "All"
        response_element.append(AllowExternalOof)

        response_string = "<?xml version='1.0' encoding='utf-8'?>\n"
        response_string += tostring(envelope_element, encoding='utf-8', method='xml')
        return response_string

    def process_set_request(self, elem):
        raise ServerException('Not implemented')

class OofController(BaseController):
    """The constroller class for OutOfOffice requests."""

    @restrict('POST')
    def index(self, **kwargs):
        try:
            if "environ" in kwargs:
                environ = kwargs["environ"]
            else:
                environ = None

            rqh = OofHandler(environ)
            response.headers["content-type"] = "text/xml"
            body = rqh.process(request)
        except:
            response.status = 500
            response.headers["content-type"] = "text/plain"
            # TODO: disable the showing of exception in prod
            log.error(traceback.format_exc())
            body = "An exception occurred:\n" + traceback.format_exc()

        return body
