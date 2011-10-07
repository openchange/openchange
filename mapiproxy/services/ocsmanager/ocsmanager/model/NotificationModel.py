from pylons import config
from lxml import etree
from ocsmanager.lib.utils import validateDocXML
import re

class NotificationModel:

    def getNewMailParams(self, payload):
        """Retrieve newmail parameters."""
        (error, xmlData) = validateDocXML(payload)
        if error is True: return (error, xmlData)

        # Retrieve MAPIStore management object
        mgmt = config['mapistore']

        # Initialize dictionary
        params = {}

        # Retrieve notification and ensure it's newmail
        notification = xmlData.find('notification')
        if notification is None: return (True, 'Missing Notification')
        if not 'category' in notification.attrib: return (True, 'Missing notification type')
        if notification.attrib['category'] != 'newmail': return (True, 'Invalid notification type')

        # backend parameter
        param = notification.find('backend')
        if param is None or param.text is None: return (True, 'Invalid/Missing backend parameter')
        if mgmt.registered_backend(param.text) is False: return (True, 'Specified backend is invalid')
        params['backend'] = param.text

        # username parameter
        param = notification.find('username')
        if param is None or param.text is None: return (True, 'Invalid/Missing username parameter')
        params['username'] = param.text

        # folder parameter
        param = notification.find('folder')
        if param is None or param.text is None: return (True, 'Invalid/Missing folder parameter')
        params['folder'] = param.text

        # messageID parameter
        param = notification.find('messageID')
        if param is None or param.text is None: retrun (True, 'Invalid/Missing messageID parameter')
        params['messageID'] = param.text

        return (False, params)
