import ldap
import logging
from pylons import config

from base64 import urlsafe_b64encode as encode
from base64 import urlsafe_b64decode as decode

log = logging.getLogger(__name__)

class RPCProxyAuthenticateModel:

    def __init__(self):
        self.auth_type = None
        self.ldap = {}
        self.ldap['host'] = config['ocsmanager']['rpcproxy']['ldap_host']
        self.ldap['post'] = config['ocsmanager']['rpcproxy']['ldap_port']
        self.ldap['basedn'] = config['ocsmanager']['rpcproxy']['ldap_basedn']

        return

    def Authenticate(self, auth):
        """ Authenticate user depending on the authentication type.
        Return 0 on success otherwise 1.
        """
        if auth is None or len(auth) != 2:
            return 1

        if auth[0] == 'Basic':
            return self.AuthenticateBasic(auth[1])
        elif auth[0] == 'NTLM':
            return self.AuthenticateNTLM(auth[1])
        else:
            return 1

    def AuthenticateBasic(self, payload):
        """ Implement Basic authentication scheme support.
        """
        blob = decode(payload)
        credentials = blob.split(':')
        if len(credentials[0].split('\\')) == 2:
            username = credentials[0].split('\\')[1]
        else:
            username = credentials[0]
        
        try:
            l = ldap.open(self.ldap['host'])
            l.protocol_version = ldap.VERSION3
            username = 'CN=%s,%s' % (username, self.ldap['basedn'])
            password = credentials[1]
            l.simple_bind(username, password)
        except ldap.LDAPError, e:
            log.debug(e)
            return 1

        return 0

    def AuthenticateNTLM(self, payload):
        """ Implement NTLM authentication scheme support.
        """
        return 1
