"""Routes configuration

The more specific and detailed routes should be defined first so they
may take precedent over the more generic routes. For more information
refer to the routes manual at http://routes.groovie.org/docs/
"""
from routes import Mapper

def make_map(config):
    """Create, configure and return the routes Mapper"""
    map = Mapper(directory=config['pylons.paths']['controllers'],
                 always_scan=config['debug'])
    map.minimization = False
    map.explicit = False

    # The ErrorController route (handles 404/500 error pages); it should
    # likely stay at the top, ensuring it can always be resolved
    map.connect('/error/{action}', controller='error')
    map.connect('/error/{action}/{id}', controller='error')

    # CUSTOM ROUTES HERE
    map.connect('/autodiscover/{action}.xml', controller="autodiscover")
    map.connect('/Autodiscover/{action}.xml', controller="autodiscover")
    map.connect('/ews/oab.xml', controller="oab", action="get_oab",
                conditions={'method': ["GET", "POST"]})
    map.connect('/ews/oab.xml', controller="oab", action="head_oab",
                conditions={'method': ["HEAD"]})
    map.connect('/ews/{controller}')
    map.connect('/ews/{controller}.wsdl')
    map.connect('/{controller}/{action}')
    map.connect('/{controller}/{action}/{id}')

    # RPC over HTTP
    map.connect('/rpc/rpcproxy.dll', controller='rpcproxy')

    return map
