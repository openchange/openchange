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
import sys

from channels import RPCProxyInboundChannelHandler,\
    RPCProxyOutboundChannelHandler


class RPCProxyApplication(object):
    def __init__(self):
        print >>sys.stderr, "RPCProxy started"

    def __call__(self, environ, start_response):
        if "wsgi.errors" in environ:
            log_stream = environ["wsgi.errors"]
        else:
            log_stream = sys.stderr

        logHandler = logging.StreamHandler(log_stream)
        fmter = logging.Formatter("[%(process)d] [%(levelname)s] %(message)s")
        logHandler.setFormatter(fmter)

        logger = logging.Logger("rpcproxy")
        logger.setLevel(logging.INFO)
        logger.addHandler(logHandler)
        self.logger = logger

        if "REQUEST_METHOD" in environ:
            method = environ["REQUEST_METHOD"]
            method_method = "_do_" + method
            if hasattr(self, method_method):
                method_method_method = getattr(self, method_method)
                response = method_method_method(environ, start_response)
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

    def _do_RPC_IN_DATA(self, environ, start_response):
        handler = RPCProxyInboundChannelHandler(self.logger)
        return handler.sequence(environ, start_response)

    def _do_RPC_OUT_DATA(self, environ, start_response):
        handler = RPCProxyOutboundChannelHandler(self.logger)
        return handler.sequence(environ, start_response)
