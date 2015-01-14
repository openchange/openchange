from ocsmanager.tests import *

class TestOofController(TestController):

    def test_index(self):
        response = self.app.get(url(controller='oof', action='index'))
        # Test response...
