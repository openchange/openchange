#!/usr/bin/env python2.7

import os
import sys
import hashlib
from base64 import urlsafe_b64encode as encode

def main():
	if len(sys.argv) != 2:
		print '%s password' % (sys.argv[0])

	salt = os.urandom(4)
	h = hashlib.sha1(sys.argv[1])
	h.update(salt)
	print "{SSHA}" + encode(h.digest() + salt)
	sys.exit()

if __name__ == "__main__":
	main()
