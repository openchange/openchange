import httplib
import socket

from samba.gensec import Security
from samba.auth import AuthContext


__all__ = ['NTLMAuthHandler']


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

    def _in_progress_response(self, start_response, ntlm_data=None):
        status = "401 %s" % httplib.responses[401]
        www_auth_value = "NTLM"
        if ntlm_data is None:
            www_auth_value = "NTLM"
        else:
            www_auth_value = "NTLM %s" % ntlm_data.encode("base64").strip()
        content = "More data needed..."
        headers = [("WWW-Authenticate", www_auth_value),
                   ("Content-Type", "text/plain"),
                   ("Content-Length", "%d" % len(content))]
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

    def __call__(self, env, start_response):
        client = env["REMOTE_ADDR"] # temporary
        # if "REMOTE_ADDR" in env and "REMOTE_PORT" in env:
        #     client = "%(REMOTE_ADDR)s:%(REMOTE_PORT)s".format(env)
        # else:
        #     input_file = env["wsgi.input"]
        #     if isinstance(input_file, socket.socket):
        #         name = input_file.getpeername()
        #         client = "%s:%d" % name
        #     else:
        #         client = None

        if client is not None:
            if not client in self.client_status: # first stage
                server = Security.start_server(auth_context=AuthContext())
                server.start_mech_by_name("ntlmssp")
                self.client_status[client] = {"stage": "stage0",
                                              "server": server}
                response = self._in_progress_response(start_response)
            else:
                status_stage = self.client_status[client]["stage"]

                if status_stage == "ok":
                    # client authenticated previously
                    response = self.application(env, start_response)
                elif status_stage == "stage0":
                    # test whether client supports "NTLM"
                    if "HTTP_AUTHORIZATION" in env:
                        auth = env["HTTP_AUTHORIZATION"]
                        server = self.client_status[client]["server"]
                        auth_msg = server.update(auth[5:].decode('base64'))
                        response = self._in_progress_response(start_response,
                                                              auth_msg[1])
                        self.client_status[client]["stage"] = "stage1"
                    else:
                        del(self.client_status[client])
                        response \
                            = self._failure_response(start_response,
                                                     "missing auth header 1")
                elif status_stage == "stage1":
                    if "HTTP_AUTHORIZATION" in env:
                        auth = env["HTTP_AUTHORIZATION"]
                        server = self.client_status[client]["server"]
                        try:
                            auth_msg = server.update(auth[5:].decode('base64'))
                        except RuntimeError: # a bit violent...
                            auth_msg = (0,)

                        if auth_msg[0] == 1:
                            # authentication completed
                            self.client_status[client]["stage"] = "ok"
                            response = self.application(env, start_response)
                        else:
                            # we start over
                            self.client_status[client]["stage"] = "stage0"
                            response = \
                                self._in_progress_response(start_response)
                    else:
                        del(self.client_status[client])
                        response \
                            = self._failure_response(start_response,
                                                     "missing auth header")
                else:
                    raise RuntimeError("this condition should not occur")
        else:
            response = self._failure_response(start_response,
                                              "missing REMOTE_X vars")

        if isinstance(response, list):
            response.append(str(env))

        return response


middleware = NTLMAuthHandler
