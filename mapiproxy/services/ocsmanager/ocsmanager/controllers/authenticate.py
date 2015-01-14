import logging
import hashlib
import os

from base64 import urlsafe_b64encode as encode
from base64 import urlsafe_b64decode as decode

from pylons import request, response, session, tmpl_context as c, url
from pylons.controllers.util import abort, redirect
from pylons.decorators.rest import restrict
from ocsmanager.model import AuthenticateModel

from ocsmanager.lib.base import BaseController, render

log = logging.getLogger(__name__)

class AuthenticateController(BaseController):

    def _auth_abort(self, code, message):
        c.code = code
        c.message = message
        return render('/error.xml')

    @restrict('POST')
    def token(self):
        """ Return a session token, one-time hash and password hash
        for the user.
        """
        # Ensure Content-type is text/xml
        if request.headers.get("Content-Type", "").startswith("text/xml") is False:
            return self._auth_abort(417, 'Invalid Parameter')

        # Retrieve request XML body
        payload = request.body
        if payload is None:
            log.error('Empty payload in auth:token()')
            return self._auth_abort(417, 'Invalid Parameter')

        # Retrieve the salt from the model
        authModel = AuthenticateModel.AuthenticateModel()
        login = authModel.getTokenLogin(payload)
        if login is None:
            return self._auth_abort(417, 'Invalid Parameter')

        salt = authModel.getTokenLoginSalt(login)
        if salt is None:
            log.debug('Invalid user %s', login)
            salt = encode(hashlib.sha1(os.urandom(4)).digest())

        session['token'] = encode(hashlib.sha1(os.urandom(8)).digest())
        session['token_salt'] = encode(hashlib.sha1(os.urandom(8)).digest())
        session['salt'] = salt
        session['login'] = login
        session.save()

        c.token_salt = session['token_salt']
        c.salt = salt

        response.set_cookie('token', session['token'])
        response.headers['content-type'] = 'text/xml; charset=utf-8'
        return render('/token.xml')

    @restrict('POST')
    def login(self):
        """Authenticate the user on ocsmanager.
        """

        if not "ocsmanager" in request.cookies: return self._auth_abort(403, 'Invalid Session')
        if not "token" in session: return self._auth_abort(403, 'Invalid Session')
        if not "token" in request.cookies: return self._auth_abort(403, 'Invalid Token')
        if request.cookies.get('token') != session['token']: return self._auth_abort(403, 'Invalid Token')
        if not "login" in session: return self._auth_abort(403, 'Invalid Session')

        payload = request.body
        if payload is None:
            log.error('Empty payload in auth:login()')
            return self._auth_abort(417, 'Invalid Parameter')

        authModel = AuthenticateModel.AuthenticateModel()
        (error, msg) = authModel.verifyPassword(session['login'], session['token_salt'], session['salt'], payload)
        if error is True:
            response.delete_cookie('token')
            session['token'] = None
            return self._auth_abort(401, 'Invalid credentials')

        # Authentication was successful, remove auth token - no longer needed
        session['token'] = None
        response.delete_cookie('token')
        session['tokenLogin'] = hashlib.sha1(os.urandom(8)).hexdigest()
        session.save()
        c.tokenLogin = encode(session['tokenLogin'])
        c.ttl = 10
        return render('/login.xml')
