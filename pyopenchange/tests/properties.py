#!/usr/bin/env python2.7

import sys

sys.path.append("python")

from openchange import mapi
import time

SPropValue = mapi.SPropValue()
SPropValue.add(mapi.PidTagSubject, "This is a test")
SPropValue.add(mapi.PidTagImportance, 4096)
SPropValue.add(mapi.PidTagMessageToMe, True)
SPropValue.add(mapi.PidTagMessageCcMe, False)
SPropValue.add(mapi.PidTagStartDate, time.time())

SPropValue.dump("Python: ")

