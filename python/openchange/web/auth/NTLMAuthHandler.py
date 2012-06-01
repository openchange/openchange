# NTLMAuthHandler.py -- OpenChange RPC-over-HTTP implementation
#
# Copyright (C) 2012  Wolfgang Sourdeau <wsourdeau@inverse.ca>
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

"""This module provides the NTLMAuthHandler class, a WSGI middleware that
enables NTLM authentication via RPC to Samba.

It works by proxying the NTLMSSP payload between the client and the samba
server. Accessorily it could be used against an MS Exchange service, but this
is untested.

"""

import httplib
from socket import socket, _socketobject, SHUT_RDWR, AF_INET, AF_UNIX, \
    SOCK_STREAM, MSG_WAITALL, error as socket_error
from struct import pack, error as struct_error
import sys
from uuid import uuid4, UUID

from openchange.utils.packets import *


COOKIE_NAME = "ocs-ntlm-auth"
SAMBA_PORT = 1024


class NTLMAuthHandler(object):
    """
    HTTP/1.0 ``NTLM`` authentication middleware

    Parameters: application -- the application object that is called only upon
    successful authentication.

    """

    def __init__(self, application, samba_host="localhost"):
        # TODO: client expiration and/or cleanup
        self.client_status = {}
        self.application = application
        self.samba_host = samba_host

    def _in_progress_response(self, start_response,
                              ntlm_data=None, client_id=None):
        status = "401 %s" % httplib.responses[401]
        content = "More data needed..."
        headers = [("Content-Type", "text/plain"),
                   ("Content-Length", "%d" % len(content))]
        if ntlm_data is None:
            www_auth_value = "NTLM"
        else:
            enc_ntlm_data = ntlm_data.encode("base64")
            www_auth_value = ("NTLM %s"
                              % enc_ntlm_data.strip().replace("\n", ""))

        if client_id is not None:
            # MUST occur when ntlm_data is None, can still occur otherwise
            headers.append(("Set-Cookie", "%s=%s" % (COOKIE_NAME, client_id)))

        headers.append(("WWW-Authenticate", www_auth_value))
        start_response(status, headers)

        return [content]

    def _get_cookies(self, env):
        cookies = {}
        if "HTTP_COOKIE" in env:
            cookie_str = env["HTTP_COOKIE"]
            for pair in cookie_str.split(";"):
                (key, value) = pair.strip().split("=")
                cookies[key] = value

        return cookies

    def _handle_negotiate(self, client_id, env, start_response):
        # print >>sys.stderr, "* client auth stage0"

        auth = env["HTTP_AUTHORIZATION"]
        ntlm_payload = auth[5:].decode("base64")

        # print >> sys.stderr, "connecting to host"
        try:
            server = socket(AF_INET, SOCK_STREAM)
            server.connect((self.samba_host, SAMBA_PORT))
        except:
            print >>sys.stderr, \
                ("NTLMAuthHandler: caught exception when connecting to samba"
                 " host")
            raise

        # print >> sys.stderr, "host: %s" % str(server.getsockname())

        # print >> sys.stderr, "building bind packet"
        packet = RPCBindOutPacket()
        packet.ntlm_payload = ntlm_payload
        
        # print >> sys.stderr, "sending bind packet"
        server.sendall(packet.make())

        # print >> sys.stderr, "sent bind packet, receiving response"

        packet = RPCPacket.from_file(server)
        # print >> sys.stderr, "response parsed: %s" % packet.pretty_dump()

        if isinstance(packet, RPCBindACKPacket):
            # print >> sys.stderr, "ACK received"

            client_id = str(uuid4())
            self.client_status[client_id] = {"status": "challenged",
                                             "server": server}

            response = self._in_progress_response(start_response,
                                                  packet.ntlm_payload,
                                                  client_id)
        else:
            # print >> sys.stderr, "NAK received"
            server.shutdown(SHUT_RDWR)
            server.close()
            response = self._in_progress_response(start_response)

        return response

    def _handle_auth(self, client_id, env, start_response):
        # print >>sys.stderr, "* client auth stage1"

        server = self.client_status[client_id]["server"]
        # print >> sys.stderr, "host: %s" % str(server.getsockname())

        auth = env["HTTP_AUTHORIZATION"]

        ntlm_payload = auth[5:].decode("base64")

        # print >> sys.stderr, "building auth_3 and ping packets"
        packet = RPCAuth3OutPacket()
        packet.ntlm_payload = ntlm_payload
        server.sendall(packet.make())

        # This is a hack:
        # A ping at this stage will trigger a connection close
        # from Samba and an error from Exchange. Since a successful
        # authentication does not trigger a response from the server, this
        # provides a simple way to ensure that it passed, without actually
        # performing an RPC operation on the "mgmt" interface. The choice of
        # "mgmt" was due to the fact that we want to keep this authenticator
        # middleware to be reusable for other Samba services while "mgmt"
        # seemes to be the only available interface from Samba outside of the
        # ones provided by OpenChange.
        packet = RPCPingOutPacket()
        packet.call_id = 2
        server.sendall(packet.make())
        # print >> sys.stderr, "sent auth3 and ping packets, receiving response"

        try:
            packet = RPCPacket.from_file(server)
            if isinstance(packet, RPCFaultPacket):
                if packet.header["call_id"] == 2:
                    # the Fault packet related to our Ping operation
                    success = True
                else:
                    success = False
            else:
                raise ValueError("unexpected packet")
        except socket_error:
            # Samba closed the connection
            success = True
        except struct_error:
            # Samba closed the connection
            success = True

        server.shutdown(SHUT_RDWR)
        server.close()

        if success:
            del(self.client_status[client_id]["server"])

            # authentication completed
            self.client_status[client_id]["status"] = "ok"
            response = self.application(env, start_response)
        else:
            # we start over with the whole process
            del(self.client_status[client_id])
            response = self._in_progress_response(start_response)

        return response

    def __call__(self, env, start_response):
        # TODO: validate authorization payload

        # print >>sys.stderr, "starting request: %d" % os.getpid()
        # old model that only works with mod_wsgi:
        # if "REMOTE_ADDR" in env and "REMOTE_PORT" in env:
        #     client_id = "%(REMOTE_ADDR)s:%(REMOTE_PORT)s".format(env)

        has_auth = "HTTP_AUTHORIZATION" in env

        cookies = self._get_cookies(env)
        if COOKIE_NAME in cookies:
            client_id = cookies[COOKIE_NAME]
        else:
            client_id = None

        # print >>sys.stderr, "client_id: %s (known: %s)" % (str(client_id), client_id in self.client_status)

        if has_auth:
            if client_id is None or client_id not in self.client_status:
                # stage 0, where the cookie has not been set yet and where we
                # know the NTLM payload is a NEGOTIATE message
                response = self._handle_negotiate(client_id,
                                                  env, start_response)
            else:
                # stage 1, where the client has already received the challenge
                # from the server and is now sending an AUTH message
                response = self._handle_auth(client_id, env, start_response)
        else:
            if client_id is None or client_id not in self.client_status:
                # this client has never been seen
                response = self._in_progress_response(start_response, None)
            else:
                # authenticated, where no NTLM payload is provided anymore
                response = self.application(env, start_response)

        return response
