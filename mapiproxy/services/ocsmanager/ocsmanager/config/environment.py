"""Pylons environment configuration"""
import os

from mako.lookup import TemplateLookup
from pylons.configuration import PylonsConfig
from pylons.error import handle_mako_error

import ocsmanager.lib.app_globals as app_globals
import ocsmanager.lib.helpers
import ocsmanager.lib.config as OCSConfig
from ocsmanager.config.routing import make_map
import openchange.mapistore as mapistore

# samba
import samba.param
from samba.samdb import SamDB
from samba.auth import system_session, admin_session

FIRST_ORGANIZATION = "First Organization"
FIRST_ORGANIZATION_UNIT = "First Administrative Group"

def _load_samba_environment():
    """Load the samba configuration vars from smb.conf and the sam.db.
    
    """

    params = samba.param.LoadParm()
    params.load_default()

    netbiosname = params.get("netbios name")
    hostname = netbiosname.lower()

    dnsdomain = params.get("realm")
    dnsdomain = dnsdomain.lower()

    serverrole = params.get("server role")
    if serverrole == "domain controller":
        domaindn = "DC=" + dnsdomain.replace(".", ",DC=")
    else:
        domaindn = "CN=" + netbiosname

    rootdn = domaindn
    configdn = "CN=Configuration," + rootdn
    firstorg = FIRST_ORGANIZATION
    firstou = FIRST_ORGANIZATION_UNIT

    samdb_ldb = SamDB(url=params.samdb_url(), lp=params)
    sam_environ = {"samdb_ldb": samdb_ldb,
                   "domaindn": domaindn,
                   "firstorgdn": ("CN=%s,CN=Microsoft Exchange,CN=Services,%s"
                                  % (firstorg, configdn)),
                   "legacyserverdn": ("/o=%s/ou=%s/cn=Configuration/cn=Servers"
                                      "/cn=%s"
                                      % (firstorg, firstou, netbiosname)),
                   "hostname": hostname,
                   "dnsdomain": dnsdomain}

    # OpenChange dispatcher DB names

    return sam_environ


def load_environment(global_conf, app_conf):
    """Configure the Pylons environment via the ``pylons.config``
    object
    """
    config = PylonsConfig()
    
    # Pylons paths
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    paths = dict(root=root,
                 controllers=os.path.join(root, 'controllers'),
                 static_files=os.path.join(root, 'public'),
                 templates=[os.path.join(root, 'templates')])

    # Initialize config with the basic options
    config.init_app(global_conf, app_conf, package='ocsmanager', paths=paths)

    config['routes.map'] = make_map(config)
    config['pylons.app_globals'] = app_globals.Globals(config)
    config['pylons.h'] = ocsmanager.lib.helpers
    
    # Setup cache object as early as possible
    import pylons
    pylons.cache._push_object(config['pylons.app_globals'].cache)
    

    # Create the Mako TemplateLookup, with the default auto-escaping
    config['pylons.app_globals'].mako_lookup = TemplateLookup(
        directories=paths['templates'],
        error_handler=handle_mako_error,
        module_directory=os.path.join(app_conf['cache_dir'], 'templates'),
        input_encoding='utf-8', default_filters=['escape'],
        imports=['from webhelpers.html import escape'])

    # CONFIGURATION OPTIONS HERE (note: all config options will override
    # any Pylons config options)
    ocsconfig = OCSConfig.OCSConfig(os.path.join(config.get('here'), 'ocsmanager.ini'))
    config['ocsmanager'] = ocsconfig.load()

    config['samba'] = _load_samba_environment()

    mapistore.set_mapping_path(config['ocsmanager']['main']['mapistore_data'])
    config['mapistore'] = mapistore.mapistore(config['ocsmanager']['main']['mapistore_root']).management()
    if config['ocsmanager']['main']['debug'] == "yes":
        config['mapistore'].verbose = True;
    
    return config
