#
# This module defines unittest test-cases for the class util.WarningsAndErrors
#
import StringIO
import re
import unittest

import util

class TestCaseWarningsAndErrors(unittest.TestCase):
    """
    This test case test to add warnings and errors to an instance of
    util.WarningsAndErrors and print them to a StringIO instance.
    """

    def testNoWarningsNoErrors(self):
        err = StringIO.StringIO()
        w = util.WarningsAndErrors(stderr=err)
        self.assertEqual(w.warnings(), [])
        self.assertEqual(w.errors(), [])
        self.assertFalse(w.hasWarnings())
        self.assertFalse(w.hasErrors())
        self.assertFalse(w.printWarnings())
        self.assertFalse(w.printErrors())
        self.assertEqual(err.getvalue(), "")
        err.close()

    def testSingleWarnings(self):
        err = StringIO.StringIO()
        w = util.WarningsAndErrors(stderr=err)
        w.addWarning(util.Line("foo.txt", 17), "This is a warning")
        self.assertEqual(len(w.warnings()), 1)
        self.assertEqual(w.errors(), [])
        self.assertTrue(w.hasWarnings())
        self.assertFalse(w.hasErrors())
        self.assertTrue(w.printWarnings())
        self.assertFalse(w.printErrors())
        self.assertTrue(re.match("^foo.txt(\\(|:)17(\\) |): warning: This is a warning\n$", err.getvalue()))
        err.close()

    def testSingleError(self):
        err = StringIO.StringIO()
        w = util.WarningsAndErrors(stderr=err)
        w.addError(util.Line("bar.cpp", 33), "This is an error")
        self.assertEqual(w.warnings(), [])
        self.assertEqual(len(w.errors()), 1)
        self.assertFalse(w.hasWarnings())
        self.assertTrue(w.hasErrors())
        self.assertFalse(w.printWarnings())
        self.assertTrue(w.printErrors())
        self.assertTrue(re.match("^bar.cpp(\\(|:)33(\\) |): error( C0000|): This is an error\n$", err.getvalue()))
        err.close()

def suite():
    """
    Returns the unittest.TestSuite() which contains all
    unittest.TestCase() instances defined in this file.

    This can be used e.g. as
    @code
    import TestWarningsAndErrors
    unittest.TextTestRunner(verbosity=2).run(TestWarningsAndErrors.suite())
    @endcode
    """
    return unittest.TestSuite([
            unittest.TestLoader().loadTestsFromTestCase(TestCaseWarningsAndErrors),
            ])
