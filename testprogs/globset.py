class GLOBSetRunner:
    def __init__(self, buffer, pos):
        self.buffer = buffer
        self.start_pos = pos
        self.pos = pos
        self.end = False
        self.stack = []
        self.stackByteLength = 0
        self.ranges = []

    def _doPush1(self):
        self._doPush(1)

    def _doPush2(self):
        self._doPush(2)

    def _doPush3(self):
        self._doPush(3)

    def _doPush4(self):
        self._doPush(4)

    def _doPush5(self):
        self._doPush(5)

    def _doPush6(self):
        self._doPush(6)

    def _doPush(self, nbr):
        bytes = self.buffer[self.pos:self.pos+nbr]
        # print "push %d bytes: (%s)" % (nbr, ", ".join(["0x%.2x" % ord(x) for x in bytes]))
        self.stack.append(bytes)
        self.stackByteLength = self.stackByteLength + nbr
        self.pos = self.pos + nbr
        if self.stackByteLength == 6:
            # print "  buffer filled"
            self._doRange()
            self._doPop()

    def _doPop(self):
        bytes = self.stack.pop()
        nbr = len(bytes)
        # print "popped %d bytes" % nbr
        self.stackByteLength = self.stackByteLength - nbr

    def _doBitmask(self):
        combined = self._rangeValue("".join(self.stack) + self.buffer[self.pos])
        mask = ord(self.buffer[self.pos+1])
        self.pos = self.pos + 2

        blank = False
        lowValue = combined
        highValue = lowValue
        # print "doBitmask: start (start: %x)" % lowValue

        for x in xrange(8):
            bit = 1 << x
            # print "mask: %s; bit: %s" % (bin(mask), bin(bit))
            if blank:
                if (mask & bit) != 0:
                    blank = False
                    lowValue = combined + ((x + 1) << 40)
                    highValue = lowValue
                    # print "doBitmask: new record (ends: %x)" % lowValue
            else:
                if (mask & bit) == 0:
                    # print "doBitmask: commit: [%.12x:%.12x]" % (lowValue, highValue)
                    self.ranges.append((lowValue, highValue))
                    blank = True
                else:
                    highValue = combined + ((x + 1) << 40)
                    # print "doBitmask: extending range (highValue: %x)" % highValue

        if not blank:
            self.ranges.append((lowValue, highValue))

    def _doRange(self):
        nbr = 6 - self.stackByteLength
        
        combined = "".join(self.stack)
        if nbr > 0:
            # print "pair covering %d bytes" % nbr
            lowValue = self._rangeValue(combined
                                        + self.buffer[self.pos:self.pos+nbr])
            self.pos = self.pos + nbr
            highValue = self._rangeValue(combined
                                        + self.buffer[self.pos:self.pos+nbr])
            self.pos = self.pos + nbr
        elif nbr == 0:
            # print "singleton (implied)"
            lowValue = self._rangeValue(combined)
            highValue = lowValue
        else:
            raise Exception, "reached negative range count"

        # print "doRange: [%.12x:%.12x]" % (lowValue, highValue)

        self.ranges.append((lowValue, highValue))

    def _rangeValue(self, bytes):
        value = 0

        base = 1
        for x in bytes:
            value = value + ord(x) * base
            base = base * 256

        return value

    def _doEnd(self):
        # print "end reached"
        self.end = True
        if len(self.stack) > 0:
            raise Exception, "stack should be empty"

    def run(self):
        command_methods = { 0x01: self._doPush1,
                            0x02: self._doPush2,
                            0x03: self._doPush3,
                            0x04: self._doPush4,
                            0x05: self._doPush5,
                            0x06: self._doPush6,
                            0x50: self._doPop,
                            0x42: self._doBitmask,
                            0x52: self._doRange,
                            0x00: self._doEnd }

        while not self.end:
            command = ord(self.buffer[self.pos])
            if command_methods.has_key(command):
                self.pos = self.pos + 1
                command_method = command_methods[command]
                command_method()
            else:
                # print "buffer: %s..." % ["%.2x" % ord(x) for x in self.buffer[self.start_pos:self.pos]]
                # for x in self.ranges:
                #     print "[%.12x:%.12x]" % x
                raise Exception, "unknown command: %x at pos %d" % (command, self.pos)

        return (self.pos - self.start_pos)
