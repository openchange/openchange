#!/usr/bin/python

import optparse
import os,sys

from openchange import mapistore

parser = optparse.OptionParser(usage="%prog --username=<username> [options]", version="%prog 1.0")
parser.add_option("--username", type="string", help="Set the username to use with mapistore")
parser.add_option("--mapping-path", type="string", help="Specify mapistore mapping path")
parser.add_option("--openchangedb", type="string", help="Specify openchangedb URL")

group = optparse.OptionGroup(parser, "Debug Options")
group.add_option("-d", "--debuglevel", type="int", dest="debuglevel", default=0)
parser.add_option_group(group)

opts,args = parser.parse_args()

if not opts.username:
    print "[ERR] --username option required"
    parser.print_help()
    sys.exit(1)

if opts.mapping_path:
    mapistore.set_mapping_path(opts.mapping_path)

mstore = mapistore.MAPIStore()

if opts.debuglevel is not None:
    mstore.debuglevel = opts.debuglevel

if opts.openchangedb is not None:
    mstore.set_parm("mapiproxy:openchangedb", opts.openchangedb)

mstore.dump()

mstore.initialize(opts.username)
backend_names = mstore.list_backends();
print backend_names
print mstore.capabilities()
