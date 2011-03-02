#!/usr/bin/python
# -*- coding: utf-8 -*-

from openchange import mapi
import time

import unittest

class TestMapi(unittest.TestCase):
	def setUp(self):
		self.SPropValue = mapi.SPropValue()
		pass

	def test_noargs(self): # we need two args
		self.assertRaises(TypeError, self.SPropValue.add)
		
	def test_onearg(self): # we need two args
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagGender)

	def test_i2(self):
		self.SPropValue.add(mapi.PidTagGender, 3)

	def test_i2_duplicate(self):
		self.SPropValue.add(mapi.PidTagGender, 3)
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagGender, 2)

	def test_i2_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagGender, "wrong type for I2")

	def test_long(self):
		self.SPropValue.add(mapi.PidTagImportance, 4096)

	def test_long_duplicate(self):
		self.SPropValue.add(mapi.PidTagImportance, 4096)
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagImportance, 4097)

	def test_long_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagImportance, "wrong type for long")

	def test_double(self):
		self.SPropValue.add(0x8F010005, 3.1415) # PT_DOUBLE

	def test_double_duplicate(self):
		self.SPropValue.add(0x8F010005, 3.1415) # PT_DOUBLE
		self.assertRaises(TypeError, self.SPropValue.add, 0x8F010005, 42.7)

	def test_double_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, 0x8F010005, "wrong type for double")

	def test_errortype(self):
		self.SPropValue.add(mapi.PidTagGivenName_Error, 0x80040502)

	def test_errortype_duplicate(self):
		self.SPropValue.add(mapi.PidTagGivenName_Error, 0x80040502)
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagGivenName_Error, 0x80040305)

	def test_errortype_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagGivenName_Error, "wrong type for error")

	def test_boolean(self):
		self.SPropValue.add(mapi.PidTagProcessed, True)

	def test_boolean_duplicate(self):
		self.SPropValue.add(mapi.PidTagProcessed, True)
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagProcessed, True)

	def test_boolean_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagProcessed, "wrong type for boolean")

	def test_i8(self):
		self.SPropValue.add(mapi.PidTagInstID, 0x12345678L)

	def test_i8_duplicate(self):
		self.SPropValue.add(mapi.PidTagInstID, 0x12345678L)
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagInstID, 0x12345677L)

	def test_i8_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagInstID, "wrong type for I8")

	def test_string8(self):
		self.SPropValue.add(mapi.PidTagAddressBookHierarchicalRootDepartment, "Development")

	def test_string8_duplicate(self):
		self.SPropValue.add(mapi.PidTagAddressBookHierarchicalRootDepartment, "Development")
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagAddressBookHierarchicalRootDepartment, "Research")

	def test_string8_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagAddressBookHierarchicalRootDepartment, 423) # wrong type

	def test_unicode(self):
		self.SPropValue.add(mapi.PidTagComment, "value of the comment")

	def test_unicode_duplicate(self):
		self.SPropValue.add(mapi.PidTagComment, "value of the comment")
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagComment, "duplicate")

	def test_unicode_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagComment, 3) # wrong type

	def test_filetime(self):
		self.SPropValue.add(mapi.PidTagStartDate, time.time())

	def test_filetime_duplicate(self):
		self.SPropValue.add(mapi.PidTagStartDate, time.time())
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagStartDate, time.time())

	def test_filetime_wrong_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, mapi.PidTagStartDate, "wrong type for time")

	def test_dump_no_entries(self):
		self.SPropValue.dump("test_dump_no_entries:")
		
	def test_dump_one_entry(self):
		self.SPropValue.add(mapi.PidTagComment, "value of the comment")
		self.SPropValue.dump("test_dump_one_entry:")
	
	def test_bad_type(self):
		self.assertRaises(TypeError, self.SPropValue.add, 0x12340000, 1) # 0000 isn't a valid property type

	def test_dump_multiple_entries(self):
		self.SPropValue.add(mapi.PidTagGender, 3)
		self.SPropValue.add(mapi.PidTagImportance, 4096)
		self.SPropValue.add(0x8F010005, 3.1415) # PT_DOUBLE
		self.SPropValue.add(mapi.PidTagGivenName_Error, 0x80040502)
		self.SPropValue.add(mapi.PidTagProcessed, True)
		self.SPropValue.add(mapi.PidTagInstID, 0x12345678L)
		self.SPropValue.add(mapi.PidTagAddressBookHierarchicalRootDepartment, "Development")
		self.SPropValue.add(mapi.PidTagComment, "value of the comment")
		self.SPropValue.add(mapi.PidTagStartDate, time.time())
		self.SPropValue.dump("test_dump_multiple_entries:")
		
	# we need at least one argument, should raise an error if no args
	def test_dump_no_args(self):
		self.assertRaises(TypeError, self.SPropValue.dump)

	def tearDown(self):
		del(self.SPropValue)

if __name__ == '__main__':
	unittest.main()
