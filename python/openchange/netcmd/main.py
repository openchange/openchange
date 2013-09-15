# OpenChange implementation
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2013
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

"""The main openchange-tool command implementation."""

from openchange.netcmd import SuperCommand
from openchange.netcmd.openchangedb import cmd_openchangedb

class cmd_openchangetool(SuperCommand):
    """Main OpenChange administration tool."""

    subcommands = {}
    subcommands["openchangedb"] = cmd_openchangedb()

