import httplib
import socket
import uuid

from samba.gensec import Security
from samba.auth import AuthContext


__all__ = ['NTLMAuthHandler']

COOKIE_NAME="ocs-ntlm-auth"


class NTLMAuthHandler(object):
    """
    HTTP/1.0 ``NTLM`` authentication middleware

    Parameters: application -- the application object that is called only upon
    successful authentication.

    """

    def __init__(self, application):
        # TODO: client expiration and/or cleanup
        self.client_status = {}
        self.application = application

    def _in_progress_response(self, start_response, ntlm_data=None, client_id=None):
        status = "401 %s" % httplib.responses[401]
        content = "More data needed..."
        headers = [("Content-Type", "text/plain"),
                   ("Content-Length", "%d" % len(content))]
        if ntlm_data is None:
            www_auth_value = "NTLM"
        else:
            enc_ntlm_data = ntlm_data.encode("base64")
            www_auth_value = ("NTLM %s"
                              % enc_ntlm_data.strip().replace("\n", ""))
        if client_id is not None:
            # MUST occur when ntlm_data is None, can still occur otherwise
            headers.append(("Set-Cookie", "%s=%s" % (COOKIE_NAME, client_id)))

        headers.append(("WWW-Authenticate", www_auth_value))
        start_response(status, headers)

        return [content]

    def _failure_response(self, start_response, explanation=None):
        status = "403 %s" % httplib.responses[403]
        content = "Authentication failure"
        if explanation is not None:
            content = content + " (%s)" % explanation
        headers = [("Content-Type", "text/plain"),
                   ("Content-Length", "%d" % len(content))]
        start_response(status, headers)

        return [content]

    def _get_cookies(self, env):
        cookies = {}
        if "HTTP_COOKIE" in env:
            cookie_str = env["HTTP_COOKIE"]
            for pair in cookie_str.split(";"):
                (key, value) = pair.strip().split("=")
                cookies[key] = value

        return cookies

    def __call__(self, env, start_response):
        cookies = self._get_cookies(env)
        if COOKIE_NAME in cookies:
            client_id = cookies[COOKIE_NAME]
        else:
            client_id = None

        # old model that only works with mod_wsgi:
        # if "REMOTE_ADDR" in env and "REMOTE_PORT" in env:
        #     client_id = "%(REMOTE_ADDR)s:%(REMOTE_PORT)s".format(env)

        if client_id is None or client_id not in self.client_status:
            # first stage
            server = Security.start_server(auth_context=AuthContext())
            server.start_mech_by_name("ntlmssp")
            client_id = str(uuid.uuid4())

            if "HTTP_AUTHORIZATION" in env:
                # Outlook may directly have sent a NTLM payload
                auth = env["HTTP_AUTHORIZATION"]
                auth_msg = server.update(auth[5:].decode('base64'))
                response = self._in_progress_response(start_response,
                                                      auth_msg[1],
                                                      client_id)
                self.client_status[client_id] = {"stage": "stage1",
                                                 "server": server}
            else:
                self.client_status[client_id] = {"stage": "stage0",
                                                 "server": server}
                response = self._in_progress_response(start_response, None, client_id)
        else:
            status_stage = self.client_status[client_id]["stage"]

            if status_stage == "ok":
                # client authenticated previously
                response = self.application(env, start_response)
            elif status_stage == "stage0":
                # test whether client supports "NTLM"
                if "HTTP_AUTHORIZATION" in env:
                    auth = env["HTTP_AUTHORIZATION"]
                    server = self.client_status[client_id]["server"]
                    auth_msg = server.update(auth[5:].decode('base64'))
                    response = self._in_progress_response(start_response,
                                                          auth_msg[1])
                    self.client_status[client_id]["stage"] = "stage1"
                else:
                    del(self.client_status[client_id])
                    response = self._failure_response(start_response,
                                                      "failure at '%s'"
                                                      % status_stage)
            elif status_stage == "stage1":
                if "HTTP_AUTHORIZATION" in env:
                    auth = env["HTTP_AUTHORIZATION"]
                    server = self.client_status[client_id]["server"]
                    try:
                        auth_msg = server.update(auth[5:].decode('base64'))
                    except RuntimeError: # a bit violent...
                        auth_msg = (0,)

                    if auth_msg[0] == 1:
                        # authentication completed
                        self.client_status[client_id]["stage"] = "ok"
                        del(self.client_status[client_id]["server"])
                        response = self.application(env, start_response)
                    else:
                        # we start over with the whole process

                        server = Security.start_server(auth_context=AuthContext())
                        server.start_mech_by_name("ntlmssp")
                        self.client_status[client_id] = {"stage": "stage0",
                                                         "server": server}
                        response = self._in_progress_response(start_response)
                else:
                    del(self.client_status[client_id])
                    response = self._failure_response(start_response,
                                                      "failure at '%s'"
                                                      % status_stage)
            else:
                raise RuntimeError("none shall pass!")

        return response


middleware = NTLMAuthHandler
