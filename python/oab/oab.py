import struct

# TESTY
#from samba.param import LoadParm
#from samba.auth import system_session
#from samba import samdb, ldb
from pprint import pprint


class OAB:
    MAX_UL_TOT_RECORDS = 16777212
    BROWSE_HEADER_SIZE = 12
    BROWSE_ENTRY_SIZE  = 32

    def __init__(self):
        pass

    def createFiles(self, accounts, directory):
#        contents = self._generateFileContents(accounts)
        pass

    def generateFileContents(self, accounts):
        nAccounts = len(accounts)
        if (nAccounts == 0):
            raise Exception('Trying to create OAB files without accounts');
        elif (nAccounts  > OAB.MAX_UL_TOT_RECORDS):
            # XXX remember to do the check fopr attr in individual files
            # XXX removing excessive accounts
            nAccounts = nAccounts[0:OAB.MAX_UL_TOT_RECORDS-1]

        browseFileIndex, browseFileContents = self._browseFileContents(accounts)
        rdnFileContents = self._rdnFileContents(accounts, browseFileIndex, browseFileContents)
        anrFileContents = self._anrFileContents(accounts, browseFileIndex, browseFileContents)

        self._generate_RDN_hash(browseFileContents, rdnFileContents, anrFileContents)
        # print '--------------------------'
        # print '# Browse file:'
        # pprint(browseFileContents)
        # print '# RDN file:'
        # pprint(rdnFileContents)
        # print '# ANR file:'
        # pprint(anrFileContents)
        # print '--------------------------'

        return { 'browse' : browseFileContents,
                 'rdn'    : rdnFileContents,
                 'anr'    : anrFileContents,
               }



    def _browseFileContents(self, accounts):
        contents = self._browseFileHeader(accounts)
        index    = {}
        for acc in accounts:
            print "Browse record " + acc['samAccountName'] + ' : ' + str(len(contents))
            record  = self._browseRecord(acc)
            index[acc['samAccountName']] = len(contents)
            contents += record

        # calculate RDN hash for put in browse file
        return (index, contents)

    def _browseFileHeader(self, accounts):
        header = bytearray(OAB.BROWSE_HEADER_SIZE)
        # ulVersion
        header[0] = 0x0E
        header[1] = 0x00
        # ulSerial: later
        # ulTotRec
        packedNAccounts = struct.pack('<I', len(accounts))
        header[4] = packedNAccounts[0]
        header[5] = packedNAccounts[1]
        header[6] = packedNAccounts[2]
        header[7] = packedNAccounts[3]
        return header

    def _browseRecord(self, account):
        record = bytearray(32)
        # oRDN 4 later..
        # oDetails 4 set to null for now XXX
        # cbDetails 2 set to null for now
        # bDispType
        if account['type'] == 'mailuser':
            record[10] = 0x0
        elif account['type'] == 'distlist':
            record[10] = 0x1
        else:
            raise Exception("Unknow account type when settinf bDispType " + record['type'])

        # a 1b 1 (can receive rich content)
        if account['SendRichInfo']:
            record[11] = 0x80
        else:
            record[11] = 0x00
        # mailobj (7b)
        if account['type'] == 'mailuser':
            record[11] += 0x06
        elif account['type'] == 'distlist':
            record[11] += 0x08
        elif account['type'] == 'folder':
            record[11] += 0x03
        else:
            raise Exception("Unknow account type when setting type in bObjType byte " + record['type'])
        # oSmtp (4 bytes)
        # oAlias (4 bytes): A 32-bit unsigned integer that specifies the offset of the alias record in the
        # ANR Index file.

        # oLocation (4 bytes): A 32-bit unsigned integer that specifies the offset of the office location
        # record in the ANR Index file.

        # oSurname (4 bytes): A 32-bit unsigned integer that specifies the offset of the surname record
        # in the ANR Index file.
        return record

    def _rdnFileContents(self, accounts, browseFileIndex, browseFileContents):
        contents = self._rdnHeader(accounts)

        pdn = self._rdnPdnRecords(accounts, len(contents))
        pprint(pdn)
        contents += pdn[1]
        offsetByPdn = pdn[0]

        # now we have the offset of the first RDN and we can set oRoot
        oRootPacked = self._pack_uint(len(contents))
        contents[12:16]  = oRootPacked[0:4]

        oPrev = 0;
        oNextBase = len(contents)
        lastAccount = len(accounts) - 1
        for i in range(0, lastAccount +1):
            acc = accounts[i]
            browse_offset = browseFileIndex[acc['samAccountName']]
            iBrowse = browse_offset  + 3 # iBrowse has 3 offset


            rdn, pdn = acc['dn'].split(',', 1)
            rdn = rdn.split('=', 1)[1]

            record = self._rdnRecord(rdn, pdn, offsetByPdn, oPrev, oNextBase, iBrowse)
            oPrev = len(contents)
            record_offset = len(contents)
            contents += record
            if i == lastAccount:
                oNextBase = 0
            else:
                oNextBase = len(contents)
            # add oRDN entry to browse file
            browseFileContents[browse_offset+0:browse_offset+4] = self._pack_uint(record_offset)

            rdn, pdn = acc['mail'].split('@', 1)
            rdn += '@'
            record = self._rdnRecord(rdn, pdn, offsetByPdn, oPrev, oNextBase, iBrowse)
            oPrev = len(contents)
            record_offset = len(contents)
            contents += record
            oNextBase = len(contents) # no effect if last account
            # add oSMTP entry to browse file
            browseFileContents[browse_offset+12:browse_offset+16] = self._pack_uint(record_offset)

        return contents


    def _rdnHeader(self, accounts):
        header = bytearray(16)
        # ulVersion
        header[0:4] = 0x0E, 0x00, 0x00, 0x00
        # ulSerial (4 bytes): later
        # ulTotRecs (4 bytes)
        nEntries = len(accounts)*2 # each account has a dn and mail attrs
        header[8:12] = self._pack_uint(nEntries)

        # oRoot (4 bytes): A 32-bit unsigned integer that specifies the offset of the root RDN2_REC
        # to be calculated and set later
        return header

        # returns the tuple (offsetByPdn, pdnRecordsByteArray)
    def _rdnPdnRecords(self, accounts, offset):
        offsetByPdn = {}
        records = bytearray();
        for acc in accounts:
            newPdns = []
            newPdns.append(acc['dn'].split(',', 1)[1])
            newPdns.append(acc['mail'].split('@', 1)[1])

            for pdn in newPdns:
                if pdn in offsetByPdn:
                    break
                pdnBytes = bytearray(pdn)
                pdnBytes.append(0x00);
                offsetByPdn[pdn] = offset;
                offset += len(pdnBytes)
                records += pdnBytes

        return (offsetByPdn, records)

    def _rdnRecord(self, rdn, pdn, offsetByPdn, oPrev, oNextBase, iBrowse):
        record = bytearray(24) # min size, RDN records are variable
        # XXX degenerate tree: oLT, rLT -> oPrev, oNext
        # oLT 4b
        record[0:4] = struct.pack('<I', oPrev)[0:4]
        # rLT 4b
        # iBrowse (4 bytes):
        record[8:12] = self._pack_uint(iBrowse)
        # oPrev (4 bytes)
        record[12:16] = struct.pack('<I', oPrev)[0:4]
        # oNext (4 bytes)
        # set later

        # oParentDN (4 bytes)
        print rdn + ' pdn: ' + pdn + ' offset ' + str(offsetByPdn[pdn]) + ' iBrowse ' + str(iBrowse)
        record[20:24] = self._pack_uint(offsetByPdn[pdn])

        # acKey (variable):
        record +=  bytearray(rdn) + b'0'

        oNext = 0
        if (oNextBase != 0):
            oNext = oNextBase + len(record)
        # rLT
        record[4:8] = struct.pack('<I', oNext)[0:4]
        # oNext
        record[16:20] = struct.pack('<I', oNext)[0:4]

        return record

    def _generate_RDN_hash(self, browse, rdn_index, anr_index):
        rdn_hash = bytearray(4) # 0x00000000 initial value
        browse_offset = OAB.BROWSE_HEADER_SIZE
        browse_len    = len(browse)
        while browse_offset < browse_len:
            oRDN = self._unpack_uint(browse[browse_offset:browse_offset+4])
            rdn_offset = oRDN + 24
            rdn_end    = rdn_index.find(b'\x00', rdn_offset)
            if rdn_end == -1:
                raise Error("Error generating RDN hash: cannot found RDN starting at " + len(rdn_offset))
            rdn_bytes = rdn_index[rdn_offset:rdn_end]
            # pad rdn_bytes so they are multiple of 4
            rdn_bytes_len = len(rdn_bytes)
            mod4 = rdn_bytes_len % 4
            if mod4 > 0:
                missing = (4 - mod4)
                pad =  '\x00' * missing
                rdn_bytes += bytearray(pad)
                rdn_bytes_len += missing

            uint_ptr = 0
            while uint_ptr < rdn_bytes_len:
                for offset in range (0, 4):
                    rdn_hash[offset] ^= rdn_bytes[uint_ptr + offset]
                uint_ptr += 4

            # left shift after processing entry
            # XX not sure, specially for caryover
            for nByte in (3, 2, 1, 0):
                if (nByte > 0) and (rdn_hash[nByte] >= 128):
                    # carry over
                     rdn_hash[nByte] -= 128
                     if rdn_hash[nByte -1] != 255:
                         rdn_hash[nByte -1] += 1
                     rdn_hash[nByte] <<= 1

            browse_offset += OAB.BROWSE_ENTRY_SIZE

        # set ulSerial
        browse[4:8]    = rdn_hash
        rdn_index[4:8] = rdn_hash
        anr_index[4:8] = rdn_hash


    def _anrFileContents(self, accounts,  browseFileIndex, browseFileContents):
        anr_attrs = ['displayName', 'sn', 'office', 'alias']
        attr_ibrowse_offset = {
            'displayName' : 16,
            'alias'       : 20,
            'office'      : 24,
            'sn'          : 28
        }
        nRecords = self._count_attributes(accounts, anr_attrs)
        contents = self._anrHeader(accounts, nRecords)

        oPrev = 0;
        oNextBase = 0
        next_record = 0
        for acc in accounts:
            for attr in anr_attrs:
                browse_offset = browseFileIndex[acc['samAccountName']]
                iBrowse =  browse_offset + 3 # iBrowse has 3 offs
                if not(attr in acc):
                    continue

                record_offset = len(contents)

                if ((next_record+1) == nRecords):
                    oNextBase = 0
                else:
                    oNextBase = record_offset

                attrValue = acc[attr]
                print "ANR record " + str(next_record) + ' '+ attr + ':' + str(attrValue) + ' offset: ' + str(len(contents)) +  ' oPrev: ' + str(oPrev) + ' nextBase: ' + str(oNextBase) + ' iBrowse: ' + str(iBrowse)

                record    = self._anrRecord(attrValue, oPrev, oNextBase, iBrowse, attr == 'alias')
                contents += record
                oPrev = record_offset
                next_record += 1

                # add entry to browse file
                browseFileOffset = browse_offset + attr_ibrowse_offset[attr]
                browseFileContents[browseFileOffset:browseFileOffset+4] = self._pack_uint(record_offset)

        return contents

    def _anrHeader(self, accounts, nRecords):
        header = bytearray(12)
        # ulVersion (4 bytes):
        header[0:4] = self._pack_uint(0x0000000E)
        # ulSerial (4 bytes)
        # to be calculated later
        # ulTotRecs (4 bytes):
        # XXX attr for alias

        header[8:12] = self._pack_uint(nRecords)

        return header

    def _anrRecord(self, attrValue, oPrev, oNextBase, iBrowse, alias):
        record = bytearray(20) # 20 is only the fixed  bytes
        # XXX degenerate tree: oLT, oGT -> oPrev, oNext
        record[0:4] = self._pack_uint(oPrev) #oLt
        # oGT (4 bytes) , later
        # iBrowse 3b
        record[8:12] = self._pack_uint(iBrowse)
        # a/b 1b
        ab = 0x00
        if alias:
            ab = 0x80
        record[11] = ab
        record[12:16] = self._pack_uint(oPrev) # oPrev
        # oNext (4 bytes), later
        # acKey (variable)
        record +=  bytearray(attrValue) + b'0'

        oNext = 0
        if (oNextBase != 0):
            oNext = oNextBase + len(record)
        # rLT
        record[4:8] = self._pack_uint(oNext)
        # oNext
        record[16:20] = self._pack_uint(oNext)

        return record

    def _pack_uint(self, uint):
        return struct.pack('<I', uint)

    def _unpack_uint(self, barray):
        bstr = str(bytearray(barray[0:4]))
        return struct.unpack('<I', bstr)[0]

    def _count_attributes(self, accounts, attrs):
        count = 0
        for acc in accounts:
            for attr in attrs:
                if attr in acc:
                    count += 1
        return count

    def endClass():
        pass




def accountsList():
    samdb_url = '/var/lib/samba/private/sam.ldb'
    db = samdb.SamDB(url=samdb_url, session_info=system_session(), lp=LoadParm())

    accounts = []
    basedn =  db.domain_dn()
    res = db.search(base=basedn, scope=ldb.SCOPE_SUBTREE, expression="(|(objectclass=user))(objectclass=group)))")

    # XXX add subnames, alias, office

    for entry in res:
        mailAttr =  entry.get('mail')
        if not mailAttr:
            continue

        account = {}
        account['mail'] = mailAttr.get(0)
        account['dn'] = str(entry.dn)
        account_type = ''
        for oclass in entry.get('objectclass'):
            if oclass == "user":
                account_type = 'mailuser'
                break;
            elif oclass == "group":
                account_type = 'distlist'
                break

            if account_type == '':
                # Not valid objectclass!
                continue

        account['type'] = account_type
        account['samAccountName']= entry.get('samAccountName').get(0)
        account['displayName'] = account['samAccountName']
        account['SendRichInfo'] = 1 # for now always on
        sn =  entry.get('sn')
        if sn:
            account['sn'] = sn.get(0)
        office = entry.get('physicalDeliveryOfficeName')
        if office:
            account['office'] = office.get(0)
        # XXX TODO alias mail otherMailbox?
        # account['alias']
        accounts.append(account)

    return accounts


# # test code
# accounts = accountsList()
# pprint(accounts)
# oab = OAB()
# oab.createFiles(accounts, '/tmp')
