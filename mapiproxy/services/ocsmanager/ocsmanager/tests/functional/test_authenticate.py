from ocsmanager.tests import *
import paste.httpexceptions as httpexceptions
from xml.etree import ElementTree as ET

class TestAuthenticateController(TestController):

    def test_token(self):
        """ Test token function with XML payload using valid username. """
        response = self.app.post(url(controller='authenticate', action='token'),
                                 params={'payload':'<!DOCTYPE ocsmanager><xml><login>jkerihuel</login></xml>'})
        xmlData = ET.XML(response.body)
        assert xmlData is not None, "expected valid XML to be returned"

        tokens = xmlData.findall("token")
        assert tokens is not None, "No token received"
        assert len(tokens) == 2, "2 tokens expected got %d" % len(tokens)
        number = 0
        for token in tokens:
            assert "type" in token.attrib, 'no type option specified: %s' % token.attrib
            assert token.text is not None, 'no text value for token type=%s' % token.attrib['type']
            if "type" in token.attrib:
                if token.attrib["type"] == "session": number += 1
                if token.attrib["type"] == "salt" : number += 1
        assert number == 2, "Invalid token types: got %d on 2" % number

        salt = xmlData.find("salt")
        assert salt is not None, "No salt received"

    def test_token_no_login(self):
        """ Test token function with XML payload without username. """
        response = self.app.post(url(controller='authenticate', action='token'),
                                 params={'payload':'<!DOCTYPE ocsmanager><xml><login></login></xml>'})
        xmlData = ET.XML(response.body)
        assert xmlData is not None, "expected valid XML to be returned"
        error = xmlData.find('error')
        assert error is not None
        code = error.attrib['code']
        assert code == '417', "Invalid error code %s, expected 417" % code

    def test_token_no_payload(self):
        """ Test token with no payload. Expect error XML with code 417."""
        response = self.app.post(url(controller='authenticate', action='token'), 
                                 params='')
        xmlData = ET.XML(response.body)
        assert xmlData is not None, "expected valid XML to be returned"
        error = xmlData.find('error')
        assert error is not None
        code = error.attrib['code']
        assert code == '417', "Invalid error code %s, expected 417" % code

