#
# This module defines unittest test-cases for the class
# tweaks_and_apis.TweaksParser
#
import os.path
import os
import unittest
import sys
import StringIO

import tweaks_and_apis
import util

class TestCaseTweaksParser(unittest.TestCase):

    def createParser(self):
        """
        This method creates a tweaks_and_apis.TweaksParser instance. To
        initialize the parser, an empty, not loaded util.FeatureDefinition
        instance is used and the sourceRoot is set to ".". For the scope of
        these tests we don't need to access the correct source root, nor
        the correct data of a util.FeatureDefinition instance.

        @return the created parser instance
        """
        return tweaks_and_apis.TweaksParser(feature_def=util.FeatureDefinition(),
                                            sourceRoot=".")

    def parseTweakDefinitionLines(self, filename, linenr, name, owner, parser, lines):
        """
        Creates a tweaks_and_apis.Tweak instance from the specified data.
        The specified lines are parsed by the parser (i.e. the method
        tweaks_and_apis.TweaksParser.parseTweakDefinition() is called),
        dependencies are resolved (tweaks_and_apis.TweaksParser.finishDependsOn)
        and the tweak is added to the parser
        (tweaks_and_apis.TweaksParser.addTweak()).

        @param filename is only used to initialize the instance and create
          possible warning and error messages.
        @param linenr is only used to initialize the instance and create
          possible warning and error messages.
        @param name is the name of the Tweak. Tweak.name() will return this
          value and the parser stores the tweak at this name.
        @param owner is only used to initialize the instance.
        @param parser is the tweaks_and_apis.TweaksParser instance that is used
          to parse the lines and store the parsed tweak.
        @param lines is an array of lines in the format "key:value". These
          lines should contain a valid definition block for the tweak to
          parse. parser.parseTweakDefinition() is called for the key, value
          pair of the line.

        @return the tweaks_and_apis.Tweak instance
        """
        tweak = tweaks_and_apis.Tweak(filename, linenr, name, owner)
        for line in lines:
            (what, value) = line.split(":")
            parser.parseTweakDefinition(line, tweak, what.lower(), value)
        parser.finishDependsOn(tweak)
        parser.addTweak(tweak)
        return tweak

    def parseAPIExportDefinitionLines(self, filename, linenr, name, owner, parser, lines):
        """
        Creates a tweaks_and_apis.APIExport instance from the specified data.
        The specified lines are parsed by the parser (i.e. the method
        tweaks_and_apis.TweaksParser.parseAPIExportDefinition() is called),
        dependencies are resolved (tweaks_and_apis.TweaksParser.finishDependsOn)
        and the api export is added to the parser
        (tweaks_and_apis.TweaksParser.addAPIExport()).

        @param filename is only used to initialize the instance and create
          possible warning and error messages.
        @param linenr is only used to initialize the instance and create
          possible warning and error messages.
        @param name is the name of the api. APIExport.name() will return this
          value and the parser stores the api export at this name.
        @param owner is only used to initialize the instance.
        @param parser is the tweaks_and_apis.TweaksParser instance that is used
          to parse the lines and store the parsed api.
        @param lines is an array of lines in the format "key:value". These
          lines should contain a valid definition block for the api export to
          parse. parser.parseAPIExportDefinition() is called for the key, value
          pair of the line.

        @return the tweaks_and_apis.APIExport instance
        """
        api = tweaks_and_apis.APIExport(filename, linenr, name, owner)
        for line in lines:
            (what, value) = line.split(":")
            parser.parseAPIExportDefinition(line, api, what.lower(), value)
        parser.finishDependsOn(api)
        parser.verifyAPIExport(api)
        parser.addAPIExport(api)
        return api

    def parseAPIImportDefinitionLines(self, filename, linenr, name, owner, parser, lines):
        """
        Creates a tweaks_and_apis.APIImport instance from the specified data.
        The specified lines are parsed by the parser (i.e. the method
        tweaks_and_apis.TweaksParser.parseAPIImportDefinition() is called),
        dependencies are resolved (tweaks_and_apis.TweaksParser.finishImportIf)
        and the api import is added to the parser
        (tweaks_and_apis.TweaksParser.addAPIImport()).

        @param filename is only used to initialize the instance and create
          possible warning and error messages.
        @param linenr is only used to initialize the instance and create
          possible warning and error messages.
        @param name is the name of the api. APIImport.name() will return this
          value and the parser stores the api at this name.
        @param owner is only used to initialize the instance.
        @param parser is the tweaks_and_apis.TweaksParser instance that is used
          to parse the lines and store the parsed api.
        @param lines is an array of lines in the format "key:value". These
          lines should contain a valid definition block for the api import to
          parse. parser.parseAPIImportDefinition() is called for the key, value
          pair of the line.

        @return the tweaks_and_apis.APIImport instance
        """
        api = tweaks_and_apis.APIImport(filename, linenr, name, owner)
        for line in lines:
            (what, value) = line.split(":")
            parser.parseAPIImportDefinition(line, api, what.lower(), value)
        parser.finishDependsOn(api)
        parser.verifyAPIImport(api)
        parser.addAPIImport(api)
        return api

    def testParseTweakDefinition1(self):
        """
        Tests that tweaks_and_apis.TweaksParser parses a simple tweak definition
        correctly.
        """
        parser = self.createParser()
        tweak = self.parseTweakDefinitionLines(
            "foo/module.tweaks", 1, "TWEAK_FOO", "test", parser,
            ["defines:FOO",
             "value:This is some value",
             "depends on:API_BAR",
             "category: footprint, memory,setting",
             "enabled for:desktop, minimal, smartphone, tv, mini",
             "disabled for:none",
             "conflicts with:API_CONFLICT",
             "value for desktop, tv:18"])

        self.assertEqual(tweak.name(), "TWEAK_FOO")
        self.assertEqual(tweak.id(), "TWEAK_FOO")
        self.assertEqual(tweak.type(), "tweak")
        self.assertFalse(tweak.isDeprecated())
        self.assertEqual(tweak.define(), "FOO")
        self.assertEqual(tweak.value(), "This is some value")
        self.assertEqual(tweak.dependsOnStrings(), ["API_BAR"])
        self.assertEqual(tweak.dependencies(), set(["API_BAR"]))
        self.assertEqual(tweak.symbols(), set(["TWEAK_FOO", "FOO"]))
        self.assertEqual(str(tweak.dependsOn()), "(defined API_BAR && API_BAR == YES)")
        profiles = ["desktop", "minimal", "smartphone", "tv", "mini"]
        for profile in profiles:
            self.assertTrue(tweak.isEnabledFor(profile))
            self.assertFalse(tweak.isDisabledFor(profile))
            if profile in ["desktop", "tv", "phone"]:
                self.assertTrue(tweak.hasValueFor(profile))
                self.assertEqual(tweak.valueFor(profile), "18")
            else: self.assertFalse(tweak.hasValueFor(profile))
        self.assertEqual(tweak.categories(), ["footprint", "memory", "setting"])
        self.assertEqual(tweak.conflictsWith(), set(["API_CONFLICT"]))
        self.assertTrue(parser.hasTweak("TWEAK_FOO"))
        self.assertEqual(parser.tweakByName("TWEAK_FOO"), tweak)

    def testParseAPIExportDefinition1(self):
        """
        Tests that tweaks_and_apis.TweaksParser parses a simple api export
        definition correctly.
        """
        parser = self.createParser()
        api = self.parseAPIExportDefinitionLines(
            "foo/module.export", 1, "API_BAR", "test", parser,
            ["defines:BAR",
             "depends on:FEATURE_BAZ",
             "conflicts with:API_CONFLICT"])

        self.assertEqual(api.name(), "API_BAR")
        self.assertEqual(api.id(), "API_BAR")
        self.assertEqual(api.type(), "api-export")
        self.assertFalse(api.isDeprecated())
        self.assertEqual(api.defines(), ["BAR"])
        self.assertEqual(api.dependsOnStrings(), ["FEATURE_BAZ"])
        self.assertEqual(api.dependencies(), set(["FEATURE_BAZ"]))
        self.assertEqual(api.symbols(), set(["API_BAR", "BAR"]))
        self.assertEqual(str(api.dependsOn()), "FEATURE_BAZ == YES")
        self.assertEqual(api.conflictsWith(), set(["API_CONFLICT"]))

    def testParseAPIImportDefinition1(self):
        """
        Tests that tweaks_and_apis.TweaksParser parses a simple api import
        definition correctly.

        And test that the parser's DependentItemStore calculates the
        dependencies correctly.
        """
        parser = self.createParser()
        tweak_baz = self.parseTweakDefinitionLines(
            "foo/module.tweaks", 1, "TWEAK_BAZ", "test", parser,
            ["defines:BAZ",
             "enabled for:none",
             "disabled for:desktop, minimal, smartphone, tv, mini",])
        self.assertEqual(tweak_baz.symbols(), set(["TWEAK_BAZ", "BAZ"]))
        self.assertTrue(parser.hasTweak("TWEAK_BAZ"))
        self.assertEqual(parser.tweakByName("TWEAK_BAZ"), tweak_baz)

        api_bar_im = self.parseAPIImportDefinitionLines(
            "foo/module.import", 1, "API_BAR", "test", parser,
            ["import if:TWEAK_BAZ"])
        self.assertEqual(api_bar_im.name(), "API_BAR")
        self.assertEqual(api_bar_im.id(), "APIImport(API_BAR,foo/module.import:1)")
        self.assertEqual(api_bar_im.type(), "api-import")
        self.assertFalse(api_bar_im.isDeprecated())
        self.assertEqual(api_bar_im.importIfStrings(), ["TWEAK_BAZ"])
        self.assertEqual(api_bar_im.symbols(), set(["API_BAR"]))

        api_bar_ex = self.parseAPIExportDefinitionLines(
            "foo/module.export", 1, "API_BAR", "test", parser,
            ["defines:BAR"])
        self.assertEqual(api_bar_ex.name(), "API_BAR")
        self.assertEqual(api_bar_ex.id(), "API_BAR")
        self.assertEqual(api_bar_ex.type(), "api-export")
        self.assertFalse(api_bar_ex.isDeprecated())
        self.assertEqual(api_bar_ex.defines(), ["BAR"])
        self.assertEqual(api_bar_ex.dependsOnStrings(), [])
        self.assertEqual(api_bar_ex.dependencies(), set())
        self.assertEqual(api_bar_ex.symbols(), set(["API_BAR", "BAR"]))
        # After associating the import with the export, the export's
        # dependencies include now the dependencies of the import:
        api_bar_ex.addImport(api_bar_im)
        api_bar_ex.finishImportIf()
        self.assertEqual(str(api_bar_ex.importIf()), "TWEAK_BAZ == YES")
        self.assertEqual(api_bar_ex.dependencies(), set(["TWEAK_BAZ"]))

        # add tweak and api-export to the parser's item (the api-import is not
        # needed there):
        parser.addItems([tweak_baz, api_bar_ex])
        # verify the items are correctly registered with its symbols and id:
        for item in [tweak_baz, api_bar_ex]:
            self.assertEqual(parser.dependentItemStore().getItem(item.id()), item)
            for symbol in item.symbols():
                self.assertEqual(parser.dependentItemStore().itemIdsBySymbol(symbol), set([item.id()]))

        # no dependency yet calculated:
        self.assertFalse(parser.dependentItemStore().hasDependencyCacheFor(api_bar_ex))

        # calculate dependencies for the api-export:
        parser.dependentItemStore().updateDependencyCache(api_bar_ex)
        self.assertEqual(parser.dependentItemStore().dependencyCache(api_bar_ex), set(["TWEAK_BAZ"]))
        self.assertFalse(parser.dependentItemStore().isIndependentItem(api_bar_ex))
        self.assertTrue(parser.dependentItemStore().dependsOnOther(api_bar_ex, tweak_baz))

    def testResolveDependencies(self):
        """
        Tests that TweaksParser.resolveDependencies() resolves all dependencies
        of a set of tweaks and apis and populates the array
        TweaksParser.__dependent_items in the correct order.
        """
        parser = self.createParser()
        tweak_foo = self.parseTweakDefinitionLines(
            "foo/module.tweaks", 1, "TWEAK_FOO", "test", parser,
            ["defines:FOO",
             "depends on:BAR, API_TEST2",
             "enabled for:desktop, minimal, smartphone, tv, mini",
             "disabled for:none"])
        self.assertEqual(tweak_foo.symbols(), set(["TWEAK_FOO", "FOO"]))
        self.assertEqual(tweak_foo.dependencies(), set(["BAR", "API_TEST2"]))

        api_test1_im = self.parseAPIImportDefinitionLines(
            "foo/module.import", 1, "API_TEST1", "test", parser,
            ["import if:TWEAK_FOO",
             "import if:API_TEST2"])
        self.assertEqual(api_test1_im.name(), "API_TEST1")
        self.assertEqual(api_test1_im.symbols(), set(["API_TEST1"]))
        self.assertEqual(api_test1_im.dependencies(), set(["TWEAK_FOO", "API_TEST2"]))

        api_test1_ex = self.parseAPIExportDefinitionLines(
            "foo/module.export", 1, "API_TEST1", "test", parser,
            ["defines:TEST1"])
        api_test1_ex.addImport(api_test1_im)
        api_test1_ex.finishImportIf()
        self.assertEqual(api_test1_ex.name(), "API_TEST1")
        self.assertEqual(api_test1_ex.id(), "API_TEST1")
        self.assertEqual(api_test1_ex.symbols(), set(["API_TEST1", "TEST1"]))
        self.assertEqual(api_test1_ex.dependencies(), set(["TWEAK_FOO", "API_TEST2"]))

        api_test2_im = self.parseAPIImportDefinitionLines(
            "foo/module.import", 1, "API_TEST2", "test", parser,
            ["import if:BAZ"])
        self.assertEqual(api_test2_im.name(), "API_TEST2")
        self.assertEqual(api_test2_im.symbols(), set(["API_TEST2"]))
        self.assertEqual(api_test2_im.dependencies(), set(["BAZ"]))

        api_test2_ex = self.parseAPIExportDefinitionLines(
            "foo/module.export", 1, "API_TEST2", "test", parser,
            ["defines:TEST2"])
        api_test2_ex.addImport(api_test2_im)
        api_test2_ex.finishImportIf()
        self.assertEqual(api_test2_ex.name(), "API_TEST2")
        self.assertEqual(api_test2_ex.id(), "API_TEST2")
        self.assertEqual(api_test2_ex.symbols(), set(["API_TEST2", "TEST2"]))
        self.assertEqual(api_test2_ex.dependencies(), set(["BAZ"]))

        parser.addItems([tweak_foo, api_test1_ex, api_test2_ex])
        self.assertEqual(parser.tweaks(), [tweak_foo])
        self.assertEqual(parser.api_exports(), [api_test1_ex, api_test2_ex])

        # verify the items are correctly registered with its symbols and id:
        for item in [tweak_foo, api_test1_ex, api_test2_ex]:
            self.assertEqual(parser.dependentItemStore().getItem(item.id()), item)
            for symbol in item.symbols():
                self.assertEqual(parser.dependentItemStore().itemIdsBySymbol(symbol), set([item.id()]))

        parser.dependentItemStore().updateDependencyCache(api_test2_ex)
        self.assertEqual(parser.dependentItemStore().dependencyCache(api_test2_ex), set([]))
        parser.dependentItemStore().updateDependencyCache(api_test1_ex)
        self.assertEqual(parser.dependentItemStore().dependencyCache(api_test1_ex), set(["TWEAK_FOO", "API_TEST2"]))
        parser.dependentItemStore().updateDependencyCache(tweak_foo)
        self.assertEqual(parser.dependentItemStore().dependencyCache(tweak_foo), set(["API_TEST2"]))
        self.assertTrue(parser.dependentItemStore().isIndependentItem(api_test2_ex))
        self.assertTrue(parser.dependentItemStore().dependsOnOther(api_test1_ex, tweak_foo))
        self.assertTrue(parser.dependentItemStore().dependsOnOther(api_test1_ex, api_test2_ex))
        self.assertTrue(parser.dependentItemStore().dependsOnOther(tweak_foo, api_test2_ex))

        # resolveDependencies() is expected to arrange the items in the
        # correct order such that the items that depend on another item are
        # behind that other item - here:
        # api_test1_ex depends on tweak_foo and api_test2_ex
        # tweak_foo depends on api_test2_ex
        # api_test2_ex depends on nothing:
        parser.resolveDependencies()
        self.assertEqual(parser.dependentItems(), [api_test2_ex, tweak_foo, api_test1_ex])

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
            unittest.TestLoader().loadTestsFromTestCase(TestCaseTweaksParser),
            ])
    return suite
