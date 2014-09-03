# -*- coding: utf-8 -*-
#
# Copyright (C) 2011-2014  Julien Kerihuel <jkerihuel@openchange.org>
#                          Enrique J. Hernández <ejhernandez@zentyal.com>
#                          Samuel Cabrero <scabrero@zentyal.com>
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
"""
Load and retrieve OCSManager configuration
"""
import ConfigParser
import os
import logging
import re
import sys


log = logging.getLogger(__name__)


class OCSConfig(object):
    """OCSConfig class documentation. Load and retrieve ocsmanager
    options from configuration file.
    """

    def __init__(self, config):
        """Initialize OCSConfig instance.
        """

        self.cfg = ConfigParser.ConfigParser()
        self.config = config
        self.d = {}

    def __get_section(self, section=None):
        if not self.cfg.has_section(section):
            log.error("%s: Missing [%s] section", self.config, section)
            sys.exit()

    def __get_option(self, section=None, option=None, shash=None, ohash=None, dflt=None):
        if dflt is None and not self.cfg.has_option(section, option):
            log.error("%s: Missing %s option in [%s] section", self.config, option, section)
            sys.exit()

        if dflt is not None and not self.cfg.has_option(section, option):
            cfg_option = dflt
        else:
            cfg_option = self.cfg.get(section, option)

        if shash is not None:
            if not shash in self.d:
                self.d[shash] = {}
        else:
            if not section in self.d:
                self.d[section] = {}

        if shash is None and ohash is None:
            self.d[section][option] = cfg_option
        if shash is not None and ohash is None:
            self.d[shash][option] = cfg_option
        if shash is not None and ohash is not None:
            self.d[shash][ohash] = cfg_option
        if shash is None and ohash is not None:
            self.d[section][ohash] = cfg_option

    def __get_bool_option(self, section=None, option=None, dflt=None):
        if dflt is None and not self.cfg.has_option(section, option):
            log.error("%s: Missing %s option in [%s] section", self.config, option, section)
            sys.exit()

        if dflt is not None and not self.cfg.has_option(section, option):
            cfg_option = dflt
        else:
            cfg_option = self.cfg.getboolean(section, option)

        self.d[section][option] = cfg_option

    def __get_list_option(self, section=None, option=None, dflt=None, delimiter=',\s*'):
        if dflt is None and not self.cfg.has_option(section, option):
            log.error("%s: Missing %s option in [%s] section", self.config, option, section)
            sys.exit()

        if dflt is not None and not self.cfg.has_option(section, option):
            cfg_option = dflt
        else:
            cfg_option = self.cfg.get(section, option)
            cfg_option = re.split(delimiter, cfg_option)

        if section not in self.d:
            self.d[section] = {}

        self.d[section][option] = cfg_option

    def __parse_main(self):
        """Parse [main] configuration section.
        """

        self.__get_section('main')
        self.__get_option('main', 'auth', 'auth', 'type')
        self.__get_option('main', 'mapistore_root')
        self.__get_option('main', 'mapistore_data')
        self.__get_option('main', 'debug')
        if self.d['main']['debug'] != "yes" and self.d['main']['debug'] != "no":
            log.error("%s: invalid debug value: %s. Must be set to yes or no", self.config, self.d['main']['debug'])
            sys.exit()

        if not self.cfg.has_section('auth:%s' % self.d['auth']['type']):
            log.error("%s: Missing [auth:%s] section", self.config, self.d['auth']['type'])
            sys.exit()

    def __parse_auth(self):
        section = 'auth:%s' % self.d['auth'].get('type')
        self.__get_section(section)

        if self.d['auth']['type'] == 'single':
            self.__get_option(section, 'username', 'auth', None)
            self.__get_option(section, 'password', 'auth', None)
            if self.d['auth']['password'].startswith('{SSHA}'):
                self.d['auth']['encryption'] = 'ssha'
            else:
                self.d['auth']['encryption'] = 'plain'

        if self.d['auth']['type'] == 'file':
            self.__getoption(section, 'file', 'auth', None)

        if self.d['auth']['type'] == 'ldap':
            self.__get_option(section, 'host', 'auth', None)
            self.__get_option(section, 'port', 'auth', None, 389)
            self.__get_option(section, 'bind_dn', 'auth', None)
            self.__get_option(section, 'bind_pw', 'auth', None)
            self.__get_option(section, 'basedn', 'auth', None)
            self.__get_option(section, 'filter', 'auth', None, '(cn=%s)')
            self.__get_option(section, 'attrs', 'auth', None, '*')

    def __parse_rpcproxy(self):
        self.__get_section('rpcproxy:ldap')
        self.__get_option('rpcproxy:ldap', 'host', 'rpcproxy', 'ldap_host')
        self.__get_option('rpcproxy:ldap', 'port', 'rpcproxy', 'ldap_port')
        self.__get_option('rpcproxy:ldap', 'basedn', 'rpcproxy', 'ldap_basedn')

    def __parse_autodiscover(self):
        self.__get_section('autodiscover')
        self.__get_list_option('autodiscover', 'internal_networks', dflt=['0.0.0.0/0'])

    def __parse_autodiscover_rpcproxy(self):
        self.__get_section('autodiscover:rpcproxy')
        # Have to set a default value to avoid missing option exception
        self.__get_option('autodiscover:rpcproxy', 'external_hostname', dflt="__none__")
        self.__get_bool_option('autodiscover:rpcproxy', 'ssl', dflt=False)
        self.__get_bool_option('autodiscover:rpcproxy', 'enabled', dflt=True)

    def __parse_outofoffice(self):
        self.__get_section('outofoffice')
        self.__get_option('outofoffice', 'sieve_script_path')
        self.__get_bool_option('outofoffice', 'sieve_script_path_mkdir',
                               dflt=False)

    def load(self):
        """Load the configuration file.
        """

        try:
            os.stat(self.config)
        except OSError as e:
            log.error("%s (%s): %s", self.config, e.args[0], e.args[1])
            sys.exit()

        try:
            self.cfg.read(self.config)
        except ConfigParser.MissingSectionHeaderError:
            log.error("%s: Invalid Configuration File", self.config)
            sys.exit()

        self.__parse_main()
        self.__parse_auth()
        self.__parse_rpcproxy()
        self.__parse_autodiscover()
        self.__parse_autodiscover_rpcproxy()
        self.__parse_outofoffice()

        return self.d
