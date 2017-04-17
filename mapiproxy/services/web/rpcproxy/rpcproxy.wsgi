#!/usr/bin/env python2.7
#
# rpcproxy.wsgi -- OpenChange RPC-over-HTTP implementation
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

# this is the WSGI starting point for rpcproxy

import logging
import traceback

from openchange.web.auth.NTLMAuthHandler import *
from rpcproxy.RPCProxyApplication import *

def application(environ, start_response):

  SAMBA_HOST = environ.get('SAMBA_HOST', "127.0.0.1")
  RPCPROXY_LOGLEVEL = environ.get('RPCPROXY_LOGLEVEL', logging.INFO)
  log_level = logging.getLevelName(RPCPROXY_LOGLEVEL)

  # set a basic logger here for NTLMAuthHandler
  logging.basicConfig(level=log_level)
  log = logging.getLogger(__name__)

  app = NTLMAuthHandler(RPCProxyApplication(samba_host=SAMBA_HOST,
                                            log_level=log_level))
  try:
    return app(environ, start_response)
  except Exception as e:
    trace = traceback.format_exc()
    log.critical("Uncaught exception: %s\n%s", e,trace)
    status = "500 Internal Error"
    response_headers = [("content-type", "text/plain"),
                        ("content-length", str(len(status)))]
    start_response(status, response_headers)
    return status
