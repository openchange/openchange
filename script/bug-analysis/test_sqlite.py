"""
Unit tests over SQLite backend for Crash Database
"""
from apport.report import Report
import os
from unittest import TestCase
from sqlite import CrashDatabase


class CrashDatabaseTestCase(TestCase):

    def setUp(self):
        self.crash_base = os.path.sep + 'tmp'
        self.crash_base_url = 'file://' + self.crash_base + '/'
        self.crash_path = os.path.join(self.crash_base, 'test.crash')
        self.r = Report()
        self.r['ExecutablePath'] = '/usr/bin/napoleon-solod'
        self.r['Package'] = 'libnapoleon-solo1 1.2-1'
        self.r['Signal'] = '11'
        self.r['StacktraceTop'] = """foo_bar (x=2) at crash.c:28
d01 (x=3) at crash.c:29
raise () from /lib/libpthread.so.0
<signal handler called>
__frob (x=4) at crash.c:30"""

    def tearDown(self):
        if os.path.exists(self.crash_path):
            os.unlink(self.crash_path)
        exe_crash_base = os.path.join(self.crash_base, '1_usr_bin_napoleon-solod')
        if os.path.exists(exe_crash_base):
            os.unlink(exe_crash_base)

    def test_create_db_default(self):
        try:
            CrashDatabase(None, {})
            self.assertTrue(os.path.isfile(os.path.expanduser('~/crashdb.sqlite')))
        finally:
            os.unlink(os.path.expanduser('~/crashdb.sqlite'))

    def test_crashes_base_url(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        self.assertEqual(cb.base_url, self.crash_base_url)

    def test_crashes_base_url_is_none(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        self.assertIsNone(cb.base_url)

    def test_upload_download(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})

        crash_id = cb.upload(self.r)
        self.assertEqual(crash_id, 1)

        report = cb.download(1)
        self.assertIsInstance(report, Report)
        self.assertIn('Signal', report)
        self.assertEqual(report['Signal'], '11')

    def test_upload_suggested_name(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})

        crash_id = cb.upload(self.r, suggested_file_name='paca.crash')
        self.assertEqual(crash_id, 1)
        self.assertTrue(os.path.isfile(os.path.join(self.crash_base, 'paca.crash')))

    def test_failed_upload_no_URL(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        self.assertRaises(ValueError, cb.upload, self.r)

    def test_failed_upload_invalid_URL_scheme(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})

        self.r['_URL'] = 'invalid://scheme/path'
        self.assertRaises(ValueError, cb.upload, self.r)

    def test_failed_download(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        self.assertRaises(Exception, cb.download, 23232)

    def test_get_id_url(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})
        self.assertEqual("#1", cb.get_id_url(None, 1))
        self.assertEqual("#1: napoleon-solod crashed with SIGSEGV in foo_bar()",
                         cb.get_id_url(self.r, 1))

    def test_update(self):
        """
        Test complete update
        """
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)

        self.r['SourcePackage'] = 'adios'
        self.r['Signal'] = u'9'
        cb.update(crash_id, self.r, 'a comment to add')

        report = cb.download(crash_id)
        self.assertIn('SourcePackage', report)
        self.assertEqual(report['Signal'], u'9')

    def test_update_with_key_filter(self):
        """
        Test a partial update
        """
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)

        self.r['SourcePackage'] = 'adios'
        self.r['Signal'] = u'9'
        cb.update(crash_id, self.r, 'a comment to add', key_filter=('Package', 'SourcePackage'))

        report = cb.download(crash_id)
        self.assertIn('SourcePackage', report)
        self.assertNotEqual(report['Signal'], u'9')

    def test_failed_update_no_URL(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:'})

        self.r['_URL'] = self.crash_base_url + 'test.crash'
        crash_id = cb.upload(self.r)

        del self.r['_URL']
        self.assertRaises(ValueError, cb.update, *(crash_id, self.r, 'comment'))

    def test_get_distro_release(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)

        self.assertIsNone(cb.get_distro_release(crash_id))

        self.r['DistroRelease'] = 'Ubuntu 14.04'
        crash_id = cb.upload(self.r)
        self.assertEqual(cb.get_distro_release(crash_id), 'Ubuntu 14.04')

    def test_get_unretraced(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        self.assertEqual(cb.get_unretraced(), [])

        crash_id = cb.upload(self.r)
        self.assertEqual(cb.get_unretraced(), [crash_id])

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
        cb.update(crash_id, self.r, "")
        self.assertEqual(cb.get_unretraced(), [])

        self.r['Stacktrace'] = """#8  0x00007ff5aae8e159 in ldb_msg_find_ldb_val (msg=<optimised out>, attr_name=<optimised out>) at ../common/ldb_msg.c:399
        el = <optimised out>
#9  0x00007ff5aae8e669 in ldb_msg_find_attr_as_string (msg=<optimised out>, attr_name=<optimised out>, default_value=0x0) at ../common/ldb_msg.c:584
        v = <optimised out>
#10 0x00007ff5905d0e5f in ?? ()
No symbol table info available.
#11 0x0000000000000081 in ?? ()
No symbol table info available.
#12 0x0000000000000000 in ?? ()
No symbol table info available."""
        cb.update(crash_id, self.r, "")
        self.assertEqual(cb.get_unretraced(), [crash_id])

    def test_get_unfixed(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        self.assertEqual(cb.get_unfixed(), set())

        crash_id = cb.upload(self.r)
        self.assertEqual(cb.get_unfixed(), set([crash_id]))

        cb.close_duplicate(self.r, crash_id, crash_id)
        self.assertEqual(cb.get_unfixed(), set())

    def test_close_duplicate(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)
        self.assertIsNone(cb.duplicate_of(crash_id))

        crash_id2 = cb.upload(self.r)
        self.assertIsNone(cb.duplicate_of(crash_id2))

        cb.close_duplicate(self.r, crash_id2, crash_id)
        self.assertEqual(cb.duplicate_of(crash_id2), crash_id)

        # Remove current duplicate thing
        cb.close_duplicate(self.r, crash_id2, None)
        self.assertIsNone(cb.duplicate_of(crash_id2))

    # Tests related with components
    def test_app_components_get_set(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)

        self.assertEqual(cb.get_app_components(crash_id), [])

        cb.set_app_components(crash_id, ['sand'])
        self.assertEqual(cb.get_app_components(crash_id), ['sand'])

        cb.set_app_components(crash_id, ['sand'])
        self.assertEqual(cb.get_app_components(crash_id), ['sand'])

    def test_app_components_remove(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)

        self.assertRaises(ValueError, cb.remove_app_component, *(crash_id, 'sand'))
        self.assertIsNone(cb.remove_app_component(crash_id))

        cb.set_app_components(crash_id, ['sand'])
        self.assertIsNone(cb.remove_app_component(crash_id, 'sand'))
        self.assertEqual(cb.get_app_components(crash_id), [])

        cb.set_app_components(crash_id, ['sand'])
        self.assertIsNone(cb.remove_app_component(crash_id))
        self.assertEqual(cb.get_app_components(crash_id), [])

    # Tests related with client side duplicates
    def test_client_side_duplicates_get_add(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)

        self.assertEqual(cb.get_client_side_duplicates(crash_id), [])
        self.assertEqual(cb.n_client_side_duplicates(crash_id), 0)

        cb.add_client_side_duplicate(crash_id, 'file:///foo')
        self.assertEqual(cb.get_client_side_duplicates(crash_id), ['file:///foo'])
        self.assertEqual(cb.n_client_side_duplicates(crash_id), 1)

        cb.add_client_side_duplicate(crash_id, 'https://foobar.org')
        self.assertEqual(cb.get_client_side_duplicates(crash_id), ['file:///foo', 'https://foobar.org'])
        self.assertEqual(cb.n_client_side_duplicates(crash_id), 2)

    def test_client_side_duplicates_remove(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)

        self.assertRaises(ValueError, cb.remove_client_side_duplicate, *(crash_id, 'file:///foo'))
        self.assertIsNone(cb.remove_client_side_duplicate(crash_id))

        cb.add_client_side_duplicate(crash_id, 'file:///foo')
        self.assertIsNone(cb.remove_client_side_duplicate(crash_id, 'file:///foo'))
        self.assertEqual(cb.get_client_side_duplicates(crash_id), [])

        cb.add_client_side_duplicate(crash_id, 'file:///foo')
        self.assertIsNone(cb.remove_client_side_duplicate(crash_id))
        self.assertEqual(cb.get_client_side_duplicates(crash_id), [])
        self.assertEqual(cb.n_client_side_duplicates(crash_id), 0)

    def test_set_get_tracker_url(self):
        cb = CrashDatabase(None, {'dbfile': ':memory:', 'crashes_base_url': self.crash_base_url})
        crash_id = cb.upload(self.r)

        self.assertIsNone(cb.get_tracker_url(crash_id))
        self.assertIsNone(cb.get_tracker_url(22))

        self.assertIsNone(cb.set_tracker_url(crash_id, 'https://tracker.org/122'))
        self.assertEqual(cb.get_tracker_url(crash_id), 'https://tracker.org/122')
