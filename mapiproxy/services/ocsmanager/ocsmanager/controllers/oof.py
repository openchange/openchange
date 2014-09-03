import logging
import codecs
import traceback
import urllib
import base64
import struct
import ldb
import os
import os.path
import shutil
import string
import json

from pylons import request, response, session, tmpl_context as c, url
from pylons.controllers.util import abort, redirect
from pylons.decorators.rest import restrict
from pylons import config
from xml.etree.ElementTree import Element, ElementTree, tostring
from xml.etree.ElementTree import register_namespace
from cStringIO import StringIO
from time import time, strftime, localtime
from pwd import getpwnam
from ocsmanager.lib.base import BaseController, render
from string import Template

log = logging.getLogger(__name__)

namespaces = {
    'q': 'http://schemas.xmlsoap.org/soap/envelope/',
    'm': 'http://schemas.microsoft.com/exchange/services/2006/messages',
    't': 'http://schemas.microsoft.com/exchange/services/2006/types',
    'e': 'http://schemas.microsoft.com/exchange/services/2006/errors',
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
            while self.http_server_name is None:
                env_name = server_env_names.next()
                if env_name in env:
                    self.http_server_name = (env[env_name].split(":"))[0]
        except StopIteration:
            pass

    def decode_ntlm_auth(self, env):
        """Decode the HTTP_AUTHORIZATION header

        Extract the target domain, the username and the workstation from
        HTTP_AUTHORIZATION header
        """
        header = env['HTTP_AUTHORIZATION']
        if header.startswith('NTLM '):
            blob_b64 = header[5:]
            blob = base64.b64decode(blob_b64)
            (signature, msgtype) = struct.unpack('@ 8s I', blob[0:12])
            if (msgtype == 3):
                (tgt_len, tgt_alloc, tgt_offset) = \
                    struct.unpack('@h h I', blob[28:36])
                if tgt_len > 0:
                    self.target = blob[tgt_offset:tgt_offset + tgt_len]
                    self.target = self.target.decode('UTF-16')

                (user_len, user_alloc, user_offset) = \
                    struct.unpack('@h h I', blob[36:44])
                if user_len > 0:
                    self.username = blob[user_offset:user_offset + user_len]
                    self.username = self.username.decode('UTF-16')
                    self.username = self.username.split('@')[0]

                (wks_len, wks_alloc, wks_offset) = \
                    struct.unpack('@h h I', blob[44:52])
                if wks_len > 0:
                    self.workstation = blob[wks_offset:wks_offset + wks_len]
                    self.workstation = self.workstation.decode('UTF-16')

        if self.username is None:
            raise ServerException('User name not found in request NTLM '
                                  'authorization token')

    def fetch_ldb_record(self, ldb_filter):
        """
        Fetchs a record from LDB
        """
        samdb = config["samba"]["samdb_ldb"].get_samdb()
        base_dn = config["samba"]["domaindn"]
        res = samdb.search(base=base_dn, scope=ldb.SCOPE_SUBTREE,
                           expression=ldb_filter, attrs=["*"])
        if len(res) == 1:
            ldb_record = res[0]
        else:
            raise DbException('Error fetching database entry. Expected '
                              'one result but got %s' % len(res))

        return ldb_record

    def check_mailbox(self, request_mailbox):
        """Check the mailbox belongs to user

        Checks that the mailbox specified in the request belongs to the user
        who is making the request
        """
        user_ldb_record = self.fetch_ldb_record(
            "(&(objectClass=user)(samAccountName=%s))" % self.username)
        mbox_ldb_record = self.fetch_ldb_record(
            "(&(objectClass=user)(mail=%s))" % request_mailbox)

        user_sid = user_ldb_record['objectSID'][0]
        mbox_sid = mbox_ldb_record['objectSID'][0]

        if user_sid == mbox_sid:
            return True

        samdb = config["samba"]["samdb_ldb"].get_samdb()
        # ID of the user who is making the request
        user_sid = samdb.schema_format_value('objectSid', user_sid)
        # Mailbox ID of the mailbox for which the attempt was made
        mbox_sid = samdb.schema_format_value('objectSid', mbox_sid)
        raise AccessDeniedException(
            'Microsoft.Exchange.Data.Storage.AccessDeniedException: '
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
            body = envelope.find("q:Body", namespaces=namespaces)
            if body is None:
                raise InvalidRequestException('Invalid syntax')

            soap_req = body.find("m:GetUserOofSettingsRequest",
                                 namespaces=namespaces)
            if soap_req is not None:
                return self.process_get_request(soap_req)

            soap_req = body.find("m:SetUserOofSettingsRequest",
                                 namespaces=namespaces)
            if soap_req is not None:
                return self.process_set_request(soap_req)

            raise InvalidRequestException('Unknown SOAP request')

        raise InvalidRequestException('No body in request')

    def _header_element(self):
        header_element = Element("{%s}Header" % namespaces['q'])

        ServerVersionInfo = Element("{%s}ServerVersionInfo" % namespaces['t'])
        ServerVersionInfo.set('MajorVersion', '8')
        ServerVersionInfo.set('MinorVersion', '1')
        ServerVersionInfo.set('MajorBuildNumber', '240')
        ServerVersionInfo.set('MinorBuildNumber', '5')
        header_element.append(ServerVersionInfo)
        return header_element

    def _body_element(self):
        return Element("{%s}Body" % namespaces['q'])

    def _fault_element_from_exception(self, e):
        fault_element = Element("{%s}Fault" % namespaces['q'])
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
        return fault_element

    def _address_from_request(self, elem):
        mailbox_element = elem.find("t:Mailbox", namespaces=namespaces)
        address_element = mailbox_element.find("t:Address",
                                               namespaces=namespaces)
        return address_element.text

    def _response_string(self,  envelope_element):
        response_string = "<?xml version='1.0' encoding='utf-8'?>\n"
        response_string += tostring(envelope_element,
                                    encoding='utf-8', method='xml')
        return response_string

    def process_get_request(self, elem):
        # Prepare the response
        envelope_element = Element("{%s}Envelope" % namespaces['q'])

        header_element = self._header_element()
        envelope_element.append(header_element)

        body_element = self._body_element()
        envelope_element.append(body_element)

        # Check that the mailbox specified in the request belong to the user
        # who is making the request
        mailbox = self._address_from_request(elem)
        try:
            self.check_mailbox(mailbox)
        except Exception as e:
            fault_element = self._fault_element_from_exception(e)
            body_element.append(fault_element)
            return self._response_string(envelope_element)

        # Retrieve OOF settings
        oof = OofSettings()
        log.debug("Loading OOF settings from sieve script")
        oof.from_sieve(mailbox)

        # Build the command response
        response_element = Element("{%s}GetUserOofSettingsResponse" %
                                   namespaces['m'])
        body_element.append(response_element)

        # Attach info to response
        response_message_element = Element("ResponseMessage")
        response_message_element.set('ResponseClass', 'Success')
        response_element.append(response_message_element)

        response_code_element = Element("ResponseCode")
        response_code_element.text = "NoError"
        response_message_element.append(response_code_element)

        oof_settings_element = Element("{%s}OofSettings" % namespaces['t'])
        response_element.append(oof_settings_element)

        oof.to_xml(oof_settings_element, response_element)

        return self._response_string(envelope_element)

    def process_set_request(self, elem):
        # Prepare the response
        envelope_element = Element("{%s}Envelope" % namespaces['q'])

        header_element = self._header_element()
        envelope_element.append(header_element)

        body_element = self._body_element()
        envelope_element.append(body_element)

        # Check that the mailbox specified in the request belong to the user
        # who is making the request
        mailbox = self._address_from_request(elem)
        try:
            self.check_mailbox(mailbox)
        except Exception as e:
            fault_element = self._fault_element_from_exception(e)
            body_element.append(fault_element)
            return self._response_string(envelope_element)

        settings_element = elem.find("t:UserOofSettings",
                                     namespaces=namespaces)

        response_element = Element("{%s}SetUserOofSettingsResponse" %
                                   namespaces['m'])
        body_element.append(response_element)

        response_message_element = Element("ResponseMessage")
        response_element.append(response_message_element)

        response_code_element = Element("ResponseCode")
        response_message_element.append(response_code_element)

        try:
            # Set settings
            oof = OofSettings()
            oof.from_xml(settings_element)
            oof.to_sieve(mailbox)
        except Exception as e:
            response_message_element.set('ResponseClass', 'Error')
            response_code_element.text = 'ErrorInvalidParameter'
            message_text = Element('MessageText')
            message_text.text = str(e)
            response_message_element.append(message_text)
            return self._response_string(envelope_element)

        response_message_element.set('ResponseClass', 'Success')
        response_code_element.text = "NoError"

        return self._response_string(envelope_element)


class OofSettings(object):
    """Converts xml request to sieve script and vice versa"""

    def __init__(self):
        self._sieve_script_header = "# OpenChange OOF script\n"
        self._config = {}
        self._config['state'] = None
        self._config['external_audience'] = None
        self._config['duration_start_time'] = None
        self._config['duration_end_time'] = None
        self._config['internal_reply_message'] = None
        self._config['external_reply_message'] = None

    def _mkdir(self, path):
        """Make directory and set mode, owner and group"""
        if os.path.isdir(path):
            pass
        elif os.path.isfile(path):
            raise OSError("a file with the same name as the desired "
                          "dir, '%s', already exists." % newdir)
        else:
            (head, tail) = os.path.split(path)
            if head and not os.path.isdir(head):
                self._mkdir(head)
            if tail:
                sinfo = os.stat(head)
                log.debug("Making directory '%s'" % path)
                os.mkdir(path)
                os.chmod(path, sinfo.st_mode)
                os.chown(path, sinfo.st_uid, sinfo.st_gid)

    def _sieve_path(self, mailbox):
        """Retrieve the sieve script path for a mailbox

           The default value if not configured is ~/.dovecot.sieve
        """
        # Read the sieve script path template from config
        oof_conf = config['ocsmanager']['outofoffice']
        sieve_script_path_template = oof_conf['sieve_script_path']
        sieve_script_path_mkdir = oof_conf['sieve_script_path_mkdir']
        log.debug("Sieve script path template is '%s'" %
                  sieve_script_path_template)

        # Build the expansion variables for template
        (user, domain) = mailbox.split('@')

        # Substitute in template
        t = Template(sieve_script_path_template)
        sieve_path_script = t.substitute(domain=domain, user=user,
                                         fulluser=mailbox)
        log.debug("Expanded sieve script path for mailbox '%s' is '%s'" %
                  (mailbox, sieve_path_script))

        # If sieve script path mkdir enabled create hierarchy if not exists
        (head, tail) = os.path.split(sieve_path_script)
        if (sieve_script_path_mkdir):
            self._mkdir(head)
        else:
            if not os.path.isdir(head):
                raise Exception("Sieve script directory '%s' not exists" %
                                head)

        sieve_path_config = os.path.join(head, 'oof-settings')
        sieve_path_backup = None

        if os.path.isfile(sieve_path_script):
            if not self._isOofScript(sieve_path_script):
                sieve_path_backup = sieve_path_script + '.user'
                shutil.copyfile(sieve_path_script, sieve_path_backup)
                shutil.copystat(sieve_path_script, sieve_path_backup)
        elif os.path.exists(sieve_path_script):
            raise Exception("Sieve script path '%s' exists and it is "
                            "not a regular file" % sieve_script_path)

        return (sieve_path_script, sieve_path_backup, sieve_path_config)

    def _isOofScript(self, path):
        """
        Checks if the sieve script is the Zentyal OOF script, looking for the
        header
        """
        isOof = False
        with open(path, 'r') as f:
            line = f.readline()
            isOof = (line == self._sieve_script_header)
        return isOof

    def _to_json(self):
        """
        Dump the OOF settings to a JSON string
        """
        return json.dumps(self._config)

    def from_sieve(self, mailbox):
        """
        Loads OOF settings for specified mailbox
        """
        (script_path, user_path, settings_path) = self._sieve_path(mailbox)
        if os.path.isfile(settings_path):
            with open(settings_path, 'r') as f:
                line = f.readline()
                self._config = json.loads(line)
        else:
            # Load default settings
            self._config['state'] = 'Disabled'
            self._config['external_audience'] = 'All'
            self._config['duration_start_time'] = '1970-01-01T00:00:00Z'
            self._config['duration_end_time'] = '2099-12-12T00:00:00Z'
            self._config['internal_reply_message'] = \
                base64.b64encode('I am out of office.')
            self._config['external_reply_message'] = \
                base64.b64encode('I am out of office.')

    def to_sieve(self, mailbox):
        (sieve_path_script, sieve_path_user, sieve_path_config) = \
            self._sieve_path(mailbox)

        template = """$header\n\n"""
        template += """require ["date","relational","vacation"];\n\n"""

        if self._config['state'] == 'Scheduled':
            template += ("if allof(currentdate :value "
                         "\"ge\" \"date\" \"$start\", "
                         "currentdate :value \"le\" \"date\" \"$end\") {\n")

        internal_domain = mailbox.split('@')[1]
        external_message = ''
        external_message += base64.b64decode(
            self._config['external_reply_message'])
        external_message = external_message.replace('"', '\\"')
        external_message = external_message.replace(';', '\\;')
        internal_message = ''
        internal_message += base64.b64decode(
            self._config['internal_reply_message'])
        internal_message = internal_message.replace('"', '\\"')
        internal_message = internal_message.replace(';', '\\;')

        if ((self._config['state'] == 'Scheduled') or
                (self._config['state'] == 'Enabled')):
            template += ("if address :matches :domain"
                         "\"from\" \"$internal_domain\" {\n")
            template += "vacation\n"
            template += "    :subject \"$subject\"\n"
            template += "    :days    0\n"
            template += "    :mime    \"MIME-Version: 1.0\r\n"
            template += "Content-Type: text/html; charset=UTF-8\r\n"
            template += ("<!DOCTYPE HTML PUBLIC \\\"-//W3C//DTD HTML "
                         "4.0 Transitional//EN\\\">\r\n")
            template += "$internal_message\";\n"
        if self._config['external_audience'] == 'All':
            template += "} else {\n"
            template += "vacation\n"
            template += "    :subject \"$subject\"\n"
            template += "    :days    0\n"
            template += "    :mime    \"MIME-Version: 1.0\r\n"
            template += "Content-Type: text/html; charset=UTF-8\r\n"
            template += ("<!DOCTYPE HTML PUBLIC \\\"-//W3C//DTD HTML "
                         "4.0 Transitional//EN\\\">\r\n")
            template += "$external_message\";\n"
        template += "}\n"

        if self._config['state'] == 'Scheduled':
            template += "}\n\n"

        if sieve_path_user is not None:
            template += 'include :personal "' + include + '";\n\n'

        script = string.Template(template).substitute(
            header=self._sieve_script_header,
            start=self._config['duration_start_time'],
            end=self._config['duration_end_time'],
            subject="Out of office automatic reply",
            internal_domain=internal_domain,
            external_message=external_message,
            internal_message=internal_message
        )

        if sieve_path_config is not None:
            (head, tail) = os.path.split(sieve_path_config)
            sinfo = os.stat(head)
            with open(sieve_path_config, 'w') as f:
                f.write(self._to_json())
                os.chmod(sieve_path_config, 0640)
                os.chown(sieve_path_config, sinfo.st_uid, sinfo.st_gid)

        log.info("\n\n\nSIEVE_PATH_SCRIPT %s" % sieve_path_script)
        (head, tail) = os.path.split(sieve_path_script)
        sinfo = os.stat(head)
        with open(sieve_path_script, 'w') as f:
            f.write(script.encode('utf8'))
            os.chmod(sieve_path_script, 0755)
            os.chown(sieve_path_script, sinfo.st_uid, sinfo.st_gid)

    def from_xml(self, settings_element):
        """
        Load settings from XML root element
        """
        oof_state_element = settings_element.find('t:OofState',
                                                  namespaces=namespaces)
        if oof_state_element is not None:
            self._config['state'] = oof_state_element.text

        external_audience_element = settings_element.find(
            't:ExternalAudience', namespaces=namespaces)
        if external_audience_element is not None:
            self._config['external_audience'] = external_audience_element.text
            if self._config['external_audience'] == 'Known':
                raise Exception("Reply to external contacts not implemented")

        duration_element = settings_element.find(
            't:Duration', namespaces=namespaces)
        if duration_element is not None:
            start_time_element = duration_element.find(
                't:StartTime', namespaces=namespaces)
            if start_time_element is not None:
                self._config['duration_start_time'] = \
                    start_time_element.text.split('T')[0]

            end_time_element = duration_element.find(
                't:EndTime', namespaces=namespaces)
            if end_time_element is not None:
                self._config['duration_end_time'] = \
                    end_time_element.text.split('T')[0]

        internal_reply_element = settings_element.find(
            't:InternalReply', namespaces=namespaces)
        if internal_reply_element is not None:
            message_element = internal_reply_element.find(
                't:Message', namespaces=namespaces)
            if message_element is not None:
                text = message_element.text
                bom = unicode(codecs.BOM_UTF16_LE, "UTF-16LE")
                if text.startswith(bom):
                    text = text.lstrip(bom)
                text = text.encode('UTF-8')
                self._config['internal_reply_message'] = base64.b64encode(text)

        external_reply_element = settings_element.find(
            't:ExternalReply', namespaces=namespaces)
        if external_reply_element is not None:
            message_element = external_reply_element.find(
                't:Message', namespaces=namespaces)
            if message_element is not None:
                text = message_element.text
                bom = unicode(codecs.BOM_UTF16_LE, "UTF-16LE")
                if text.startswith(bom):
                    text = text.lstrip(bom)
                text = text.encode('UTF-8')
                self._config['external_reply_message'] = base64.b64encode(text)

    def to_xml(self, oof_settings_element, response_element):
        """
        Fill the XML root element with OOF settings
        """
        if self._config['state'] is not None:
            oof_state_element = Element('OofState')
            oof_state_element.text = self._config['state']
            oof_settings_element.append(oof_state_element)

        if self._config['external_audience'] is not None:
            external_audience_element = Element("ExternalAudience")
            external_audience_element.text = self._config['external_audience']
            oof_settings_element.append(external_audience_element)

        duration_element = Element("Duration")
        oof_settings_element.append(duration_element)

        if self._config['duration_start_time'] is not None:
            StartTime = Element("StartTime")
            StartTime.text = self._config['duration_start_time']
            duration_element.append(StartTime)

        if self._config['duration_end_time'] is not None:
            EndTime = Element("EndTime")
            EndTime.text = self._config['duration_end_time']
            duration_element.append(EndTime)

        InternalReply = Element("InternalReply")
        oof_settings_element.append(InternalReply)

        if self._config['internal_reply_message'] is not None:
            MessageInternal = Element("Message")
            MessageInternal.text = base64.b64decode(
                self._config['internal_reply_message'])
            InternalReply.append(MessageInternal)

        ExternalReply = Element("ExternalReply")
        oof_settings_element.append(ExternalReply)

        if self._config['external_reply_message'] is not None:
            MessageExternal = Element("Message")
            MessageExternal.text = base64.b64decode(
                self._config['external_reply_message'])
            ExternalReply.append(MessageExternal)

        AllowExternalOof = Element("AllowExternalOof")
        AllowExternalOof.text = 'All'  # self._config['external_audience']
        response_element.append(AllowExternalOof)

    def dump(self):
        log.info("State: %s" % self._config['state'])
        log.info("External audience: %s" % self._config['external_audience'])
        log.info("Duration start: %s" % self._config['duration_start_time'])
        log.info("Duration end: %s" % self._config['duration_end_time'])
        log.info("Internal reply msg: %s" %
                 self._config['internal_reply_message'])
        log.info("External reply msg: %s" %
                 self._config['external_reply_message'])


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
        except Exception as e:
            response.status = 500
            response.headers["content-type"] = "text/plain"
            # TODO: disable the showing of exception in prod
            log.error(traceback.format_exc())
            body = "An exception occurred:\n" + traceback.format_exc()

        return body
