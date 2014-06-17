#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) 2014 Enrique J. Hernández
# Author: Enrique J. Hernández <ejhernandez@zentyal.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""
SQLite CrashDatabase implementation.

It is useful to have a complete implementation of crash management in a single host.
"""
import apport.crashdb
from apport.report import Report
from io import BytesIO
import os.path
import sqlite3
import sys


if sys.version_info.major == 2:
    from urllib import urlopen
    from urlparse import urlparse, urljoin
    from httplib import HTTP as HTTPConnection
    from httplib import HTTPException
    _python2 = True
else:
    from http.client import HTTPConnection, HTTPException
    from urllib.request import urlopen
    from urllib.parse import urlparse, urljoin
    _python2 = False


class CrashDatabase(apport.crashdb.CrashDatabase):
    """
    Simple implementation of crash database interface which keeps everything
    in a simple SQLite file.
    """
    def __init__(self, auth_file, options):
        """
        Initialize the crash database connection

        Options are:

        * dbfile: the file to store the database. If you supply None, then
                  it will create a new file at ~/crashdb.sqlite.
        """
        apport.crashdb.CrashDatabase.__init__(self, auth_file, options)
        self.dbfile = options.get('dbfile', os.path.expanduser('~/crashdb.sqlite'))
        self.base_url = options.get('crashes_base_url', None)
        if self.base_url is not None and urlparse(self.base_url).scheme not in ('file', 'http'):
            raise ValueError('crashes_base_url option has not a valid scheme: %s' % self.base_url)

        init = not os.path.exists(self.dbfile) or self.dbfile == ':memory:' or \
            os.path.getsize(self.dbfile) == 0
        self.db = sqlite3.connect(self.dbfile, timeout=7200)
        self.format_version = 1
        if init:
            self.__create_db()
            self.last_crash_id = 0
        else:
            with self.db:
                cur = self.db.cursor()
                cur.execute("""SELECT MAX(crash_id) FROM crashes""")
                row = cur.fetchone()
                if row and row[0] is not None:
                    self.last_crash_id = row[0]
                else:
                    self.last_crash_id = 0

    def upload(self, report, progress_callback=None):
        """
        Upload the report to the database.

        No progress callback is implemented yet.

        The report is not uploaded but the pointer indicated by _URL attribute in the report.

        :raise ValueError: if the report does not have _URL attribute or crashes_base_url option is not set
        """
        cur = self.db.cursor()
        app, version = report['Package'].split(' ', 1)

        if '_URL' not in report and self.base_url is None:
            raise ValueError('This backend requires _URL attribute to upload a crash report or crashes_base_url configuration option')

        if '_URL' not in report:
            report['_URL'] = urljoin(self.base_url, self._report_file_name(report))

        stacktrace = None
        if 'Stacktrace' in report:
            stacktrace = sqlite3.Binary(bytes(report['Stacktrace']))

        # Store it in the destination URL
        self._upload_report_file(report)

        cur.execute("""INSERT INTO crashes
                       (crash_id, crash_url, title, app, version, sym_stacktrace, distro_release)
                       VALUES (?, ?, ?, ?, ?, ?, ?)""",
                    (self.last_crash_id + 1, report['_URL'], report.standard_title(),
                     app, version, stacktrace, report.get('DistroRelease', None)))
        self.db.commit()
        self.last_crash_id = cur.lastrowid
        return self.last_crash_id

    def download(self, id):
        """
        Download the report from the database

        :raises TypeError: if the report does not exist
        """
        cur = self.db.cursor()
        cur.execute("""SELECT crash_url
                       FROM crashes
                       WHERE crash_id = ?""", [id])
        url = cur.fetchone()[0]

        fh = urlopen(url)

        # Actually read the content
        buf = BytesIO(bytes(fh.read()))

        report = Report()
        report.load(buf)
        return report

    def get_comment_url(self, report, handle):
        """
        Not implemented. Always returned None.
        """
        return None

    def get_id_url(self, report, id):
        """
        Return URL for a given report ID.

        The report is passed in case building the URL needs additional
        information from it, such as the SourcePackage name.

        Return None if URL is not available or cannot be determined.Return URL for a given report ID.

        The report is passed in case building the URL needs additional
        information from it, such as the SourcePackage name.

        Return None if URL is not available or cannot be determined.

        Dummy implementation: It returns the id and the standard title.
        """
        try:
            return "#%d: %s" % (id, report.standard_title())
        except:
            return "#%d" % id

    def update(self, id, report, comment, change_description=False,
               attachment_comment=None, key_filter=None):
        """
        Update the given report ID with the data from this report.

        Attachments are unsupported by this moment.

        See apport.crashdb.CrashDatabase.update for more information.

        :raise ValueError: if the report does not have _URL attribute
        """
        db_report = self.download(id)
        # Insert the comment
        with self.db:
            cur = self.db.cursor()
            if comment:
                if change_description:
                    cur.execute("""UPDATE crashes SET description = ? WHERE crash_id = ?""",
                                (comment, id))
                else:
                    cur.execute("""INSERT INTO crash_comments(crash_id, comment) VALUES (?, ?)""",
                                (id, comment))

            if key_filter:
                for k in key_filter:
                    if k in report:
                        db_report[k] = report[k]
            else:
                db_report.update(report)

            # Do what upload does
            if '_URL' not in report and self.base_url is None:
                raise ValueError('This backend requires _URL attribute to update a crash report')

            self._update_report_file(db_report)

            stacktrace = None
            if 'Stacktrace' in db_report:
                stacktrace = sqlite3.Binary(bytes(db_report['Stacktrace']))

            cur.execute("""UPDATE crashes SET crash_url = ?, title = ?, sym_stacktrace = ?, distro_release = ?
                           WHERE crash_id = ?""",
                        (db_report['_URL'], db_report.standard_title(), stacktrace,
                         db_report.get('DistroRelease', None), id))

    def set_credentials(self, username, password):
        """
        Not Implemented
        """
        pass

    def get_distro_release(self, id):
        """
        Get 'DistroRelease: <release>' from the report ID.
        """
        distro_release = None
        with self.db:
            cur = self.db.cursor()
            cur.execute("""SELECT distro_release
                           FROM crashes WHERE crash_id = ?""", [id])
            distro_release = cur.fetchone()[0]
        return distro_release

    def get_unretraced(self):
        """
        Return set of crash IDs which have not been retraced yet.

        This should only include crashes which match the current host
        architecture.

        :return: the list of crash untraced identifiers
        :rtype: list
        """
        ids = []
        with self.db:
            cur = self.db.cursor()
            cur.execute("""SELECT crash_id FROM crashes
                           WHERE sym_stacktrace IS NULL""")
            rows = cur.fetchall()
            ids = [row[0] for row in rows]
        return ids

    def get_dup_unchecked(self):
        """
        Unimplemented right now
        """
        return []

    def mark_retraced(self, id):
        """
        Mark crash id as retraced.
        """
        # Do nothing as self.update method is in charge of it
        pass

    def _mark_dup_checked(self, id, report):
        """
        Unimplemented right now
        """
        pass

    def duplicate_of(self, id):
        """
        Return master ID for a duplicate bug.

        If the bug is not a duplicate, return None.
        """
        cur = self.db.cursor()
        cur.execute('SELECT master_id FROM crashes WHERE crash_id = ?',
                    [id])
        row = cur.fetchone()
        if row:
            return row[0]
        return None

    def close_duplicate(self, report, id, master):
        """
        Mark a crash id as duplicate of given master ID.

        If master is None, id gets un-duplicated.
        """
        with self.db:
            cur = self.db.cursor()
            if master is None:
                cur.execute("""UPDATE crashes SET state = NULL, master_id = NULL
                               WHERE crash_id = ?""",
                            [id])
            else:
                cur.execute("""UPDATE crashes SET state = 'duplicated', master_id = ?
                               WHERE crash_id = ?""", [master, id])

    def get_fixed_version(self, id):
        """
        Return the package version that fixes a given crash.

        Return None if the crash is not yet fixed, or an empty string if the
        crash is fixed, but it cannot be determined by which version. Return
        'invalid' if the crash report got invalidated, such as closed a
        duplicate or rejected.

        This function should make sure that the returned result is correct. If
        there are any errors with connecting to the crash database, it should
        raise an exception (preferably IOError).
        """
        fixed_version = None
        with self.db:
            cur = self.db.cursor()
            cur.execute("""SELECT fixed_version
                           FROM crashes WHERE crash_id = ?""", [id])
            fixed_version = cur.fetchone()[0]
        return fixed_version

    def __create_db(self):
        """
        Create the DB
        """
        with self.db:
            cur = self.db.cursor()
            cur.execute('CREATE TABLE version (format INTEGER NOT NULL)')
            cur.execute('INSERT INTO version VALUES (?)', [self.format_version])

            # sym_stacktrace is a blob due to a bug in printing int array within
            # a struct in gdb
            cur.execute("""CREATE TABLE crashes (
            crash_id INTEGER NOT NULL,
            crash_url VARCHAR(1024) NOT NULL,
            title VARCHAR(512),
            app VARCHAR(64) NOT NULL,
            version VARCHAR(64),
            distro_release VARCHAR(64),
            description TEXT,
            sym_stacktrace BLOB,
            fixed_version VARCHAR(64),
            state VARCHAR(64),
            master_id INTEGER,
            CONSTRAINT master_fk FOREIGN KEY(master_id) REFERENCES crashes(crash_id),
            CONSTRAINT crashes_pk PRIMARY KEY(crash_id))""")

            cur.execute('CREATE INDEX master_index ON crashes(master_id)')

            cur.execute("""CREATE TABLE crash_comments (
            crash_id INTEGER NOT_NULL,
            comment TEXT)""")

    def _upload_report_file(self, report):
        '''Upload the report file based on scheme'''
        url = urlparse(report['_URL'])
        if url.scheme == 'file':
            # Write the file directly
            with open(url.path, 'wb') as f:
                report.write(f)
        elif url.scheme == 'http':
            blob = BytesIO()
            report.write(blob)
            blob.flush()
            blob.seek(0)

            if ':' in url.netloc:
                host, port = url.netloc.split(':')
            else:
                host, port = url.netloc, 80
            file_name = url.path.rsplit('/', 1)[-1]
            post_multipart(host, port, url.path, [], [('upload', file_name, blob.getvalue())])
        else:
            raise ValueError('Unhandled scheme: %s' % url.scheme)

    def _update_report_file(self, report):
        '''Update the report file based on scheme'''
        self._upload_report_file(report)

    def _report_file_name(self, report):
        sep = '_'
        exe_path = report['ExecutablePath'].replace(os.path.sep, sep)
        return str(self.last_crash_id + 1) + sep + exe_path


def post_multipart(host, port, selector, fields, files):
    content_type, body = encode_multipart_formdata(fields, files)
    h = HTTPConnection(host, port)
    h.putrequest('POST', selector)
    h.putheader('content-type', content_type)
    h.putheader('content-length', str(len(body)))
    h.endheaders()
    if _python2:
        h.send(body)
    else:
        h.send(body.encode('utf-8'))
    if _python2:
        errcode, errmsg, headers = h.getreply()
        if errcode != 200:
            raise HTTPException("%s: %s" % (errcode, errmsg))
        return h.file.read()
    else:
        res = h.getresponse()
        if res.status != 200:
            raise HTTPException("%s: %s" % (res.status, res.reason))
        return res.read()


def encode_multipart_formdata(fields, files):
    LIMIT = '----------lImIt_of_THE_fIle_eW_$'
    CRLF = '\r\n'
    L = []
    for (key, value) in fields:
        L.append('--' + LIMIT)
        L.append('Content-Disposition: form-data; name="%s"' % key)
        L.append('')
        L.append(value)
    for (key, filename, value) in files:
        L.append('--' + LIMIT)
        L.append('Content-Disposition: form-data; name="%s"; filename="%s"' % (key, filename))
        L.append('Content-Type: application/octet-stream')
        L.append('')
        if _python2:
            L.append(value)
        else:
            L.append(value.decode())
    L.append('--' + LIMIT + '--')
    L.append('')
    body = CRLF.join(L)
    content_type = 'multipart/form-data; boundary=%s' % LIMIT
    return content_type, body
