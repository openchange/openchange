#!/usr/bin/python

import sys

sys.path.extend(("/usr/local/samba/lib/python2.7/site-packages",
                 "/home/wsourdeau/src/openchange/mapiproxy/services/ocsmanager/rpcproxy"))

# from rpcproxy.NTLMAuthHandler import *
from rpcproxy.RPCProxyApplication import *

#FIFO = "/tmp/fifo"


#application = NTLMAuthHandler(RPCProxyApplication())
application = RPCProxyApplication()
