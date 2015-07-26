#!/usr/bin/env python2.7

import sys

sys.path.append("python")

import os
import openchange
import openchange.ocpf as ocpf
import openchange.mapi as mapi
from openchange.ocpf import Ocpf

filename = os.getcwd() + "/../../libocpf/examples/sample_appointment.ocpf";
ctx = Ocpf(filename, ocpf.OCPF_FLAGS_READ)

ret = ctx.parse()
ret = ctx.set_SPropValue_array()
SPropValue = ctx.get_SPropValue()
ret = ctx.dump()
SPropValue.dump("1. SPropValue:  ")


del SPropValue
SPropValue = mapi.SPropValue()
SPropValue.add(mapi.PR_CONVERSATION_TOPIC, "New subject from python")
SPropValue.add(mapi.PR_NORMALIZED_SUBJECT, "New subject from python")
ret = ctx.add_SPropValue(SPropValue)
ret = ctx.set_SPropValue_array()

print "\n"
print "New SpropValue after replacing subject:"
print "======================================="
del SPropValue
SPropValue = ctx.get_SPropValue()
SPropValue.dump("2. SPropValue:  ")

print 
print "Writing ocpf_test.ocpf file:"
print "============================"
filename = os.getcwd() + "/ocpf_test.ocpf";
ctx2 = Ocpf(filename, ocpf.OCPF_FLAGS_CREATE)
# dummy test to ensure we can set an ocpf parameter properly
ret = ctx2.write_init(0x12345678)
ret = ctx2.write(SPropValue)
ret = ctx2.write_commit()
ret = ctx2.dump()

del ctx
