#!/usr/bin/env python2.7
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
Methods related to redmine management tracker.

This is specific to OpenChange.
"""
import redmine


class RedmineClient(object):
    """Wrapper to manage redmine integration"""

    def __init__(self, url, key, project=None, component_conf={},
                 custom_fields=[], reporter_field=None, status={}):
        """Initialise the client

        :param str url: the URL where the redmine is hosted
        :param str key: the authentication API REST key
        :param int or str project: the project identifier.
                                   It is None, then it takes the default one.
        :param dict component_conf: the component configuration to set
                                    the component based on the configuration file.
        :param list custom_fields: the custom fields that are mandatory to set
                                   when creating the issue.
        :param dict reporter_field: field to set reporter email

        :param dict status: indicate which are valid closed status
                            identifiers and reopened status
                            identifiers for reopening closed issues
                            once a new duplicate is uploaded.
                            *closed* is a tuple with valid closed status ids
                            *reopened* is the status to go after reopening an issue
        """
        self.redmine = redmine.Redmine(url, key=key)
        self.project = project
        self.component_conf = component_conf
        self.custom_fields = custom_fields
        self.reporter_field = reporter_field

        # Use Bug by now, we can look for Crash Report tracker as well
        tracker_elems = filter(lambda t: t['name'] == 'Bug', self.redmine.tracker.all())
        if len(tracker_elems) > 0:
            self.tracker_id = tracker_elems[0]['id']
        else:
            self.tracker_id = None

        self.status = None
        # Make sure closed and reopened are consistent
        if 'closed' in status and 'reopened' in status:
            self.status = status
            # Make sure an iterable is always used for closed status
            if not hasattr(self.status['closed'], '__contains__'):
                self.status['closed'] = (self.status['closed'],)

    def create_issue(self, id, report, comps=None, reporter=None):
        """Create a new issue based on a report.

        It sets:

          * the tracker as Bug
          * the subject with the crash_id and the crash report title
          * the description with the StacktraceTop, Components, Crash file, Distro Release
            and OpenChange server package.

        If there is a configuration to set the component based on OC app components,
        it uses it.

        :param int id: the crash identifier
        :param Report report: the crash report object
        :param list comps: application components. Optional.
        :param str reporter: the email from the reporter. Optional.
        :returns: the newly created issue
        :rtype: redmine.resource.Issue
        """
        issue = self.redmine.issue.new()
        issue.project_id = self.project
        issue.subject = "CR-%d: %s" % (id, report.standard_title())
        issue.tracker_id = self.tracker_id

        description = ""
        if comps:
            description += "Components: %s\n\n" % ', '.join(comps)

        if 'StacktraceTop' in report:
            description += "Stacktrace top 5 functions:\n<pre>%s</pre>\n\n" % report['StacktraceTop']
        if '_OrigURL' in report:
            _, base_name = report['_OrigURL'].rsplit('/', 1)
            description += "Crash file: %s\n" % base_name

        description += self._dump_distro_release(report)
        description += self._dump_package(report)

        issue.description = description
        custom_fields = []

        if comps and self.component_conf:
            match = None
            for app_comp in comps:
                if app_comp in self.component_conf:
                    match = self.component_conf[app_comp]
                    break

            if match is None and 'default' in self.component_conf:
                match = self.component_conf['default']

            if match:
                if '_custom_field_id' in self.component_conf:
                    custom_fields.append({'id': self.component_conf['_custom_field_id'], 'value': match})
                else:
                    issue.category_id = match

        if self.custom_fields:
            custom_fields.extend(self.custom_fields)

        if reporter and self.reporter_field:
            if 'custom_field' in self.reporter_field:
                custom_fields.append({'id': self.reporter_field['custom_field'], 'value': reporter})

        issue.custom_fields = custom_fields

        issue.save()
        return issue

    def add_duplicate(self, report, issue_id, dup_crash_url):
        """Add a duplicate to the journal of a given issue.

        Re-open an issue if it is closed if supported.

        :param Report report: the duplicated report
        :param int issue_id: the issue identifier
        :param str dup_crash_url: the URL of the duplicate crash
        :returns: if the action of adding was successful
        :rtype: bool
        """
        _, base_name = dup_crash_url.rsplit('/', 1)
        note = 'A new duplicate has been found: {0}\n'.format(base_name)
        note += self._dump_distro_release(report)
        note += self._dump_package(report)
        try:
            self.redmine.issue.update(issue_id,
                                      notes=note)
        except redmine.exceptions.ResourceNotFoundError:
            # We cannot add the duplicate
            return False

        # Re-open the issue if it closed
        if self.status is not None:
            issue = self.redmine.issue.get(issue_id)
            if issue.status.id in self.status['closed']:
                self.redmine.issue.update(issue_id,
                                          status_id=self.status['reopened'])
        return True

    # Helper methods to dump info to Redmine in plain text
    def _dump_distro_release(self, report):
        if 'DistroRelease' in report:
            return '\nDistro Release: {0}\n'.format(report['DistroRelease'])
        return ""

    def _dump_package(self, report):
        dumped = ""
        if 'Dependencies' in report:
            pkgs = (e for e in report['Dependencies'].split('\n') if e.startswith('openchangeserver'))
            for pkg in pkgs:
                dumped += '\nPackage: {0}\n'.format(pkg)
        return dumped
