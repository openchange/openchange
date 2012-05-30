#!/usr/bin/python
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

import sys

sys.path.extend(("/usr/local/samba/lib/python2.7/site-packages",
                 "/home/wsourdeau/src/openchange/mapiproxy/services/ocsmanager/rpcproxy"))

from rpcproxy.NTLMAuthHandler import *
from rpcproxy.RPCProxyApplication import *

application = NTLMAuthHandler(RPCProxyApplication())
# application = RPCProxyApplication()
