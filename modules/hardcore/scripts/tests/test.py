#!/usr/bin/env python

#
# this is a simple unittest script which all runs unit.TestSuite:s
# that are defined in modules/hardcore/scripts/tests/cases/
#
import sys
import os.path
import unittest

# be able to load all hardcore scripts:
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "base"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "features"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "opera"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "actions"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "keys"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "mh"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "scripts"))

# import all test cases
sys.path.insert(1, os.path.join(sys.path[0], "cases"))
import TestWarningsAndErrors
import TestModuleSources
import TestSourcesSet
import TestJumboCompileUnit
import TestOperaModule
import TestTweaksParser

# define all testsuites:
alltests = unittest.TestSuite([
        TestWarningsAndErrors.suite(),
        TestSourcesSet.suite(),
        TestModuleSources.suite(),
        TestJumboCompileUnit.suite(),
        TestOperaModule.suite(),
        TestTweaksParser.suite()
        ])

# run all testsuites:
test_result = unittest.TextTestRunner(verbosity=2).run(alltests)
if test_result.wasSuccessful():
    sys.exit(0)
else: sys.exit(1)
