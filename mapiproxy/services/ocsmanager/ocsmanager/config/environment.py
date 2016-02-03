# environment.py
# -*- coding: utf-8 -*-
#
# Copyright (C) 2011-2014  Julien Kerihuel <jkerihuel@openchange.org>
#                          Jean Raby <jraby@inverse.ca>
#                          Jesús García Saez <jgarcia@zentyal.com>
#                          Enrique J. Hernández <ejhernandez@zentyal.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
"""Pylons environment configuration"""
import os

from mako.lookup import TemplateLookup
from pylons.configuration import PylonsConfig
from pylons.error import handle_mako_error

import ocsmanager.lib.app_globals as app_globals
import ocsmanager.lib.helpers
import ocsmanager.lib.config as OCSConfig
import openchange.mapistore as mapistore
from ocsmanager.config.routing import make_map
from ocsmanager.lib.openchangedb import get_openchangedb
from ocsmanager.lib.samdb import SamDBWrapper

# samba
import samba.param
from samba.auth import system_session
from samba.credentials import Credentials
import ldb

import time
import logging
logger = logging.getLogger(__name__)


def _load_samba_environment(retries):
    """Load the samba configuration vars from smb.conf and the sam.db."""
    params = samba.param.LoadParm()
    params.load_default()

    netbiosname = params.get("netbios name")
    hostname = netbiosname.lower()

    dnsdomain = params.get("realm")
    dnsdomain = dnsdomain.lower()

    creds = Credentials()
    creds.guess(params)
    creds.set_machine_account(params)

    samdb_url = params.get('dcerpc_mapiproxy:samdb_url')
    if samdb_url is None:
        samdb_url = params.samdb_url()

    samdb_ldb = None
    backoff = 0.2
    next_retry = 0
    while samdb_ldb is None:
        try:
            samdb_ldb = SamDBWrapper(url=samdb_url,
                                     session_info=system_session(),
                                     credentials=creds,
                                     lp=params)
        except ldb.LdbError as ex:
            number, desc = ex
            if number == ldb.ERR_OPERATIONS_ERROR:
                # this is a cannot connect error
                if retries != 0:
                    next_retry += 1
                    if next_retry > retries:
                        logger.error("Maximum samba connection retries reached (%i)"
                                     % retries)
                        raise ex
                logger.warn("Cannot connect to samba server. Backing off for %.2f seconds"
                             % backoff)
                time.sleep(backoff)
                backoff *= 2
                if backoff > 60:
                    backoff = 60
            else:
                raise ex

    domaindn = samdb_ldb.domain_dn()

    rootdn = domaindn
    configdn = "CN=Configuration," + rootdn

    firstorg = None
    firstorg_basedn = 'CN=Microsoft Exchange,CN=Services,' + configdn
    res = samdb_ldb.search(base=firstorg_basedn, scope=ldb.SCOPE_ONELEVEL, expression='(objectClass=msExchOrganizationContainer)', attrs=['cn'])
    if res:
        if len(res) > 1:
            logger.warn('More than one exchange organization found')
        firstorg = res[0]['cn'][0]
    if not firstorg:
        raise Exception("Cannot find first exchange organization in samba database")

    firstou = None
    firstou_basedn = "CN=Administrative Groups,CN=%s,%s" %(firstorg, firstorg_basedn)
    res = samdb_ldb.search(base=firstou_basedn, scope=ldb.SCOPE_ONELEVEL, expression='(objectClass=msExchAdminGroup)', attrs=['cn'])
    if res:
        if len(res) > 1:
            logger.warn('More than one exchange administration group found')
        firstou = res[0]['cn'][0]
    if not firstou:
        raise Exception("Cannot find exchange first organization unit in samba database")    

    username_mail = False
    if params.get("auth:usernames are emails") == 'yes':
        username_mail = True

    sam_environ = {"samdb_ldb": samdb_ldb,
                   "private_dir": params.get("private dir"),
                   "domaindn": domaindn,
                   "oc_user_basedn": "CN=%s,CN=%s,CN=%s,%s" \
                       % (firstou, firstorg, netbiosname, domaindn),
                   "firstorgdn": ("CN=%s,CN=Microsoft Exchange,CN=Services,%s"
                                  % (firstorg, configdn)),
                   "legacyserverdn": ("/o=%s/ou=%s/cn=Configuration/cn=Servers"
                                      "/cn=%s"
                                      % (firstorg, firstou, netbiosname)),
                   "hostname": hostname,
                   "dnsdomain": dnsdomain,
                   'username_mail': username_mail,
    }

    return sam_environ


def load_environment(global_conf, app_conf):
    """Configure the Pylons environment via the ``pylons.config`` object"""
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
    ocsconfig = OCSConfig.OCSConfig(global_conf['__file__'])
    config['ocsmanager'] = ocsconfig.load()
    
    samba_retries = config['ocsmanager']['main']['samba_retries']
    config['samba'] = _load_samba_environment(samba_retries)
    config['ocdb'] = get_openchangedb(config['samba']['samdb_ldb'].lp)

    mapistore.set_mapping_path(config['ocsmanager']['main']['mapistore_data'])
    mstore = mapistore.MAPIStore(config['ocsmanager']['main']['mapistore_root'])
    config['mapistore'] = mstore

    return config
