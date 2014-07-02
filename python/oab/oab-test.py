import unittest
import struct
from pprint import pprint

import sys
sys.path.append('.')

from oab import OAB

class TestOAB(unittest.TestCase):
    def _browse_entries(self, browseContents):
        entries = {}

        record_position = 12 # 12 is the header size
        while record_position < len(browseContents):
            entries[record_position] = 1
            record_position += 32 # 32 b size of browse entry

        print "Browse entries"
        pprint(entries)

        return entries

    def _check_rdn_consistence(self, browse_entries, rdnContents):
        rdnContentsSize = len(rdnContents)

        nAccounts = struct.unpack('<I', rdnContents[8:12])
        self.assertTrue(nAccounts > 0)

        # DDD
        oRootBytes = str(bytearray(rdnContents[12:16]))
        print 'oRootBytes=' + str(type(oRootBytes)) + ' -> '
        pprint(oRootBytes)
        # DDD

        oRoot = self._unpack_uint(rdnContents[12:16])
        print 'oRoot= ' + str(type(oRoot)) + ' -> ' + str(oRoot) # DDD
        self.assertTrue(oRoot > 16) # always will be more that 18(size header)



        pdnPart = rdnContents[16:oRoot]
        self.assertTrue(len(pdnPart) > 0)

        pdnByOffset = {}
        begSearch = 0
        pdnOffset = 16
        while begSearch < len(pdnPart):
             found = pdnPart.find(b'\x00', begSearch)
             if found == -1:
                 self.assertTrue(False, msg='Not NUL byte find in pdn search beginning at ' + begSearch)
                 break
             pdnBytes = pdnPart[begSearch:found] # we not use found +1 bz we are not interested in NUL byte
             pdnLen   =  len(pdnBytes) + 1 # +1 -> NUL
             pdn = str(pdnBytes)
             pdnByOffset[pdnOffset] = pdn

             pdnOffset += pdnLen
             begSearch = found + 1

        pprint(pdnByOffset)

        rdnPart = pdnPart[oRoot:] # DDD
        pprint (rdnPart) # DDD

        # checking using prev/next links
        # XXX degenerate tree; tree not checked
        prevLink = 0
        nextLink = oRoot


        while nextLink != 0:
            print "RDN with offset " + str(nextLink)

            iBrowse = self._unpack_uint(rdnContents[nextLink+8:nextLink+12])
            if iBrowse < 3:
                self.assertTrue(False, msg="iBrowse bad value " + str(iBrowse))
            iBrowse -= 3
            print "iBrowse: " + str(iBrowse) # XXX
            self.assertTrue(iBrowse in browse_entries)


            oPrev = self._unpack_uint(rdnContents[nextLink+12:nextLink+16])
            self.assertTrue(oPrev < rdnContentsSize)
            self.assertTrue(oPrev == prevLink)

            oNext = self._unpack_uint(rdnContents[nextLink+16:nextLink+20])
            self.assertTrue(oNext < rdnContentsSize)

            oParentDN = self._unpack_uint(rdnContents[nextLink+20:nextLink+24])
            print 'oParentDN ' + str(oParentDN)
            self.assertTrue(oParentDN in pdnByOffset)

            found = rdnContents.find(b'0', nextLink+24)
            rdnBytes  =  rdnContents[nextLink+24:found]
            rdn = str(rdnBytes)
            print 'RDN=' + rdn

            prevLink = nextLink
            nextLink = oNext

    def _check_anr_consistence(self, browse_entries, anr_contents):
        anr_contents_size = len(anr_contents)
        nAccounts = self._unpack_uint(anr_contents[8:12])
        print 'ANR nAccounts ' + str(nAccounts)
        self.assertTrue(nAccounts > 0)


        # checking using prev/next links
        # XXX degenerate tree; tree not checked
        prevLink = 0
        nextLink = 12 # 12b header
        while nextLink != 0:
            print "ANR with expected offset " + str(nextLink) + '  prev link ' + str(prevLink)

            iBrowse = self._unpack_uint(anr_contents[nextLink+8:nextLink+12])
            if iBrowse < 3:
                self.assertTrue(False, msg="iBrowse bad value " + str(iBrowse))
            iBrowse -= 3
            print "ANR iBrowse: " + str(iBrowse) # XXX
            self.assertTrue(iBrowse in browse_entries)

            oPrev = self._unpack_uint(anr_contents[nextLink+12:nextLink+16])
            print 'ANR oPrev: ' + str(oPrev)
            self.assertTrue(oPrev < anr_contents_size)
            self.assertTrue(oPrev == prevLink)

            oNext = self._unpack_uint(anr_contents[nextLink+16:nextLink+20])
            self.assertTrue(oNext < anr_contents_size)

            found = anr_contents.find(b'0', nextLink+20)
            anr_bytes  =  anr_contents[nextLink+20:found]
            anr = str(anr_bytes)
            print 'ANR=' + anr

            prevLink = nextLink
            nextLink = oNext


    def test_basic_accounts(self):
        accounts = [{'SendRichInfo': 1,
                     'dn': 'CN=asdvsdffv a,OU=\xd1\x80\xd1\x83\xd1\x81\xd0\xba\xd0\xb8,DC=zentyal-domain,DC=lan',
                     'mail': 'u2@zentyal-domain.lan',
                     'samAccountName': 'u2',
                     'sn': 'a',
                     'type': 'mailuser'},
                    {'SendRichInfo': 1,
                     'dn': 'CN=Administrator,CN=Users,DC=zentyal-domain,DC=lan',
                     'mail': 'administrator@zentyal-domain.lan',
                     'samAccountName': 'Administrator',
                     'sn': '-',
                     'type': 'mailuser'},
                    {'SendRichInfo': 1,
                     'dn': 'CN=sdsdsd dfg,CN=Users,DC=zentyal-domain,DC=lan',
                     'mail': 'u1@zentyal-domain.lan',
                     'samAccountName': 'u1',
                     'sn': 'dfg',
                     'type': 'mailuser'},
                    {'SendRichInfo': 1,
                     'dn': 'CN=Guest,CN=Users,DC=zentyal-domain,DC=lan',
                     'mail': 'guest@zentyal-domain.lan',
                     'samAccountName': 'Guest',
                     'sn': '-',
                     'type': 'mailuser'},
                    {'SendRichInfo': 1,
                     'dn': 'CN=a ad,CN=Users,DC=zentyal-domain,DC=lan',
                     'mail': 'ad@zentyal-domain.lan',
                     'samAccountName': 'ad',
                     'sn': 'ad',
                     'type': 'mailuser'}]

        oab = OAB()
        contents = oab.generateFileContents(accounts)
#        print "Contents generated"
#        pprint(contents)
        browse_entries = self._browse_entries(contents['browse'])
        self._check_rdn_consistence(browse_entries, contents['rdn'])
        self._check_anr_consistence(browse_entries, contents['anr'])


    def _unpack_uint(self, barray):
        bstr = str(bytearray(barray[0:4]))
        return struct.unpack('<I', bstr)[0]


if __name__ == '__main__':
    unittest.main()
