#!/usr/bin/python
#
# OpenChange debugging script
#
# Copyright (C) Julien Kerihuel <j.kerihuel@openchange.org> 2015
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#
# Usage example:
# ./script/fxdump.py - | ndrdump -l libmapi.so exchange_debug FastTransferSourceGetBuffer in
# [0000] 03 00 12 40 02 01 E1 65   00 00 00 00 02 01 E0 65   ...@...e .......e
# [0010] 16 00 00 00 53 64 9A AE   39 18 32 4A BB 4E 18 39   ....Sd.. 9.2J.N.9
# [0020] 52 F0 5C BD 00 00 00 00   03 F6 40 00 08 30 80 33   R.\..... ..@..0.3
# [0030] 89 D7 C5 AD D0 01 02 01   E2 65 14 00 00 00 A3 3D   ........ .e.....=
# [0040] 77 9F 1A 38 A4 40 A9 1B   E2 30 04 EE 43 BD 00 00   w..8.@.. .0..C...
# [0050] 04 1A 02 01 E3 65 2C 00   00 00 16 53 64 9A AE 39   .....e,. ...Sd..9
# [0060] 18 32 4A BB 4E 18 39 52   F0 5C BD 00 00 00 00 00   .2J.N.9R .\......
# [0070] 24 14 A3 3D 77 9F 1A 38   A4 40 A9 1B E2 30 04 EE   $..=w..8 .@...0..
# [0080] 43 BD 00 00 04 1A 1F 00   01 30 0E 00 00 00 4F 00   C....... .0....O.
# [0090] 75 00 74 00 62 00 6F 00   78 00 00 00 14 00 49 67   u.t.b.o. x.....Ig
# [00A0] 01 00 00 00 00 00 03 F4   03 00 39 66 FB 03 00 00   ........ ..9f....
# [00B0] 02 01 DA 36 06 00 00 00   01 04 00 00 10 00 40 00   ...6.... ......@.
# [00C0] 07 30 00 37 90 FE BE AD   D0 01 0B 00 F4 10 00 00   .0.7.... ........
# [00D0] 0B 00 F6 10 00 00 1F 00   13 36 12 00 00 00 49 00   ........ .6....I.
# [00E0] 50 00 46 00 2E 00 4E 00   6F 00 74 00 65 00 00 00   P.F...N. o.t.e...
# [00F0] 03 00 17 36 00 00 00 00   0B 00 0A 36 00 00 03 00   ...6.... ...6....
# [0100] 12 40 02 01 E1 65 00 00   00 00 02 01 E0 65 16 00   .@...e.. .....e..
# [0110] 00 00 53 64 9A AE 39 18   32 4A BB 4E 18 39 52 F0   ..Sd..9. 2J.N.9R.
# [0120] 5C BD 00 00 00 00 03 F7   40 00 08 30 00 CA 21 D8   \....... @..0..!.
# [0130] C5 AD D0 01 02 01 E2 65   14 00 00 00 A3 3D 77 9F   .......e .....=w.
# [0140] 1A 38 A4 40 A9 1B E2 30   04 EE 43 BD 00 00 04 1B   .8.@...0 ..C.....
# [0150] 02 01 E3 65 2C 00 00 00   16 53 64 9A AE 39 18 32   ...e,... .Sd..9.2
# [0160] 4A BB 4E 18 39 52 F0 5C   BD 00 00 00 00 00 26 14   J.N.9R.\ ......&.
# [0170] A3 3D 77 9F 1A 38 A4 40   A9 1B E2 30 04 EE 43 BD   .=w..8.@ ...0..C.
# [0180] 00 00 04 1B 1F 00 01 30   0A 00 00 00 53 00 65 00   .......0 ....S.e.
# [0190] 6E 00 74 00 00 00 14 00   49 67 01 00 00 00 00 00   n.t..... Ig......
# [01A0] 03 F4 03 00 39 66 FB 03   00 00 02 01 DA 36 06 00   ....9f.. .....6..
# [01B0] 00 00 01 04 00 00 10 00   40 00 07 30 00 37 90 FE   ........ @..0.7..
# [01C0] BE AD D0 01 0B 00 F4 10   00 00 0B 00 F6 10 00 00   ........ ........
# [01D0] 1F 00 13 36 12 00 00 00   49 00 50 00 46 00 2E 00   ...6.... I.P.F...
# [01E0] 4E 00 6F 00 74 00 65 00   00 00 03 00 17 36 00 00   N.o.t.e. .....6..
# [01F0] 00 00 0B 00 0A 36 00 00   03 00 3A 40 02 01 96 67   .....6.. ..:@...g
# [0200] 1C 00 00 00 53 64 9A AE   39 18 32 4A BB 4E 18 39   ....Sd.. 9.2J.N.9
# [0210] 52 F0 5C BD 04 00 00 00   00 52 00 24 01 31 50 00   R.\..... .R.$.1P.
# [0220] 03 00 17 40 39 00 00 00   53 64 9A AE 39 18 32 4A   ...@9... Sd..9.2J
# [0230] BB 4E 18 39 52 F0 5C BD   05 00 00 00 00 03 52 F5   .N.9R.\. ......R.
# [0240] FF 50 05 00 00 00 00 04   52 01 12 50 05 00 00 00   .P...... R..P....
# [0250] 00 04 52 22 93 50 05 00   00 00 01 04 52 02 05 50   ..R".P.. ....R..P
# [0260] 00 03 00 3B 40 03 00 14   40                       ...;@... @
# ^D ^D
# Marker          : IncrSyncChg (0x40120003)
#     PidTagParentSourceKey: SBinary cb=0
#     PidTagSourceKey: SBinary cb=22
#         [0000] 53 64 9A AE 39 18 32 4A   BB 4E 18 39 52 F0 5C BD   Sd..9.2J .N.9R.\.
#         [0010] 00 00 00 00 03 F6
#     PidTagLastModificationTime: 'mar jun 23 17:04:03 2015 CEST'
#     PidTagChangeKey: SBinary cb=20
#         [0000] A3 3D 77 9F 1A 38 A4 40   A9 1B E2 30 04 EE 43 BD   .=w..8.@ ...0..C.
#         [0010] 00 00 04 1A
#     PidTagPredecessorChangeList: SBinary cb=44
#         [0000] 16 53 64 9A AE 39 18 32   4A BB 4E 18 39 52 F0 5C   .Sd..9.2 J.N.9R.\
#         [0010] BD 00 00 00 00 00 24 14   A3 3D 77 9F 1A 38 A4 40   .Sd..9.2 J.N.9R.\
#         [0020] A9 1B E2 30 04 EE 43 BD   00 00 04 1A
#     PidTagDisplayName: 'Outbox'
#     PidTagParentFolderId: 0xf403000000000001 (-863846703525003263)
#     PidTagRights: 0x000003fb (1019)
#     PidTagExtendedFolderFlags: SBinary cb=6
#         [0000] 01 04 00 00 10 00  ......
#     PidTagCreationTime: 'mar jun 23 16:15:02 2015 CEST'
#     PidTagAttributeHidden: 'False'
#     PidTagAttributeReadOnly: 'False'
#     PidTagContainerClass: 'IPF.Note'
#     0x36170003               : 0x00000000 (0)
#     PidTagSubfolders: 'False'
# Marker          : IncrSyncChg (0x40120003)
#     PidTagParentSourceKey: SBinary cb=0
#     PidTagSourceKey: SBinary cb=22
#         [0000] 53 64 9A AE 39 18 32 4A   BB 4E 18 39 52 F0 5C BD   Sd..9.2J .N.9R.\.
#         [0010] 00 00 00 00 03 F7
#     PidTagLastModificationTime: 'mar jun 23 17:04:04 2015 CEST'
#     PidTagChangeKey: SBinary cb=20
#         [0000] A3 3D 77 9F 1A 38 A4 40   A9 1B E2 30 04 EE 43 BD   .=w..8.@ ...0..C.
#         [0010] 00 00 04 1B
#     PidTagPredecessorChangeList: SBinary cb=44
#         [0000] 16 53 64 9A AE 39 18 32   4A BB 4E 18 39 52 F0 5C   .Sd..9.2 J.N.9R.\
#         [0010] BD 00 00 00 00 00 26 14   A3 3D 77 9F 1A 38 A4 40   .Sd..9.2 J.N.9R.\
#         [0020] A9 1B E2 30 04 EE 43 BD   00 00 04 1B
#     PidTagDisplayName: 'Sent'
#     PidTagParentFolderId: 0xf403000000000001 (-863846703525003263)
#     PidTagRights: 0x000003fb (1019)
#     PidTagExtendedFolderFlags: SBinary cb=6
#         [0000] 01 04 00 00 10 00  ......
#     PidTagCreationTime: 'mar jun 23 16:15:02 2015 CEST'
#     PidTagAttributeHidden: 'False'
#     PidTagAttributeReadOnly: 'False'
#     PidTagContainerClass: 'IPF.Note'
#     0x36170003               : 0x00000000 (0)
#     PidTagSubfolders: 'False'
# Marker          : IncrSyncStateBegin (0x403A0003)
#     0x67960102               : SBinary cb=28
#         [0000] 53 64 9A AE 39 18 32 4A   BB 4E 18 39 52 F0 5C BD   Sd..9.2J .N.9R.\.
#         [0010] 04 00 00 00 00 52 00 24   01 31 50 00
#     MetaTagIdsetGiven:
##             [0xf50300000000:0xff0300000000]
#             [0x010400000000:0x120400000000]
#             [0x220400000000:0x930400000000]
#             [0x020401000000:0x050401000000]
# Marker          : IncrSyncStateEnd (0x403B0003)
#                            Marker          : IncrSyncEnd (0x40140003)
#
#

import fileinput
import sys
import re
from binascii import unhexlify

binary = ''
for line in fileinput.input():
    if len(line) == 1 and line[0] == '\n':
        break
    m = re.findall(r'[0-9A-Z]{2}\s+', line)
    hexdata =''.join(m[:16]).replace(' ', '').strip('\n')
    binary += unhexlify(hexdata)
print binary
