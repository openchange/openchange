"""
SQLite CrashDatabase implementation.

It is useful to have a complete implementation of crash management in a single host.
"""
import apport.crashdb
from apport.report import Report
from io import BytesIO
import os.path
import sqlite3


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
        """
        cur = self.db.cursor()
        app, version = report['Package'].split(' ', 1)
        buf = BytesIO()
        report.write(buf)
        buf.seek(0)  # Start over again
        cur.execute("""INSERT INTO crashes
                       (crash_id, crash, title, app, version, sym_stacktrace, distro_release)
                       VALUES (?, ?, ?, ?, ?, ?, ?)""",
                    (self.last_crash_id + 1, buf.read(), report.standard_title(), app, version,
                     report.get('Stacktrace', None), report.get('DistroRelease', None)))
        self.db.commit()
        self.last_crash_id = cur.lastrowid
        return self.last_crash_id

    def download(self, id):
        """
        Download the report from the database

        :raises TypeError: if the report does not exist
        """
        cur = self.db.cursor()
        cur.execute("""SELECT crash
                       FROM crashes
                       WHERE crash_id = ?""", [id])
        buf = cur.fetchone()[0]
        report = Report()
        if isinstance(buf, unicode):
            buf = bytes(buf)
        report.load(BytesIO(buf))
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
            buf = BytesIO()
            db_report.write(buf)
            buf.seek(0)

            cur.execute("""UPDATE crashes SET crash = ?, title = ?, sym_stacktrace = ?, distro_release = ?
                           WHERE crash_id = ?""",
                        (buf.read(), db_report.standard_title(), report.get('Stacktrace', None),
                         report.get('DistroRelease', None), id))

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

            cur.execute("""CREATE TABLE crashes (
            crash_id INTEGER NOT NULL,
            crash BLOB NOT_NULL,
            title VARCHAR(64),
            app VARCHAR(64) NOT NULL,
            version VARCHAR(64),
            distro_release VARCHAR(64),
            description TEXT,
            sym_stacktrace TEXT,
            fixed_version VARCHAR(64),
            state VARCHAR(64),
            master_id INTEGER,
            CONSTRAINT master_fk FOREIGN KEY(master_id) REFERENCES crashes(crash_id),
            CONSTRAINT crashes_pk PRIMARY KEY(crash_id))""")

            cur.execute('CREATE INDEX master_index ON crashes(master_id)')

            cur.execute("""CREATE TABLE crash_comments (
            crash_id INTEGER NOT_NULL,
            comment TEXT)""")
