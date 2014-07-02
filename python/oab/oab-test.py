import unittest
import struct
from pprint import pprint

import sys
sys.path.append('.')

from oab import OAB

class TestOAB(unittest.TestCase):
    def _browse_entries(self, browse):
        entries = {}

        offset = 12 # 12 is the header size
        browse_len = len(browse)
        # check that it has the correct size
        self.assertEquals((browse_len-12)%32, 0)

        while offset < browse_len:
            entry = {}
            entry['oRDN']      = self._unpack_uint(browse[offset:offset+4])
            entry['oDetails']  = self._unpack_uint(browse[offset+4:offset+8])
            entry['cbDetails'] = self._unpack_ushort(browse[offset+8:offset+10])
            entry['bDispType'] = self._unpack_uchar(browse[offset+10:offset+11])
            entry['SendRichInfo'] = browse[offset+11] & 0x80
            if browse[offset + 11] & 0x06:
                entry['type'] = 'mailuser'
            elif browse[offset + 11] & 0x08:
                entry['type'] = 'distlist'
            elif browse[offset + 11] & 0x03:
                entry['type'] = 'folder'
            else:
                self.assertTrue(False, 'Bad binary value in browse entry type:' + str(browse[11]))
            entry['oSMTP']  = self._unpack_uint(browse[offset+12:offset+16])
            entry['oDispName']  = self._unpack_uint(browse[offset+16:offset+20])
            entry['oAlias']  = self._unpack_uint(browse[offset+20:offset+24])
            entry['oLocation']  = self._unpack_uint(browse[offset+24:offset+28])
            entry['oSurname']  = self._unpack_uint(browse[offset+28:offset+32])

            entries[offset] = entry
            offset += 32 # 32 b size of browse entry

        return entries

    def _check_rdn_consistence(self, browse_entries, rdnContents):
        entry_by_offset = {}
        rdnContentsSize = len(rdnContents)

        nAccounts = struct.unpack('<I', rdnContents[8:12])
        self.assertTrue(nAccounts > 0)

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

            oLT = self._unpack_uint(rdnContents[nextLink+0:nextLink+4])
            oGT = self._unpack_uint(rdnContents[nextLink+4:nextLink+8])

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

            entry_by_offset[nextLink] = {
                'oLT' : oLT,
                'oGT' : oGT,
                'iBrowse' : iBrowse,
                'oParentDN' : oParentDN,
                'oNext'    : oNext,
                'oPrev'    : oPrev,
                'rdn' : rdn
            }

            prevLink = nextLink
            nextLink = oNext

        return entry_by_offset

    def _check_anr_consistence(self, browse_entries, anr_contents):
        entry_by_offset = {}
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

            oLT = self._unpack_uint(anr_contents[nextLink+0:nextLink+4])
            oGT = self._unpack_uint(anr_contents[nextLink+4:nextLink+8])

            iBrowse = self._unpack_uint(anr_contents[nextLink+8:nextLink+12])
            if iBrowse < 3:
                self.assertTrue(False, msg="iBrowse bad value " + str(iBrowse))
            iBrowse -= 3
            self.assertTrue(iBrowse in browse_entries)

            oPrev = self._unpack_uint(anr_contents[nextLink+12:nextLink+16])
            self.assertTrue(oPrev < anr_contents_size)
            self.assertTrue(oPrev == prevLink)

            oNext = self._unpack_uint(anr_contents[nextLink+16:nextLink+20])
            self.assertTrue(oNext < anr_contents_size)

            found = anr_contents.find(b'0', nextLink+20)
            anr_bytes  =  anr_contents[nextLink+20:found]
            anr = str(anr_bytes)
#            print 'ANR=' + anr

            entry_by_offset[nextLink] = {
                'oLT' : oLT,
                'oGT' : oGT,
                'iBrowse' : iBrowse,
                'oPrev' : oPrev,
                'oNext': oNext,
                'anr'  : anr
            }

            prevLink = nextLink
            nextLink = oNext

        return entry_by_offset

    def _check_browse_references(self, browse_entries, rdn_by_offset, anr_by_offset):
        for offset in browse_entries:
            browse_entry = browse_entries[offset]
            for rdnPointer in ['oRDN', 'oSMTP']:
                if (rdnPointer in browse_entry) and (browse_entry[rdnPointer] != 0):
                    rdn_offset = browse_entry[rdnPointer]
                    self.assertTrue(rdn_offset in  rdn_by_offset, msg='Not found pointer for ' + rdnPointer)
                    self.assertEquals(offset, rdn_by_offset[rdn_offset]['iBrowse'])
            for anrPointer in ['oDispName', 'oAlias', 'oLocation', 'oSurname']:
                if (anrPointer in browse_entry) and (browse_entry[anrPointer] != 0):
                    anr_offset = browse_entry[anrPointer]
                    self.assertTrue(anr_offset in anr_by_offset, msg='Mot found pointer for ' + anrPointer)
                    self.assertEquals(offset, anr_by_offset[anr_offset]['iBrowse'])


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
#        print "Browse by offset"
#        pprint(browse_entries)

        rdn_by_offset = self._check_rdn_consistence(browse_entries, contents['rdn'])
#        print "RDN by offset"
#        pprint(rdn_by_offset)

        anr_by_offset = self._check_anr_consistence(browse_entries, contents['anr'])
#        print "ANR by offset"
#        pprint(anr_by_offset)
        self._check_browse_references(browse_entries, rdn_by_offset, anr_by_offset)


    def _unpack_uint(self, barray):
        bstr = str(bytearray(barray[0:4]))
        return struct.unpack('<I', bstr)[0]

    def _unpack_ushort(self, barray):
        bstr = str(bytearray(barray[0:2]))
        return struct.unpack('<H', bstr)[0]

    def _unpack_uchar(self, barray):
        bstr = str(barray[0])
        return struct.unpack('<B', bstr)

if __name__ == '__main__':
    unittest.main()
