import logging
import traceback
import urllib

from pylons import request, response, session, tmpl_context as c, url
from pylons.controllers.util import abort, redirect
from pylons.decorators.rest import restrict
from xml.etree.ElementTree import Element, ElementTree, tostring
from cStringIO import StringIO
from time import time, strftime, localtime

from ocsmanager.lib.base import BaseController, render

log = logging.getLogger(__name__)

REQUEST_XMLNS = ("http://schemas.microsoft.com/exchange/services/2006/messages")

class OofHandler(object):
    """This class parses the XML request, interprets it, find the requested
    answers and spills back an appropriate XML response.
    """

    def __init__(self, req, env):
        self.environ = env
        self.request = None
        self._parse_request(req.body)

        self.http_server_name = None
        server_env_names = iter(["HTTP_X_FORWARDED_SERVER",
                                 "HTTP_X_FORWARDED_HOST",
                                 "HTTP_HOST"])
        try:
            while self.http_server_name == None:
                env_name = server_env_names.next()
                if env_name in self.environ:
                    self.http_server_name \
                        = (self.environ[env_name].split(":"))[0]
        except StopIteration:
            pass

    def _parse_request(self, body):
        if body is not None and len(body) > 0:
            body = urllib.unquote_plus(body)
            tree = ElementTree(file=StringIO(body))
            log.debug(tree)

    def _append_elements(self, top_element, tree_dict):
        for (tag, value) in tree_dict.iteritems():
            new_element = Element(tag)
            if isinstance(value, dict):
                self._append_elements(new_element, value)
            else:
                new_element.text = value
            top_element.append(new_element)

    def _append_error(self, resp_element, error_code):
        error_messages = {500: "Unknown e-mail address",
                          501: "No configuration information available" \
                              " for e-mail address",
                          600: "Invalid request",
                          601: "Configuration information not available",
                          602: "Error in configuration information for" \
                              " e-mail address",
                          603: "An internal error occurred",
                          0: "An unknown error occurred"}

        seconds_as_float = time()
        seconds_as_int = int(seconds_as_float)
        millisecs = int((seconds_as_float - seconds_as_int) * 1000000)
        time_val = "%s.%d" % (strftime("%2H:%2M:%2S",
                                       localtime(seconds_as_int)),
                              millisecs)

        server_id = abs(hash(self.http_server_name))
        error_element = Element("Error", {"Time": time_val,
                                          "Id": str(server_id)})
        resp_element.append(error_element)

        if error_code in error_messages:
            message = error_messages[error_code]
        else:
            message = error_messages[0]

        response_tree = {"ErrorCode": str(error_code),
                         "Message": message}
        self._append_elements(error_element, response_tree)
        error_element.append(Element("DebugData"))

    def response(self):
        """The method the actually performs the class's parsing and
        interpretation work.

        Returns a fully formatted XML response.
        """

        ## TODO:
        # error handling when self.request is none

        top_element = Element("Envelope", {"xmlns": "http://schemas.xmlsoap.org/soap/envelope/"})

        # Create the header
        header_element = Element("Header")
        server_version_info_element = Element("ServerVersionInfo", {"xmlns": "http://schemas.microsoft.com/exchange/services/2006/types"});
        header_element.append(server_version_info_element)
        top_element.append(header_element)

        # Create the body
        body_element = Element("Body")
        top_element.append(body_element)

        if self.request is not None:
            pass
        else:
            self._append_error(body_element, 600)

        body = ("<?xml version='1.0' encoding='utf-8'?>\n"
                + tostring(top_element, "utf-8"))
        return body


class OofController(BaseController):
    """The constroller class for OutOfOffice requests."""

    @restrict('POST', 'GET')
    def index(self, **kwargs):
        #TODO: authentication
        try:
            if "environ" in kwargs:
                environ = kwargs["environ"]
            else:
                environ = None

            rqh = OofHandler(request, environ)
            response.headers["content-type"] = "application/xml"
            body = rqh.response()
        except:
            response.status = 500
            response.headers["content-type"] = "text/plain"
            # TODO: disable the showing of exception in prod
            body = "An exception occurred:\n" + traceback.format_exc()

        return body
