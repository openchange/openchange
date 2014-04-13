import logging

from pylons import request, response
# from ocsmanager.model import RPCProxyAuthenticateModel

from ocsmanager.lib.base import BaseController

log = logging.getLogger(__name__)

class RpcproxyController(BaseController):

    def index(self):
        # auth = RPCProxyAuthenticateModel.RPCProxyAuthenticateModel()
        if request.method == 'RPC_IN_DATA':
            log.debug('IN Channel Request')
            # retval = auth.Authenticate(request.authorization)
            response.headers['Content-Type'] = 'application/rpc'
            # if retval == 1:
            #    log.debug('Authentication failure')
            # return
        elif request.method == 'RPC_OUT_DATA':
            log.debug('OUT Channel Request')
            # retval = auth.Authenticate(request.authorization)
            response.headers['Content-Type'] = 'application/rpc'

            #if retval == 1:
            #    log.debug('Authentication failure')

        return ''

