#!/usr/bin/python

import os
import ocpf
from ocpf import Ocpf

filename = os.getcwd() + "/../sample_appointment.ocpf";
ctx = Ocpf(filename, ocpf.OCPF_FLAGS_READ)

ret = ctx.parse()
ret = ctx.set_SPropValue_array()
ret = ctx.dump()

filename = os.getcwd() + "/ocpf_test.ocpf";
ctx2 = Ocpf(filename, ocpf.OCPF_FLAGS_CREATE)
# dummy test to ensure we can set an ocpf parameter properly
ret = ctx2.write_init(0x12345678)
ret = ctx2.dump()

del ctx
