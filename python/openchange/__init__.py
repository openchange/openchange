#!/usr/bin/python

# OpenChange Python bindings
# Copyright (C) Jelmer Vernooij <jelmer@openchange.org> 2008
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

__docformat__ = 'restructuredText'

import os, sys

def find_samba_python_path():
    """Find the python path for the Samba python modules

    :return: Python path to Samba python modules, or None if 
        it was already in the path
    """
    try:
        import samba
        return None # No extra path necessary
    except ImportError:
        SAMBA_PREFIXES = ["/usr/local/samba", "/opt/samba"]
        PYTHON_NAMES = ["python2.3", "python2.4", "python2.5", "python2.6"]
        for samba_prefix in SAMBA_PREFIXES:
            for python_name in PYTHON_NAMES:
                path = os.path.join(samba_prefix, "lib", python_name, 
                                    "site-packages")
                if os.path.isdir(os.path.join(path, "samba")):
                    return path
        raise ImportError("Unable to find the Samba python path")
