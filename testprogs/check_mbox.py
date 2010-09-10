#!/usr/bin/env python

import argparse
import mailbox
import os
import shutil
import subprocess
import tempfile

mboxparser = argparse.ArgumentParser(description='Test an mbox file for general compatibility')
mboxparser.add_argument('--mbox', required=True)

args = mboxparser.parse_args()

###-----------------------
print "Internal (python mailbox) parser:"
print "\tmessage count", len(mailbox.mbox(args.mbox))

###-----------------------
print "mbox2eml:"
emldir = tempfile.mkdtemp()
print "\treturn code:", subprocess.call(["mbox2eml", "--input-file", args.mbox, "--output-dir", emldir])
print "\tmessage count", len(os.listdir(emldir))
shutil.rmtree(emldir, ignore_errors=True)

###-----------------------
print "mailx:"
p1 = subprocess.Popen(["mailx", "-H", "-f", args.mbox,], stdout=subprocess.PIPE)
p2 = subprocess.Popen(["wc", "-l"], stdin=p1.stdout, stdout=subprocess.PIPE)
print "\tmessage count", p2.communicate()[0]



