#
# This module defines unittest test-cases for the class
# module_sources.ModuleSources
#
import os.path
import StringIO
import unittest
import exceptions
import re
import tempfile

import module_sources

class TestCaseModuleSources_parseOptions(unittest.TestCase):
    """
    This test case test the method
    module_sources.ModuleSources.parseOptions() with various different
    strings.
    """

    def testParseOptionsEmpty(self):
        """
        An empty string or None should always create an empty
        dictionary.
        """
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions(""), {})
        self.assertEqual(m.parseOptions(None), {})

    def testParseOptionsSingleOption(self):
        """
        A single option should be added with value '1'.
        Whitespace is not ignored.
        """
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions("foo"), {'foo':['1']})
        self.assertEqual(m.parseOptions(" foo\t"), {" foo\t":['1']})

    def testParseOptionsSingleOptionValue(self):
        """
        A key=value option should be added with the specified value.
        """
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions("foo=bar"), {'foo':['bar']})

    def testParseOptionsMultipleOption(self):
        """
        Multiple options are seperated by a semicolon.
        """
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions("foo;bar"), {'foo':['1'],'bar':['1']})

    def testParseOptionsMultipleOptionValue(self):
        """
        With multiple options, each option may have a value.
        """
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions("foo=17;bar=9;baz"), {'foo':['17'],'bar':['9'],'baz':['1']})

    def testParseOptions_winnopch(self):
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions("winnopch"), {'pch':'0', 'system_includes':'1'})

    def testParseOptions_nopch(self):
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions("no-pch"), {'pch':['0']})

    def testParseOptions_jumbo(self):
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions("jumbo"), {'jumbo':['1']})

    def testParseOptions_nojumbo(self):
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(m.parseOptions("no-jumbo"), {'jumbo':['0']})

class TestCaseModuleSources_load(unittest.TestCase):
    """
    This test case tests the method
    module_sources.ModuleSources.load(). A file module.sources is
    generated with different content in a temporary directory and
    loaded. At the end of the test the file and the temporary
    directory are removed.
    """
    def moduleSourcesFile(self):
        """
        Returns the path to "module.sources" in the temporary
        directory that was created on setUp().
        """
        return os.path.join(self.__tmpdir, "module.sources")

    def setUp(self):
        """
        Creates a temporary filename and the temporary directory in
        which the module.sources file will be written.
        """
        if not (os.path.exists(tempfile.gettempdir())):
            os.mkdir(tempfile.gettempdir())
        self.__tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        """
        Removes the file module.sources from the temporary directory
        and removes the temporary directory.
        """
        if os.path.exists(self.moduleSourcesFile()):
            os.remove(self.moduleSourcesFile())
        os.rmdir(self.__tmpdir)

    def writeModuleSources(self, content):
        """
        Writes the specified content into the module.sources file in
        the temporary directory that was created in the setUp() step.
        The file is removed in tearDown().
        """
        f = None
        try:
            f = open(self.moduleSourcesFile(), "w")
            f.write(content)
        finally:
            if f: f.close()

    def testLoadPathNotExist(self):
        """
        Tests that 'foo/module.sources' does not exist and an IOError
        exception is raised on executing the test.
        """
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertRaises(IOError, m.load, os.path.join('foo', 'module.sources'))

    def testLoadEmptyFile(self):
        """
        Write an empty module.sources file and test that loading it
        does not create any file entries.
        """
        self.writeModuleSources("")
        m = module_sources.ModuleSources("modules", "my_module")
        # no files
        self.assertEqual(m.load(self.moduleSourcesFile()), [])
        self.assertFalse(m.hasWarningsOrErrors())

    def testLoadCommentFile(self):
        """
        Write a module.sources file with only comment-lines and
        whitespace lines and test that loading it doe not create any
        file entries.
        """
        output = StringIO.StringIO()
        print >>output, "# This is a test with"
        print >>output, "\t# a few comments and whitespace line"
        print >>output, "\t\t   \t\r"
        print >>output, ""
        self.writeModuleSources(output.getvalue())
        m = module_sources.ModuleSources("modules", "my_module")
        # no files
        self.assertEqual(m.load(self.moduleSourcesFile()), [])
        self.assertFalse(m.hasWarningsOrErrors())

    def testLoadSimpleFile(self):
        """
        Write a module.sources with some source-files (without any
        comments or options) and test that these source-files are
        parsed correctly.
        """
        output = StringIO.StringIO()
        files = ['file1.cpp', 'file2.cpp', 'src/file3.cpp']
        for filename in files: print >>output, filename
        self.writeModuleSources(output.getvalue())
        nopch = []
        pch = ['file1.cpp', 'file2.cpp', 'src/file3.cpp']
        pch_system_includes = []
        all_files = nopch + pch + pch_system_includes
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(set(m.load(self.moduleSourcesFile())), set(all_files))
        self.assertEqual(set(m.sources_all()), set(all_files))
        self.assertEqual(m.sources_nopch(), nopch)
        self.assertEqual(m.sources_pch(), pch)
        self.assertEqual(m.sources_pch_system_includes(), pch_system_includes)
        self.assertFalse(m.hasWarningsOrErrors())

    def testLoadSimpleFileComments(self):
        """
        Write a module.sources with some source-files and some
        comments and empty lines and test that whitespace and comments
        are ignored.
        """
        output = StringIO.StringIO()
        print >>output, "# This is a test with"
        print >>output, "\t# a few comments"
        print >>output, "\t\t   \t"
        print >>output, "src/File_1.cpp  \r"
        print >>output, "File_2.cpp# and some comments  "
        print >>output, "src/File_3.cpp\t \t # after filenames\t "
        print >>output, ""
        self.writeModuleSources(output.getvalue())
        nopch = []
        pch = ['src/File_1.cpp', 'File_2.cpp', 'src/File_3.cpp']
        pch_system_includes = []
        all_files = nopch + pch + pch_system_includes
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(set(m.load(self.moduleSourcesFile())), set(all_files))
        self.assertEqual(set(m.sources_all()), set(all_files))
        self.assertEqual(m.sources_nopch(), nopch)
        self.assertEqual(m.sources_pch(), pch)
        self.assertEqual(m.sources_pch_system_includes(), pch_system_includes)
        self.assertFalse(m.hasWarningsOrErrors())
        self.assertFalse(m.printWarningsAndErrors())

    def testLoadSimpleFileForSubModule(self):
        """
        Write a module.sources with some source-files (without any
        comments or options) where the module-name contains some
        slashes, i.e. the module is a sub-module of some other
        module. Test that the filenames are correct.
        """
        output = StringIO.StringIO()
        files = ['file1.cpp', 'file2.cpp', 'src/file3.cpp']
        for filename in files: print >>output, filename
        self.writeModuleSources(output.getvalue())
        nopch = []
        pch = ['file1.cpp', 'file2.cpp', 'src/file3.cpp']
        pch_system_includes = []
        all_files = nopch + pch + pch_system_includes
        m = module_sources.ModuleSources("modules", "foo/bar/my_sub_module")
        self.assertEqual(set(m.load(self.moduleSourcesFile())), set(all_files))
        self.assertEqual(set(m.sources_all()), set(all_files))
        self.assertEqual(m.sources_nopch(), nopch)
        self.assertEqual(m.sources_pch(), pch)
        self.assertEqual(m.sources_pch_system_includes(), pch_system_includes)
        self.assertFalse(m.hasWarningsOrErrors())
        jumbo_units = m.jumboCompileUnits()
        self.assertEqual(len(jumbo_units), 1)
        self.assertEqual(jumbo_units[0].getModuleDir(), os.path.join("modules", "foo", "bar", "my_sub_module"))
        self.assertEqual(jumbo_units[0].getFilepath(), os.path.join("modules", "foo", "bar", "my_sub_module", "foo_bar_my_sub_module_jumbo.cpp"))

    def testLoadFilesWithOptions(self):
        """
        Write a module.sources where some files have one or more
        options and that these options are correctly parsed.
        """
        output = StringIO.StringIO()
        print >>output, "# single option after filename:"
        print >>output, "File_1.cpp  # [no-pch]"
        print >>output, "# single option with value after filename:"
        print >>output, "File_2.cpp  # [;pch=0;]"
        print >>output, "# multiple option after filename:"
        print >>output, "File_3.cpp  # [system_includes;no-pch]"
        print >>output, "File_4.cpp"
        print >>output, "# only use first option, the second [] and all"
        print >>output, "# its content are ignored:"
        print >>output, "File_5.cpp # [pch][no-pch]"
        self.writeModuleSources(output.getvalue())

        nopch = ['File_1.cpp', 'File_2.cpp', 'File_3.cpp']
        pch = ['File_4.cpp', 'File_5.cpp']
        pch_system_includes = []
        all_files = nopch + pch + pch_system_includes

        errors = StringIO.StringIO()
        m = module_sources.ModuleSources("modules", "my_module", stderr=errors)
        self.assertEqual(set(m.load(self.moduleSourcesFile())), set(all_files))
        self.assertEqual(set(m.sources_all()), set(all_files))
        self.assertEqual(m.sources_nopch(), nopch)
        self.assertEqual(m.sources_pch(), pch)
        self.assertEqual(m.sources_pch_system_includes(), pch_system_includes)
        # expecting several errors, because source-files with
        # different options are not split into different jumbo-compile
        # units
        self.assertTrue(m.hasWarningsOrErrors())
        self.assertTrue(m.printWarningsAndErrors())
        self.assertTrue(re.match(".*File_3\.cpp.*File_4\.cpp.*", errors.getvalue(), re.DOTALL))

    def testLoadFilesWithCommonOptions(self):
        """
        Write a module.sources file, which contains common options for
        several files. And each single file can override single
        options.
        """
        output = StringIO.StringIO()
        print >>output, "# single option for the following group:"
        print >>output, "  # [system_includes;;]"
        print >>output, "File_1.cpp"
        print >>output, "# this is a comment no options: [no-pch]"
        print >>output, "File_2.cpp"
        print >>output, "# [] resetting options"
        print >>output, "File_3.cpp"
        print >>output, "  # [no-pch]"
        print >>output, "File_4.cpp # [system_includes] adding single option"
        print >>output, "File_5.cpp#[pch=1] overriding single option"
        print >>output, "File_6.cpp # [pch=1;system_includes] adding and overriding"
        self.writeModuleSources(output.getvalue())
        nopch = ['File_4.cpp']
        pch = ['File_3.cpp', 'File_5.cpp']
        pch_system_includes = ['File_1.cpp', 'File_2.cpp', 'File_6.cpp']
        all_files = nopch + pch + pch_system_includes

        errors = StringIO.StringIO()
        m = module_sources.ModuleSources("modules", "my_module", stderr=errors)
        self.assertEqual(set(m.load(self.moduleSourcesFile())), set(all_files))
        self.assertEqual(set(m.sources_all()), set(all_files))
        self.assertEqual(m.sources_nopch(), nopch)
        self.assertEqual(m.sources_pch(), pch)
        self.assertEqual(m.sources_pch_system_includes(), pch_system_includes)
        # expecting several errors, because source-files with
        # different options are not split into different jumbo-compile
        # units
        self.assertTrue(m.hasWarningsOrErrors())
        self.assertTrue(m.printWarningsAndErrors())
        self.assertTrue(re.match(".*File_3\.cpp.*File_4\.cpp.*File_5\.cpp.*", errors.getvalue(), re.DOTALL))

    def testLoadFilesWithJumboOptions(self):
        """
        Write a module.sources file, which contains options for
        jumbo-compile for several files.
        """
        output = StringIO.StringIO()
        print >>output, "# [no-jumbo]"
        print >>output, "File_1.cpp"
        print >>output, "File_2.cpp # [system_includes]"
        print >>output, "File_3.cpp # [no-pch]"
        print >>output, "File_4.cpp # [no-pch;system_includes]"
        print >>output, "# [] resetting options"
        print >>output, "File_5.cpp"
        print >>output, "File_6.cpp"
        print >>output, "# [jumbo=jumbo2.cpp;no-pch] new jumbo compile unit"
        print >>output, "File_7.cpp"
        print >>output, "File_8.cpp"
        print >>output, "File_9.cpp# [jumbo=jumbo3.cpp;system_includes]"
        print >>output, "File_10.cpp# [jumbo=jumbo3.cpp;system_includes]"
        print >>output, "# [jumbo=jumbo4.cpp;system_includes]"
        print >>output, "File_11.cpp"
        print >>output, "File_12.cpp"
        self.writeModuleSources(output.getvalue())

        # files for plain compile:
        plain_nopch = ['File_3.cpp', 'File_4.cpp', 'File_7.cpp', 'File_8.cpp', 'File_9.cpp', 'File_10.cpp']
        plain_pch = ['File_1.cpp', 'File_5.cpp', 'File_6.cpp']
        plain_pch_system_includes = ['File_2.cpp', 'File_11.cpp', 'File_12.cpp']
        plain_all = set(plain_nopch + plain_pch + plain_pch_system_includes)
        # files for jumbo-compile
        jumbo_nopch = ['File_3.cpp', 'File_4.cpp', 'jumbo2.cpp', 'jumbo3.cpp', 'jumbo4.cpp']
        jumbo_pch = ['File_1.cpp']
        jumbo_pch_jumbo = ['my_module_jumbo.cpp']
        jumbo_pch_system_includes = ['File_2.cpp']
        jumbo_all = set(jumbo_nopch + jumbo_pch + jumbo_pch_jumbo + jumbo_pch_system_includes)

        errors = StringIO.StringIO()
        m = module_sources.ModuleSources("modules", "my_module")
        self.assertEqual(set(m.load(self.moduleSourcesFile())), plain_all)
        self.assertEqual(set(m.sources_all()), plain_all)
        self.assertEqual(m.sources_nopch(), plain_nopch)
        self.assertEqual(m.sources_pch(), plain_pch)
        self.assertEqual(m.sources_pch_system_includes(), plain_pch_system_includes)
        self.assertEqual(set(m.jumbo_sources_all()), jumbo_all)
        self.assertEqual(m.jumbo_sources_nopch(), jumbo_nopch)
        self.assertEqual(m.jumbo_sources_pch(), jumbo_pch)
        self.assertEqual(m.jumbo_sources_pch_jumbo(), jumbo_pch_jumbo)
        self.assertEqual(m.jumbo_sources_pch_system_includes(), jumbo_pch_system_includes)
        self.assertFalse(m.hasWarningsOrErrors())
        self.assertFalse(m.printWarningsAndErrors())
        self.assertEqual(errors.getvalue(), "")


def suite():
    """
    Returns the unittest.TestSuite() which contains all
    unittest.TestCase() instances defined in this file.

    This can be used e.g. as
    @code
    import TestModuleSources
    unittest.TextTestRunner(verbosity=2).run(TestModuleSources.suite())
    @endcode
    """
    suite = unittest.TestSuite([
            unittest.TestLoader().loadTestsFromTestCase(TestCaseModuleSources_parseOptions),
            unittest.TestLoader().loadTestsFromTestCase(TestCaseModuleSources_load)
            ])
    return suite
