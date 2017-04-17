#!/usr/bin/env python2.7
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
        * crashes_base_url: the crashes will be stored in this URL.
        """
        apport.crashdb.CrashDatabase.__init__(self, auth_file, options)
        self.dbfile = options.get('dbfile', os.path.expanduser('~/crashdb.sqlite'))
        self.base_url = options.get('crashes_base_url', None)
        if self.base_url is not None and urlparse(self.base_url).scheme not in ('file', 'http'):
            raise ValueError('crashes_base_url option has not a valid scheme: %s' % self.base_url)
        if self.base_url is not None and self.base_url[-1] != '/':
            self.base_url += '/'

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

    def upload(self, report, suggested_file_name=None, progress_callback=None):
        """
        Upload the report to the database.

        No progress callback is implemented yet.

        The report is not uploaded but the pointer indicated by _URL attribute in the report.

        If we receive _OrigURL attribute in the report, we store it as well in the DB.

        :param str suggested_file_name: the suggested file name where to store the report
        :raise ValueError: if the report does not have _URL attribute or crashes_base_url option is not set.
        """
        cur = self.db.cursor()
        app, version = report['Package'].split(' ', 1)

        if '_URL' not in report and self.base_url is None:
            raise ValueError('This backend requires _URL attribute to upload a crash report '
                             'or crashes_base_url configuration option')

        if '_URL' not in report:
            report['_URL'] = urljoin(self.base_url, self._report_file_name(report, suggested_file_name))

        stacktrace = None
        if 'Stacktrace' in report:
            if _python2 and isinstance(report['Stacktrace'], unicode):
                stacktrace = sqlite3.Binary(report['Stacktrace'].encode('utf-8'))
            else:
                stacktrace = sqlite3.Binary(bytes(report['Stacktrace']))

        # Store it in the destination URL
        self._upload_report_file(report)

        cur.execute("""INSERT INTO crashes
                       (crash_id, crash_url, title, app, version, sym_stacktrace, distro_release, orig_crash_url)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
                    (self.last_crash_id + 1, report['_URL'], report.standard_title(),
                     app, version, stacktrace, report.get('DistroRelease', None),
                     report.get('_OrigURL', None)))
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
                if _python2 and isinstance(db_report['Stacktrace'], unicode):
                    stacktrace = sqlite3.Binary(db_report['Stacktrace'].encode('utf-8'))
                else:
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
                           WHERE sym_stacktrace IS NULL
                           UNION
                           SELECT crash_id FROM crashes
                           WHERE sym_stacktrace LIKE '%No symbol table info available.%'""")
            rows = cur.fetchall()
            ids = [row[0] for row in rows]
        return ids

    def get_unfixed(self):
        """
        Return an ID set of all crashes which are not yet fixed.

        The list must not contain bugs which were rejected or duplicate.

        This function should make sure that the returned list is correct. If
        there are any errors with connecting to the crash database, it should
        raise an exception (preferably IOError).
        """
        ids = set()
        with self.db:
            cur = self.db.cursor()
            cur.execute("""SELECT crash_id FROM crashes
                           WHERE (state IS NULL OR state <> 'duplicated')""")
            for row in cur.fetchall():
                ids.add(row[0])
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

    """
    This set of methods are exclusive for OpenChange mining tool (oc-crash-digger)

    * Set/Get application components from a crash
    * Set/Get client side duplicates (URLs)
    * Management of tracker URL for a crash
    """
    def set_app_components(self, id, components):
        """
        Set the component for a crash

        :param list components: the crash app components
        """
        with self.db:
            cur = self.db.cursor()
            for component in components:
                cur.execute("""SELECT COUNT(*)
                               FROM crash_app_components
                               WHERE crash_id = ? AND component = ?""",
                            [id, component])
                comp_num = cur.fetchone()[0]
                if comp_num == 0:
                    cur.execute("""INSERT INTO crash_app_components (crash_id, component)
                                   VALUES (?, ?)""",
                                [id, component])

    def get_app_components(self, id):
        """
        Get the components for a crash

        :returns: the components for a crash
        :rtype: list
        """
        with self.db:
            cur = self.db.cursor()
            cur.execute("""SELECT component
                           FROM crash_app_components
                           WHERE crash_id = ?""", [id])
            comps = [row[0] for row in cur.fetchall()]
        return comps

    def remove_app_component(self, id, component=None):
        """
        Remove components for a crash

        :param str component: if it is None, all components are deleted
        :raise ValueError: if the component is not None but it is not a crash app component
        """
        with self.db:
            cur = self.db.cursor()
            if component is None:
                cur.execute("""DELETE FROM crash_app_components
                               WHERE crash_id = ?""", [id])
            else:
                cur.execute("""DELETE FROM crash_app_components
                               WHERE crash_id = ? AND component = ?""", [id, component])
                if cur.rowcount == 0:
                    raise ValueError("%s is not an application component of crash %d" % (component, id))

    def add_client_side_duplicate(self, id, crash_url):
        """
        Add a client side duplicate

        :param int id: the crash id to set the duplicate from
        :param str crash_url: the original crash url duplicate
        """
        with self.db:
            cur = self.db.cursor()
            cur.execute("""INSERT INTO client_side_duplicates (crash_id, url)
                           VALUES (?, ?)""",
                        [id, crash_url])

    def get_client_side_duplicates(self, id):
        """
        Get the client side duplicates for a crash id

        :param int id: the crash id to get the duplicates from
        :returns: the urls from the duplicated crashes
        :rtype: list
        """
        with self.db:
            cur = self.db.cursor()
            cur.execute("""SELECT url
                           FROM client_side_duplicates
                           WHERE crash_id = ?""", [id])
            urls = [row[0] for row in cur.fetchall()]
        return urls

    def remove_client_side_duplicate(self, id, crash_url=None):
        """
        Remove client side duplicates for a crash

        :param int id: the crash id to remove the client side duplicates from
        :param str crash_url: if it is None, all crash urls are deleted
        :raise ValueError: if the component is not None but it is not a crash app component
        """
        with self.db:
            cur = self.db.cursor()
            if crash_url is None:
                cur.execute("""DELETE FROM client_side_duplicates
                               WHERE crash_id = ?""", [id])
            else:
                cur.execute("""DELETE FROM client_side_duplicates
                               WHERE crash_id = ? AND url = ?""", [id, crash_url])
                if cur.rowcount == 0:
                    raise ValueError("%s is not an crash URL duplicate of crash %d" % (crash_url, id))

    def n_client_side_duplicates(self, id):
        """
        Get the number of client side duplicates

        :param int id: the crash id to get the stats
        :returns: the number of client side duplicates
        :rtype: int
        """
        with self.db:
            cur = self.db.cursor()
            cur.execute("""SELECT COUNT(*) FROM client_side_duplicates
                           WHERE crash_id = ?""", [id])
            n_dups = cur.fetchone()[0]
        return n_dups

    def set_tracker_url(self, id, url):
        """Set the tracker URL to track the crash report resolution.

        It is assumed the issue is properly set.

        :param int id: the crash identifier
        :param str url: the tracker URL
        """
        with self.db:
            cur = self.db.cursor()
            cur.execute("""UPDATE crashes SET tracker_url = ?
                           WHERE crash_id = ?""", [url, id])

    def get_tracker_url(self, id):
        """Get the tracker URL to track the crash report resolution.

        :param int id: the crash identifier
        :returns: the crash tracker URL, None if not found
        :rtype: Str
        """
        with self.db:
            cur = self.db.cursor()
            cur.execute("""SELECT tracker_url FROM crashes
                           WHERE crash_id = ?""", [id])
            row = cur.fetchone()
            if row:
                return row[0]
            return None

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
            orig_crash_url VARCHAR(1024),
            tracker_url VARCHAR(1024),
            CONSTRAINT master_fk FOREIGN KEY(master_id) REFERENCES crashes(crash_id),
            CONSTRAINT crashes_pk PRIMARY KEY(crash_id))""")

            cur.execute('CREATE INDEX master_index ON crashes(master_id)')

            cur.execute("""CREATE TABLE crash_comments (
            crash_id INTEGER NOT_NULL,
            comment TEXT)""")

            cur.execute("""CREATE TABLE crash_app_components (
            crash_id INTEGER NOT NULL,
            component VARCHAR(64) NOT NULL,
            CONSTRAINT crashes_fk FOREIGN KEY(crash_id) REFERENCES crashes(crash_id)
            )""")

            cur.execute("""CREATE TABLE client_side_duplicates (
            crash_id INTEGER NOT NULL,
            url VARCHAR(1024) NOT NULL,
            CONSTRAINT crashes_fk FOREIGN KEY(crash_id) REFERENCES crashes(crash_id)
            )""")

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
                port = int(port)
            else:
                host, port = url.netloc, 80
            file_name = url.path.rsplit('/', 1)[-1]
            post_multipart(host, port, url.path, [], [('upload', file_name, blob.getvalue())])
        else:
            raise ValueError('Unhandled scheme: %s' % url.scheme)

    def _update_report_file(self, report):
        '''Update the report file based on scheme'''
        self._upload_report_file(report)

    def _report_file_name(self, report, suggested_file_name):
        if suggested_file_name:
            return suggested_file_name
        else:
            sep = '_'
            exe_path = report['ExecutablePath'].replace(os.path.sep, sep)
            return str(self.last_crash_id + 1) + sep + exe_path + '.crash'


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
    s = BytesIO()
    for element in L:
        s.write(str(element))
        s.write(CRLF)
    body = s.getvalue()
    content_type = 'multipart/form-data; boundary=%s' % LIMIT
    return content_type, body
