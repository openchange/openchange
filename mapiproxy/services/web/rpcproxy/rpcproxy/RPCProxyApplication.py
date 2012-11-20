# RPCProxyApplication.py -- OpenChange RPC-over-HTTP implementation
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
from errno import EEXIST
from os import umask, mkdir, rmdir, listdir, getpid
from os.path import join
from uuid import uuid4
from shutil import rmtree
import sys

from channels import RPCProxyInboundChannelHandler,\
    RPCProxyOutboundChannelHandler


class RPCProxyApplication(object):
    def __init__(self, samba_host, log_level=logging.DEBUG):
        # we keep a reference to the rmtree function until our instance is
        # deleted
        self.rmtree = rmtree

        dirname = "/tmp/rpcproxy"
        try:
            mkdir(dirname)
        except:
            pass

        self.sockets_dir = dirname

        print >>sys.stderr, "RPCProxy started"

        # has_socket_dir = False
        # umask(0077)
        # while not has_socket_dir:
            # # leafname = "rpcproxy-%s" % str(uuid4())
            # leafname = "rpcproxy" # % getpid()
            # dirname = "/tmp/%s" % leafname
            # try:
            #     mkdir(dirname)
            #     has_socket_dir = True
            #     self.sockets_dir = dirname
            # except OSError, e:
            #     if e.errno != EEXIST:
            #         raise

        self.samba_host = samba_host
        self.log_level = log_level

    def __del__(self):
        # self.rmtree(self.sockets_dir)
        pass

    def __call__(self, environ, start_response):
        if "REQUEST_METHOD" in environ:
            method = environ["REQUEST_METHOD"]
            method_name = "_do_" + method
            if hasattr(self, method_name):
                if "wsgi.errors" in environ:
                    log_stream = environ["wsgi.errors"]
                else:
                    log_stream = sys.stderr

                logHandler = logging.StreamHandler(log_stream)
                fmter = logging.Formatter("[%(process)d:%(name)s] %(levelname)s: %(message)s")
                logHandler.setFormatter(fmter)

                if "REMOTE_PORT" in environ:
                    rmt_port = environ["REMOTE_PORT"]
                else:
                    rmt_port = "<unknown>"

                logger = logging.Logger(method + ":" + rmt_port)
                logger.setLevel(self.log_level)
                logger.addHandler(logHandler)
                # logger.set_name(method)

                channel_method = getattr(self, method_name)
                channel = channel_method(logger)
                response = channel.sequence(environ, start_response)
            else:
                response = self._unsupported_method(environ, start_response)
        else:
            response = self._unsupported_method(environ, start_response)

        return response

    @staticmethod
    def _unsupported_method(environ, start_response):
        msg = "Unsupported method"
        start_response("501 Not Implemented", [("Content-Type", "text/plain"),
                                               ("Content-length",
                                                str(len(msg)))])

        return [msg]

    def _do_RPC_IN_DATA(self, logger):
        return RPCProxyInboundChannelHandler(self.sockets_dir, logger)

    def _do_RPC_OUT_DATA(self, logger):
        return RPCProxyOutboundChannelHandler(self.sockets_dir,
                                              self.samba_host,
                                              logger)
