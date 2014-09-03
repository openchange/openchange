# -*- coding: utf-8 -*-
#
# Copyright (C) 2014  Enrique J. Hernández <ejhernandez@zentyal.com>
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
Wrapper over LDB connection to reconnect when the connection has failed
"""
import logging


from ldb import LdbError
from samba.samdb import SamDB


log = logging.getLogger(__name__)


class SamDBWrapper(object):
    """Class to wrap the connection to reconnect when the connection is lost"""
    def __init__(self):
        self.samdb_ldb = None

    def set_samdb(self, *args, **kwargs):
        """Create a new connection to LDB.

        The arguments are blindly passed to SamDB object
        """
        self.samdb_args = args
        self.samdb_kwargs = kwargs
        self.samdb_ldb = SamDB(*args, **kwargs)
        return self.samdb_ldb

    def get_samdb(self):
        """Get the connection from LDB.

        If it fails because of a reconnection, then it tries to reconnect using
        the parameters set by set_ldb method.
        """
        if self.samdb_ldb:
            try:
                self.samdb_ldb.get_serverName()
            except LdbError as [num, msg]:
                if num == 1:
                    log.warn('Trying to reconnect after %s' % msg)
                    self.samdb_ldb = SamDB(*self.samdb_args,
                                           **self.samdb_kwargs)
                else:
                    # Re-raise the original exception
                    raise
        return self.samdb_ldb
