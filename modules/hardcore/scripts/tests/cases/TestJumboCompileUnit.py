#
# This module defines unittest test-cases for the class
# module_sources.JumboCompileUnit
#
import sys
import os
import os.path
import StringIO
import unittest

import util
import module_sources

class TestCaseJumboCompileUnit_getModuleDir(unittest.TestCase):
    def testGetModuleDir(self):
        u = module_sources.JumboCompileUnit(
            type="modules", module="my_module",
            name="my_module_jumbo.cpp", pch=True, system_includes=False)
        self.assertEqual(u.getModuleDir(), os.path.join("modules", "my_module"))
        u = module_sources.JumboCompileUnit(
            type="modules", module="foo/bar/my_sub_module",
            name="my_module_jumbo.cpp", pch=True, system_includes=False)
        self.assertEqual(u.getModuleDir(), os.path.join("modules", "foo", "bar", "my_sub_module"))


class TestCaseJumboCompileUnit_getFilePath(unittest.TestCase):
    """
    This test case tests the method
    module_sources.JumboCompileUnit.getFilePath()
    """
    def testGetFilepath(self):
        """
        Tests that module_sources.JumboCompileUnit.getFilepath()
        returns the correct path to the jumbo compile unit with the
        platform's path separator.
        """
        u = module_sources.JumboCompileUnit(
            type='modules', module='foo', name='src/foo_jumbo.cpp',
            pch=True, system_includes=False)
        self.assertEqual(u.getFilepath(), os.path.join('modules', 'foo', 'src', 'foo_jumbo.cpp'))
        u = module_sources.JumboCompileUnit(
            type="modules", module="foo/bar/my_sub_module",
            name="my_module_jumbo.cpp", pch=True, system_includes=False)
        self.assertEqual(u.getFilepath(), os.path.join("modules", "foo", "bar", "my_sub_module", "my_module_jumbo.cpp"))


class TestCaseJumboCompileUnit_generateSourceFile(unittest.TestCase):
    """
    This test case tests the method
    module_sources.JumboCompileUnit.generateSourceFile()
    """

    def setUp(self):
        """
        Creates a temporary filename and the temporary directory in
        which the file 'modules/foo/src/foo_jumbo.cpp' will be
        written.
        """
        # this module is assumed to be called from
        # modules/hardcore/scripts/tests/test.py, so we can get the
        # source-root by going up 4 levels:
        self.__source_root = os.path.normpath(os.path.join(sys.path[0], '..', '..', '..', '..'))
        os.makedirs(self.moduleDir())

    def tearDown(self):
        """
        Removes the temporary directory.
        """
        def removeFile(name):
            if os.path.exists(name): os.remove(name)
        def removeDir(dir, files=[]):
            if os.path.exists(dir):
                for file in files: removeFile(os.path.join(dir, file))
                os.rmdir(dir)

        removeDir(os.path.join(self.sourceRoot(), 'tests', 'foo', 'src'),
                  ['.gitignore', 'foo_jumbo.cpp'])
        removeDir(os.path.join(self.sourceRoot(), 'tests', 'foo'),
                  ['module.generated', '.gitignore'])
        os.rmdir(os.path.join(self.sourceRoot(), 'tests'))

    def sourceRoot(self):
        """
        Returns the directory which contains the opera source
        tree. The directory "modules/hardcore/" is expected to be
        relative to the returned path. That source root directory is
        assigned in setUp().
        """
        return self.__source_root

    def moduleDir(self):
        """
        Returns the directory sourceRoot()/tests/foo which is the
        directory for the module "foo". The directory is created
        on setUp() and removed on tearDown().
        """
        return os.path.join(self.sourceRoot(), 'tests', 'foo')

    def testGenerateSourceFile(self):
        """
        Tests that the method
        module_sources.JumboCompileUnit.generateSourceFile()
        generates a jumbo-compile unit as expected.
        """
        u = module_sources.JumboCompileUnit(
            type='tests', module='foo', name='src/foo_jumbo.cpp',
            pch=True, system_includes=False)
        u.addSourceFile('src/foo_bar.cpp')
        u.addSourceFile('src/foo_baz.cpp')
        # first call writes the jumbo-compile-unit
        self.assertTrue(u.generateSourceFile(self.sourceRoot(), self.sourceRoot()))
        # second call doesn't change it
        self.assertFalse(u.generateSourceFile(self.sourceRoot(), self.sourceRoot()))

        # the file module.generated contains the name of the generated
        # jumbo compile unit:
        module_generated = StringIO.StringIO()
        print >>module_generated, "src/foo_jumbo.cpp"
        self.assertTrue(module_generated, os.path.join(self.moduleDir(), 'module.generated'))

        jumbo_file = os.path.join(self.moduleDir(), 'src', 'foo_jumbo.cpp')
        self.assertTrue(os.path.exists(jumbo_file))
        f = open(jumbo_file, 'r')
        jumbo_content = f.read()
        f.close()
        os.remove(jumbo_file)
        self.assertTrue("#include \"core/pch_jumbo.h\"" in jumbo_content)
        self.assertTrue("#include \"tests/foo/src/foo_bar.cpp\"" in jumbo_content)
        self.assertTrue("#include \"tests/foo/src/foo_baz.cpp\"" in jumbo_content)

    def testGenerateSourceFileNoPch(self):
        """
        Tests that the method
        module_sources.JumboCompileUnit.generateSourceFile()
        generates a jumbo-compile unit as expected.
        """
        u = module_sources.JumboCompileUnit(
            type='tests', module='foo', name='src/foo_jumbo.cpp',
            pch=False, system_includes=False)
        u.addSourceFile('src/foo_bar.cpp')
        u.addSourceFile('src/foo_baz.cpp')
        # first call writes the jumbo-compile-unit
        self.assertTrue(u.generateSourceFile(self.sourceRoot(), self.sourceRoot()))
        # second call doesn't change it
        self.assertFalse(u.generateSourceFile(self.sourceRoot(), self.sourceRoot()))

        jumbo_file = os.path.join(self.moduleDir(), 'src', 'foo_jumbo.cpp')
        self.assertTrue(os.path.exists(jumbo_file))
        f = open(jumbo_file, 'r')
        jumbo_content = f.read()
        f.close()
        os.remove(jumbo_file)
        self.assertTrue("#include \"core/pch_jumbo.h\"" in jumbo_content)
        self.assertTrue("#include \"tests/foo/src/foo_bar.cpp\"" in jumbo_content)
        self.assertTrue("#include \"tests/foo/src/foo_baz.cpp\"" in jumbo_content)

    def testGenerateSourceFileSystemIncludes(self):
        """
        Tests that the method
        module_sources.JumboCompileUnit.generateSourceFile()
        generates a jumbo-compile unit as expected.
        """
        u = module_sources.JumboCompileUnit(
            type='tests', module='foo', name='src/foo_jumbo.cpp',
            pch=True, system_includes=True)
        u.addSourceFile('src/foo_bar.cpp')
        u.addSourceFile('src/foo_baz.cpp')
        # first call writes the jumbo-compile-unit
        self.assertTrue(u.generateSourceFile(self.sourceRoot(), self.sourceRoot()))
        # second call doesn't change it
        self.assertFalse(u.generateSourceFile(self.sourceRoot(), self.sourceRoot()))

        jumbo_file = os.path.join(self.moduleDir(), 'src', 'foo_jumbo.cpp')
        self.assertTrue(os.path.exists(jumbo_file))
        f = open(jumbo_file, 'r')
        jumbo_content = f.read()
        f.close()
        os.remove(jumbo_file)
        self.assertTrue("#include \"core/pch_system_includes.h\"" in jumbo_content)
        self.assertTrue("#include \"tests/foo/src/foo_bar.cpp\"" in jumbo_content)
        self.assertTrue("#include \"tests/foo/src/foo_baz.cpp\"" in jumbo_content)

    def testGenerateSourceFileNoPchSystemIncludes(self):
        """
        Tests that the method
        module_sources.JumboCompileUnit.generateSourceFile()
        generates a jumbo-compile unit as expected.
        """
        u = module_sources.JumboCompileUnit(
            type='tests', module='foo', name='src/foo_jumbo.cpp',
            pch=False, system_includes=True)
        u.addSourceFile('src/foo_bar.cpp')
        u.addSourceFile('src/foo_baz.cpp')
        # first call writes the jumbo-compile-unit
        self.assertTrue(u.generateSourceFile(self.sourceRoot(), self.sourceRoot()))
        # second call doesn't change it
        self.assertFalse(u.generateSourceFile(self.sourceRoot(), self.sourceRoot()))

        jumbo_file = os.path.join(self.moduleDir(), 'src', 'foo_jumbo.cpp')
        self.assertTrue(os.path.exists(jumbo_file))
        f = open(jumbo_file, 'r')
        jumbo_content = f.read()
        f.close()
        os.remove(jumbo_file)
        self.assertTrue("#include \"core/pch_system_includes.h\"" in jumbo_content)
        self.assertTrue("#include \"tests/foo/src/foo_bar.cpp\"" in jumbo_content)
        self.assertTrue("#include \"tests/foo/src/foo_baz.cpp\"" in jumbo_content)

def suite():
    """
    Returns the unittest.TestSuite() which contains all
    unittest.TestCase() instances defined in this file.

    This can be used e.g. as
    @code
    import TestJumboCompileUnit
    unittest.TextTestRunner(verbosity=2).run(TestJumboCompileUnit.suite())
    @endcode
    """
    suite = unittest.TestSuite([
            unittest.TestLoader().loadTestsFromTestCase(TestCaseJumboCompileUnit_getModuleDir),
            unittest.TestLoader().loadTestsFromTestCase(TestCaseJumboCompileUnit_getFilePath),
            unittest.TestLoader().loadTestsFromTestCase(TestCaseJumboCompileUnit_generateSourceFile),
            ])
    return suite
