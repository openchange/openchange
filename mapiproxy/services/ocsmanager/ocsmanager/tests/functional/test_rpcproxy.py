from ocsmanager.tests import TestController, url


class TestRpcproxyController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='rpcproxy', action='index'))  # NOQA
        # Test response...
