from ocsmanager.tests import TestController, url

class TestNotificationController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='notification', action='index')) # NOQA
        # Test response...
