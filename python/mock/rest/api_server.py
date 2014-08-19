# -*- coding: utf-8 -*-
#
# Copyright (C) 2014  Kamen Mazdrashki <kmazdrashki@zentyal.com>
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
Simple Mock server for Openchange REST API implementation
"""
import os
from flask import (Flask,
                   request,
                   abort,
                   jsonify,
                   send_from_directory)
from handler.kissHanlder import ApiHandler

app = Flask(__name__)
app.config.from_object(__name__)
# app.config.from_pyfile()


@app.route('/', methods=['GET'])
@app.route('/index.html', methods=['GET'])
def index():
    """Default is to show documentation"""
    return send_from_directory(os.path.abspath('./doc/'), 'index.html')


@app.route('/_static/<path:filename>', methods=['GET'])
def static_file(filename):
    """Server resources for documentation"""
    return send_from_directory('./doc/_static', filename)


@app.route('/info/', methods=['GET'])
def module_info():
    """Get information for this backend"""
    handler = ApiHandler(user_id='any')
    ret_val = handler.info_get()
    handler.close_context()
    return jsonify(ret_val)


def _module_folders_dir_impl(parent_id):
    """List of folders that are children of parent_id"""
    handler = ApiHandler(user_id='any')
    ret_val = ''
    try:
        ret_val = handler.folders_dir(parent_id)
    except KeyError:
        abort(404)
    finally:
        handler.close_context()
    return ret_val


@app.route('/folders/', methods=['GET'])
@app.route('/folders/<int:folder_id>/folders/', methods=['GET'])
def module_folders_get_folders(folder_id=0):
    """List root level folders"""
    return jsonify(items=_module_folders_dir_impl(folder_id))


@app.route('/folders/', methods=['POST'])
def module_folders_create():
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    ret_val = ''
    try:
        ret_val = handler.folders_create(data.get('parent_id'), data.get('name'), data.get('comment'))
    except KeyError:
        abort(404)
    finally:
        handler.close_context()
    return jsonify(ret_val)


@app.route('/folders/<int:folder_id>', methods=['HEAD'])
def module_folders_head(folder_id):
    """List root level folders"""
    handler = ApiHandler(user_id='any')
    ret_val = handler.folders_id_exists(folder_id)
    handler.close_context()
    if not ret_val:
        abort(404)
    return jsonify()


@app.route('/folders/<int:folder_id>/', methods=['GET'])
def module_folders_get(folder_id):
    """Fetch single folder by its ID"""
    handler = ApiHandler(user_id='any')
    ret_val = ''
    try:
        ret_val = handler.folders_get_folder(folder_id)
    except KeyError:
        abort(404)
    finally:
        handler.close_context()
    return jsonify(ret_val)


if __name__ == '__main__':
    app.debug = True
    app.run()
