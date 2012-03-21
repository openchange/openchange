from pylons import config
from lxml import etree
import re

from base64 import urlsafe_b64encode as encode
from base64 import urlsafe_b64decode as decode

from ocsmanager.model.auth import SingleAuthenticateModel as single
from ocsmanager.lib.utils import validateDocXML
#from ocsmanager.model.auth import LDAPAuthenticateModel as ldap
#from ocsmanager.model.auth import FileAuthenticateModel as mfile

class AuthenticateModel:

    def __init__(self):
        auth_type = config['ocsmanager']['auth']['type']
        if auth_type == 'single':
            self.model = single.SingleAuthenticateModel()
#        elif auth_type == 'ldap':
#            self.model = ldap.LDAPAuthenticateModel()
#        elif auth_type == 'file':
#            self.model = mfile.FileAuthenticateModel()
        else:
            log.error('Unsupported authentication scheme: %s', auth_type)
            sys.exit()

    def getTokenLoginSalt(self, login):
        """Returns the salt of the SSHA user password on success,
        otherwise None."""
        return self.model.getSalt(login)

    def getSessionToken(self, payload):
        """Validate XML document and extract authentication token from
        the payload."""
        (error, xmlData) = validateDocXML(payload)
        if error is True: return None

        token = xmlData.find('token')
        if token is None: return None
        return decode(token.text)

        

    def getTokenLogin(self, payload):
        """Validate XML document and retrieve the login from XML payload."""
        (error, xmlData) = validateDocXML(payload)
        if error is True: return None

        login = xmlData.find('login')
        if login is None: return None

        # Ensure login is made of allowed characters (prevent from injections)
        if re.match(r'^[A-Za-z0-9_.@-]+$', login.text) is None: return None

        return login.text

    def verifyPassword(self, login, token_salt, salt, payload):
        """Check if the supplied login hash is correct.
        """
        (error, xmlData) = validateDocXML(payload)
        if error is True: return (error, xmlData)

        token = xmlData.find('token')
        if token is None: return (True, 'No token parameter found')
 
        return self.model.verifyPassword(login, token_salt, salt, token.text)
