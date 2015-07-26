#!/usr/bin/env python2.7
# -*- coding: utf-8 -*-

import gdb
import os

bt = str(gdb.execute("bt full", False, True))
for line in bt.split('\n'):
	if "dcesrv_request" in line:
		frame = line.split(" ")[0].replace('#', '')
		gdb.execute("frame %s" % frame)
		gdb.execute("dump binary memory /tmp/pkt_in call->ndr_pull->data (call->ndr_pull->data + call->ndr_pull->data_size)")
		os.system("ndrdump -l libmapi.so exchange_emsmdb 0xb in /tmp/pkt_in")
