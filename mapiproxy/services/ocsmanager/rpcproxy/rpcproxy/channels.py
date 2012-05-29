# channels.py -- OpenChange RPC-over-HTTP implementation
#
# Copyright (C) 2012  Julien Kerihuel <j.kerihuel@openchange.org>
#                     Wolfgang Sourdeau <wsourdeau@inverse.ca>
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

import os
from select import poll, POLLIN, POLLOUT, POLLHUP, POLLERR, POLLNVAL
from socket import socket, AF_INET, AF_UNIX, SOCK_STREAM, MSG_WAITALL, \
    error as socket_error
from struct import pack, unpack_from
from traceback import format_exc
import sys
from time import time, sleep
from uuid import UUID

# from rpcproxy.RPCH import RPCH, RTS_FLAG_ECHO
from fdunix import send_socket, receive_socket
from utils import prettify_dict
from packets import RTS_CMD_CONNECTION_TIMEOUT, RTS_CMD_VERSION, \
    RTS_CMD_RECEIVE_WINDOW_SIZE, RTS_CMD_CONNECTION_TIMEOUT, \
    RTS_FLAG_ECHO, \
    RTSInPacket, RTSOutPacket


"""Documentation:

1. "Connection Establishment" sequence (from RPCH.pdf, 3.2.1.5.3.1)

  client -> IN request -> proxy in
  # server -> legacy server response -> proxy in
  # server -> legacy server response -> proxy out
  client -> Out request -> proxy out
  client -> A1 -> proxy out
  client -> B1 -> proxy in
  # proxy out -> A2 -> server
  proxy out -> OUT channel response -> client
  # proxy in -> B2 -> server
  proxy out -> A3 -> client
  # server -> C1 -> proxy out
  # server -> B3 -> proxy in
  proxy out -> C2 -> client

2. internal unix socket protocols

   Note: OUT proxy is always the server

 * establishing virtual connection
 OUT proxy listens on unix socket
 IN proxy connects to OUT proxy
 IN -> OUT: "IP"
 IN -> OUT: in_window_size
 IN -> OUT: in_conn_timeout
 OUT -> IN: sends connection to OpenChange
 (TODO: socket close at this point?)

 * channel recycling (unused yet, hypothethical)
 When new OUT conn arrives:
 new OUT -> OUT: "OP"
 OUT -> new OUT: OUT listening socket (fdunix)
 OUT -> new OUT: IN socket (fdunix)
 OUT -> new OUT: oc socket (fdunix)
 close OUT socket locally
"""


# those id must have the same length
INBOUND_PROXY_ID = "IP"
OUTBOUND_PROXY_ID = "OP"
SOCKETS_DIR = "/tmp/rpcproxy"
OC_HOST = "127.0.0.1"

class RPCProxyChannelHandler(object):
    def __init__(self, logger):
        self.logger = logger

        self.bytes_read = 0
        self.bytes_written = 0
        self.startup_time = time()

        self.channel_cookie = None
        self.connection_cookie = None

    def handle_echo_request(self, environ, start_response):
        self.logger.info("handling echo request")

        packet = RTSOutPacket()
        packet.flags = RTS_FLAG_ECHO
        data = packet.make()

        start_response("200 Success", [("Content-length", "%d" % packet.size),
                                       ("Content-Type", "application/rpc")])

        return [data]

    @staticmethod
    def success_response(environ, start_response):
        start_response("200 Success", [("Content-Type", "application/rpc"),
                                       ("Content-length", "0")])
        return [""]

    def log_connection_stats(self):
        self.logger.info("request took %f ms; %d bytes received; %d bytes sent"
                         % ((time() - self.startup_time),
                            self.bytes_read, self.bytes_written))


class RPCProxyInboundChannelHandler(RPCProxyChannelHandler):
    def __init__(self, logger):
        RPCProxyChannelHandler.__init__(self, logger)
        self.oc_conn = None
        self.window_size = 0
        self.conn_timeout = 0
        self.client_keepalive = 0
        self.association_group_id = None

    def _receive_conn_b1(self):
        # CONN/B1 RTS PDU (TODO: validation)
        # receive the cookie
        self.logger.info("IN: receiving CONN/B1")
        packet = RTSInPacket(self.client_socket, logger=self.logger)
        self.connection_cookie = str(UUID(bytes=packet.commands[1]["Cookie"]))
        self.channel_cookie = str(UUID(bytes=packet.commands[2]["Cookie"]))
        self.client_keep_alive = packet.commands[4]["ClientKeepalive"]
        self.association_group_id = str(UUID(bytes=packet.commands[5] \
                                             ["AssociationGroupId"]))
        self.bytes_read = self.bytes_read + packet.size

    def _connect_to_OUT_channel(self):
        # FIXME: we might need to keep a persistant connection to the OUT
        # channel

        # connect as a client to the cookie unix socket
        socket_name = os.path.join(SOCKETS_DIR, self.connection_cookie)
        self.logger.info("IN: connecting to OUT via unix socket '%s'"
                         % socket_name)
        sock = socket(AF_UNIX, SOCK_STREAM)
        connected = False
        attempt = 0
        while not connected:
            try:
                attempt = attempt + 1
                sock.connect(socket_name)
                connected = True
            except socket_error:
                self.logger.info("IN: handling socket.error: %s"
                                 % str(sys.exc_info()))
                if attempt < 10:
                    self.logger.warn("IN: reattempting to connect to OUT"
                                     " channel... (%d/10)" % attempt)
                    sleep(1)

        if connected:
            self.logger.info("IN: connection succeeded")
            self.logger.info("IN: sending window size and connection timeout")

            # identify ourselves as the IN proxy
            sock.sendall(INBOUND_PROXY_ID)

            # send window_size to 256Kib (max size allowed)
            # and conn_timeout (in seconds, max size allowed)
            sock.sendall(pack("<ll", (256 * 1024), 14400000))

            # recv oc socket
            self.oc_conn = receive_socket(sock)

            self.logger.info("IN: oc_conn received (fileno=%d)"
                             % self.oc_conn.fileno())
            sock.close()
        else:
            self.logger.error("too many failed attempts to establish a"
                              " connection to OUT channel")

        return connected

    def _runloop(self):
        self.logger.info("IN: runloop")

        status = True
        while status:
            try:
                oc_packet = RTSInPacket(self.client_socket)
                self.bytes_read = self.bytes_read + oc_packet.size

                self.logger.info("IN: packet headers = "
                                 + oc_packet.pretty_dump())

                if oc_packet.is_rts:
                    # or oc_packet.header["ptype"] == DCERPC_PKT_AUTH3):
                    # we do not forward rts packets
                    self.logger.info("IN: ignored RTS packet")
                else:
                    self.logger.info("IN: sending packet to OC")
                    self.oc_conn.sendall(oc_packet.data)
                    self.bytes_written = self.bytes_written + oc_packet.size
            except IOError:
                status = False
                # exc = sys.exc_info()
                self.logger.error("IN: client connection closed")
                self._notify_OUT_channel()

    def _notify_OUT_channel(self):
        self.logger.info("IN: notifying OUT channel of shutdown")

        socket_name = os.path.join(SOCKETS_DIR, self.connection_cookie)
        self.logger.info("IN: connecting to OUT via unix socket '%s'"
                         % socket_name)
        sock = socket(AF_UNIX, SOCK_STREAM)
        connected = False
        attempt = 0
        while not connected:
            try:
                attempt = attempt + 1
                sock.connect(socket_name)
                connected = True
            except socket_error:
                self.logger.info("IN: handling socket.error: %s"
                                 % str(sys.exc_info()))
                if attempt < 10:
                    self.logger.warn("IN: reattempting to connect to OUT"
                                     " channel... (%d/10)" % attempt)
                    sleep(1)

        if connected:
            self.logger.info("IN: connection succeeded")
            sock.sendall(INBOUND_PROXY_ID + "q")
            sock.close()
        else:
            self.logger.error("too many failed attempts to establish a"
                              " connection to OUT channel")

    def _terminate_oc_socket(self):
        self.oc_conn.close()

    def sequence(self, environ, start_response):
        self.logger.info("IN: processing request")
        if "REMOTE_PORT" in environ:
            self.logger.info("IN: remote port = %s" % environ["REMOTE_PORT"])
        # self.logger.info("IN: path: ' + self.path)

        content_length = int(environ["CONTENT_LENGTH"])
        self.logger.info("IN: request size is %d" % content_length)

        # echo request
        if content_length <= 0x10:
            self.logger.info("IN: Exiting (1) from do_RPC_IN_DATA")
            for data in self.handle_echo_request(environ, start_response):
                yield data
        elif content_length >= 128:
            self.logger.info("IN: Processing IN channel request")

            self.client_socket = environ["wsgi.input"]
            self._receive_conn_b1()
            connected = self._connect_to_OUT_channel()

            for data in self.success_response(environ, start_response):
                yield data

            if connected:
                self._runloop()

            self._terminate_oc_socket()

            self.log_connection_stats()
            self.logger.info("IN: Exiting (2) from do_RPC_IN_DATA")
        else:
            raise Exception("This content-length is not handled")

        # OLD CODE
        # msg = "RPC_IN_DATA method"

        # content_length = environ["CONTENT_LENGTH"]
        # # echo request
        # if content_length <= 10:
        #     pass

        # start_response("200 OK", [("Content-Type", "text/plain"),
        #                           ("Content-length", "%s" % len(msg))])

        # return [msg]

class RPCProxyOutboundChannelHandler(RPCProxyChannelHandler):
    def __init__(self, logger):
        RPCProxyChannelHandler.__init__(self, logger)
        self.unix_socket = None
        self.oc_conn = None
        self.in_window_size = 0
        self.in_conn_timeout = 0

    def _receive_conn_a1(self, client_socket):
        # receive the cookie
        # TODO: validation of CONN/A1
        self.logger.info("OUT: receiving CONN/A1")
        packet = RTSInPacket(client_socket)
        self.connection_cookie = str(UUID(bytes=packet.commands[1]["Cookie"]))
        self.channel_cookie = str(UUID(bytes=packet.commands[2]["Cookie"]))

    def _send_conn_a3(self):
        self.logger.info("OUT: sending CONN/A3 to client")
            # send the A3 response to the client
        packet = RTSOutPacket(logger=self.logger)
        # we set the min timeout value allowed, as we would actually need
        # either configuration values from Apache or from some config file
        packet.add_command(RTS_CMD_CONNECTION_TIMEOUT, 120000)
        self.bytes_written = self.bytes_written + packet.size

        return packet.make()

    def _send_conn_c2(self):
        self.logger.info("OUT: sending CONN/C2 to client")
            # send the C2 response to the client
        packet = RTSOutPacket()
        # we set the min timeout value allowed, as we would actually need
        # either configuration values from Apache or from some config file
        packet.add_command(RTS_CMD_VERSION, 1)
        packet.add_command(RTS_CMD_RECEIVE_WINDOW_SIZE, self.in_window_size)
        packet.add_command(RTS_CMD_CONNECTION_TIMEOUT, self.in_conn_timeout)
        self.bytes_written = self.bytes_written + packet.size

        return packet.make()

    def _setup_oc_socket(self):
        # create IP connection to OpenChange
        self.logger.info("OUT: connecting to OC_HOST:1024")
        connected = False
        while not connected:
            try:
                oc_conn = socket(AF_INET, SOCK_STREAM)
                oc_conn.connect((OC_HOST, 1024))
                connected = True
            except socket_error:
                self.logger.info("OUT: failure to connect, retrying...")
                sleep(1)
        self.logger.info("OUT: connection to OC succeeeded (fileno=%d)"
                         % oc_conn.fileno())
        self.oc_conn = oc_conn

    def _setup_channel_socket(self):
        # TODO: add code to create missing socket dir
        # create the corresponding unix socket
        socket_name = os.path.join(SOCKETS_DIR, self.connection_cookie)
        self.logger.info("OUT: creating unix socket '%s'" % socket_name)
        if os.access(socket_name, os.F_OK):
            os.remove(socket_name)
        sock = socket(AF_UNIX, SOCK_STREAM)
        sock.bind(socket_name)
        sock.listen(2)
        self.unix_socket = sock

    def _wait_IN_channel(self):
        self.logger.info("OUT: waiting for connection from IN")
        # wait for the IN channel to connect as a B1 should be occurring
        # on the other side
        in_sock = self.unix_socket.accept()[0]
        data = in_sock.recv(2, MSG_WAITALL)
        if data != INBOUND_PROXY_ID:
            raise IOError("connection must be from IN proxy (1): /%s/"
                          % data)

        self.logger.info("OUT: receiving window size + conn_timeout")
            # receive the WindowSize + ConnectionTimeout
        (self.in_window_size, self.in_conn_timeout) = \
            unpack_from("<ll", in_sock.recv(8, MSG_WAITALL))
            # send OC socket
        self.logger.info("OUT: sending OC socket to IN")
        send_socket(in_sock, self.oc_conn)
        in_sock.close()

    def _runloop(self):
        self.logger.info("OUT: runloop")

        unix_fd = self.unix_socket.fileno()
        oc_fd = self.oc_conn.fileno()

        fd_pool = poll()
        fd_pool.register(unix_fd, POLLIN)
        fd_pool.register(oc_fd, POLLIN)

        # Listen for data from the listener
        status = True
        while status:
            for data in fd_pool.poll(1000):
                fd = data[0]
                event_no = data[1]
                if fd == oc_fd:
                    # self.logger.info("received event '%d' on oc socket" % event_no)
                    if event_no & POLLHUP > 0:
                        # FIXME: notify IN channel?
                        self.logger.info("OUT: connection closed from OC")
                        status = False
                    elif event_no & POLLIN > 0:
                        oc_packet = RTSInPacket(self.oc_conn)

                        self.logger.info("OUT: packet headers = "
                                         + oc_packet.pretty_dump())

                        self.logger.info("OUT: sending data to client")
                        self.bytes_read = self.bytes_read + oc_packet.size
                        self.bytes_written = self.bytes_written + oc_packet.size
                        yield oc_packet.data
                        # else:
                        #     self.logger.info("ignored event '%d' on oc socket" % event_no)
                elif fd == unix_fd:
                    self.logger.info("OUT: ignored event '%d' on unix socket"
                                     % event_no)
                    # FIXME: we should listen to what the IN channel has to say
                    status = False
                else:
                    raise Exception("invalid poll event: %s" % str(data))
            # write(oc_packet.header_data)
            # write(oc_packet.data)
            # self.logger.info("OUT: data sent to client")

    def _terminate_sockets(self):
        socket_name = os.path.join(SOCKETS_DIR, self.connection_cookie)
        self.logger.info("OUT: removing and closing unix socket '%s'"
                         % socket_name)
        if os.access(socket_name, os.F_OK):
            os.remove(socket_name)
        self.unix_socket.close()
        self.oc_conn.close()

    def sequence(self, environ, start_response):
        self.logger.info("OUT: processing request")
        if "REMOTE_PORT" in environ:
            self.logger.info("OUT: remote port = %s" % environ["REMOTE_PORT"])
        # self.logger.info("OUT: path: ' + self.path)
        content_length = int(environ["CONTENT_LENGTH"])
        self.logger.info("OUT: request size is %d" % content_length)

        if content_length <= 0x10:
            # echo request
            for data in self.handle_echo_request(environ, start_response):
                yield data
        elif content_length == 76:
            self.logger.info("OUT: Processing nonreplacement Out channel request")

            client_socket = environ["wsgi.input"]
            self._receive_conn_a1(client_socket)

            # Content-length = 1 Gib
            start_response("200 Success",
                           [("Content-Type", "application/rpc"),
                            ("Content-length", "%d" % (1024 ** 3))])

            yield self._send_conn_a3()
            self._setup_oc_socket()
            self._setup_channel_socket()
            self._wait_IN_channel()

            yield self._send_conn_c2()
            self.logger.info("OUT: total bytes sent yet: %d" % self.bytes_written)
            for data in self._runloop():
                yield data
            self._terminate_sockets()
        elif content_length == 120:
            # Out channel request: replacement OUT channel
            raise Exception("Replacement OUT channel request not handled")
        else:
            raise Exception("This content-length is not handled")

        self.log_connection_stats()
        self.logger.info("OUT: Exiting from do_RPC_OUT_DATA")
