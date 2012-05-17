import ldap
import logging


logger = logging.getLogger(__name__)


class OCSAuthenticator(object):
    def __init__(self, config):
        self.config = config

    def __call__(self, environ, username, password):
        result = False

        config = self.config['ocsmanager']['rpcproxy'] 
        # FIXME: we should perform an indirect bind, based on sAMAccountName
        userdn = 'CN=%s,%s' % (username, config['ldap_basedn'])

        try:
            # FIXME: should be # 'ocsmanager' alone since we require auth from
            # all actions
            l = ldap.open(config['ldap_host'])
            l.protocol_version = ldap.VERSION3
            l.simple_bind(userdn, password)
            l.result() # this will cause an exception in case of a failure
            result = True
        except ldap.LDAPError, e:
            logger.info("authentication failure for '%s'", userdn)
            logger.debug(e)
        l.unbind()

        return result

