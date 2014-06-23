"""Pylons middleware initialization"""

from beaker.middleware import SessionMiddleware
from paste.cascade import Cascade
from paste.registry import RegistryManager
from paste.urlparser import StaticURLParser
from paste.deploy.converters import asbool
from pylons.middleware import ErrorHandler, StatusCodeRedirect
from pylons.wsgiapp import PylonsApp
from routes.middleware import RoutesMiddleware

from openchange.web.auth.NTLMAuthHandler import NTLMAuthHandler

from ocsmanager.config.environment import load_environment

# from paste.auth.basic import AuthBasicHandler
# from ocsmanager.model.OCSAuthenticator import *


def make_app(global_conf, full_stack=True, static_files=True, **app_conf):
    """Create a Pylons WSGI application and return it

    ``global_conf``
        The inherited configuration for this application. Normally from
        the [DEFAULT] section of the Paste ini file.

    ``full_stack``
        Whether this application provides a full WSGI stack (by default,
        meaning it handles its own exceptions and errors). Disable
        full_stack when this application is "managed" by another WSGI
        middleware.

    ``static_files``
        Whether this application serves its own static files; disable
        when another web server is responsible for serving them.

    ``app_conf``
        The application's local configuration. Normally specified in
        the [app:<name>] section of the Paste ini file (where <name>
        defaults to main).

    """
    # Configure the Pylons environment
    config = load_environment(global_conf, app_conf)

    # The Pylons WSGI app
    app = PylonsApp(config=config)

    # Routing/Session Middleware
    app = RoutesMiddleware(app, config['routes.map'])
    app = SessionMiddleware(app, config)

    # CUSTOM MIDDLEWARE HERE (filtered by error handling middlewares)

    if asbool(full_stack):
        # Handle Python exceptions
        app = ErrorHandler(app, global_conf, **config['pylons.errorware'])

        # Display error documents for 401, 403, 404 status codes (and
        # 500 when debug is disabled)
        if asbool(config['debug']):
            app = StatusCodeRedirect(app, [417])
        else:
            app = StatusCodeRedirect(app, [400, 401, 403, 404, 417, 500])

    # authenticator = OCSAuthenticator(config)
    # app = AuthBasicHandler(app, "OCSManager", authenticator)
    auth_handler = NTLMAuthHandler(app)

    def ntlm_env_setter(environ, start_response):
        for var in ["SAMBA_HOST", "NTLMAUTHHANDLER_WORKDIR"]:
            try:
                environ[var] = app_conf[var]
            except KeyError:
                # FIXME: logging?
                pass
        return auth_handler(environ, start_response)

    # Establish the Registry for this application
    app = RegistryManager(ntlm_env_setter)

    if asbool(static_files):
        # Serve static files
        static_app = StaticURLParser(config['pylons.paths']['static_files'])
        app = Cascade([static_app, app])
    app.config = config
    return app
