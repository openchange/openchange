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
from os import umask, mkdir, rmdir, listdir
from os.path import join
from uuid import uuid4
import sys


from channels import RPCProxyInboundChannelHandler,\
    RPCProxyOutboundChannelHandler


class RPCProxyApplication(object):
    def __init__(self, samba_host, log_level=logging.DEBUG):
        print >>sys.stderr, "RPCProxy started"

        has_socket_dir = False
        umask(0077)
        while not has_socket_dir:
            leafname = "rpcproxy-%s" % str(uuid4())
            dirname = "/tmp/%s" % leafname
            try:
                mkdir(dirname)
                has_socket_dir = True
                self.sockets_dir = dirname
            except OSError, e:
                if e.errno != EEXIST:
                    raise

        self.samba_host = samba_host
        self.log_level = log_level

    def __del__(self):
        for filename in listdir(self.sockets_dir):
            print >>sys.stderr, \
                "RPCProxyApplication: removing stale socket '%s'" % filename
            unlink(join(self.sockets_dir, filename))
        rmdir(self.sockets_dir)

    def __call__(self, environ, start_response):
        if "REQUEST_METHOD" in environ:
            method = environ["REQUEST_METHOD"]
            method_method = "_do_" + method
            if hasattr(self, method_method):
                if "wsgi.errors" in environ:
                    log_stream = environ["wsgi.errors"]
                else:
                    log_stream = sys.stderr

                logHandler = logging.StreamHandler(log_stream)
                fmter = logging.Formatter("[%(name)-%(process)d] %(levelname)s: %(message)s")
                logHandler.setFormatter(fmter)
                logHandler.set_name(method)

                logger = logging.Logger("rpcproxy")
                logger.setLevel(logging.INFO)
                logger.addHandler(logHandler)

                method_method_method = getattr(self, method_method)
                response = method_method_method(logger, environ, start_response)
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

    def _do_RPC_IN_DATA(self, logger, environ, start_response):
        handler = RPCProxyInboundChannelHandler(self.sockets_dir, self.logger)
        return handler.sequence(environ, start_response)

    def _do_RPC_OUT_DATA(self, logger, environ, start_response):
        handler = RPCProxyOutboundChannelHandler(self.sockets_dir,
                                                 self.samba_host,
                                                 self.logger)
        return handler.sequence(environ, start_response)
