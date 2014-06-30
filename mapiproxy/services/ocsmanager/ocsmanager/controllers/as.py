# as.py -- GetUserAvailability Exchange Web Service
# -*- coding: utf-8 -*-
#
# Copyright (C) 2012-2014  Julien Kerihuel <jkerihuel@openchange.org>
#                          Enrique J. Hernández <ejhernandez@zentyal.com>
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
import logging

import datetime
from datetime import timedelta
from time import time

from pylons import request, response, session, tmpl_context as c, url
from pylons import config
from pylons.controllers.util import abort, redirect
from pylons.decorators.rest import restrict

import rpclib
from rpclib.application import Application
from rpclib.decorator import rpc, srpc
from rpclib.interface.wsdl import Wsdl11
from rpclib.protocol.soap import Soap11
from rpclib.service import ServiceBase
from rpclib.server.wsgi import WsgiApplication
from rpclib.util.simple import wsgi_soap_application

from lxml.etree import Element, ElementTree, tostring

import ldb
from openchange import mapistore
from ocsmanager.lib.base import BaseController, render

from ews_types import *


log = logging.getLogger(__name__)

"""
* sample REQUEST (availability):
<?xml version="1.0"?>
<q:Envelope xmlns:q="http://schemas.xmlsoap.org/soap/envelope/">
  <q:Body>
    <ex12m:GetUserAvailabilityRequest xmlns:ex12m="http://schemas.microsoft.com/exchange/services/2006/messages">
      <ex12t:TimeZone xmlns:ex12t="http://schemas.microsoft.com/exchange/services/2006/types">
        <ex12t:Bias>300</ex12t:Bias>
        <ex12t:StandardTime>
          <ex12t:Bias>0</ex12t:Bias>
          <ex12t:Time>02:00:00</ex12t:Time>
          <ex12t:DayOrder>1</ex12t:DayOrder>
          <ex12t:Month>11</ex12t:Month>
          <ex12t:DayOfWeek>Sunday</ex12t:DayOfWeek>
        </ex12t:StandardTime>
        <ex12t:DaylightTime>
          <ex12t:Bias>-60</ex12t:Bias>
          <ex12t:Time>02:00:00</ex12t:Time>
          <ex12t:DayOrder>2</ex12t:DayOrder>
          <ex12t:Month>3</ex12t:Month>
          <ex12t:DayOfWeek>Sunday</ex12t:DayOfWeek>
        </ex12t:DaylightTime>
      </ex12t:TimeZone>
      <ex12m:MailboxDataArray>
        <ex12t:MailboxData xmlns:ex12t="http://schemas.microsoft.com/exchange/services/2006/types">
          <ex12t:Email>
            <ex12t:Address>openchange@inverse.ca</ex12t:Address>
            <ex12t:RoutingType>SMTP</ex12t:RoutingType>
          </ex12t:Email>
          <ex12t:AttendeeType>Organizer</ex12t:AttendeeType>
          <ex12t:ExcludeConflicts>false</ex12t:ExcludeConflicts>
        </ex12t:MailboxData>
      </ex12m:MailboxDataArray>
      <ex12t:SuggestionsViewOptions xmlns:ex12t="http://schemas.microsoft.com/exchange/services/2006/types">
        <ex12t:MaximumNonWorkHourResultsByDay>0</ex12t:MaximumNonWorkHourResultsByDay>
        <ex12t:MeetingDurationInMinutes>30</ex12t:MeetingDurationInMinutes>
        <ex12t:DetailedSuggestionsWindow>
          <ex12t:StartTime>2012-04-29T00:00:00</ex12t:StartTime>
          <ex12t:EndTime>2012-06-10T00:00:00</ex12t:EndTime>
        </ex12t:DetailedSuggestionsWindow>
        <ex12t:GlobalObjectId/>
      </ex12t:SuggestionsViewOptions>
    </ex12m:GetUserAvailabilityRequest>
  </q:Body>
</q:Envelope>


Exchange response (reformatted):

<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <soap:Header>
    <t:ServerVersionInfo xmlns:t="http://schemas.microsoft.com/exchange/services/2006/types" MajorVersion="8" MinorVersion="0" MajorBuildNumber="685" MinorBuildNumber="24"/>
  </soap:Header>
  <soap:Body>
    <GetUserAvailabilityResponse xmlns="http://schemas.microsoft.com/exchange/services/2006/messages">
      <SuggestionsResponse>
        <ResponseMessage ResponseClass="Success">
          <ResponseCode>NoError</ResponseCode>
        </ResponseMessage>
        <SuggestionDayResultArray>
          <SuggestionDayResult xmlns="http://schemas.microsoft.com/exchange/services/2006/types">
            <Date>2012-04-29T00:00:00</Date>
            <DayQuality>Poor</DayQuality>
            <SuggestionArray/>
          </SuggestionDayResult>
          <SuggestionDayResult xmlns="http://schemas.microsoft.com/exchange/services/2006/types">
            <Date>2012-04-30T00:00:00</Date>
            <DayQuality>Poor</DayQuality>
            <SuggestionArray/>
          </SuggestionDayResult>
          ...
          <SuggestionDayResult xmlns="http://schemas.microsoft.com/exchange/services/2006/types">
            <Date>2012-06-07T00:00:00</Date>
            <DayQuality>Poor</DayQuality>
            <SuggestionArray/>
          </SuggestionDayResult>
          <SuggestionDayResult xmlns="http://schemas.microsoft.com/exchange/services/2006/types">
            <Date>2012-06-08T00:00:00</Date>
            <DayQuality>Poor</DayQuality>
            <SuggestionArray/>
          </SuggestionDayResult>
          <SuggestionDayResult xmlns="http://schemas.microsoft.com/exchange/services/2006/types">
            <Date>2012-06-09T00:00:00</Date>
            <DayQuality>Poor</DayQuality>
            <SuggestionArray/>
          </SuggestionDayResult>
        </SuggestionDayResultArray>
      </SuggestionsResponse>
    </GetUserAvailabilityResponse>
  </soap:Body>
</soap:Envelope>

* request2: (freebusy)

<?xml version="1.0"?>
<q:Envelope xmlns:q="http://schemas.xmlsoap.org/soap/envelope/">
  <q:Body>
    <ex12m:GetUserAvailabilityRequest xmlns:ex12m="http://schemas.microsoft.com/exchange/services/2006/messages">
      <ex12t:TimeZone xmlns:ex12t="http://schemas.microsoft.com/exchange/services/2006/types">
        <ex12t:Bias>300</ex12t:Bias>
        <ex12t:StandardTime>
          <ex12t:Bias>0</ex12t:Bias>
          <ex12t:Time>02:00:00</ex12t:Time>
          <ex12t:DayOrder>1</ex12t:DayOrder>
          <ex12t:Month>11</ex12t:Month>
          <ex12t:DayOfWeek>Sunday</ex12t:DayOfWeek>
        </ex12t:StandardTime>
        <ex12t:DaylightTime>
          <ex12t:Bias>-60</ex12t:Bias>
          <ex12t:Time>02:00:00</ex12t:Time>
          <ex12t:DayOrder>2</ex12t:DayOrder>
          <ex12t:Month>3</ex12t:Month>
          <ex12t:DayOfWeek>Sunday</ex12t:DayOfWeek>
        </ex12t:DaylightTime>
      </ex12t:TimeZone>
      <ex12m:MailboxDataArray>
        <ex12t:MailboxData xmlns:ex12t="http://schemas.microsoft.com/exchange/services/2006/types">
          <ex12t:Email>
            <ex12t:Address>wsourdeau@inverse.local</ex12t:Address>
            <ex12t:RoutingType>SMTP</ex12t:RoutingType>
          </ex12t:Email>
          <ex12t:AttendeeType>Required</ex12t:AttendeeType>
        </ex12t:MailboxData>
      </ex12m:MailboxDataArray>
      <ex12t:FreeBusyViewOptions xmlns:ex12t="http://schemas.microsoft.com/exchange/services/2006/types">
        <ex12t:TimeWindow>
          <ex12t:StartTime>2012-04-18T09:00:00</ex12t:StartTime>
          <ex12t:EndTime>2012-05-18T09:00:00</ex12t:EndTime>
        </ex12t:TimeWindow>
        <ex12t:MergedFreeBusyIntervalInMinutes>30</ex12t:MergedFreeBusyIntervalInMinutes>
        <ex12t:RequestedView>Detailed</ex12t:RequestedView>
      </ex12t:FreeBusyViewOptions>
    </ex12m:GetUserAvailabilityRequest>
  </q:Body>
</q:Envelope>


Error response (unknown user):

<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <soap:Header>
    <t:ServerVersionInfo
    xmlns:t="http://schemas.microsoft.com/exchange/services/2006/types"
    MajorVersion="8" MinorVersion="0" MajorBuildNumber="685"
    MinorBuildNumber="24"/>
  </soap:Header>
  <soap:Body>
    <GetUserAvailabilityResponse
    xmlns="http://schemas.microsoft.com/exchange/services/2006/messages">
      <FreeBusyResponseArray>
        <FreeBusyResponse>
          <ResponseMessage ResponseClass="Error">
            <MessageText>Unable to resolve e-mail address
            &lt;&gt;SMTP:wsourdeau@inverse.ca to an Active Directory
            object.</MessageText>
            <ResponseCode>ErrorMailRecipientNotFound</ResponseCode>
            <DescriptiveLinkKey>0</DescriptiveLinkKey>
            <MessageXml>
              <ExceptionType
              xmlns="http://schemas.microsoft.com/exchange/services/2006/errors">Microsoft.Exchange.InfoWorker.Common.Availability.MailRecipientNotFoundException</ExceptionType>
              <ExceptionCode
              xmlns="http://schemas.microsoft.com/exchange/services/2006/errors">5009</ExceptionCode>
            </MessageXml>
          </ResponseMessage>
          <FreeBusyView>
            <FreeBusyViewType
            xmlns="http://schemas.microsoft.com/exchange/services/2006/types">None</FreeBusyViewType>
          </FreeBusyView>
        </FreeBusyResponse>
      </FreeBusyResponseArray>
    </GetUserAvailabilityResponse>
  </soap:Body>
</soap:Envelope>


success response:

<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <soap:Header>
    <t:ServerVersionInfo
    xmlns:t="http://schemas.microsoft.com/exchange/services/2006/types"
    MajorVersion="8" MinorVersion="0" MajorBuildNumber="685"
    MinorBuildNumber="24"/>
  </soap:Header>
  <soap:Body>
    <GetUserAvailabilityResponse
    xmlns="http://schemas.microsoft.com/exchange/services/2006/messages">
      <FreeBusyResponseArray>
        <FreeBusyResponse>
          <ResponseMessage ResponseClass="Success">
            <ResponseCode>NoError</ResponseCode>
          </ResponseMessage>
          <FreeBusyView>
            <FreeBusyViewType
            xmlns="http://schemas.microsoft.com/exchange/services/2006/types">FreeBusy</FreeBusyViewType>
            <CalendarEventArray
            xmlns="http://schemas.microsoft.com/exchange/services/2006/types">
              <CalendarEvent>
                <StartTime>2012-05-15T00:00:00-04:00</StartTime>
                <EndTime>2012-05-16T00:00:00-04:00</EndTime>
                <BusyType>Free</BusyType>
              </CalendarEvent>
              <CalendarEvent>
                <StartTime>2012-05-16T08:00:00-04:00</StartTime>
                <EndTime>2012-05-16T08:30:00-04:00</EndTime>
                <BusyType>Busy</BusyType>
              </CalendarEvent>
            </CalendarEventArray>
            <WorkingHours
            xmlns="http://schemas.microsoft.com/exchange/services/2006/types">
              <TimeZone>
                <Bias>300</Bias>
                <StandardTime>
                  <Bias>0</Bias>
                  <Time>02:00:00</Time>
                  <DayOrder>1</DayOrder>
                  <Month>11</Month>
                  <DayOfWeek>Sunday</DayOfWeek>
                </StandardTime>
                <DaylightTime>
                  <Bias>-60</Bias>
                  <Time>02:00:00</Time>
                  <DayOrder>2</DayOrder>
                  <Month>3</Month>
                  <DayOfWeek>Sunday</DayOfWeek>
                </DaylightTime>
              </TimeZone>
              <WorkingPeriodArray>
                <WorkingPeriod>
                  <DayOfWeek>Monday Tuesday Wednesday Thursday
                  Friday</DayOfWeek>
                  <StartTimeInMinutes>480</StartTimeInMinutes>
                  <EndTimeInMinutes>1020</EndTimeInMinutes>
                </WorkingPeriod>
              </WorkingPeriodArray>
            </WorkingHours>
          </FreeBusyView>
        </FreeBusyResponse>
      </FreeBusyResponseArray>
    </GetUserAvailabilityResponse>
  </soap:Body>
</soap:Envelope>

"""

class ExchangeService(ServiceBase):
    @staticmethod
    def _open_user_calendar_folder(email):
        # lookup usercn from email
        ldbdb = config["samba"]["samdb_ldb"]
        base_dn = "CN=Users,%s" % config["samba"]["domaindn"]
        ldb_filter = "(&(objectClass=user)(mail=%s))" % email
        res = ldbdb.search(base=base_dn, scope=ldb.SCOPE_SUBTREE,
                           expression=ldb_filter, attrs=["cn", "sAMAccountName"])
        if len(res) != 1:
            return (None, "ErrorMailRecipientNotFound")

        user_cn = res[0]["cn"][0]
        username = res[0]["sAMAccountName"][0]
        ocdb = config["ocdb"]
        error_code = None
        for uri in ocdb.get_calendar_uri(user_cn, email):
            if uri.find("/personal") > -1:  # FIXME: this is evil
                context = config["mapistore"].add_context(uri, username)
                folder = context.open()
                if folder is None:
                    error_code = "ErrorNoFreeBusyAccess"
                return (folder, error_code)
        return (None, "ErrorNoFreeBusyAccess")

    @staticmethod
    def _timezone_datetime(year, tz_time):
        # we round the dates to midnight since events are unlikely to start at
        # such an early time of day
        return datetime.datetime(year, tz_time.Month + 1, tz_time.DayOrder + 1)

    @staticmethod
    def _freebusy_date(timezone, utcdate):
        bias = timezone.Bias
        if timezone.DaylightTime is not None:
            std_datetime = ExchangeService._timezone_datetime(utcdate.year, timezone.StandardTime)
            dst_datetime = ExchangeService._timezone_datetime(utcdate.year, timezone.DaylightTime)
            if std_datetime < dst_datetime:
                if utcdate >= std_datetime and utcdate < dst_datetime:
                    bias = bias + timezone.StandardTime.Bias
                else:
                    bias = bias + timezone.DaylightTime.Bias
            else:
                if utcdate >= dst_datetime and utcdate < std_datetime:
                    bias = bias + timezone.DaylightTime.Bias
                else:
                    bias = bias + timezone.StandardTime.Bias

        return utcdate - datetime.timedelta(0, bias * 60)

    @staticmethod
    def _freebusy_response(cal_folder, timezone, freebusy_view_options):
        if freebusy_view_options is None:
            start = datetime.datetime.now()
            end = start + timedelta(days=42)  # Maximum time up to Exchange 2010 is 42 days, 62 days above
        else:
            start = freebusy_view_options.TimeWindow.StartTime
            end = freebusy_view_options.TimeWindow.EndTime

        # a = time()
        # print "fetching freebusy"
        freebusy_props = cal_folder.fetch_freebusy_properties(start, end)
        # print "fetched freebusy: %f secs" % (time() - a)

        fb_response = FreeBusyResponse()
        fb_response.ResponseMessage = ResponseMessageType()
        fb_response.ResponseMessage.ResponseClass = "Success"
        fb_response.ResponseMessage.ResponseCode = "NoError" 
        fb_response.FreeBusyView = FreeBusyView()
        fb_response.FreeBusyView.FreeBusyViewType = "FreeBusy"

        events = []
        fb_types = {"free": "Free",
                    "tentative": "Tentative",
                    "busy": "Busy",
                    "away": "OOF"}

        for (fb_attribute, label) in fb_types.iteritems():
            fb_event_list = getattr(freebusy_props, fb_attribute)
            for fb_event in fb_event_list:
                event = CalendarEvent()
                event.StartTime = ExchangeService._freebusy_date(timezone, fb_event[0])
                event.EndTime = ExchangeService._freebusy_date(timezone, fb_event[1])
                event.BusyType = label
                events.append(event)
        fb_response.FreeBusyView.CalendarEventArray = events

        fb_response.FreeBusyView.WorkingHours = WorkingHours()
        fb_response.FreeBusyView.WorkingHours.TimeZone = timezone

        period = WorkingPeriod()
        period.DayOfWeek = "Monday Tuesday Wednesday Thursday Friday"
        period.StartTimeInMinutes = 8 * 60
        period.EndTimeInMinutes = 18 * 60
        fb_response.FreeBusyView.WorkingHours.WorkingPeriodArray = [period]

        return fb_response

    @staticmethod
    def _freebusy_lookup_error_response(error_code):
        fb_response = FreeBusyResponse()
        fb_response.ResponseMessage = ResponseMessageType()
        fb_response.ResponseMessage.ResponseClass = "Error"
        fb_response.ResponseMessage.MessageText \
            = "Unable to open the requested user's calendar"
        fb_response.ResponseMessage.ResponseCode = error_code
        fb_response.FreeBusyView = FreeBusyView()
        fb_response.FreeBusyView.FreeBusyViewType = "None"

        return [fb_response]

    @staticmethod
    def _suggestions_response(timezone, suggestions_view_options):
        suggestions = SuggestionsResponseType()
            
        suggestions.ResponseMessage = ResponseMessageType(ResponseClass="NoError")

        start = suggestions_view_options.DetailedSuggestionsWindow.StartTime
        end = suggestions_view_options.DetailedSuggestionsWindow.EndTime

        current = start
        delta_day = datetime.timedelta(days=1)

        suggestions_list = []
        while current < end:
            suggestion = SuggestionDayResult()
            suggestion.Date = current
            suggestion.DayQuality = "Poor"
            suggestion.SuggestionArray = []

            suggestions_list.append(suggestion)
            current = current + delta_day

        suggestions.SuggestionDayResultArray = suggestions_list

        return suggestions
       
    @staticmethod
    def _suggestions_lookup_error_response():
        suggestions = SuggestionsResponseType()
        suggestions.ResponseMessage = ResponseMessageType(ResponseClass="NoError")
        suggestions.ResponseMessage.ResponseClass = "Error"
        suggestions.ResponseMessage.MessageText \
            = "Unable to open the requested user's calendar"
        suggestions.ResponseMessage.ResponseCode = "ErrorMailRecipientNotFound"
        suggestions.SuggestionDayResultArray = []

        return suggestions
 
    @rpc(SerializableTimeZone, ArrayOfMailboxData, FreeBusyViewOptionsType, SuggestionsViewOptionsType,
         _in_message_name="GetUserAvailabilityRequest",
         _in_variable_names={"timezone": "TimeZone",
                             "mailbox_data_array": "MailboxDataArray",
                             "freebusy_view_options": "FreeBusyViewOptions",
                             "suggestions_view_options": "SuggestionsViewOptions"},
         _out_message="GetUserAvailabilityResponse",
         _out_header=ServerVersionInfo,
         _out_variable_names=("FreeBusyResponseArray", "SuggestionsResponse"),
         _returns=(ArrayOfFreeBusyResponse,SuggestionsResponseType),
         _soap_body_style="document")
    def GetUserAvailabilityRequest(ctx, 
                                   timezone = None, mailbox_data_array = None,
                                   freebusy_view_options = None,
                                   suggestions_view_options = None):
        ctx.out_header = ServerVersionInfo(MajorVersion=8,
                                           MinorVersion=0,
                                           MajorBuildNumber=685,
                                           MinorBuildNumber=24)


        fb_requests = []
        for x in xrange(len(mailbox_data_array)):
            user_email = mailbox_data_array[x].Email.Address
            (calendar_folder, error_code) = ExchangeService._open_user_calendar_folder(user_email)
            fb_requests.append({"folder": calendar_folder, "email": user_email, "error_code": error_code})

        freebusy = []
        for fb_request in fb_requests:
            calendar_folder = fb_request["folder"]
            if calendar_folder is None:
                log.warn("no calendar folder found for '%s'" % fb_request["email"])
                fb_response \
                    = ExchangeService._freebusy_lookup_error_response(fb_request["error_code"])
            else:
                fb_response \
                    = ExchangeService._freebusy_response(calendar_folder,
                                                         timezone,
                                                         freebusy_view_options)
            freebusy.append(fb_response)

        if not freebusy:
            freebusy = None

        if suggestions_view_options is not None:
            suggestions \
                = ExchangeService._suggestions_response(timezone,
                                                        suggestions_view_options)
        else:
            suggestions = None
        
        return (freebusy, suggestions)

EwsApp = Application([ExchangeService], EWS_M_NS,
                     name="ExchangeApplication",
                     interface=Wsdl11(), in_protocol=Soap11(), out_protocol=Soap11())

AsController = WsgiApplication(EwsApp)
