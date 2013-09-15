# openchangedb management
#
# Copyright Julien Kerihuel <j.kerihuel@openchange.org> 2013
# Copyright Jelmer Vernooij <jelmer@openchange.org> 2013.
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

import os
import sys
from openchange import getopt as options
from openchange import mailbox
from openchange.urlutils import openchangedb_url

DEFAULTSITE = "Default-First-Site-Name"
FIRST_ORGANIZATION = "First Organization"
FIRST_ORGANIZATION_UNIT = "First Administrative Group"

from openchange.netcmd import (
    Command,
    CommandError,
    SuperCommand,
    Option
    )

class cmd_openchangedb_provision(Command):
    """Provision openchange database."""

    def __init__(self, errf=sys.stderr):
        self.errf = errf
        self.rootdn = None
        self.domaindn = None
        self.configdn = None
        self.schemadn = None
        self.dnsdomain = None
        self.netbiosname = None
        self.domain = None
        self.hostname = None
        self.serverrole = None
        self.firstorg = None
        self.firstou = None
        self.firstorgdn = None
        # OpenChange dispatcher database specific
        self.ocfirstorgdn = None
        self.ocserverdn = None
    
    synopsys = "%prog [options]"

    takes_optiongroups = {
        "openchangeopts": options.OpenChangeOptions,
        "versionopts": options.VersionOptions,
        }

    takes_options = [
        Option("--smbconf", type="string", metavar="SMBCONF",
               help="Set custom smb.conf configuration file"),
        Option("--first-organization", type="string", metavar="First Organization",
               help="set organization"),
        Option("--first-organization-unit", type="string", metavar="First Organization Unit",
               help="set first organization unit"),
        ]
    take_args = []

    def guess_names_from_smbconf(self, lp, firstorg=None, firstou=None):
        """Guess configuration settings to use from smb.conf.
    
        :param lp: Loadparm context.
        :param firstorg: First Organization
        :param firstou: First Organization Unit
        """

        netbiosname = lp.get("netbios name")
        hostname = netbiosname.lower()

        dnsdomain = lp.get("realm")
        dnsdomain = dnsdomain.lower()

        serverrole = lp.get("server role")
        # Note: "server role" can have many forms, even for the same function:
        # "member server", "domain controller", "active directory domain
        # controller"...
        if "domain controller" in serverrole or serverrole == "member server":
            domain = lp.get("workgroup")
            domaindn = "DC=" + dnsdomain.replace(".", ",DC=")
        else:
            domain = netbiosname
            domaindn = "CN=" + netbiosname
            
        rootdn = domaindn
        configdn = "CN=Configuration," + rootdn
        schemadn = "CN=Schema," + configdn
        sitename = DEFAULTSITE

        self.serverrole = serverrole
        self.rootdn = rootdn
        self.domaindn = domaindn
        self.configdn = configdn
        self.schemadn = schemadn
        self.dnsdomain = dnsdomain
        self.domain = domain
        self.netbiosname = netbiosname
        self.hostname = hostname
        self.sitename = sitename

        if firstorg is None:
            firstorg = FIRST_ORGANIZATION

        if firstou is None:
            firstou = FIRST_ORGANIZATION_UNIT

        self.firstorg = firstorg
        self.firstou = firstou
        self.firstorgdn = "CN=%s,CN=Microsoft Exchange,CN=Services,%s" % (firstorg, configdn)
        self.serverdn = "CN=%s,CN=Servers,CN=%s,CN=Sites,%s" % (netbiosname, sitename, configdn)

        # OpenChange dispatcher DB names
        self.ocserverdn = "CN=%s,%s" % (self.netbiosname, self.domaindn)
        self.ocfirstorg = firstorg
        self.ocfirstorgdn = "CN=%s,CN=%s,%s" % (firstou, self.ocfirstorg, self.ocserverdn)

    def run(self, openchangeopts=None,
            versionopts=None,
            first_organization=None, 
            first_organization_unit=None,
            smbconf=None):

        lp = openchangeopts.get_loadparm()
        if smbconf is None:
            smbconf = lp.configfile
            if smbconf is None:
                raise CommandError("smb.conf not found!")

        self.guess_names_from_smbconf(lp, first_organization,
                                      first_organization_unit)

        print "OpenChange Database Provisioning"
        try:
            openchange_ldb = mailbox.OpenChangeDB(openchangedb_url(lp))
            openchange_ldb.setup()
            print "* Provisioning root DSE"
            openchange_ldb.add_rootDSE(self.ocserverdn, self.firstorg, self.firstou)

            # Add a server object
            # It is responsible for holding the GlobalCount identifier (48 bytes)
            # and the Replica identifier
            print "* Provisioning server object"
            openchange_ldb.add_server(self.ocserverdn, self.netbiosname, self.firstorg, self.firstou)

            print "* Provisioning public folders"
            openchange_ldb.add_public_folders(self)
            print "OK."
        except Exception, e:
            raise CommandError(openchangedb_url(lp), e)

        return

class cmd_openchangedb(SuperCommand):
    """OpenChange Database management."""

    subcommands = {}
    subcommands["provision"] = cmd_openchangedb_provision()
