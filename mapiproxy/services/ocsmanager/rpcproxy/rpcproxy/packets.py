# packets.py -- OpenChange RPC-over-HTTP implementation
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

import logging
from socket import _socketobject, MSG_WAITALL
from struct import pack, unpack_from


PFC_FIRST_FRAG = 1
PFC_LAST_FRAG = 2
PFC_PENDING_CANCEL = 4
PFC_RESERVED_1 = 8
PFC_CONC_MPX = 16
PFC_DID_NOT_EXECUTE = 32
PFC_MAYBE = 64
PFC_OBJECT_UUID = 128
PFC_FLAG_LABELS = ("PFC_FIRST_FRAG",
                   "PFC_LAST_FRAG", 
                   "PFC_PENDING_CANCEL", 
                   "PFC_RESERVED_1", 
                   "PFC_CONC_MPX", 
                   "PFC_DID_NOT_EXECUTE",
                   "PFC_MAYBE", 
                   "PFC_OBJECT_UUID")


# taken from dcerpc.idl
DCERPC_PKT_REQUEST = 0
DCERPC_PKT_PING = 1
DCERPC_PKT_RESPONSE = 2
DCERPC_PKT_FAULT = 3
DCERPC_PKT_WORKING = 4
DCERPC_PKT_NOCALL = 5
DCERPC_PKT_REJECT = 6
DCERPC_PKT_ACK = 7
DCERPC_PKT_CL_CANCEL = 8
DCERPC_PKT_FACK = 9
DCERPC_PKT_CANCEL_ACK = 10
DCERPC_PKT_BIND = 11
DCERPC_PKT_BIND_ACK = 12
DCERPC_PKT_BIND_NAK = 13
DCERPC_PKT_ALTER = 14
DCERPC_PKT_ALTER_RESP = 15
DCERPC_PKT_UNKNOWN_16 = 16
DCERPC_PKT_SHUTDOWN = 17
DCERPC_PKT_CO_CANCEL = 18
DCERPC_PKT_ORPHANED = 19
DCERPC_PKT_RTS = 20
DCERPC_PKG_LABELS = ("DCERPC_PKT_REQUEST",
                     "DCERPC_PKT_PING",
                     "DCERPC_PKT_RESPONSE",
                     "DCERPC_PKT_FAULT",
                     "DCERPC_PKT_WORKING",
                     "DCERPC_PKT_NOCALL",
                     "DCERPC_PKT_REJECT",
                     "DCERPC_PKT_ACK",
                     "DCERPC_PKT_CL_CANCEL",
                     "DCERPC_PKT_FACK",
                     "DCERPC_PKT_CANCEL_ACK",
                     "DCERPC_PKT_BIND",
                     "DCERPC_PKT_BIND_ACK",
                     "DCERPC_PKT_BIND_NAK",
                     "DCERPC_PKT_ALTERA",
                     "DCERPC_PKT_ALTER_RESPA",
                     "DCERPC_PKT_UNKNOWN_16",
                     "DCERPC_PKT_SHUTDOWN",
                     "DCERPC_PKT_CO_CANCEL",
                     "DCERPC_PKT_ORPHANED",
                     "DCERPC_PKT_RTS")

RTS_FLAG_NONE = 0
RTS_FLAG_PING = 1
RTS_FLAG_OTHER_CMD = 2
RTS_FLAG_RECYCLE_CHANNEL = 4
RTS_FLAG_IN_CHANNEL = 8
RTS_FLAG_OUT_CHANNEL = 0x10
RTS_FLAG_EOF = 0x20
RTS_FLAG_ECHO = 0x40
RTS_FLAG_LABELS = ("RTS_FLAG_PING",
                   "RTS_FLAG_OTHER_CMD",
                   "RTS_FLAG_RECYCLE_CHANNEL",
                   "RTS_FLAG_IN_CHANNEL",
                   "RTS_FLAG_OUT_CHANNEL",
                   "RTS_FLAG_EOF",
                   "RTS_FLAG_ECHO")

RTS_CMD_RECEIVE_WINDOW_SIZE = 0
RTS_CMD_FLOW_CONTROL_ACK = 1
RTS_CMD_CONNECTION_TIMEOUT = 2
RTS_CMD_COOKIE = 3
RTS_CMD_CHANNEL_LIFETIME = 4
RTS_CMD_CLIENT_KEEPALIVE = 5
RTS_CMD_VERSION = 6
RTS_CMD_EMPTY = 7
RTS_CMD_PADDING = 8
RTS_CMD_NEGATIVE_ANCE = 9
RTS_CMD_ANCE = 10
RTS_CMD_CLIENT_ADDRESS = 11
RTS_CMD_ASSOCIATION_GROUP_ID = 12
RTS_CMD_DESTINATION = 13
RTS_CMD_PING_TRAFFIC_SENT_NOTIFY = 14

RTS_CMD_SIZES = (8, 28, 8, 20, 8, 8, 8, 4, 8, 4, 4, 8, 20, 8, 8)
RTS_CMD_DATA_LABELS = ("ReceiveWindowSize",
                       "FlowControlAck",
                       "ConnectionTimeout",
                       "Cookie",
                       "ChannelLifetime",
                       "ClientKeepalive",
                       "Version",
                       "Empty",
                       "Padding",
                       "NegativeANCE",
                       "ANCE",
                       "ClientAddress",
                       "AssociationGroupId",
                       "Destination",
                       "PingTrafficSentNotify")


class RTSParsingException(IOError):
    """This exception occurs when a serious issue occurred while parsing an
    RTS packet.

    """

    pass


class RPCPacket(object):
    def __init__(self, data, logger=None):
        self.logger = logger

        # BLOB level
        self.data = data
        self.size = 0

        # parsed offset from the start of the "data" blob
        self.offset = 0

        # header is common to all PDU
        self.header = None

    @staticmethod
    def from_file(input_file, logger=None):
        """This static method acts as a constructor and returns an input
        packet with the proper class, based on the packet headers.
        The "input_file" parameter must either be a file or a sockect object.

        """

        if isinstance(input_file, _socketobject):
            def read_file(count):
                return input_file.recv(count, MSG_WAITALL)
        elif hasattr(file, "read") and callable(file.read):
            def read_file(count):
                return input_file.read(count)
        else:
            raise ValueError("'input_file' must either be a socket object or"
                             " provide a 'read' method")

        fields = ("rpc_vers", "rpc_vers_minor", "ptype", "pfc_flags", "drep",
                  "frag_length", "auth_length", "call_id")

        header_data = read_file(16)
        # TODO: value validation
        values = unpack_from("<bbbblhhl", header_data)
        if values[2] == DCERPC_PKT_RTS:
            packet_class = RPCRTSPacket
        else:
            packet_class = RPCPacket
        body_data = read_file(values[5] - 16)

        packet = packet_class(header_data + body_data, logger)
        packet.header = dict(zip(fields, values))
        packet.offset = 16
        packet.size = values[5]
        packet.parse()

        return packet

    def parse(self):
        pass

    def pretty_dump(self):
        (fields, values) = self.make_dump_output()

        output = []
        for pos in xrange(len(fields)):
            output.append("%s: %s" % (fields[pos], str(values[pos])))

        return "; ".join(output)

    def make_dump_output(self):
        values = []
        
        ptype = self.header["ptype"]
        values.append(DCERPC_PKG_LABELS[ptype])

        flags = self.header["pfc_flags"]
        if flags == 0:
            values.append("None")
        else:
            flag_values = []
            for exp in xrange(7):
                flag = 1 << exp
                if flags & flag > 0:
                    flag_values.append(PFC_FLAG_LABELS[exp])
            values.append(", ".join(flag_values))

        fields = ["ptype", "pfc_flags", "drep", "frag_length",
                  "auth_length", "call_id"]
        for field in fields[2:]:
            values.append(self.header[field])

        return (fields, values)

# FIXME: command parameters are either int32 values or binary blobs, both when
# parsing and when producing
class RPCRTSPacket(RPCPacket):
    parsers = None

    def __init__(self, data, logger=None):
        RPCPacket.__init__(self, data, logger)

        # RTS commands
        self.commands = []

    def parse(self):
        fields = ("flags", "nbr_commands")
        values = unpack_from("<hh", self.data, self.offset)
        self.offset = self.offset + 4
        self.header.update(zip(fields, values))

        for counter in xrange(self.header["nbr_commands"]):
            self._parse_command()

        if (self.size != self.offset):
            raise RTSParsingException("sizes do not match: expected = %d,"
                                      " actual = %d"
                                      % (self.size, self.offset))

    def _parse_command(self):
        (command_type,) = unpack_from("<l", self.data, self.offset)
        if command_type < 0 or command_type > 15:
            raise RTSParsingException("command type unknown: %d"
                                      % command_type)
        self.offset = self.offset + 4

        command = {"type": command_type}
        command_size = RTS_CMD_SIZES[command_type]
        if command_size > 4:
            data_size = command_size - 4
            if command_type in self.parsers:
                parser = self.parsers[command_type]
                data_value = parser(self, data_size)
            elif data_size == 4:
                # commands with int32 values
                (data_value,) = unpack_from("<l", self.data, self.offset)
                self.offset = self.offset + 4
            else:
                raise RTSParsingException("command is badly handled: %d"
                                          % command_type)

            data_label = RTS_CMD_DATA_LABELS[command_type]
            command[data_label] = data_value

        self.commands.append(command)

    def _parse_command_flow_control_ack(self, data_size):
        data_blob = self.data[self.offset:self.offset+data_size]
        self.offset = self.offset + data_size
        # dumb method
        return data_blob
    
    def _parse_command_cookie(self, data_size):
        data_blob = self.data[self.offset:self.offset+data_size]
        self.offset = self.offset + data_size
        # dumb method
        return data_blob

    def _parse_command_padding_data(self, data_size):
        # the length of the padding bytes is specified in the
        # ConformanceCount field
        (count,) = unpack_from("<l", self.data, self.offset)
        self.offset = self.offset + 4

        data_blob = self.data[self.offset:self.offset+count]
        self.offset = self.offset + count

        return data_value

    def _parse_command_client_address(self, data_blob):
        (address_type,) = unpack_from("<l", self.data, self.offset)
        self.offset = self.offset + 4

        if address_type == 0: # ipv4
            address_size = 4
        elif address_type == 1: # ipv6
            address_size = 16
        else:
            raise RTSParsingException("unknown client address type: %d"
                                      % address_type)

        data_blob = self.data[self.offset:self.offset+address_size]

        # compute offset with padding, which is ignored
        self.offset = self.offset + address_size + 12

        return data_value

    def make_dump_output(self):
        (fields, values) = RPCPacket.make_dump_output(self)
        fields.extend(("flags", "nbr_commands"))

        flags = self.header["flags"]
        if flags == RTS_FLAG_NONE:
            values.append("RTS_FLAG_NONE")
        else:
            flags_value = []
            for exp in xrange(7):
                flag = 1 << exp
                if flags & flag > 0:
                    flags_value.append(RTS_FLAG_LABELS[exp])
                values.append(", ".join(flags_value))

        values.append(self.header["nbr_commands"])

        return (fields, values)


# Those are the parser method for commands with a size > 4. They are defined
# here since the "RPCRTSPacket" symbol is not accessible as long as the class
# definition is not over
RPCRTSPacket.parsers = {RTS_CMD_FLOW_CONTROL_ACK: RPCRTSPacket._parse_command_flow_control_ack,
                        RTS_CMD_COOKIE: RPCRTSPacket._parse_command_cookie,
                        RTS_CMD_ASSOCIATION_GROUP_ID: RPCRTSPacket._parse_command_cookie,
                        RTS_CMD_PADDING: RPCRTSPacket._parse_command_padding_data,
                        RTS_CMD_CLIENT_ADDRESS: RPCRTSPacket._parse_command_client_address}


class RPCRTSOutPacket(object):
    def __init__(self, logger=None):
        self.logger = logger
        self.size = 0

        # RTS packets
        self.flags = RTS_FLAG_NONE
        self.command_data = []

    def make(self):
        if self.command_data is None:
            raise RTSParsingException("packet already returned")

        self._make_header()

        data = "".join(self.command_data)
        data_size = len(data)

        if (data_size != self.size):
            raise RTSParsingException("sizes do not match: declared = %d,"
                                      " actual = %d" % (self.size, data_size))
        self.command_data = None

        if self.logger is not None:
            self.logger.info("returning packet: %s" % repr(data))

        return data

    def _make_header(self):
        header_data = pack("<bbbbbbbbhhlhh",
                           5, 0, # rpc_vers, rpc_vers_minor
                           DCERPC_PKT_RTS, # ptype
                           PFC_FIRST_FRAG | PFC_LAST_FRAG, # pfc_flags
                           # drep: RPC spec chap14.htm (Data Representation Format Label)
                           (1 << 4) | 0, 0, 0, 0,
                           (20 + self.size), # frag_length
                           0, # auth_length
                           0, # call_id
                           self.flags,
                           len(self.command_data))
        self.command_data.insert(0, header_data)
        self.size = self.size + 20

    def add_command(self, command_type, *args):
        if command_type < 0 or command_type > 15:
            raise RTSParsingException("command type unknown: %d (%s)" %
                                      (command_type, str(type(command_type))))

        self.size = self.size + 4

        values = [pack("<l", command_type)]

        command_size = RTS_CMD_SIZES[command_type]
        if command_size > 4:
            if command_type == RTS_CMD_FLOW_CONTROL_ACK:
                data = self._make_command_flow_control_ack(args[0])
            elif (command_type == RTS_CMD_COOKIE
                  or command_type == RTS_CMD_ASSOCIATION_GROUP_ID):
                data = self._make_command_cookie(args[0])
            elif command_type == RTS_CMD_PADDING:
                data = self._make_command_padding_data(args[0])
            elif command_type == RTS_CMD_CLIENT_ADDRESS:
                data = self._make_command_client_address(args[0])
            else:
                # command with int32 value
                data = pack("<l", args[0])
                self.size = self.size + 4
            values.append(data)

        self.command_data.append("".join(values))
        
    def _make_command_flow_control_ack(self, data_blob):
        # dumb method
        len_data = len(data_blob)
        if len_data != 24:
            raise RTSParsingException("expected a length of %d bytes,"
                                      " received %d" % (24, len_data))
        self.size = self.size + len_data

        return data_blob
    
    def _make_command_cookie(self, data_blob):
        # dumb method
        len_data = len(data_blob)
        if len_data != 16:
            raise RTSParsingException("expected a length of %d bytes,"
                                      " received %d" % (16, len_data))
        self.size = self.size + len_data

        return data_blob

    def _make_command_padding_data(self, data_blob):
        len_data = len(data_blob)
        data = pack("<l", len_data) + data_blob
        self.size = self.size + 4 + len_data

        return data

    def _make_command_client_address(self, data_blob):
        len_data = len(data_blob)
        if len_data == 4:
            address_type = 0 # ipv4
        elif len_data == 16:
            address_type = 1 # ipv6
        else:
            raise RTSParsingException("cannot deduce address type from data"
                                      " length: %d" % len_data)

        data = pack("<l", address_type) + data_blob + 12 * chr(0)
        self.size = self.size + 4 + len_data + 12

        return data
