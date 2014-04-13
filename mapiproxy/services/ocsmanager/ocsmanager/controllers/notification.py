import logging

from pylons import request, session, tmpl_context as c
from pylons.decorators.rest import restrict
from ocsmanager.model import AuthenticateModel
from ocsmanager.model import NotificationModel

from ocsmanager.lib.base import BaseController, render

log = logging.getLogger(__name__)

class NotificationController(BaseController):

    def _abort(self, code, message):
        c.code = code
        c.message = message
        return render('/error.xml')

    @restrict('PUT')
    def newmail(self):
        """ Send a newmail notification to be dispatched to OpenChange
        Server.
        """
        notification = NotificationModel.NotificationModel()
        authModel = AuthenticateModel.AuthenticateModel()
        token = authModel.getSessionToken(request.body)
        if token is None: return self._abort(472, 'Invalid token')
        if token != session['tokenLogin']: return self._abort(403, 'Access forbidden')

        (error,params) = notification.getNewMailParams(request.body)
        if error is True: return self._abort(417, params)

        print params

        return render('/notification.xml')
        
