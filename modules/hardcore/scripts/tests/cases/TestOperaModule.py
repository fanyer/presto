#
# This module defines unittest test-cases for the class
# module.Module
#
import os.path
import os
import unittest
import sys
import tempfile
import StringIO
import shutil
import re

import opera_module
import util

class TestCaseModule(unittest.TestCase):

    def setUp(self):
        # this module is assumed to be called from
        # modules/hardcore/scripts/tests/test.py, so we can get the
        # source-root by going up 4 levels:
        self.__source_root = os.path.normpath(os.path.join(sys.path[0], '..', '..', '..', '..'))

    def testBasic(self):
        """
        Tests that some basic methods of an opera_module.Module
        instance return the expected values.
        """
        module = opera_module.Module(self.__source_root,
                                     self.__source_root,
                                     type='foo',
                                     name='bar',
                                     version='baz_1_0')
        self.assertEqual(module.type(), 'foo')
        self.assertEqual(module.name(), 'bar')
        self.assertEqual(module.version(), 'baz_1_0')
        self.assertEqual(module.relativePath(), os.path.join('foo', 'bar'))
        self.assertEqual(module.fullPath(), os.path.join(self.__source_root, 'foo', 'bar'))
        self.assertEqual(module.getModuleFilePath(), 'foo/bar/bar_module.h')
        self.assertEqual(module.getModuleClassName(), 'BarModule')
        self.assertEqual(module.getTweaksFile(), os.path.join(self.__source_root, 'foo', 'bar', 'module.tweaks'))
        self.assertEqual(module.getAPIExportFile(), os.path.join(self.__source_root, 'foo', 'bar', 'module.export'))
        self.assertEqual(module.getAPIImportFile(), os.path.join(self.__source_root, 'foo', 'bar', 'module.import'))
        self.assertEqual(module.getActionsFile(), os.path.join(self.__source_root, 'foo', 'bar', 'module.actions'))
        self.assertEqual(module.getSourcesFile(), os.path.join(self.__source_root, 'foo', 'bar', 'module.sources'))

    def testIdName(self):
        """
        Tests that opera_module.Module.id_name() returns the expected
        name for several different module names.
        """
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='bar')
        self.assertEqual(module.id_name(), 'bar')
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='BAR')
        self.assertEqual(module.id_name(), 'BAR')
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='bar/baz-1.0')
        self.assertEqual(module.id_name(), 'bar_baz_1_0')

    def testModuleClassName(self):
        """
        Tests that opera_module.Module.getModuleClassName() returns
        the expected class name for several different module names.
        """
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='bar')
        self.assertEqual(module.getModuleClassName(), 'BarModule')
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='BAR',
                                     version='baz_1_1')
        self.assertEqual(module.getModuleClassName(), 'BarModule')
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='bar/baz-1.0',
                                     version='baz_1_0')
        self.assertEqual(module.getModuleClassName(), 'BarBaz10Module')
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='foo_bar')
        self.assertEqual(module.getModuleClassName(), 'FooBarModule')
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='foo_bar_baz')
        self.assertEqual(module.getModuleClassName(), 'FooBarBazModule')
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='ecmascript')
        self.assertEqual(module.getModuleClassName(), 'EcmaScriptModule')
        module = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                     name='foo_1_2_3___bar___7baz')
        self.assertEqual(module.getModuleClassName(), 'Foo123Bar7bazModule')

    def testCompare(self):
        """
        Tests that modules are compared by index first and then by
        module name if no index is set.
        Note: the module's index is set on loading the file
        modules/hardcore/opera/modules.txt. That defines the
        initialization order of the modules.
        """
        # first: compare by name
        module1 = opera_module.Module(self.__source_root, self.__source_root, type='foo',
                                      name='bar')
        module2 = opera_module.Module(self.__source_root, self.__source_root, type='aaa',
                                     name='foo')
        self.assertTrue(module1 == module1)
        self.assertFalse(module1 == module2)
        self.assertFalse(module1 < module1)
        self.assertTrue(module1 <= module2)
        self.assertTrue(module1 < module2)
        # compare by index: a module with index set is lower than a
        # module without index
        module2.setIndex(17)
        self.assertTrue(module1 >= module2)
        self.assertTrue(module1 > module2)
        # compare by index: two modules with index set are compared by
        # the index value
        module1.setIndex(562)
        self.assertTrue(module1 >= module2)
        self.assertTrue(module1 > module2)

class TestCaseModule_getModuleSources(unittest.TestCase):
    def moduleSourcesFile(self):
        """
        Returns the path to "module.sources" in the temporary
        directory that was created on setUp().
        """
        return os.path.join(self.__tmpdir, 'test', 'foo', 'module.sources')

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
        if os.path.exists(os.path.join(self.__tmpdir, 'test')):
            if os.path.exists(os.path.join(self.__tmpdir, 'test', 'foo')):
                if os.path.exists(self.moduleSourcesFile()):
                    os.remove(self.moduleSourcesFile())
                os.rmdir(os.path.join(self.__tmpdir, 'test', 'foo'))
            os.rmdir(os.path.join(self.__tmpdir, 'test'))
        os.rmdir(self.__tmpdir)

    def writeModuleSources(self, content):
        """
        Writes the specified content into the module.sources file in
        the temporary directory that was created in the setUp() step.
        The file is removed in tearDown().
        """
        path = os.path.join(self.__tmpdir, 'test', 'foo')
        util.makedirs(path)
        f = None
        try:
            f = open(self.moduleSourcesFile(), "w")
            f.write(content)
        finally:
            if f: f.close()

    def testNoModuleSources(self):
        module = opera_module.Module(self.__tmpdir, self.__tmpdir, type='test', name='foo')
        self.assertFalse(module.hasModuleFile())
        self.assertFalse(module.hasSourcesFile())
        self.assertRaises(IOError, module.getModuleSources)

    def testLoadEmptyFile(self):
        """
        Write an empty module.sources file and test that loading it
        does not create any file entries.
        """
        self.writeModuleSources("")
        module = opera_module.Module(self.__tmpdir, self.__tmpdir, type='test', name='foo')
        self.assertTrue(module.hasSourcesFile())
        self.assertTrue(module.getModuleSources())
        self.assertEqual(module.files(), [])

    def testLoadSimpleFile(self):
        """
        Write a simple module.sources file and test loading it.
        """
        output = StringIO.StringIO()
        print >>output, "src/foo.cpp"
        print >>output, "src/bar.cpp"
        self.writeModuleSources(output.getvalue())
        module = opera_module.Module(self.__tmpdir, self.__tmpdir, type='test', name='foo')
        self.assertTrue(module.hasSourcesFile())
        self.assertTrue(module.getModuleSources())
        self.assertEqual(module.files(), ['src/foo.cpp', 'src/bar.cpp'])


class TestCaseModuleSet(unittest.TestCase):
    """
    This test case tests opera_module.ModuleSet.load() and
    opera_module.ModuleSet.generateSourcesSetup() by creating a small
    source tree with simple readme.txt, modules and module.sources
    files.
    """
    def setUp(self):
        """
        Creates a temporary filename and the temporary directory which
        will be the test source root.
        """
        if not (os.path.exists(tempfile.gettempdir())):
            os.mkdir(tempfile.gettempdir())
        self.__tmpdir = tempfile.mkdtemp()
        # this module is assumed to be called from
        # modules/hardcore/scripts/tests/test.py, so we can get the
        # source-root by going up 4 levels:
        source_root = os.path.normpath(os.path.join(sys.path[0], '..', '..', '..', '..'))
        # copy the jumbo template from the original source-root to the
        # temporary source-root:
        jumbo_template = os.path.join('modules', 'hardcore', 'base', 'jumbo_compile_template.cpp')
        os.makedirs(os.path.join(self.__tmpdir, 'modules', 'hardcore', 'base'))
        shutil.copyfile(os.path.join(source_root, jumbo_template),
                        os.path.join(self.__tmpdir, jumbo_template))
        # create first "type" directory "test" which has modules "foo"
        # and "bar" and each module has two source-files:
        os.mkdir(os.path.join(self.__tmpdir, 'test'))
        f = open(os.path.join(self.__tmpdir, 'test', 'readme.txt'), 'w')
        print >>f, '# this file was generated to test opera_module.ModuleSet'
        print >>f, 'foo'
        print >>f, 'bar'
        f.close()
        os.mkdir(os.path.join(self.__tmpdir, 'test', 'foo'))
        f = open(os.path.join(self.__tmpdir, 'test', 'foo', 'module.sources'), 'w')
        print >>f, 'src/foo_1.cpp'
        print >>f, 'src/foo_2.cpp'
        f.close()
        os.mkdir(os.path.join(self.__tmpdir, 'test', 'bar'))
        f = open(os.path.join(self.__tmpdir, 'test', 'bar', 'module.sources'), 'w')
        print >>f, 'src/baz_1.cpp'
        print >>f, 'src/baz_2.cpp'
        f.close()
        # create second "type" directory "mod" which has modules
        # "my_mod" and "yo_mod" and each module has two source-files:
        os.mkdir(os.path.join(self.__tmpdir, 'mod'))
        f = open(os.path.join(self.__tmpdir, 'mod', 'readme.txt'), 'w')
        print >>f, '# this file was generated to test opera_module.ModuleSet'
        print >>f, 'my_mod'
        print >>f, 'yo_mod'
        f.close()
        os.mkdir(os.path.join(self.__tmpdir, 'mod', 'my_mod'))
        f = open(os.path.join(self.__tmpdir, 'mod', 'my_mod', 'module.sources'), 'w')
        print >>f, 'MyA.cpp'
        print >>f, 'MyB.cpp'
        f.close()

        f = open(os.path.join(self.__tmpdir, 'mod', 'my_mod', 'MyA.cpp'), 'w')
        print >>f, "// This is MyA.cpp"
        f.close()

        os.mkdir(os.path.join(self.__tmpdir, 'mod', 'yo_mod'))
        f = open(os.path.join(self.__tmpdir, 'mod', 'yo_mod', 'module.sources'), 'w')
        print >>f, 'Yo1.cpp'
        print >>f, 'Yo2.cpp'
        f.close()

    def tearDown(self):
        """
        Removes the file module.sources from the temporary directory
        and removes the temporary directory.
        """
        def removeFile(name):
            if os.path.exists(name): os.remove(name)
        def removeDir(dir, files=[]):
            if os.path.exists(dir):
                for file in files: removeFile(os.path.join(dir, file))
                os.removedirs(dir)

        # create a temporary file to not delete tempfile.gettempdir()
        # on calling os.removedirs()
        tmp = tempfile.TemporaryFile()
        setup = { 'test' : ['foo', 'bar'],
                  'mod' : ['my_mod', 'yo_mod'] }
        removeFile(os.path.join(self.__tmpdir, 'mod', 'my_mod', 'MyA.cpp'))
        for type in setup.keys():
            type_dir = os.path.join(self.__tmpdir, type)
            for module in setup[type]:
                removeDir(os.path.join(type_dir, module),
                          ['.gitignore',
                           'module.sources',
                           'module.generated',
                           '%s_jumbo.cpp' % module])
            removeDir(type_dir, ['readme.txt'])
        removeDir(os.path.join(self.__tmpdir, 'modules', 'hardcore', 'base'),
                  ['.gitignore', 'jumbo_compile_template.cpp'])
        setupDir = os.path.join(self.__tmpdir, 'modules', 'hardcore', 'setup')
        for name in ('plain', 'jumbo'):
            removeDir(os.path.join(setupDir, name, 'sources'),
                      ['.gitignore',
                       'sources.all', 'sources.nopch', 'sources.pch',
                       'sources.pch_system_includes', 'sources.pch_jumbo'])
        removeDir(os.path.join(self.__tmpdir, 'modules', 'hardcore'),
                  ['.gitignore', 'module.generated'])
        removeDir(setupDir)
        removeDir(self.__tmpdir)
        tmp.close() # automatically removes the file

    def testLoad(self):
        """
        Tests that opera_module.ModuleSet.load() loads all Module
        instances from the generated setup.
        """
        module_set = opera_module.ModuleSet(sourceRoot=self.__tmpdir,
                                            outputRoot=self.__tmpdir,
                                            types=['test', 'mod'])
        modules = module_set.load()
        self.assertEqual(set(map(lambda m: m.name(), modules)), set(['foo', 'bar', 'my_mod', 'yo_mod']))

    def testGenerateSourcesSetup(self):
        """
        Tests that opera_module.ModuleSet.generateSourcesSetup()
        creates the files sources.all, sources.nopch, sources.pch,
        sources.pch_system_includes and sources.pch_jumbo in both
        modules/hardcore/setup/plain/sources and
        modules/hardcore/setup/jumbo/sources; and it creates all jumbo
        compile units.
        """
        module_set = opera_module.ModuleSet(sourceRoot=self.__tmpdir,
                                            outputRoot=self.__tmpdir,
                                            types=['test', 'mod'])
        modules = module_set.load()
        self.assertTrue(module_set.generateSourcesSetup())
        # there should be errors, because the files itself don't exist:
        self.assertTrue(module_set.hasErrors());
        # e.g. mod/my_mod/MyB.cpp should be reported as missing while
        # mod/my_mod/MyA.cpp exists:
        re_FileNotfound = re.compile(".*file '(.*)' not found.*", re.I)
        files_not_found = []
        for error in module_set.errors():
            print error
            match = re_FileNotfound.match(str(error))
            if match: files_not_found.append(match.group(1))
        print >>sys.stderr, files_not_found
        self.assertFalse('mod/my_mod/MyA.cpp' in files_not_found)
        self.assertTrue('mod/my_mod/MyB.cpp' in files_not_found)

        all_files = [ 'test/foo/src/foo_1.cpp',
                      'test/foo/src/foo_2.cpp',
                      'test/bar/src/baz_1.cpp',
                      'test/bar/src/baz_2.cpp',
                      'mod/my_mod/MyA.cpp',
                      'mod/my_mod/MyB.cpp',
                      'mod/yo_mod/Yo1.cpp',
                      'mod/yo_mod/Yo2.cpp' ]
        jumbo_units = [ 'test/foo/foo_jumbo.cpp',
                        'test/bar/bar_jumbo.cpp',
                        'mod/my_mod/my_mod_jumbo.cpp',
                        'mod/yo_mod/yo_mod_jumbo.cpp' ]
        expected_result = {
            'plain': { 'all': all_files,
                       'pch': all_files,
                       'nopch':[], 'pch_system_includes':[], 'pch_jumbo':[] },
            'jumbo': { 'all': jumbo_units,
                       'pch_jumbo' : jumbo_units,
                       'nopch':[], 'pch':[], 'pch_system_includes':[] } }
        setupDir = os.path.join(self.__tmpdir, 'modules', 'hardcore', 'setup')
        for name in ('plain', 'jumbo'):
            dir = os.path.join(setupDir, name, 'sources')
            for type in ('all', 'nopch', 'pch', 'pch_system_includes', 'pch_jumbo'):
                expected = StringIO.StringIO()
                for file in expected_result[name][type]:
                    print >>expected, file
                filename = os.path.join(setupDir, name, 'sources', 'sources.%s' % type)
                f = open(filename)
                result = f.read()
                f.close()
                self.assertEqual(expected.getvalue(), result)
        # assert that all jumbo compile units are generated:
        for jumbo in jumbo_units:
            self.assertTrue(os.path.exists(os.path.join(self.__tmpdir, jumbo)))
        # on the second call, the content does not change:
        self.assertFalse(module_set.generateSourcesSetup())
        # remove one jumbo-unit and now generateSourcesSetup() should
        # return True:
        os.remove(os.path.join(self.__tmpdir, jumbo_units[0]))
        self.assertTrue(module_set.generateSourcesSetup())


class TestCaseModuleSet_generatePchFiles(unittest.TestCase):
    """
    This testcase tests the function opera_module.generatePchFiles().
    """
    def setUp(self):
        if not (os.path.exists(tempfile.gettempdir())):
            os.mkdir(tempfile.gettempdir())
        self.__tmpdir = tempfile.mkdtemp()
        # this module is assumed to be called from
        # modules/hardcore/scripts/tests/test.py, so we can get the
        # source-root by going up 4 levels:
        self.__source_root = os.path.normpath(os.path.join(sys.path[0], '..', '..', '..', '..'))

    def sourceRoot(self):
        """
        Returns the directory which contains the opera source
        tree. The directory "modules/hardcore/" is expected to be
        relative to the returned path. That source root directory is
        assigned in setUp().
        """
        return self.__source_root

    def tearDown(self):
        def removeFile(name):
            if os.path.exists(name): os.remove(name)
        def removeDir(dir, files=[]):
            if os.path.exists(dir):
                for file in files: removeFile(os.path.join(dir, file))
                os.removedirs(dir)
        # create a temporary file to not delete tempfile.gettempdir()
        # on calling os.removedirs()
        tmp = tempfile.TemporaryFile()
        removeDir(os.path.join(self.__tmpdir, 'core'),
                  ['pch.h', 'pch_jumbo.h', 'pch_system_includes.h'])
        removeDir(self.__tmpdir)
        tmp.close() # automatically removes the file

    def testGeneratePchFiles(self):
        # On the first call the files are generated, so the method returns True:
        self.assertTrue(opera_module.generatePchFiles(sourceRoot=self.sourceRoot(),
                                                      outputRoot=self.__tmpdir))
        for pch in ('pch.h', 'pch_jumbo.h', 'pch_system_includes.h'):
            self.assertTrue(os.path.exists(os.path.join(self.__tmpdir, 'core', pch)))
            f = open(os.path.join(self.__tmpdir, 'core', pch), 'r')
            pch_content = f.read()
            f.close()
            self.assertTrue("#include \"modules/hardcore/base/baseincludes.h\"" in pch_content)
            self.assertTrue("PRODUCT_CONFIG_FILE" in pch_content)
            if pch == 'pch_system_includes.h':
                self.assertTrue("#define ALLOW_SYSTEM_INCLUDES" in pch_content)
            else:
                self.assertFalse("#define ALLOW_SYSTEM_INCLUDES" in pch_content)
        # the second call does not change the files
        self.assertFalse(opera_module.generatePchFiles(sourceRoot=self.sourceRoot(),
                                                       outputRoot=self.__tmpdir))
        # files are changed if a platform_product_config is specified:
        self.assertTrue(opera_module.generatePchFiles(sourceRoot=self.sourceRoot(),
                                                      outputRoot=self.__tmpdir,
                                                      platform_product_config="platforms/foo/product_config.h"))
        for pch in ('pch.h', 'pch_jumbo.h', 'pch_system_includes.h'):
            f = open(os.path.join(self.__tmpdir, 'core', pch), 'r')
            pch_content = f.read()
            f.close()
            self.assertTrue("#include \"modules/hardcore/base/baseincludes.h\"" in pch_content)
            self.assertFalse("#error" in pch_content)
            self.assertTrue("#include \"platforms/foo/product_config.h\"" in pch_content)

def suite():
    """
    Returns the unittest.TestSuite() which contains all
    unittest.TestCase() instances defined in this file.

    This can be used e.g. as
    @code
    import TestModule
    unittest.TextTestRunner(verbosity=2).run(TestModule.suite())
    @endcode
    """
    suite = unittest.TestSuite([
            unittest.TestLoader().loadTestsFromTestCase(TestCaseModule),
            unittest.TestLoader().loadTestsFromTestCase(TestCaseModule_getModuleSources),
            unittest.TestLoader().loadTestsFromTestCase(TestCaseModuleSet),
            unittest.TestLoader().loadTestsFromTestCase(TestCaseModuleSet_generatePchFiles)
            ])
    return suite
