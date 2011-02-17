#!/usr/bin/python
# -*- coding: utf-8 -*-

import openchange.mapistore as mapistore
import unittest

class TestMAPIStore(unittest.TestCase):
	def setUp(self):
		pass

	def test_errstr(self):
		self.assertEqual(mapistore.errstr(0), "Success")
		self.assertEqual(mapistore.errstr(1), "Non-specific error")
		self.assertEqual(mapistore.errstr(2), "No memory available")
		self.assertEqual(mapistore.errstr(16), "Already exists")

	def tearDown(self):
		pass

if __name__ == '__main__':
	unittest.main()
