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

To test the service with curl, the following command line can be used:

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
