#
# This module defines unittest test-cases for the class
# module_sources.SourcesSet
#
import unittest
import tempfile
import os
import StringIO

import module_sources
import util

class TestCaseSourcesSet(unittest.TestCase):
    """
    This class tests adding sources to class module_sources.SourcesSet
    with different options.
    """

    def setUp(self):
        """
        Creates a temporary filename and the temporary directory in
        which the sources.* files will be written.
        """
        if not (os.path.exists(tempfile.gettempdir())):
            os.mkdir(tempfile.gettempdir())
        self.__tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        """
        Removes the temporary directory.
        """
        os.rmdir(self.__tmpdir)

    def tmpdir(self): return self.__tmpdir

    def testNoSources(self):
        s = module_sources.SourcesSet()
        self.assertEqual(s.all(), [])
        self.assertEqual(s.nopch(), [])
        self.assertEqual(s.pch(), [])
        self.assertEqual(s.pch_system_includes(), [])
        self.assertEqual(s.pch_jumbo(), [])

    def testAddSources(self):
        s = module_sources.SourcesSet()
        s.addSourceFile("000.c", pch=False, jumbo=False, system_includes=False)
        s.addSourceFile("001.c", pch=False, jumbo=False, system_includes=True)
        s.addSourceFile("010.c", pch=False, jumbo=True, system_includes=False)
        s.addSourceFile("011.c", pch=False, jumbo=True, system_includes=True)
        s.addSourceFile("100.c", pch=True, jumbo=False, system_includes=False)
        s.addSourceFile("101.c", pch=True, jumbo=False, system_includes=True)
        s.addSourceFile("110.c", pch=True, jumbo=True, system_includes=False)
        s.addSourceFile("111.c", pch=True, jumbo=True, system_includes=True)
        self.assertEqual(s.all(), ["000.c", "001.c", "010.c", "011.c", "100.c", "101.c", "110.c", "111.c"])
        self.assertEqual(s.nopch(), ["000.c", "001.c", "010.c", "011.c", "111.c"])
        self.assertEqual(s.pch(), ["100.c"])
        self.assertEqual(s.pch_system_includes(), ["101.c"])
        self.assertEqual(s.pch_jumbo(), ["110.c"])

    def testExtendNoTypeModule(self):
        s1 = module_sources.SourcesSet()
        s1.addSourceFile("a000.c", pch=False, jumbo=False, system_includes=False)
        s1.addSourceFile("a001.c", pch=False, jumbo=False, system_includes=True)
        s1.addSourceFile("a010.c", pch=False, jumbo=True, system_includes=False)
        s1.addSourceFile("a011.c", pch=False, jumbo=True, system_includes=True)
        s1.addSourceFile("a100.c", pch=True, jumbo=False, system_includes=False)
        s1.addSourceFile("a101.c", pch=True, jumbo=False, system_includes=True)
        s1.addSourceFile("a110.c", pch=True, jumbo=True, system_includes=False)
        s1.addSourceFile("a111.c", pch=True, jumbo=True, system_includes=True)

        s = module_sources.SourcesSet()
        # extend an empty set without specifying type and module, so
        # no path is prepended to the source files:
        s.extend(s1)
        self.assertEqual(s.all(), ["a000.c", "a001.c", "a010.c", "a011.c", "a100.c", "a101.c", "a110.c", "a111.c"])
        self.assertEqual(s.nopch(), ["a000.c", "a001.c", "a010.c", "a011.c", "a111.c"])
        self.assertEqual(s.pch(), ["a100.c"])
        self.assertEqual(s.pch_system_includes(), ["a101.c"])
        self.assertEqual(s.pch_jumbo(), ["a110.c"])

    def testExtendType(self):
        s1 = module_sources.SourcesSet()
        s1.addSourceFile("a000.c", pch=False, jumbo=False, system_includes=False)
        s1.addSourceFile("a001.c", pch=False, jumbo=False, system_includes=True)
        s1.addSourceFile("a010.c", pch=False, jumbo=True, system_includes=False)
        s1.addSourceFile("a011.c", pch=False, jumbo=True, system_includes=True)
        s1.addSourceFile("a100.c", pch=True, jumbo=False, system_includes=False)
        s1.addSourceFile("a101.c", pch=True, jumbo=False, system_includes=True)
        s1.addSourceFile("a110.c", pch=True, jumbo=True, system_includes=False)
        s1.addSourceFile("a111.c", pch=True, jumbo=True, system_includes=True)

        s = module_sources.SourcesSet()
        # extend an empty set with specifying a type but no module, so
        # only the type is prepended to the source file's paths:
        s.extend(s1, type='foo')
        self.assertEqual(s.all(), ["foo/a000.c", "foo/a001.c", "foo/a010.c", "foo/a011.c", "foo/a100.c", "foo/a101.c", "foo/a110.c", "foo/a111.c"])
        self.assertEqual(s.nopch(), ["foo/a000.c", "foo/a001.c", "foo/a010.c", "foo/a011.c", "foo/a111.c"])
        self.assertEqual(s.pch(), ["foo/a100.c"])
        self.assertEqual(s.pch_system_includes(), ["foo/a101.c"])
        self.assertEqual(s.pch_jumbo(), ["foo/a110.c"])

    def testExtendModule(self):
        s1 = module_sources.SourcesSet()
        s1.addSourceFile("a000.c", pch=False, jumbo=False, system_includes=False)
        s1.addSourceFile("a001.c", pch=False, jumbo=False, system_includes=True)
        s1.addSourceFile("a010.c", pch=False, jumbo=True, system_includes=False)
        s1.addSourceFile("a011.c", pch=False, jumbo=True, system_includes=True)
        s1.addSourceFile("a100.c", pch=True, jumbo=False, system_includes=False)
        s1.addSourceFile("a101.c", pch=True, jumbo=False, system_includes=True)
        s1.addSourceFile("a110.c", pch=True, jumbo=True, system_includes=False)
        s1.addSourceFile("a111.c", pch=True, jumbo=True, system_includes=True)

        s = module_sources.SourcesSet()
        # extend an empty set with specifying a module but no type, so
        # only the module is prepended to the source file's paths:
        s.extend(s1, module='foo')
        self.assertEqual(s.all(), ["foo/a000.c", "foo/a001.c", "foo/a010.c", "foo/a011.c", "foo/a100.c", "foo/a101.c", "foo/a110.c", "foo/a111.c"])
        self.assertEqual(s.nopch(), ["foo/a000.c", "foo/a001.c", "foo/a010.c", "foo/a011.c", "foo/a111.c"])
        self.assertEqual(s.pch(), ["foo/a100.c"])
        self.assertEqual(s.pch_system_includes(), ["foo/a101.c"])
        self.assertEqual(s.pch_jumbo(), ["foo/a110.c"])

    def testExtend(self):
        s1 = module_sources.SourcesSet()
        s1.addSourceFile("a000.c", pch=False, jumbo=False, system_includes=False)
        s1.addSourceFile("a001.c", pch=False, jumbo=False, system_includes=True)
        s1.addSourceFile("a010.c", pch=False, jumbo=True, system_includes=False)
        s1.addSourceFile("a011.c", pch=False, jumbo=True, system_includes=True)
        s1.addSourceFile("a100.c", pch=True, jumbo=False, system_includes=False)
        s1.addSourceFile("a101.c", pch=True, jumbo=False, system_includes=True)
        s1.addSourceFile("a110.c", pch=True, jumbo=True, system_includes=False)
        s1.addSourceFile("a111.c", pch=True, jumbo=True, system_includes=True)

        s = module_sources.SourcesSet()
        # extend an empty set with specifying type and module, so
        # both type and module are prepended to the source files:
        s.extend(s1, type='foo', module='bar')
        self.assertEqual(s.all(), ["foo/bar/a000.c", "foo/bar/a001.c", "foo/bar/a010.c", "foo/bar/a011.c", "foo/bar/a100.c", "foo/bar/a101.c", "foo/bar/a110.c", "foo/bar/a111.c"])
        self.assertEqual(s.nopch(), ["foo/bar/a000.c", "foo/bar/a001.c", "foo/bar/a010.c", "foo/bar/a011.c", "foo/bar/a111.c"])
        self.assertEqual(s.pch(), ["foo/bar/a100.c"])
        self.assertEqual(s.pch_system_includes(), ["foo/bar/a101.c"])
        self.assertEqual(s.pch_jumbo(), ["foo/bar/a110.c"])

    def testExtend2(self):
        s1 = module_sources.SourcesSet()
        s1.addSourceFile("a000.c", pch=False, jumbo=False, system_includes=False)
        s1.addSourceFile("a001.c", pch=False, jumbo=False, system_includes=True)
        s1.addSourceFile("a010.c", pch=False, jumbo=True, system_includes=False)
        s1.addSourceFile("a011.c", pch=False, jumbo=True, system_includes=True)
        s1.addSourceFile("a100.c", pch=True, jumbo=False, system_includes=False)
        s1.addSourceFile("a101.c", pch=True, jumbo=False, system_includes=True)
        s1.addSourceFile("a110.c", pch=True, jumbo=True, system_includes=False)
        s1.addSourceFile("a111.c", pch=True, jumbo=True, system_includes=True)
        s2 = module_sources.SourcesSet()
        s2.addSourceFile("b000.c", pch=False, jumbo=False, system_includes=False)
        s2.addSourceFile("b001.c", pch=False, jumbo=False, system_includes=True)
        s2.addSourceFile("b010.c", pch=False, jumbo=True, system_includes=False)
        s2.addSourceFile("b011.c", pch=False, jumbo=True, system_includes=True)
        s2.addSourceFile("b100.c", pch=True, jumbo=False, system_includes=False)
        s2.addSourceFile("b101.c", pch=True, jumbo=False, system_includes=True)
        s2.addSourceFile("b110.c", pch=True, jumbo=True, system_includes=False)
        s2.addSourceFile("b111.c", pch=True, jumbo=True, system_includes=True)

        # test extending the set with two different sets which have a
        # different module name
        s = module_sources.SourcesSet()
        s.extend(s1, 'foo', 'bar')
        s.extend(s2, 'foo', 'baz')
        self.assertEqual(s.all(), ["foo/bar/a000.c", "foo/bar/a001.c", "foo/bar/a010.c", "foo/bar/a011.c", "foo/bar/a100.c", "foo/bar/a101.c", "foo/bar/a110.c", "foo/bar/a111.c", "foo/baz/b000.c", "foo/baz/b001.c", "foo/baz/b010.c", "foo/baz/b011.c", "foo/baz/b100.c", "foo/baz/b101.c", "foo/baz/b110.c", "foo/baz/b111.c"])
        self.assertEqual(s.nopch(), ["foo/bar/a000.c", "foo/bar/a001.c", "foo/bar/a010.c", "foo/bar/a011.c", "foo/bar/a111.c", "foo/baz/b000.c", "foo/baz/b001.c", "foo/baz/b010.c", "foo/baz/b011.c", "foo/baz/b111.c"])
        self.assertEqual(s.pch(), ["foo/bar/a100.c", "foo/baz/b100.c"])
        self.assertEqual(s.pch_system_includes(), ["foo/bar/a101.c", "foo/baz/b101.c"])
        self.assertEqual(s.pch_jumbo(), ["foo/bar/a110.c", "foo/baz/b110.c"])


    def testGenerateSourcesListFilesEmpty(self):
        s = module_sources.SourcesSet()
        s.generateSourcesListFiles(self.tmpdir())
        empty = StringIO.StringIO()
        for f in ('sources.all', 'sources.nopch', 'sources.pch', 'sources.pch_system_includes', 'sources.pch_jumbo'):
            filename = os.path.join(self.tmpdir(), f)
            self.assertTrue(util.compareWithFile(empty, filename))
            os.remove(filename)

    def testGenerateSourcesListFiles(self):
        s1 = module_sources.SourcesSet()
        s1.addSourceFile("a000.c", pch=False, jumbo=False, system_includes=False)
        s1.addSourceFile("a001.c", pch=False, jumbo=False, system_includes=True)
        s1.addSourceFile("a010.c", pch=False, jumbo=True, system_includes=False)
        s1.addSourceFile("a011.c", pch=False, jumbo=True, system_includes=True)
        s1.addSourceFile("a100.c", pch=True, jumbo=False, system_includes=False)
        s1.addSourceFile("a101.c", pch=True, jumbo=False, system_includes=True)
        s1.addSourceFile("a110.c", pch=True, jumbo=True, system_includes=False)
        s1.addSourceFile("a111.c", pch=True, jumbo=True, system_includes=True)
        s2 = module_sources.SourcesSet()
        s2.addSourceFile("b000.c", pch=False, jumbo=False, system_includes=False)
        s2.addSourceFile("b001.c", pch=False, jumbo=False, system_includes=True)
        s2.addSourceFile("b010.c", pch=False, jumbo=True, system_includes=False)
        s2.addSourceFile("b011.c", pch=False, jumbo=True, system_includes=True)
        s2.addSourceFile("b100.c", pch=True, jumbo=False, system_includes=False)
        s2.addSourceFile("b101.c", pch=True, jumbo=False, system_includes=True)
        s2.addSourceFile("b110.c", pch=True, jumbo=True, system_includes=False)
        s2.addSourceFile("b111.c", pch=True, jumbo=True, system_includes=True)

        # test extending the set with two different sets which have a
        # different module name
        s = module_sources.SourcesSet()
        s.extend(s1, 'foo', 'bar')
        s.extend(s2, 'foo', 'baz')
        expected_result = {
            'sources.all' : ["foo/bar/a000.c", "foo/bar/a001.c", "foo/bar/a010.c", "foo/bar/a011.c", "foo/bar/a100.c", "foo/bar/a101.c", "foo/bar/a110.c", "foo/bar/a111.c", "foo/baz/b000.c", "foo/baz/b001.c", "foo/baz/b010.c", "foo/baz/b011.c", "foo/baz/b100.c", "foo/baz/b101.c", "foo/baz/b110.c", "foo/baz/b111.c"],
            'sources.nopch' : ["foo/bar/a000.c", "foo/bar/a001.c", "foo/bar/a010.c", "foo/bar/a011.c", "foo/bar/a111.c", "foo/baz/b000.c", "foo/baz/b001.c", "foo/baz/b010.c", "foo/baz/b011.c", "foo/baz/b111.c"],
            'sources.pch' : ["foo/bar/a100.c", "foo/baz/b100.c"],
            'sources.pch_system_includes' : ["foo/bar/a101.c", "foo/baz/b101.c"],
            'sources.pch_jumbo' : ["foo/bar/a110.c", "foo/baz/b110.c"]
            }
        s.generateSourcesListFiles(self.tmpdir())
        for f in expected_result.keys():
            content = StringIO.StringIO()
            for file in expected_result[f]:
                print >>content, file
            filename = os.path.join(self.tmpdir(), f)
            self.assertTrue(util.compareWithFile(content, filename))
            os.remove(filename)

def suite():
    """
    Returns the unittest.TestSuite() which contains all
    unittest.TestCase() instances defined in this file.

    This can be used e.g. as
    @code
    import TestSourcesSet
    unittest.TextTestRunner(verbosity=2).run(TestSourcesSet.suite())
    @endcode
    """
    return unittest.TestSuite([
            unittest.TestLoader().loadTestsFromTestCase(TestCaseSourcesSet),
            ])
