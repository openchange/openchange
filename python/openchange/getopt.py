# OpenChange-specific bits for optparse
#
# Copyright Jelmer Vernooij <jelmer@openchange.org> 2013.
# Copyright Julien Kerihuel <j.kerihuel@openchange.org> 2013
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

"""Support for parsing OpenChange-related command-line options."""

__docformat__ = "restructedText"

import optparse
import os
import sys

class OpenChangeOptions(optparse.OptionGroup):
    """General OpenChange-related command line options."""

    def __init__(self, parser):
        from samba.param import LoadParm
        optparse.OptionGroup.__init__(self, parser, 
                                      "OpenChange Common Options")
        self.add_option("-s", "--configfile", action="callback",
                        type=str, metavar="FILE", 
                        help="Samba configuration file",
                        callback=self._load_configfile)
        self.add_option("-d", "--debuglevel", action="callback",
                        type=int, metavar="DEBUGLEVEL", help="debug level",
                        callback=self._set_debuglevel)

        self._configfile = None
        self._lp = LoadParm()

    def get_loadparm_path(self):
        """Return path to the smb.conf file specified on the command line."""
        return self._configfile

    def _load_configfile(self, option, opt_str, arg, parser):
        self._configfile = arg

    def _set_debuglevel(self, option, opt_str, arg, parser):
        if arg < 0:
            raise optparse.OptionValueError("invalid %s option value: %s" %
                                            (opt_str, arg))
        self._lp.set('debug level', str(arg))

    def get_loadparm(self):
        """Return loadparm object with data specified on the command line."""
        if self._configfile is not None:
            self._lp.load(self._configfile)
        elif os.getenv("SMB_CONF_PATH") is not None:
            self._lp.load(os.getenv("SMB_CONF_PATH"))
        else:
            self._lp.load_default()
        return self._lp
