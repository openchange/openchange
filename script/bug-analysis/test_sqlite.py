"""
Unit tests over SQLite backend for Crash Database
"""
from apport.report import Report
import os
from unittest import TestCase
from sqlite import CrashDatabase


class CrashDatabaseTestCase(TestCase):

    def setUp(self):
        self.r = Report()
        self.r['Package'] = 'libnapoleon-solo1 1.2-1'
        self.r['Signal'] = '11'
        self.r['StacktraceTop'] = """foo_bar (x=2) at crash.c:28
d01 (x=3) at crash.c:29
raise () from /lib/libpthread.so.0
<signal handler called>
__frob (x=4) at crash.c:30"""

    def test_create_db_default(self):
        try:
            CrashDatabase(None, {})
            self.assertTrue(os.path.isfile(os.path.expanduser('~/crashdb.sqlite')))
        finally:
            os.unlink(os.path.expanduser('~/crashdb.sqlite'))

    def test_upload_download(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})

        crash_id = cb.upload(self.r)
        self.assertEqual(crash_id, 1)

        report = cb.download(1)
        self.assertIsInstance(report, Report)
        self.assertIn('Signal', report)
        self.assertEqual(report['Signal'], '11')

    def test_failed_download(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        self.assertRaises(Exception, cb.download, 23232)

    def test_unimplemented(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        args = [1, 2]
        for func_name in ('get_comment_url', 'get_id_url'):
            func = getattr(cb, func_name)
            self.assertIsNone(func(*args))

    def test_update(self):
        """
        Test complete update
        """
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        crash_id = cb.upload(self.r)

        self.r['SourcePackage'] = 'adios'
        self.r['Signal'] = '9'
        cb.update(crash_id, self.r, 'a comment to add')

        report = cb.download(crash_id)
        self.assertIn('SourcePackage', report)
        self.assertEqual(report['Signal'], '9')

    def test_update_with_key_filter(self):
        """
        Test a partial update
        """
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        crash_id = cb.upload(self.r)

        self.r['SourcePackage'] = 'adios'
        self.r['Signal'] = '9'
        cb.update(crash_id, self.r, 'a comment to add', key_filter=('Package', 'SourcePackage'))

        report = cb.download(crash_id)
        self.assertIn('SourcePackage', report)
        self.assertNotEqual(report['Signal'], '9')

    def test_get_distro_release(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        crash_id = cb.upload(self.r)

        self.assertIsNone(cb.get_distro_release(crash_id))

        self.r['DistroRelease'] = 'Ubuntu 14.04'
        crash_id = cb.upload(self.r)
        self.assertEqual(cb.get_distro_release(crash_id), 'Ubuntu 14.04')

    def test_get_unretraced(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        self.assertEqual(cb.get_unretraced(), [])

        crash_id = cb.upload(self.r)
        self.assertEqual(cb.get_unretraced(), [1])

        self.r['Stacktrace'] = """
 #0  0x00007f96dcfb9f77 in __GI_raise (sig=sig@entry=6) at ../nptl/sysdeps/unix/sysv/linux/raise.c:56
         resultvar = 0
         pid = 1427
         selftid = 1427
 #1  0x00007f96dcfbd5e8 in __GI_abort () at abort.c:90
         save_stage = 2
         act = {__sigaction_handler = {sa_handler = 0x0, sa_sigaction = 0x0}, sa_mask = {__val = {140286034336064, 140285996709792, 140285998988405, 5, 0, 752786625060479084, 140285929102568, 140285994568476, 140285996709792, 140285459489344, 140285999015717, 140285994520128, 140285996776629, 140285996776368, 140733249635424, 6}}, sa_flags = 56247888, sa_restorer = 0x18}
         sigs = {__val = {32, 0 <repeats 15 times>}}
 #2  0x00007f96e0deccbc in smb_panic_default (why=0x7f96e0df8b1c "internal error") at ../lib/util/fault.c:149
 No locals.
 #3  smb_panic (why=why@entry=0x7f96e0df8b1c "internal error") at ../lib/util/fault.c:162
 No locals.
 #4  0x00007f96e0dece76 in fault_report (sig=<optimized out>) at ../lib/util/fault.c:77
         counter = 1
 #5  sig_fault (sig=<optimized out>) at ../lib/util/fault.c:88
 No locals.
 #6  <signal handler called>
 No locals.
 #7  0x00007f96b9bae711 in sarray_get_safe (indx=<optimized out>, array=<optimized out>) at /build/buildd/gcc-4.8-4.8.1/src/libobjc/objc-private/sarray.h:237
 No locals.
 #8  objc_msg_lookup (receiver=0x7f96e3485278, op=0x7f96c0fae240 <_OBJC_SELECTOR_TABLE+128>) at /build/buildd/gcc-4.8-4.8.1/src/libobjc/sendmsg.c:448
 No locals.
 #9  0x00007f96c0da737a in sogo_table_get_row (table_object=<optimized out>, mem_ctx=0x7f96e33e5940, query_type=MAPISTORE_PREFILTERED_QUERY, row_id=1, data=0x7fff035a4e00) at MAPIStoreSOGo.m:1464
         e = <optimized out>
         ret = MAPISTORE_SUCCESS
         wrapper = <optimized out>
         pool = 0x7f96e3485278
         table = <optimized out>
         rc = 0
         __FUNCTION__ = "sogo_table_get_row"
         __PRETTY_FUNCTION__ = "sogo_table_get_row"
"""
        crash_id = cb.update(1, self.r, "")
        self.assertIsNotNone(crash_id)
        self.assertEqual(cb.get_unretraced(), [])

    def test_close_duplicate(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        crash_id = cb.upload(self.r)
        self.assertIsNone(cb.duplicate_of(crash_id))

        crash_id2 = cb.upload(self.r)
        self.assertIsNone(cb.duplicate_of(crash_id2))

        cb.close_duplicate(self.r, crash_id2, crash_id)
        self.assertEqual(cb.duplicate_of(crash_id2), crash_id)

        # Remove current duplicate thing
        cb.close_duplicate(self.r, crash_id2, None)
        self.assertIsNone(cb.duplicate_of(crash_id2))
