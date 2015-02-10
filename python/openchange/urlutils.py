#!/usr/bin/python
# -*- coding: utf-8 -*-

# OpenChange provisioning
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2008-2009
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2009
# Copyright (C) Brad Hards <bradh@openchange.org> 2010
# Copyright (C) Enrique J. Hernández <ejhernandez@zentyal.com> 2015
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
Set of utils for provision/migration commands
"""
import os


def openchangedb_url(lp):
    return (lp.get("mapiproxy:openchangedb") or
            os.path.join(lp.get("private dir"), "openchange.ldb"))


def indexing_url(lp):
    return (lp.get("mapistore:indexing_backend")
            or os.path.join(lp.get("private dir"), "indexing.ldb"))


def named_properties_url(lp):
    named_props_backend = lp.get("mapistore:namedproperties")
    if named_props_backend == 'mysql':
        return "mysql://{user}{passwd}@{host}/{db}".format(
            user=lp.get("namedproperties:mysql_user"),
            passwd=":" + lp.get("namedproperties:mysql_pass") if lp.get("namedproperties:mysql_pass") else "",
            host=lp.get("namedproperties:mysql_host"),
            db=lp.get("namedproperties:mysql_db"))
    elif named_props_backend == 'ldb':
        return lp.get("namedproperties:ldb_data")
    else:
        return None
