from ocsmanager.tests import *

class TestRpcproxyController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='rpcproxy', action='index'))
        # Test response...
