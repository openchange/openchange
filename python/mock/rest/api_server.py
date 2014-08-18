import os
from flask import Flask, send_from_directory
from flask import request
from pprint import pprint

__author__ = 'kamenim'

app = Flask(__name__)


@app.route('/', methods=['GET'])
@app.route('/index.html', methods=['GET'])
def index():
    """Default is to show documentation"""
    return send_from_directory(os.path.abspath('./doc/'), 'index.html')


@app.route('/_static/<path:filename>', methods=['GET'])
def static_file(filename):
    """Server resources for documentation"""
    return send_from_directory('./doc/_static', filename)


if __name__ == '__main__':
    app.debug = True
    app.run()
