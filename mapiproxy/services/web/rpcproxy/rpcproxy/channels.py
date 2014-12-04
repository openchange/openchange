# channels.py -- OpenChange RPC-over-HTTP implementation
# -*- coding: utf-8 -*-
#
# Copyright (C) 2012-2014  Julien Kerihuel <j.kerihuel@openchange.org>
#                          Wolfgang Sourdeau <wsourdeau@inverse.ca>
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
from operator import itemgetter
import os
from select import poll, POLLIN, POLLHUP
from socket import socket, AF_INET, AF_UNIX, SOCK_STREAM, MSG_WAITALL, \
    SHUT_RDWR, error as socket_error
from struct import pack, unpack_from
import sys
from time import time, sleep
from uuid import UUID


# from rpcproxy.RPCH import RPCH, RTS_FLAG_ECHO
from openchange.utils.fdunix import send_socket, receive_socket
from openchange.utils.packets import (RTS_CMD_CONNECTION_TIMEOUT,
                                      RTS_CMD_CUSTOM_OUT,
                                      RTS_CMD_DESTINATION,
                                      RTS_CMD_FLOW_CONTROL_ACK,
                                      RTS_CMD_RECEIVE_WINDOW_SIZE,
                                      RTS_CMD_VERSION,
                                      RTS_FLAG_ECHO,
                                      RTS_CMD_DATA_LABELS, RPCPacket, RPCRTSPacket, RPCRTSOutPacket)


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
 The UNIX socket is open to transmit packets from IN to OUT channel using RTS CMD

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


def _safe_close(socket_obj):
    try:
        socket_obj.shutdown(SHUT_RDWR)
        socket_obj.close()
    except:
        pass


class RPCProxyChannelHandler(object):
    def __init__(self, sockets_dir, logger):
        self.sockets_dir = sockets_dir
        self.logger = logger

        self.unix_socket = None
        self.client_socket = None # placeholder for wsgi.input

        self.bytes_read = 0
        self.bytes_written = 0
        self.startup_time = time()

        self.channel_cookie = None
        self.connection_cookie = None

    def handle_echo_request(self, environ, start_response):
        self.logger.debug("handling echo request")

        packet = RPCRTSOutPacket()
        packet.flags = RTS_FLAG_ECHO
        data = packet.make()
        self.bytes_written = self.bytes_written + packet.size

        start_response("200 Success", [("Content-length", "%d" % packet.size),
                                       ("Content-Type", "application/rpc")])

        return [data]

    def log_connection_stats(self):
        self.logger.debug("channel kept alive during %f secs;"
                         " %d bytes received; %d bytes sent"
                         % ((time() - self.startup_time),
                            self.bytes_read, self.bytes_written))


class RPCProxyInboundChannelHandler(RPCProxyChannelHandler):
    def __init__(self, sockets_dir, logger):
        RPCProxyChannelHandler.__init__(self, sockets_dir, logger)
        self.oc_conn = None
        # Window_size to 256KiB (max size allowed)
        self.window_size = 256 * 1024
        self.conn_timeout = 0
        self.client_keepalive = 0
        self.association_group_id = None
        self.local_available_window = self.window_size

    def _receive_conn_b1(self):
        # CONN/B1 RTS PDU (TODO: validation)
        # receive the cookie
        self.logger.debug("receiving CONN/B1")

        packet = RPCPacket.from_file(self.client_socket, self.logger)
        if not isinstance(packet, RPCRTSPacket):
            raise Exception("Unexpected non-rts packet received for CONN/B1")
        self.logger.debug("packet headers = " + packet.pretty_dump())

        self.connection_cookie = str(UUID(bytes=packet.commands[1]["Cookie"]))
        self.channel_cookie = str(UUID(bytes=packet.commands[2]["Cookie"]))
        self.client_keepalive = packet.commands[4]["ClientKeepalive"]
        self.association_group_id = str(UUID(bytes=packet.commands[5] \
                                                 ["AssociationGroupId"]))
        self.bytes_read = self.bytes_read + packet.size

    def _connect_to_OUT_channel(self):
        # connect as a client to the cookie unix socket
        socket_name = os.path.join(self.sockets_dir, self.connection_cookie)
        self.logger.debug("connecting to OUT via unix socket '%s'"
                         % socket_name)
        unix_socket = socket(AF_UNIX, SOCK_STREAM)
        connected = False
        attempt = 0
        while not connected and attempt < 10:
            try:
                attempt = attempt + 1
                unix_socket.connect(socket_name)
                self.unix_socket = unix_socket
                connected = True
            except socket_error:
                self.logger.debug("handling socket.error: %s"
                                 % str(sys.exc_info()))
                self.logger.warn("reattempting to connect to OUT"
                                 " channel... (%d/10)" % attempt)
                sleep(1)

        if connected:
            self.logger.debug("connection succeeded")
            self.logger.debug("sending window size and connection timeout")

            # identify ourselves as the IN proxy
            unix_socket.sendall(INBOUND_PROXY_ID)

            # send window_size to 256Kib (max size allowed)
            # and conn_timeout (in milliseconds, max size allowed)
            unix_socket.sendall(pack("<ll", self.window_size, 120000))

            # recv oc socket
            self.oc_conn = receive_socket(unix_socket)

            self.logger.debug("oc_conn received (fileno=%d)"
                              % self.oc_conn.fileno())
        else:
            self.logger.error("too many failed attempts to establish a"
                              " connection to OUT channel")

        return connected

    def _send_flow_control_ack(self):
        """Send FlowControlAckWithDestination RTS command to say the client there
        is room for sending more information"""
        self.logger.debug('Send to client the FlowControlAckWithDestination RTS command '
                          'after %d of avalaible window size' % self.local_available_window)
        rts_packet = RPCRTSOutPacket(self.logger)
        # We always returns back the same available maximum received size
        rts_packet.add_command(RTS_CMD_DESTINATION, 0)  # Forward this to client
        rts_packet.add_command(RTS_CMD_FLOW_CONTROL_ACK,
                               {'bytes_received': self.bytes_read,
                                'available_window': self.window_size,
                                'channel_cookie': self.channel_cookie})

        # Send the message to the OUT channel
        self.unix_socket.sendall(rts_packet.make())

    def _runloop(self):
        self.logger.debug("runloop")

        status = True
        while status:
            try:
                oc_packet = RPCPacket.from_file(self.client_socket,
                                                self.logger)
                self.logger.debug("packet headers = "
                                  + oc_packet.pretty_dump())
                self.bytes_read = self.bytes_read + oc_packet.size

                # Flow control
                self.local_available_window -= oc_packet.size
                # This check can be any as it is not explicitly specified
                if self.local_available_window < self.window_size / 2:
                    self._send_flow_control_ack()
                    self.local_available_window = self.window_size

                if isinstance(oc_packet, RPCRTSPacket):
                    labels = [RTS_CMD_DATA_LABELS[command["type"]]
                              for command in oc_packet.commands]
                    self.logger.debug("ignored RTS packet with commands: %s"
                                      % ", ".join(labels))
                else:
                    self.logger.debug("sending packet to OC")
                    self.oc_conn.sendall(oc_packet.data)
                    self.bytes_written = self.bytes_written + oc_packet.size
            except IOError:
                status = False
                self.logger.debug("handling socket.error: %s"
                                  % str(sys.exc_info()))
                # exc = sys.exc_info()
                self.logger.error("client connection closed")

    def sequence(self, environ, start_response):
        self.logger.debug("processing request")
        if "REMOTE_PORT" in environ:
            self.logger.debug("remote port = %s" % environ["REMOTE_PORT"])
        # self.logger.debug("path: ' + self.path)

        content_length = int(environ["CONTENT_LENGTH"])
        self.logger.debug("request size is %d" % content_length)

        # echo request
        if content_length <= 0x10:
            for data in self.handle_echo_request(environ, start_response):
                yield data
            self.logger.debug("exiting from echo request")
        elif content_length >= 128:
            self.logger.debug("processing IN channel request")

            self.client_socket = environ["wsgi.input"]
            self._receive_conn_b1()
            connected = self._connect_to_OUT_channel()

            if connected:
                start_response("200 Success",
                               [("Content-Type", "application/rpc"),
                                ("Content-length", "0")])
                self._runloop()

                # shutting down sockets
                self.logger.debug("notifying OUT channel of shutdown")
                try:
                    packet = RPCRTSOutPacket(self.logger)
                    packet.add_command(RTS_CMD_CUSTOM_OUT)
                    self.unix_socket.sendall(packet.make())
                    _safe_close(self.unix_socket)
                    self.logger.debug("OUT channel successfully notified")
                except socket_error:
                    self.logger.debug("(OUT channel already shutdown the unix socket)")
                    # OUT channel already closed the connection
                    pass

                _safe_close(self.oc_conn)

            self.log_connection_stats()
            self.logger.debug("exiting from main sequence")
            
            # TODO: error handling
            start_response("200 Success",
                           [("Content-length", "0"),
                            ("Content-Type", "application/rpc")])
            yield ""
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
    def __init__(self, sockets_dir, samba_host, logger):
        RPCProxyChannelHandler.__init__(self, sockets_dir, logger)
        self.samba_host = samba_host
        self.oc_conn = None
        self.in_window_size = 0
        self.in_conn_timeout = 0

    def _receive_conn_a1(self):
        # receive the cookie
        # TODO: validation of CONN/A1
        self.logger.debug("receiving CONN/A1")
        packet = RPCPacket.from_file(self.client_socket, self.logger)
        if not isinstance(packet, RPCRTSPacket):
            raise Exception("Unexpected non-rts packet received for CONN/A1")
        self.logger.debug("packet headers = " + packet.pretty_dump())

        self.connection_cookie = str(UUID(bytes=packet.commands[1]["Cookie"]))
        self.channel_cookie = str(UUID(bytes=packet.commands[2]["Cookie"]))

    def _send_conn_a3(self):
        self.logger.debug("sending CONN/A3 to client")
            # send the A3 response to the client
        packet = RPCRTSOutPacket(self.logger)
        # we set the min timeout value allowed, as we would actually need
        # either configuration values from Apache or from some config file
        packet.add_command(RTS_CMD_CONNECTION_TIMEOUT, 120000)
        self.bytes_written = self.bytes_written + packet.size

        return packet.make()

    def _send_conn_c2(self):
        self.logger.debug("sending CONN/C2 to client")
            # send the C2 response to the client
        packet = RPCRTSOutPacket(self.logger)
        # we set the min timeout value allowed, as we would actually need
        # either configuration values from Apache or from some config file
        packet.add_command(RTS_CMD_VERSION, 1)
        packet.add_command(RTS_CMD_RECEIVE_WINDOW_SIZE, self.in_window_size)
        packet.add_command(RTS_CMD_CONNECTION_TIMEOUT, self.in_conn_timeout)
        self.bytes_written = self.bytes_written + packet.size

        return packet.make()

    def _setup_oc_socket(self):
        # create IP connection to OpenChange
        self.logger.debug("connecting to %s:1024" % self.samba_host)
        connected = False
        while not connected:
            try:
                oc_conn = socket(AF_INET, SOCK_STREAM)
                oc_conn.connect((self.samba_host, 1024))
                connected = True
            except socket_error:
                self.logger.debug("failure to connect, retrying...")
                sleep(1)
        self.logger.debug("connection to OC succeeeded (fileno=%d)"
                         % oc_conn.fileno())
        self.oc_conn = oc_conn

    def _setup_channel_socket(self):
        # TODO: add code to create missing socket dir
        # create the corresponding unix socket

        if not os.access(self.sockets_dir, os.R_OK | os.W_OK | os.X_OK):
            raise IOError("Socket directory '%s' does not exist or has the"
                          " wrong permissions" % self.sockets_dir)

        socket_name = os.path.join(self.sockets_dir, self.connection_cookie)
        self.logger.debug("creating unix socket '%s'" % socket_name)
        if os.access(socket_name, os.F_OK):
            os.remove(socket_name)
        server_socket = socket(AF_UNIX, SOCK_STREAM)
        server_socket.bind(socket_name)
        server_socket.listen(2)
        self.server_socket = server_socket

    def _wait_IN_channel(self):
        self.logger.debug("waiting for connection from IN")

        # wait for the IN channel to connect as a B1 should be occurring
        # on the other side

        fd_pool = poll()
        fd_pool.register(self.server_socket.fileno(), POLLIN)

        # wait for 10 seconds
        data = fd_pool.poll(1000.0)
        if data is not None:
            unix_socket = self.server_socket.accept()[0]
            self.logger.debug("connection established with IN channel")

            data = unix_socket.recv(2, MSG_WAITALL)
            if data != INBOUND_PROXY_ID:
                raise IOError("connection must be from IN proxy (1): /%s/"
                              % data)

            self.logger.debug("receiving window size + conn_timeout")
            # receive the WindowSize + ConnectionTimeout
            (self.in_window_size, self.in_conn_timeout) = \
                unpack_from("<ll", unix_socket.recv(8, MSG_WAITALL))
            self.logger.debug("window size = %d; conn_timeout = %d"
                              % (self.in_window_size, self.in_conn_timeout))
            # send OC socket
            self.logger.debug("sending OC socket to IN")
            send_socket(unix_socket, self.oc_conn)
            self.unix_socket = unix_socket

            connected = True
        else:
            self.logger.info("IN channel failed to connect in the last"
                             " 10 seconds")
            connected = False
        
        return connected

    def _process_server_event(self, unix_fd, oc_fd, data):
        status = True
        server_data = None

        fd, event_no = data
        if fd == oc_fd:
            # self.logger.debug("received event '%d' on oc socket"
            #                   % event_no)
            if event_no & POLLHUP > 0:
                # FIXME: notify IN channel?
                self.logger.debug("connection closed from OC")
                status = False
            elif event_no & POLLIN > 0:
                oc_packet = RPCPacket.from_file(self.oc_conn, self.logger)
                self.logger.debug("packet headers = "
                                  + oc_packet.pretty_dump())
                if isinstance(oc_packet, RPCRTSPacket):
                    raise Exception("Unexpected rts packet received")

                self.logger.debug("sending data to client")
                self.bytes_read = self.bytes_read + oc_packet.size

                server_data = oc_packet.data
        elif fd == unix_fd:
            if event_no & POLLHUP > 0:
                # FIXME: notify IN channel?
                self.logger.debug("connection closed from IN channel")
                status = False
            elif event_no & POLLIN > 0:
                oc_packet = RPCPacket.from_file(self.unix_socket, self.logger)
                self.logger.debug("packet headers = "
                                  + oc_packet.pretty_dump())
                if not isinstance(oc_packet, RPCRTSPacket):
                    raise Exception('Unexpected non-RTS RPC packet received from IN channel')

                if RTS_CMD_CUSTOM_OUT in map(itemgetter('type'), oc_packet.commands):
                    self.logger.debug('Notified the IN channel has left out')
                    status = False
                else:
                    self.logger.debug('Sending RTS RPC packet to client')
                    self.bytes_read += oc_packet.size

                    server_data = oc_packet.data

        else:
            raise Exception("invalid poll event: %s" % str(data))

        return (status, server_data)

    def _runloop(self):
        self.logger.debug("runloop")

        unix_fd = self.unix_socket.fileno()
        oc_fd = self.oc_conn.fileno()

        fd_pool = poll()
        fd_pool.register(unix_fd, POLLIN)
        fd_pool.register(oc_fd, POLLIN)

        # Listen for data from the listener
        status = True
        while status:
            self.logger.debug("step in loop")
            chunks = fd_pool.poll(2000.0)

            if len(chunks) == 0:
                # send ping packets?
                pass
            else:
                for data in chunks:
                    event_status, server_data \
                        = self._process_server_event(unix_fd, oc_fd, data)
                    if server_data is not None:
                        self.bytes_written \
                            = self.bytes_written + len(server_data)
                        self.logger.debug("data sent")
                        yield server_data
                    status = status and event_status

    def _terminate_sockets(self):
        socket_name = os.path.join(self.sockets_dir, self.connection_cookie)
        self.logger.debug("removing and closing unix socket '%s'"
                         % socket_name)
        if os.access(socket_name, os.F_OK):
            os.remove(socket_name)
        if self.unix_socket is not None:
            _safe_close(self.unix_socket)
        _safe_close(self.server_socket)
        _safe_close(self.oc_conn)

    def sequence(self, environ, start_response):
        self.logger.debug("processing request")
        if "REMOTE_PORT" in environ:
            self.logger.debug("remote port = %s" % environ["REMOTE_PORT"])
        # self.logger.debug("path: ' + self.path)
        content_length = int(environ["CONTENT_LENGTH"])
        self.logger.debug("request size is %d" % content_length)

        if content_length <= 0x10:
            # echo request
            for data in self.handle_echo_request(environ, start_response):
                yield data
        elif content_length == 76:
            self.logger.debug("processing nonreplacement Out channel"
                              "request")

            self.client_socket = environ["wsgi.input"]
            self._receive_conn_a1()

            # Content-length = 1 Gib
            start_response("200 Success",
                           [("Content-Type", "application/rpc"),
                            ("Content-length", "%d" % (1024 ** 3))])

            yield self._send_conn_a3()
            self._setup_oc_socket()
            self._setup_channel_socket()
            connected = self._wait_IN_channel()

            if connected:
                yield self._send_conn_c2()
                self.logger.debug("total bytes sent yet: %d"
                                  % self.bytes_written)
                for data in self._runloop():
                    yield data
            self._terminate_sockets()

        elif content_length == 120:
            # Out channel request: replacement OUT channel
            raise Exception("Replacement OUT channel request not handled")
        else:
            raise Exception("This content-length is not handled")

        self.log_connection_stats()
        self.logger.debug("exiting from main sequence")
