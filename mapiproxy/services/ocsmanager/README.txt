= Installation and Setup =

 1. Install ``ocsmanager`` using easy_install::

    easy_install ocsmanager

 2. Tweak the configuration file (ocsmanager.ini) or use development.ini for testing purposes

 3. paster server inifile

= Testing with curl =

To use services available via ocsmanager, the http client must use NTLM auth.
See http://www.innovation.ch/personal/ronald/ntlm.html

As ntlm authenticates 'connections', it is very critical to use either HTTP/1.1
or HTTP Keep-Alive when using this service.
Since NTLMAuthHandler uses a cookie to store the client_id of the client,
the HTTP client used to test the service should be able to handle those.

To test the autodiscovery service with curl, the following command line can be used:

  curl -u 'user:pass' -d @postdata.xml --ntlm -b ''  http://127.0.0.1:5000/autodiscover/autodiscover.xml

postdata.xml should contain a XML payload similar to this:

<?xml version="1.0" encoding="utf-8"?>
<Autodiscover xmlns="http://schemas.microsoft.com/exchange/autodiscover/outlook/requestschema/2006">
  <Request>
    <EMailAddress>sogo1@example.com</EMailAddress>
    <AcceptableResponseSchema>http://schemas.microsoft.com/exchange/autodiscover/outlook/responseschema/2006a</AcceptableResponseSchema>
  </Request>
</Autodiscover>

Replace the EMailAddress field with valid data


To test the freebusy service:
  curl -H 'Content-Type: text/xml; charset=utf-8'  -u 'sogo1:sogo' -d @fb.xml --ntlm -b ''  http://127.0.0.1:5000/ews/as

  fb.xml:
<?xml version="1.0"?>
<q:Envelope xmlns:q="http://schemas.xmlsoap.org/soap/envelope/">
  <q:Header>
    <wsa:MessageID xmlns:wsa="http://www.w3.org/2005/08/addressing/">urn:uuid:7A3C8FB9-0AFE-4020-A362-172AD7B49B17</wsa:MessageID>
  </q:Header>
  <q:Body xmlns:wsa="http://www.w3.org/2005/08/addressing/">
    <ex12m:GetUserAvailabilityRequest xmlns:ex12m="http://schemas.microsoft.com/exchange/services/2006/messages" xmlns:ex12t="http://schemas.microsoft.com/exchange/services/2006/types">
      <ex12t:TimeZone>
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
        <ex12t:MailboxData>
          <ex12t:Email>
            <ex12t:Address>sogo2@example.com</ex12t:Address>
            <ex12t:RoutingType>SMTP</ex12t:RoutingType>
          </ex12t:Email>
          <ex12t:AttendeeType>Required</ex12t:AttendeeType>
        </ex12t:MailboxData>
        <ex12t:MailboxData>
          <ex12t:Email>
            <ex12t:Address>sogo3@example.com</ex12t:Address>
            <ex12t:RoutingType>SMTP</ex12t:RoutingType>
          </ex12t:Email>
          <ex12t:AttendeeType>Required</ex12t:AttendeeType>
        </ex12t:MailboxData>
      </ex12m:MailboxDataArray>
      <ex12t:FreeBusyViewOptions>
        <ex12t:TimeWindow>
          <ex12t:StartTime>2013-02-24T11:00:00</ex12t:StartTime>
          <ex12t:EndTime>2013-03-26T11:00:00</ex12t:EndTime>
        </ex12t:TimeWindow>
        <ex12t:MergedFreeBusyIntervalInMinutes>30</ex12t:MergedFreeBusyIntervalInMinutes>
        <ex12t:RequestedView>Detailed</ex12t:RequestedView>
      </ex12t:FreeBusyViewOptions>
    </ex12m:GetUserAvailabilityRequest>
  </q:Body>
</q:Envelope>
