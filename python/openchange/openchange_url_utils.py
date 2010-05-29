#!/usr/bin/python

# OpenChange provisioning
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2008-2009
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2009
# Copyright (C) Brad Hards <bradh@openchange.org> 2010
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

def openchangedb_url(lp):
    return os.path.join(lp.get("private dir"), "openchange.ldb")

def openchangedb_mapistore_dir(lp):
    return os.path.join(lp.get("private dir"), "mapistore")

def openchangedb_mapistore_url(lp, backend):
    if backend == "fsocpf":
	return "fsocpf://" + openchangedb_mapistore_dir(lp)
    if backend == "sqlite":
	return "sqlite://" + openchangedb_mapistore_dir(lp)
    print "#### unsupported mapistore backend type:" + backend
    print "#### using fsocpf instead"
    return "fsocpf://" + openchangedb_mapistore_dir(lp)

def openchangedb_mapistore_url_split(url):
    if url.startswith("fsocpf://"):
	return url.partition("fsocpf://")[1:]
    if url.startswith("sqlite://"):
	return url.partition("sqlite://")[1:]

def openchangedb_suffix_for_mapistore_url(url):
    if (url.startswith("fsocpf://")):
        return ""
    if (url.startswith("sqlite://")):
        return ".db"
    return ""
