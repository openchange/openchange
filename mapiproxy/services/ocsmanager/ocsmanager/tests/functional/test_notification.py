from ocsmanager.tests import *

class TestNotificationController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='notification', action='index'))
        # Test response...
