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
                   json,
                   Response,
                   send_from_directory)
from flask.ctx import after_this_request
from handler.kissHandler import ApiHandler
from openchange import mapistore


app = Flask(__name__)
app.config.from_object('default_config')
app.config.from_envvar('API_SERVER_SETTINGS', silent=True)


@app.route('/', methods=['GET'])
@app.route('/index.html', methods=['GET'])
def index():
    """Default is to show documentation"""
    return send_from_directory(os.path.abspath('./www/'), 'index.html')


@app.route('/_static/<path:filename>', methods=['GET'])
def static_file(filename):
    """Server resources for documentation"""
    return send_from_directory('./www/_static', filename)


@app.route('/info/', methods=['GET'])
def module_info():
    """Get information for this backend"""
    handler = ApiHandler(user_id='any')
    ret_val = handler.info_get()

    def _contexts_info(folder):
        # fix contexts, they are relative atm
        return {
            'url': '/folders/%s/' % folder['id'],
            'role': folder['role'],
            'name': folder['PidTagDisplayName'],
            'main_folder': True,
            'system_idx': folder['system_idx']
        }
    contexts = [_contexts_info(ctx) for ctx in ret_val['contexts']]
    contexts.append({
        'url': '/folders/',
        'role': mapistore.ROLE_FALLBACK,
        'name': 'Fallback',
        'main_folder': True,
        'sytem_idx': str(-1)
    })
    ret_val['contexts'] = contexts
    handler.close_context()
    return jsonify(ret_val)


def _module_folders_dir_impl(parent_id):
    """List of folders that are children of parent_id"""
    handler = ApiHandler(user_id='any')
    ret_val = ''
    try:
        if parent_id is None:
            abort(422, "parent_id is required parameter")
        ret_val = handler.folders_dir(parent_id)
    except KeyError:
        abort(404)
    finally:
        handler.close_context()
    return ret_val


@app.route('/folders/<int:folder_id>/folders', methods=['HEAD'])
def module_folders_head_folders(folder_id):
    """Get number of folder's child"""
    handler = ApiHandler(user_id='any')
    ret_val = ''
    try:
        ret_val = handler.folders_get_folder(folder_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()

    @after_this_request
    def add_header_X_mapistore_rowcount(response):
        response.headers['X-mapistore-rowcount'] = ret_val['item_count']
        return response

    return jsonify()


@app.route('/folders/', methods=['GET'])
@app.route('/folders/<int:folder_id>/folders', methods=['GET'])
def module_folders_get_folders(folder_id=1):
    """List root level folders"""
    properties = request.args.get('properties')
    if properties is None:
        properties = None
    else:
        properties = set(properties.split(','))
    folders = _module_folders_dir_impl(folder_id)
    # filter only requested properties
    folders = [{k: v
                for (k, v) in f.items()
                if properties is None or k in properties}
               for f in folders]
    return Response(json.dumps(folders),  mimetype='application/json')


@app.route('/folders/', methods=['POST'])
def module_folders_create():
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    ret_val = {}
    try:
        if data is None:
            abort(422, "You must supply parent_id and name at least")
        parent_id = data.get('parent_id', None)
        if parent_id is None:
            abort(422, "parent_id is required parameter")
        folder_name = data.get('PidTagDisplayName')
        if folder_name is None:
            abort(422, "name is a required parameter")
        ret_val = handler.folders_create(parent_id, folder_name, data)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return jsonify(id=ret_val['id'])


@app.route('/folders/<int:folder_id>/', methods=['PUT'])
def module_folders_put(folder_id):
    """List root level folders"""
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    try:
        handler.folders_update(folder_id, data)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return "", 201


@app.route('/folders/<int:folder_id>/', methods=['HEAD'])
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
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return jsonify(ret_val)


@app.route('/folders/<int:folder_id>/', methods=['DELETE'])
def module_folders_delete(folder_id):
    """List root level folders"""
    handler = ApiHandler(user_id='any')
    try:
        handler.folders_delete(folder_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return "", 204


@app.route('/folders/<int:folder_id>/messages', methods=['HEAD'])
def module_folders_head_messages(folder_id):
    """Get number of messages in a folder"""
    handler = ApiHandler(user_id='any')
    messages = ''
    try:
        messages = handler.folders_get_messages(folder_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()

    @after_this_request
    def add_header_X_mapistore_rowcount(response):
        response.headers['X-mapistore-rowcount'] = len(messages)
        return response

    return jsonify()


@app.route('/folders/<int:folder_id>/messages', methods=['GET'])
def module_folders_get_messages(folder_id):
    """List of messages within specified folder"""
    properties = request.args.get('properties')
    if properties is None:
        properties = None
    else:
        properties = set(properties.split(','))
    handler = ApiHandler(user_id='any')
    messages = ''
    try:
        messages = handler.folders_get_messages(folder_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    # filter only requested properties
    messages = [{k: v
                 for (k, v) in msg.items()
                 if properties is None or k in properties}
                for msg in messages]
    return Response(json.dumps(messages),  mimetype='application/json')


###############################################################################
# Common message implementation
###############################################################################

def _message_create(handler, collection, data):
    msg = {}
    try:
        if data is None:
            abort(422, "You must supply parent_id and PidTagSubject at least")
        parent_id = data.get('parent_id', None)
        if parent_id is None:
            abort(422, "parent_id is required parameter")
        subject = data.get('PidTagSubject')
        if subject is None:
            abort(422, "PidTagSubject is a required parameter")
        msg = handler.messages_create(collection, data)
    except KeyError, ke:
        abort(404, ke.message)
    return msg


@app.route('/mails/<int:msg_id>/', methods=['GET'])
@app.route('/calendars/<int:msg_id>/', methods=['GET'])
@app.route('/tasks/<int:msg_id>/', methods=['GET'])
@app.route('/contacts/<int:msg_id>/', methods=['GET'])
@app.route('/notes/<int:msg_id>/', methods=['GET'])
def module_mail_get(msg_id=0):
    """Retrieve all the properties of the calendar entry identified by id"""
    handler = ApiHandler(user_id='any')
    ret_val = ''
    try:
        ret_val = handler.messages_get(msg_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return jsonify(ret_val)


@app.route('/mails/<int:msg_id>/', methods=['PUT'])
@app.route('/calendars/<int:msg_id>/', methods=['PUT'])
@app.route('/tasks/<int:msg_id>/', methods=['PUT'])
@app.route('/contacts/<int:msg_id>/', methods=['PUT'])
@app.route('/notes/<int:msg_id>/', methods=['PUT'])
def module_mail_put(msg_id):
    """Update existing message properties"""
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    try:
        handler.messages_update(msg_id, data)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return "", 201


@app.route('/mails/<int:msg_id>/', methods=['DELETE'])
@app.route('/calendars/<int:msg_id>/', methods=['DELETE'])
@app.route('/tasks/<int:msg_id>/', methods=['DELETE'])
@app.route('/contacts/<int:msg_id>/', methods=['DELETE'])
@app.route('/notes/<int:msg_id>/', methods=['DELETE'])
def module_mail_delete(msg_id):
    """Delete message with msg_id"""
    handler = ApiHandler(user_id='any')
    try:
        handler.messages_delete(msg_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return "", 204


@app.route('/mails/<int:msg_id>/attachments', methods=['HEAD'])
@app.route('/calendars/<int:msg_id>/attachments', methods=['HEAD'])
@app.route('/tasks/<int:msg_id>/attachments', methods=['HEAD'])
@app.route('/contacts/<int:msg_id>/attachments', methods=['HEAD'])
@app.route('/notes/<int:msg_id>/attachments', methods=['HEAD'])
def module_messages_head_attachments(msg_id):
    """Get number of attachments in a message"""
    handler = ApiHandler(user_id='any')
    attachments = ''
    try:
        attachments = handler.messages_get_attachments(msg_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()

    @after_this_request
    def add_header_X_mapistore_rowcount(response):
        response.headers['X-mapistore-rowcount'] = len(attachments)
        return response

    return jsonify()


@app.route('/mails/<int:msg_id>/attachments', methods=['GET'])
@app.route('/calendars/<int:msg_id>/attachments', methods=['GET'])
@app.route('/tasks/<int:msg_id>/attachments', methods=['GET'])
@app.route('/contacts/<int:msg_id>/attachments', methods=['GET'])
@app.route('/notes/<int:msg_id>/attachments', methods=['GET'])
def module_messages_get_attachments(msg_id):
    """List of attachments within specified message"""
    properties = request.args.get('properties')
    if properties is None:
        properties = None
    else:
        properties = set(properties.split(','))
    handler = ApiHandler(user_id='any')
    attachments = ''
    try:
        attachments = handler.messages_get_attachments(msg_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    # filter only requested properties
    attachments = [{k: v
                 for (k, v) in att.items()
                 if properties is None or k in properties}
                 for att in attachments]
    return Response(json.dumps(attachments),  mimetype='application/json')


###############################################################################
# Mail service
###############################################################################

@app.route('/mails/submit/', methods=['POST'])
def module_mail_submit():
    """ Send E-mail to its recipients. The json data contains the message
    properties, including the recipients data, in the 'msg' field, and the
    processing flag in the 'serv_spool' field. """
    data = request.get_json()
    if data is None:
        abort(422, "You must supply msg at least")
    # Read message data
    msg = data.get('msg', None)
    if msg is None:
        abort(422, "msg is a required parameter")
    if 'recipients' not in msg:
        abort(422, "recipients is a required field in msg")
    # Read processing flag (No processing required/by server/by client spooler
    serv_spool = data['serv_spool']
    return "", 200


@app.route('/mails/', methods=['POST'])
def module_mail_create():
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    try:
        msg = _message_create(handler, 'mails', data)
    finally:
        handler.close_context()
    return jsonify(id=msg['id'])


###############################################################################
# Calendar service
###############################################################################

@app.route('/calendars/', methods=['POST'])
def module_calendars_create():
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    try:
        msg = _message_create(handler, 'calendars', data)
    finally:
        handler.close_context()
    return jsonify(id=msg['id'])


###############################################################################
# Tasks service
###############################################################################

@app.route('/tasks/', methods=['POST'])
def module_tasks_create():
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    try:
        msg = _message_create(handler, 'tasks', data)
    finally:
        handler.close_context()
    return jsonify(id=msg['id'])


###############################################################################
# Contacts service
###############################################################################

@app.route('/contacts/', methods=['POST'])
def module_contacts_create():
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    try:
        msg = _message_create(handler, 'contacts', data)
    finally:
        handler.close_context()
    return jsonify(id=msg['id'])


###############################################################################
# Notes service
###############################################################################

@app.route('/notes/', methods=['POST'])
def module_notes_create():
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    try:
        msg = _message_create(handler, 'notes', data)
    finally:
        handler.close_context()
    return jsonify(id=msg['id'])


###############################################################################
# Attachment
###############################################################################

@app.route('/attachments/', methods=['POST'])
def module_attachments_create():
    data = request.get_json()
    if data is None:
        abort(422, "You must supply parent_id at least")
    parent_id = data.get('parent_id', None)
    if parent_id is None:
        abort(422, "parent_id is a required parameter")
    handler = ApiHandler(user_id='any')
    att = {}
    try:
        att = handler.attachments_create(data)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return jsonify(id=att['id'])


@app.route('/attachments/<int:att_id>/', methods=['GET'])
def module_attachments_get(att_id):
    """Fetch single attachment by its ID"""
    handler = ApiHandler(user_id='any')
    ret_val = ''
    try:
        ret_val = handler.attachments_get(att_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return jsonify(ret_val)


@app.route('/attachments/<int:att_id>/', methods=['PUT'])
def module_attachments_put(att_id):
    data = request.get_json()
    handler = ApiHandler(user_id='any')
    try:
        handler.attachments_update(att_id, data)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return "", 201


@app.route('/attachments/<int:att_id>/', methods=['HEAD'])
def module_attachments_head(att_id):
    """List root level attachments"""
    handler = ApiHandler(user_id='any')
    ret_val = handler.attachments_id_exists(att_id)
    handler.close_context()
    if not ret_val:
        abort(404)
    return jsonify()


@app.route('/attachments/<int:att_id>/', methods=['DELETE'])
def module_attachment_delete(att_id):
    """Delete attachment with att_id"""
    handler = ApiHandler(user_id='any')
    try:
        handler.attachments_delete(att_id)
    except KeyError, ke:
        abort(404, ke.message)
    finally:
        handler.close_context()
    return "", 204

###############################################################################
# Main loop
###############################################################################

if __name__ == '__main__':
    app.run(host=app.config.get('HOST', None), port=app.config.get('PORT', None))
