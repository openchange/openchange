# -*- coding: utf-8 -*-
#
# Copyright (C) 2014  Kamen Mazdrashki <kamenim@openchange.org>
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
Unit testing for Openchange REST API server
"""

import unittest
import requests
import json

# Default URL for Mock server running on localhost
_SERVER_URL = 'http://localhost:5000'

class MockApiBaseTestCase(unittest.TestCase):
    """Provide helpers for the rest of out tests"""

    def head_req(self, path):
        r = requests.head(self._make_url(path))
        return r.status_code, r.text, r.headers

    def get_req(self, path):
        r = requests.get(self._make_url(path))
        return r.status_code, r.text, r.headers

    def post_req(self, path, data):
        r = requests.post(self._make_url(path),
                          headers=self._headers(),
                          data=json.dumps(data))
        return r.status_code, r.text, r.headers

    def put_req(self, path, data):
        r = requests.put(self._make_url(path),
                          headers=self._headers(),
                          data=json.dumps(data))
        return r.status_code, r.text, r.headers

    def delete_req(self, path):
        r = requests.delete(self._make_url(path))
        return r.status_code, r.text, r.headers

    @staticmethod
    def _make_url(path, querystring=None):
        if (querystring is not None) and (len(querystring) > 0):
            url = "%s%s?%s" % (_SERVER_URL, path, querystring)
        else:
            url = "%s%s" % (_SERVER_URL, path)
        return url

    @staticmethod
    def _headers():
        return {'Content-type': 'application/json', 'Accept': 'application/json'}

    @staticmethod
    def _to_json_ret(text):
        try:
            return json.loads(text)
        except (TypeError, ValueError) as e:
            return None


if __name__ == '__main__':
    unittest.main()
