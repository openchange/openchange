import os, sys
import hashlib

from base64 import urlsafe_b64encode as encode
from base64 import urlsafe_b64decode as decode
from pylons import config

class SingleAuthenticateModel(object):
    def __init__(self):
        self.username = config['ocsmanager']['auth']['username']
        self.password = config['ocsmanager']['auth']['password']
        self.encryption = config['ocsmanager']['auth']['encryption']

        if self.encryption == "plain":
            self.salt = os.urandom(4)
        elif self.encryption == "ssha":
            challenge_bytes = decode(self.password[6:])
            self.salt = challenge_bytes[20:]
        else:
            log.error("%s: Unsupported password encryption: %s", self.encryption)
            sys.exit()

    def getSalt(self, username):
        if username != self.username: return None
        return encode(self.salt)

    def verifyPassword(self, username, token_salt64, salt64, payload):
        if username != self.username: return (True, 'Invalid Username/Password')

        salt = decode(salt64)
        token_salt = decode(token_salt64)

        # Recreate the payload and compare it
        if self.encryption == "plain":
            h = hashlib.sha1(self.password)
            h.update(salt)
            sshaPassword = "{SSHA}" + encode(h.digest() + salt)
        elif self.encryption == "ssha":
            sshaPassword = self.password
        else:
            log.error("%s: Unsupported password encryption: %s", self.encryption)
            sys.exit()

        h = hashlib.sha1(str(username) + ':' + str(sshaPassword) + ':' + str(token_salt))
        h.update(token_salt)
        phash = h.hexdigest()

        # Final authentication check
        if phash == payload: return (False, None)

        return (True, 'Invalid Credentials')
