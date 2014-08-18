#!/usr/bin/python
# -*- coding: utf-8 -*-

# Copyright (C) Enrique J. Hernández 2014

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
Methods related to mail notifications.

This is specific to OpenChange.
"""
from email.mime.text import MIMEText
import smtplib


def notify_user_email(from_addr, email, tracker_url,
                          smtp_addr='localhost', duplicated=False):
    """Notify a user after sending a report to know track the issue later.

    :param str from_addr: the from email address
    :param str email: the user's email address
    :param str tracker_url: the tracker URL
    :param str smtp_addr: the STMP server
    :param bool duplicated: indicating if the sent crash report is duplicated or not
    """
    to_addr = email

    if duplicated:
        text = """This crash report is a duplicate from {0}.""".format(tracker_url)
    else:
        text = """The crash report was created at {0}.""".format(tracker_url)

    text += """
\n\nYou can follow the crash report fixing status there.\n\n
Thanks very much for reporting it!\n
----
OpenChange team"""

    msg = MIMEText(text, 'plain')

    msg['Subject'] = '[OpenChange crash report] Your crash report was uploaded!'
    msg['From'] = from_addr
    msg['To'] = to_addr

    s = smtplib.SMTP(smtp_addr)
    # sendmail function takes 3 arguments: sender's address, recipient's address
    # and message to send - here it is sent as one string.
    s.sendmail(from_addr, to_addr, msg.as_string())
    s.quit()
