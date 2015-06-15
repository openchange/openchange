#
# Copyright (C) 2014
#                          Javier Amor Garcia <jamor@zentyal.com>
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
This module provides the retrieval of the offline address book. 
For now only an empty address book is retrieved
"""
from pylons.decorators.rest import restrict
from pylons import response
from ocsmanager.lib.base import BaseController

class OabController(BaseController):
    """The constroller class for OAB requests."""

    def get_oab(self, **kwargs):
        # TODO
        response.headers["content-type"] = "application/xml"
        body = """<?xml version="1.0" encoding="UTF-8"?>
        <OAB>
        </OAB>"""
        return body

    def head_oab(self, **kwargs):
        # TODO
        pass
