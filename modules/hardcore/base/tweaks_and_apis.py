import platform
import os.path
from stat import *
import sys
import re
import time
import sys

# import from modules/hardcore/scripts
if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))

import util
import opera_module
import module_parser
import basesetup

from module_parser import ModuleItem
from module_parser import ModuleParser
from module_parser import DependentItem
from module_parser import Expression

class APIImport(module_parser.DependentItem):
    """
    A ModuleItem (or DependentItem) for an API from a module.import. An
    APIImport is always associated to a corresponding APIExport.
    """
    def __init__(self, filename, linenr, name, owners):
        module_parser.DependentItem.__init__(self, filename, linenr, name, owners, "api-import")
        self.__import_if_strings = []
        self.__import_if = module_parser.Expression()

    def id(self):
        """
        The name of an APIImport is not unique, an APIExport or another
        APIImport in the same or a different file may have the same name. So
        use the triple name, filename, line-nr to construct a unique name.
        """
        return "APIImport(%s,%s:%d)" % (self.name(), self.filename(), self.linenr())

    def dependencies(self):
        """
        An APIImport depends on both the "depends on" expression and the
        "import if" expression.
        """
        return DependentItem.dependencies(self) | self.importIf().names()

    def importIf(self):
        """
        Returns the module_parser.Expression that is used for the "import if"
        condition.
        """
        return self.__import_if

    def setImportIf(self, expression):
        """
        Sets the module_parser.Expression that shall be used for the "import if"
        condition. The expression is a result of ModuleParser.parseExpression()
        on the importIfStrings().
        """
        self.__import_if = expression

    def importAlways(self):
        """
        Returns True iff this item shall always be imported.
        """
        return ((len(self.__import_if_strings) == 0) or
                ("always" in map(str.lower, self.__import_if_strings)))

    def importIfStrings(self):
        """
        Returns the list of "import if" strings that were parsed.
        """
        if self.importAlways(): return []
        else: return self.__import_if_strings

    def addImportIfString(self, string):
        """
        Adds another "import if" string. Mutiple "import if" statements will
        be OR-combined in the importIf() Expression.
        """
        string = ", ".join(filter(None, map(str.strip, string.split(","))))
        if string and string not in self.__import_if_strings:
            self.__import_if_strings.append(string)


class APIExport(module_parser.DependentItem, module_parser.ItemWithConflictWith):
    """
    A ModuleItem (or DependentItem) for an API from a module.export.
    """
    def __init__(self, filename, linenr, name, owners):
        DependentItem.__init__(self, filename, linenr, name, owners, "api-export")
        module_parser.ItemWithConflictWith.__init__(self)
        self.__defines = set()
        self.__imports = []
        self.__import_if = module_parser.Expression()

    def symbols(self): return set([self.name()]) | self.__defines
    def defines(self): return list(self.__defines)
    def dependencies(self):
        """
        An APIExport depends on both the "depends on" expression and the
        "import if" expression.
        """
        return DependentItem.dependencies(self) | self.importIf().names()

    def importIf(self):
        """
        Returns the module_parser.Expression that is used for the "import if"
        condition.
        """
        return self.__import_if

    def setImportIf(self, expression):
        """
        Sets the module_parser.Expression that shall be used for the "import if"
        condition. The expression is an OR-combination of the import-if
        Expressions of all associated APIImport items (see addImport())
        """
        self.__import_if = expression

    def importAlways(self):
        """
        Returns True iff this item shall always be imported.
        """
        return self.importIf().isEmpty()

    def finishImportIf(self):
        if len(self.__imports) == 0:
            # Nobody imports this api, so set it to "False"
            self.setImportIf(module_parser.BoolExpression(False))
        else:
            expressions = []
            for api_import in self.__imports:
                if api_import.importIf().isEmpty():
                    # if there is one APIImport which imports this API
                    # unconditionally, we don't need to continue evaluating any
                    # of the other APIImport "import if" expressions
                    self.setImportIf(module_parser.Expression())
                    return
                else: expressions.append(api_import.importIf())
            self.setImportIf(module_parser.OrExpression(expressions))

    def addDefines(self, defines):
        """
        Adds the specified list of defines.

        @param defines is a list of strings.
        """
        self.__defines |= set(defines)

    def addImport(self, api_import): self.__imports.append(api_import)

    def getMainClause(self):
        """
        Returns the main clause that is used to define all associated defines
        in the generated tweaks_and_apis.h
        """
        result, indent = "", ""
        if self.__imports and len(self.__imports) > 0:
            result += "// %s exported at %s:%d\n" % (self.name(), self.filename(), self.linenr())
            for api_import in self.__imports:
                result += "// imported from %s:%d\n" % (api_import.filename(), api_import.linenr())
                if api_import.importIf().isEmpty():
                    result += "// always\n"
                else:
                    result += "// if %s\n" % (str(api_import.importIf()))
            importIfString = str(self.importIf())
            if importIfString == "<empty>": importIfString = None
            if importIfString:
                result += "#if %s\n" % importIfString
                indent += "\t"

            dependsOnString = str(self.dependsOn())
            if dependsOnString == "<empty>": dependsOnString = None
            if dependsOnString:
                result += "#%sif %s\n" % (indent, dependsOnString)
                indent += "\t"

            result += "#%sdefine %s YES\n" % (indent, self.name().replace('\\','\\\\'))

            for define in sorted(self.defines()):
                result += "#%sundef %s\n" % (indent, define)
                result += "#%sdefine %s\n" % (indent, define)

            if dependsOnString:
                indent = indent[:-1]

                result += "#%selse // %s\n" % (indent, dependsOnString)
                result += "#%s\tline %d \"%s\"\n" % (indent, self.linenr(), self.filename())
                result += "#%s\terror \"%s depends on %s\"\n" % (indent, self.name(), dependsOnString)
                result += "#%sendif // %s\n" % (indent, dependsOnString)

            if importIfString:
                result += "#endif // %s\n" % importIfString

            return result + "\n"
        else:
            return ""

    def getUndefineClause(self): return ""

    def getConflictsWithClause(self, tweaks_parser):
        """
        Returns the clause to check for conflicts. This clause is inserted into
        the tweaks_and_apis.h file.
        """
        if self.conflictsWith():
            result = "#if defined %s && %s == YES\n" % (self.name(), self.name())

            for conflictee in self.sortedConflictsWith():
                if conflictee.startswith("TWEAK_"):
                    result += "#\tif defined %s_TWEAKED\n" % conflictee
                    result += "#\t\terror \"%s is imported and conflicts with %s which is tweaked\"\n" % (self.name(), conflictee)
                    result += "#\tendif // defined %s_TWEAKED\n" % conflictee
                elif conflictee.startswith("API_"):
                    result += "#\tif defined %s && %s == YES\n" % (conflictee, conflictee)
                    result += "#\t\terror \"%s is imported and conflicts with %s which is also imported\"\n" % (self.name(), conflictee)
                    result += "#\tendif // %s == YES\n" % conflictee
                elif conflictee.startswith("FEATURE_"):
                    result += "#\tif %s == YES\n" % conflictee
                    result += "#\t\terror \"%s is imported and conflicts with %s which is enabled\"\n" % (self.name(), conflictee)
                    result += "#\tendif // %s == YES\n" % conflictee
                else:
                    tweaks_parser.addParserWarning(self.line(), "%s declares conflict with %s, which is not a tweak, API or feature" % (self.name(), conflictee))

            return result + "#endif // %s == YES\n" % self.name()
        else:
            return ""


class Tweak(DependentItem, module_parser.ItemWithConflictWith):
    """
    A ModuleItem (or DependentItem) for an tweak from a module.tweaks.
    """
    def __init__(self, filename, linenr, name, owners):
        module_parser.DependentItem.__init__(self, filename, linenr, name, owners, "tweak")
        module_parser.ItemWithConflictWith.__init__(self)
        self.__define = None
        self.__value = None
        self.__enabledFor = set()
        self.__disabledFor = set()
        self.__valueFor = {}
        self.__categories = []

    def symbols(self): return set([self.name(), self.define()])

    def define(self): return self.__define
    def value(self): return self.__value
    def valueFor(self, profile): return self.__valueFor.get(profile)
    def hasValueFor(self, profile=None):
        if profile is None: return bool(self.__valueFor)
        else: return profile in self.__valueFor
    def isEnabledFor(self, profile=None):
        if profile is None: return bool(self.__enabledFor)
        else: return profile in self.__enabledFor
    def isDisabledFor(self, profile=None):
        if profile is None: return bool(self.__disabledFor)
        else: return profile in self.__disabledFor
    def categories(self): return self.__categories

    def setDefine(self, define):
        self.__define = define
    def setValue(self, value):
        self.__value = value
    def setValueFor(self, profile, value):
        self.__valueFor[profile] = value
    def addCategory(self, name):
        self.__categories.append(name)
    def addEnabledFor(self, profile):
        self.__enabledFor.add(profile)
    def addDisabledFor(self, profile):
        self.__disabledFor.add(profile)

    def getMainClause(self):
        """
        Returns the main clause that is used to define the tweak and the
        associated define in the generated tweaks_and_apis.h
        """
        result, indent = "", ""
        if self.isDeprecated():
            result += "#if defined %s\n" % self.name()
            result += "#\tline %d \"%s\"\n" % (self.linenr(), self.filename().replace('\\','\\\\'))
            result += "#\terror \"%s: %s\"\n" % (self.name(), self.description())
            result += "#endif // defined %s\n" % self.name()
        else:
            dependsOnString = str(self.dependsOn())
            if dependsOnString != "<empty>":
                result += "#if %s\n" % dependsOnString
                indent = "\t"

            result += "#%sif !defined %s || %s != YES && %s != NO\n" % (indent, self.name(), self.name(), self.name())
            result += "#%s\tline %d \"%s\"\n" % (indent, self.linenr(), self.filename().replace('\\','\\\\'))
            result += "#%s\terror \"You must decide if you want to tweak %s\"\n" % (indent, self.name())
            result += "#%selif %s == YES\n" % (indent, self.name())

            if self.value() is not None:
                result += "#%s\tif !defined %s\n" % (indent, self.define())
                result += "#%s\t\tline %d \"%s\"\n" % (indent, self.linenr(), self.filename().replace('\\','\\\\'))
                result += "#%s\t\terror \"You must set define %s for tweak %s\"\n" % (indent, self.define(), self.name())
                result += "#%s\tendif // !defined %s\n" % (indent, self.define())

            result += "#%s\tdefine %s_TWEAKED\n" % (indent, self.name())

            if self.value() is None:
                result += "#%s\tdefine %s\n" % (indent, self.define())

            result += "#%selse // !defined %s || %s != YES && %s != NO\n" % (indent, self.name(), self.name(), self.name())

            if self.value() is not None:
                result += "#%s\tdefine %s %s\n" % (indent, self.define(), self.value())
            else:
                result += "#%s\tundef %s\n" % (indent, self.define())

            result += "#%sendif // !defined %s || %s != YES && %s != NO\n" % (indent, self.name(), self.name(), self.name())

            if dependsOnString != "<empty>":
                result += "#else // %s\n" % dependsOnString
                result += "#\tundef %s\n" % self.name()
                result += "#\tdefine %s NO\n" % self.name()
                result += "#endif // %s\n" % dependsOnString

        return result + "\n"

    def getConflictsWithClause(self, tweaks_parser):
        """
        Returns the clause to check for conflicts. This clause is inserted into
        the tweaks_and_apis.h file.
        """
        if self.conflictsWith():
            result = "#if defined %s_TWEAKED\n" % self.name()

            for conflictee in self.sortedConflictsWith():
                if conflictee.startswith("TWEAK_"):
                    result += "#\tif defined %s_TWEAKED\n" % conflictee
                    result += "#\t\terror \"%s is tweaked and conflicts with %s which is also tweaked\"\n" % (self.name(), conflictee)
                    result += "#\tendif // defined %s_TWEAKED\n" % conflictee
                elif conflictee.startswith("API_"):
                    result += "#\tif defined %s && %s == YES\n" % (conflictee, conflictee)
                    result += "#\t\terror \"%s is tweaked and conflicts with %s which is imported\"\n" % (self.name(), conflictee)
                    result += "#\tendif // %s == YES\n" % conflictee
                elif conflictee.startswith("FEATURE_"):
                    result += "#\tif %s == YES\n" % conflictee
                    result += "#\t\terror \"%s is tweaked and conflicts with %s which is enabled\"\n" % (self.name(), conflictee)
                    result += "#\tendif // %s == YES\n" % conflictee
                else:
                    tweaks_parser.addParserWarning(self.line(), "%s declares conflict with %s, which is not a tweak, API or feature" % (self.name(), conflictee))

            return result + "#endif // defined %s_TWEAKED\n" % self.name()
        else:
            return ""

    def getUndefineClause(self):
        return "#undef %s\n" % self.name()

CATEGORIES = frozenset(["footprint",
                        "memory",
                        "performance",
                        "standards",
                        "setting",
                        "workaround"])

def escape(string):
    return string.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n")

def escapeXML(string):
    return string.replace("&", "&amp;").replace("\"", "&quot;").replace("'", "&apos;").replace("<", "&lt;").replace(">", "&gt;")

class HandleTemplateAction:
    def __init__(self, tweaks_parser):
        self.__tweaks_parser = tweaks_parser

    def __call__(self, action, output):
        if action == "main clause":
            for dependent_item in self.__tweaks_parser.dependentItems():
                output.write(dependent_item.getMainClause())
            return True

        elif action == "conflicts with clause":
            for tweak in self.__tweaks_parser.tweaks():
                output.write(tweak.getConflictsWithClause(self.__tweaks_parser))
            for api_export in self.__tweaks_parser.api_exports():
                output.write(api_export.getConflictsWithClause(self.__tweaks_parser))
            return True

        elif action == "undefine clause":
            for dependent_item in self.__tweaks_parser.dependentItems():
                output.write(dependent_item.getUndefineClause())
            return True

        elif action == "GetTweakCount":
            output.write("\treturn %d;\n" % len([tweak for tweak in self.__tweaks_parser.tweaks() if not tweak.isDeprecated()]))
            return True

        elif action == "GetTweak":
            index = 0
            for module in self.__tweaks_parser.modules():
                for tweak in module.tweaks():
                    if not tweak.isDeprecated():
                        output.write("\tcase %d:\n" % index)
                        output.write("\t\tt.name = UNI_L(\"%s\");\n" % tweak.name())
                        output.write("#ifdef %s_TWEAKED\n" % tweak.name())
                        output.write("\t\tt.tweaked = TRUE;\n")
                        output.write("#endif // %s_TWEAKED\n" % tweak.name())

                        description = escape(tweak.description())
                        if len(description) > 1024:
                            description = description[:1020] + " ..."
                        output.write("\t\tt.description = UNI_L(\"%s\");\n" % description)

                        if tweak.value():
                            output.write("\t\tt.default_value = UNI_L(\"%s\");\n" % escape(tweak.value()))
                        else:
                            output.write("\t\tt.default_value = UNI_L(\"\");\n")

                        output.write("#ifdef %s\n" % tweak.define());
                        output.write("\t\tt.value = TOSTRING(%s);\n" % tweak.define());
                        output.write("#endif // %s\n" % tweak.define());
                        output.write("\t\tbreak;\n");
                        index += 1

            return True

        elif action == "tweaks xml":
            if self.__tweaks_parser.coreVersion() is None:
                core_version_attr = ""
            else:
                core_version_attr = " core-version=\"%s\"" % self.__tweaks_parser.coreVersion()
            output.write("<tweaks%s>\n" % core_version_attr)
            for module in self.__tweaks_parser.modules():
                if module.tweaks():

                    output.write("\t<module name='%s'>\n" % module.name())
                    for tweak in module.tweaks():
                        output.write("\t\t<tweak %s>\n" % tweak.nameToXMLAttr())
                        output.write(tweak.descriptionToXML("\t", 3))
                        output.write(tweak.ownersToXML("\t", 3))

                        if tweak.categories():
                            output.write("\t\t\t<categories>\n")
                            for category in tweak.categories():
                                output.write("\t\t\t\t<category name='%s'/>\n" % category)
                            output.write("\t\t\t</categories>\n")

                        if tweak.define():
                            output.write("\t\t\t<defines>\n")
                            if tweak.value():
                                output.write("\t\t\t\t<define name='%s' value='%s'/>\n" % (tweak.define(), escapeXML(tweak.value())))
                            else:
                                output.write("\t\t\t\t<define name='%s'/>\n" % tweak.define())
                            output.write("\t\t\t</defines>\n")

                        if tweak.value():
                            if tweak.hasValueFor() or tweak.isDisabledFor():
                                output.write("\t\t\t<profiles>\n")
                                for profile in sorted(util.PROFILES):
                                    if tweak.hasValueFor(profile):
                                        output.write("\t\t\t\t<profile name='%s' enabled='yes' value='%s'/>\n" % (profile, escapeXML(tweak.valueFor(profile))))
                                    elif tweak.isDisabledFor(profile):
                                        output.write("\t\t\t\t<profile name='%s' enabled='no'/>\n" % profile)
                                output.write("\t\t\t</profiles>\n")
                        else:
                            if tweak.isEnabledFor() or tweak.isDisabledFor():
                                output.write("\t\t\t<profiles>\n")
                                for profile in sorted(util.PROFILES):
                                    if tweak.isEnabledFor(profile):
                                        output.write("\t\t\t\t<profile name='%s' enabled='yes'/>\n" % profile)
                                    elif tweak.isDisabledFor(profile):
                                        output.write("\t\t\t\t<profile name='%s' enabled='no'/>\n" % profile)
                                output.write("\t\t\t</profiles>\n")

                        if not tweak.dependsOn().isEmpty():
                            output.write("\t\t\t<dependencies>\n")
                            output.write(tweak.dependsOn().toXML("\t", 4))
                            output.write("\t\t\t</dependencies>\n")

                        output.write("\t\t</tweak>\n")

                    output.write("\t</module>\n")

            output.write("</tweaks>\n")
            return True

        elif action == "apis xml":
            if self.__tweaks_parser.coreVersion() is None:
                core_version_attr = ""
            else:
                core_version_attr = " core-version=\"%s\"" % self.__tweaks_parser.coreVersion()
            output.write("<apis%s>\n" % core_version_attr)
            for module in self.__tweaks_parser.modules():
                if module.apiImports() or module.apiExports():
                    output.write("\t<module name='%s'>\n" % module.name())

                    if module.apiExports():
                        output.write("\t\t<exported-apis>\n")

                        for apiExport in module.apiExports():
                            output.write("\t\t\t<exported-api %s>\n" % apiExport.nameToXMLAttr())
                            output.write(apiExport.descriptionToXML("\t", 4))
                            output.write(apiExport.ownersToXML("\t", 4))

                            if apiExport.defines():
                                output.write("\t\t\t\t<defines>\n")
                                for define in sorted(apiExport.defines()):
                                    output.write("\t\t\t\t\t<define name='%s'/>\n" % define)
                                output.write("\t\t\t\t</defines>\n")

                            if not apiExport.dependsOn().isEmpty():
                                output.write("\t\t\t\t<dependencies>\n")
                                output.write(apiExport.dependsOn().toXML("\t", 5))
                                output.write("\t\t\t\t</dependencies>\n")

                            output.write("\t\t\t</exported-api>\n")

                        output.write("\t\t</exported-apis>\n")

                    if module.apiImports():
                        output.write("\t\t<imported-apis>\n")

                        for apiImport in module.apiImports():
                            output.write("\t\t\t<imported-api %s>\n" % apiImport.nameToXMLAttr())
                            output.write(apiImport.descriptionToXML("\t", 4))
                            output.write(apiImport.ownersToXML("\t", 4))

                            if not apiImport.importIf().isEmpty():
                                output.write("\t\t\t\t<dependencies>\n")
                                output.write(apiImport.importIf().toXML("\t", 5))
                                output.write("\t\t\t\t</dependencies>\n")

                            output.write("\t\t\t</imported-api>\n")

                        output.write("\t\t</imported-apis>\n")

                    output.write("\t</module>\n")

            output.write("</apis>\n")
            return True


class ProfileTemplateActionHandler:
    def __init__(self, profile, tweaks_parser):
        self.__profile = profile
        self.__tweaks_parser = tweaks_parser

    def __call__(self, action, output):
        if action == "inclusion guard":
            output.write("#ifndef MODULES_HARDCORE_TWEAKS_PROFILE_TWEAKS_%s_H\n#define MODULES_HARDCORE_TWEAKS_PROFILE_TWEAKS_%s_H\n" % (self.__profile.upper(), self.__profile.upper()))
            return True

        elif action == "inclusion guard end":
            output.write("#endif // MODULES_HARDCORE_TWEAKS_PROFILE_TWEAKS_%s_H\n" % self.__profile.upper())
            return True

        elif action == "enable profile tweaks":
            longest = 0
            sorted_tweaks = self.__tweaks_parser.sortedTweakNames()
            for name in sorted_tweaks:
                tweak = self.__tweaks_parser.tweakByName(name)
                if tweak.isDeprecated():
                    continue
                elif not tweak.value():
                    if tweak.isEnabledFor(self.__profile) or tweak.isDisabledFor(self.__profile):
                        longest = max(longest, len(name))
                elif tweak.hasValueFor(self.__profile):
                    longest = max(longest, len(name), len(tweak.define()))
                elif tweak.isDisabledFor(self.__profile):
                    longest = max(longest, len(name))

            for name in sorted_tweaks:
                tweak = self.__tweaks_parser.tweakByName(name)
                if tweak.isDeprecated():
                    continue
                elif ((not tweak.value() and tweak.isEnabledFor(self.__profile))
                      or (tweak.value() and tweak.hasValueFor(self.__profile))):
                    value = "YES"
                elif tweak.isDisabledFor(self.__profile):
                    value = "NO"
                else:
                    continue

                output.write("#define %s %s%s\n" % (name, " " * (longest - len(name)), value))
                if tweak.value() and tweak.hasValueFor(self.__profile):
                    output.write("#define %s %s%s\n" % (tweak.define(), " " * (longest - len(tweak.define())), tweak.valueFor(self.__profile)))
            return True


class TweaksParser(ModuleParser):
    def __init__(self, feature_def, sourceRoot):
        """
        @param feature_def is a loaded instance of the class
          util.FeatureDefinition. That instance provides the
          version number for the configuration to load and the
          dictionary of all features by name. If a file
          module.tweaks.{version} exists, the versioned file is
          parsed. Otherwise the file module.tweaks is parsed.
        @param sourceRoot is the root directory of the source tree
          that will be parsed.
        """
        ModuleParser.__init__(self, feature_def)
        self.__tweaks = []
        self.__tweak_by_name = {}
        self.__api_imports = []
        self.__api_exports = []
        self.__api_export_by_name = {}
        self.__modules = list(opera_module.modules(sourceRoot))
        self.__parse_api_import = module_parser.ModuleItemParseDefinition(
            type="API",
            definitions=["import if"],
            create_item=APIImport,
            parse_item_definition=self.parseAPIImportDefinition)
        self.__parse_api_export = module_parser.ModuleItemParseDefinition(
            type="API",
            definitions=["defines?", "depends on", "conflicts with"],
            create_item=APIExport,
            parse_item_definition=self.parseAPIExportDefinition)
        self.__parse_tweak = module_parser.ModuleItemParseDefinition(
            type="TWEAK",
            definitions=["define",
                         "value",
                         "depends on",
                         "category",
                         "enabled for",
                         "disabled for",
                         "conflicts with",
                         "value for \w+(?:\\s*,\\s*\w+)*"],
            create_item=Tweak,
            parse_item_definition=self.parseTweakDefinition)

    def tweaks(self): return self.__tweaks
    def api_exports(self): return self.__api_exports
    def modules(self): return self.__modules
    def sortedTweakNames(self): return sorted(self.__tweak_by_name.keys())
    def tweakByName(self, name): return self.__tweak_by_name[name]
    def hasTweak(self, name): return name in self.__tweak_by_name;

    def parseCategory(self, line, current_tweak, value):
        """
        This function is called by parseTweakDefinition() if a
        "category:" line is parsed.
        @param line is the current util.Line instance
        @param current_tweak is the Tweak instance
        @param value is the category's value, this is expected to be a
          comma separated list of categories.
        """
        for category in [c.strip() for c in value.split(",")]:
            if category in CATEGORIES:
                current_tweak.addCategory(category)
            else:
                self.addParserWarning(line, "%s uses invalid category [%s]" % (current_tweak.name(), category))

    def parseEnabledFor(self, line, current_tweak, value):
        """
        This function is called by parseTweakDefinition() if an
        "enabled for:" line is parsed.
        @param line is the current util.Line instance
        @param current_tweak is the Tweak instance
        @param value is the "enabled for" value, this is expected to
          be a comma separated list of profile names.
        """
        for profile in [p.strip() for p in value.split(",")]:
            if profile in util.PROFILES:
                if current_tweak.isDisabledFor(profile):
                    self.addParserError(line, "%s is both enabled and disabled for profile %s" % (current_tweak.name(), profile))
                else:
                    current_tweak.addEnabledFor(profile)
            elif profile and profile != "none":
                self.addParserError(line, "%s is enabled for an unknown profile '%s'" % (current_tweak.name(), profile))

    def parseDisabledFor(self, line, current_tweak, value):
        """
        This function is called by parseTweakDefinition() if a
        "disabled for:" line is parsed.
        @param line is the current util.Line instance
        @param current_tweak is the Tweak instance
        @param value is the "disabled for" value, this is expected to
          be a comma separated list of profile names.
        """
        for profile in [p.strip() for p in value.split(",")]:
            if profile in util.PROFILES:
                if current_tweak.isEnabledFor(profile):
                    self.addParserError(line, "%s is both enabled and disabled for profile %s" % (current_tweak.name(), profile))
                elif current_tweak.hasValueFor(profile):
                    self.addParserWarning(line, "%s both has a default value for and is disabled for profile %s" % (current_tweak.name(), profile))
                else:
                    current_tweak.addDisabledFor(profile)
            elif profile and profile != "none":
                self.addParserError(line, "%s is disabled for an unknown profile '%s'" % (current_tweak.name(), profile))

    def parseValueFor(self, line, current_tweak, value, profiles):
        """
        This function is called by parseTweakDefinition() if a
        "value for (...):" line is parsed.
        @param line is the current util.Line instance
        @param current_tweak is the Tweak instance
        @param value is the "value for (...)" value.
        @param profiles is the list of profile names for which to use
          the specified value.
        """
        for profile in profiles:
            if profile in util.PROFILES:
                if current_tweak.hasValueFor(profile):
                    self.addParserError(line, "%s has more than one default value for profile %s" % (current_tweak.name(), profile))
                elif current_tweak.isDisabledFor(profile):
                    self.addParserWarning(line, "%s both has a default value for and is disabled for profile %s" % (current_tweak.name(), profile))
                else:
                    current_tweak.setValueFor(profile, value)
            elif profile:
                self.addParserError(line, "%s has a default value for an unknown profile %s" % (current_tweak.name(), profile))

    def parseTweakDefinition(self, line, current_tweak, what, value):
        """
        This method is called by ModuleParser.parseFile() if a
        definition line for a tweak was found.
        @param line is the current util.Line
        @param current_item is the current Message instance
        @param what is the text before the colon in the definition line
        @param value is the text after the colon in the definition line.
        """
        if what != "value": value = value.replace(" and ", ", ")
        if what in "defines": current_tweak.setDefine(value)
        elif what == "value": current_tweak.setValue(value)
        elif what == "depends on":
            current_tweak.addDependsOnString(value)
        elif what == "category":
            self.parseCategory(line, current_tweak, value)
        elif what == "enabled for":
            self.parseEnabledFor(line, current_tweak, value)
        elif what == "disabled for":
            self.parseDisabledFor(line, current_tweak, value)
        elif what == "conflicts with":
            current_tweak.addConflictsWith(value);
        else:
            match = re.match("value for ([a-z]+(?:\\s*,\\s*[a-z]+)*)", what)
            self.parseValueFor(line, current_tweak, value,
                               [p.strip() for p in match.group(1).split(',')])

    def addTweak(self, tweak):
        """
        Adds the specified Tweak instance to the list of tweaks. This
        method should be called after a tweak has been successfully
        parsed. See also parseTweaks().
        @param tweak is the Tweak instance to add.
        """
        self.__tweak_by_name[tweak.name()] = tweak
        self.__tweaks.append(tweak)

    def parseTweaks(self, module):
        """
        Parses module.tweaks or module.tweaks.{version} for the
        specified module. For each tweak a Tweak instance is created,
        stored in the __tweaks list, in the module_parser.DependentItemStore
        (see ModuleParser.addItem()) and in the __tweak_by_name dictionaries.

        @param module is an instance of the class opera_module.Module.

        @return a list of Tweak instances that are found in the
          module's module.tweaks file.
        """
        self.setParseDefinition([self.__parse_tweak])
        result = self.parseFile(module.getTweaksFile())

        # verify the sanity of the parsed tweak definitions:
        for tweak in result:
            if tweak.name().count("_") < 2:
                self.addParserError(tweak.line(), "the tweak %s has an invalid name" % tweak.name())

            if self.hasTweak(tweak.name()):
                self.addParserError(tweak.line(), "the tweak %s is specified multiple times" % tweak.name())

            if not tweak.hasDescription():
                self.addParserError(tweak.line(), "the tweak %s has no description" % tweak.name())

            if not tweak.isDeprecated():
                if not tweak.define():
                    self.addParserError(tweak.line(), "the tweak %s has no define set" % tweak.name())
                if not tweak.categories():
                    self.addParserWarning(tweak.line(), "the tweak %s has no category set" % tweak.name())
                if not tweak.owners():
                    self.addParserWarning(tweak.line(), "the tweak %s has no owner" % tweak.name())
                if tweak.hasValueFor() and not tweak.value():
                    self.addParserError(tweak.line(), "the feature tweak %s has non-empty 'Value for' field" % tweak.name())
                if tweak.value() and tweak.isEnabledFor():
                    self.addParserError(tweak.line(), "the value tweak %s has non-empty 'Enabled for' field" % tweak.name())

            self.addTweak(tweak)

        return result

    def parseAPIImportDefinition(self, line, current_api, what, value):
        """
        This method is called by ModuleParser.parseFile() if a
        definition line for an api import was found.
        @param line is the current util.Line
        @param current_item is the current Message instance
        @param what is expected to be "import if"
        @param value is the condition for the "import if" definition
        """
        assert what == "import if"
        current_api.addImportIfString(value.replace(" and ", ", "))

    def addAPIImport(self, api_import):
        """
        Adds the specified APIImport instance to the list of imports. This
        method should be called after an api-import has been successfully
        parsed. See also parseAPIImport().
        @param api_import is the APIImport instance to add.
        """
        self.__api_imports.append(api_import)

    def parseAPIImport(self, module):
        """
        Parses module.import or module.import.{version} for the
        specified module. For each api import an APIImport instance is
        created, stored in the __api_imports list.

        @param module is an instance of the class opera_module.Module.

        @return a list of APIImport instances that are found in the
          module's module.import file.
        """
        self.setParseDefinition([self.__parse_api_import])
        result = self.parseFile(module.getAPIImportFile())

        # verify the sanity of the parsed api import definitions:
        for api_import in result:
            self.verifyAPIImport(api_import)
            self.addAPIImport(api_import)
        return result

    def verifyAPIImport(self, api_import):
        """
        Verfies the sanity of the specified APIImport. This method should be
        for each APIImport that was parsed.
        """
        if not api_import.hasDescription():
            self.addParserError(api_import.line(), "the import of %s has no description" % api_import.name())

        if api_import.isDeprecated():
            self.addParserWarning(api_import.line(), "no need to deprecate an api import, please remove the import statement" % api_import.name())
        elif not api_import.owners():
            self.addParserWarning(api_import.line(), "the import of %s has no owner" % api_import.name())

        try:
            api_import.setImportIf(self.parseExpression(
                    api_import.line(), api_import.importIfStrings()))
        except module_parser.ParseExpressionException, error:
            self.addParserError(api_import.line(), "%s: while parsing import if: %s" % (api_import.name(), error))

    def parseAPIExportDefinition(self, line, current_api, what, value):
        """
        This method is called by ModuleParser.parseFile() if a
        definition line for an api export was found.
        @param line is the current util.Line
        @param current_item is the current Message instance
        @param what is the text of an api export definitions before
          the colon.
        @param value is the text of the api export definition after
          the colon.
        """
        value = value.replace(" and ", ", ")
        if what in "defines":
            current_api.addDefines(filter(None, [d.strip() for d in value.split(",")]))
        elif what == "depends on":
            current_api.addDependsOnString(value)
        else:
            assert what == "conflicts with"
            current_api.addConflictsWith(value)

    def addAPIExport(self, api_export):
        """
        Adds the specified APIExport instance to the list of exports. This
        method should be called after an api-export has been successfully
        parsed. See also parseAPIExport().
        @param api_export is the APIExport instance to add.
        """
        self.__api_export_by_name[api_export.name()] = api_export
        self.__api_exports.append(api_export)

    def parseAPIExport(self, module):
        """
        Parses module.export or module.export.{version} for the
        specified module. For each api export an APIExport instance is
        created, stored in the __api_exports list and in the
        __api_export_by_define dictionary.

        @param module is an instance of the class opera_module.Module.

        @return a list of APIExport instances that are found in the
          module's module.export file.
        """
        self.setParseDefinition([self.__parse_api_export])
        result = self.parseFile(module.getAPIExportFile())

        for api_export in result:
            self.verifyAPIExport(api_export)
            self.addAPIExport(api_export)

        return result

    def verifyAPIExport(self, api_export):
        """
        Verfies the sanity of the specified APIExport. This method should be
        for each APIExport that was parsed.
        """
        if not api_export.hasDescription():
            self.addParserError(api_export.line(), "the API export %s has no description" % api_export.name())

        if self.__api_export_by_name.has_key(api_export.name()):
            self.addParserError(api_export.line(), "the API export %s is specified multiple times" % api_export.name())

        if not api_export.isDeprecated():
            if not api_export.defines():
                self.addParserError(api_export.line(), "the API export %s has no define set" % api_export.name())

            if not api_export.owners():
                self.addParserWarning(api_export.line(), "the API export %s has no owner" % api_export.name())

    def parseModuleTweaksAndApis(self):
        """
        Parses the module.{tweaks,import,export}.{version} of all
        modules and stores the result in the instances lists and
        dictionaries.
        """
        for module in self.modules():
            module.setTweaks(self.parseTweaks(module))
            module.setAPIImports(self.parseAPIImport(module))
            module.setAPIExports(self.parseAPIExport(module))
        self.__tweaks.sort(key=ModuleItem.name)
        self.__api_imports.sort(key=ModuleItem.id)
        self.__api_exports.sort(key=ModuleItem.name)

        for api_import in self.__api_imports:
            if api_import.name() in self.__api_export_by_name:
                api_export = self.__api_export_by_name[api_import.name()]
                api_export.addImport(api_import)
            else:
                self.addParserWarning(api_import.line(), "the API %s was never exported" % api_import.name())
        # to get the dependencies correct, we need to update the ModuleParser's
        # DependentItemStore after we associated the APIImport instances with
        # the APIExport instances:
        self.addItems(self.__tweaks)
        self.addItems(self.__api_exports)
        for api_export in self.__api_exports:
            api_export.finishImportIf()
        self.featureDefinition().setTweaksAndApisLoaded()

    def writeOutputFiles(self, sourceRoot, outputRoot):
        """
        Writes all the output files
        - modules/hardcore/base/tweaks_and_apis.h
        - modules/hardcore/tweaks/profile_tweaks_XXX.h
        to the specified outputRoot (where XXX iterates over all
        profiles).

        And writes the output files
        - modules/hardcore/opera/setupinfo_tweaks.cpp
        - modules/hardcore/documentation/apis.{version}.xml
        - modules/hardcore/documentation/tweaks.{version}.xml
        to the specified sourceRoot.

        This method should only be called after the methods
        parseModuleTweaksAndApis() and resolveDependencies() have
        finished.

        @param sourceRoot is the root directory of the source tree
          that was parsed. Some of the output files are always
          generated relative to the sourceRoot.
        @param outputRoot is the root directory relative to which to
          generate the output files that depend on the mainline
          configuration that is given in the FeatureDef which was
          specified in the TweaksParser's constructor.

        @return True if the output files (with exception of the
          documentation files) have changed).
        """
        hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')

        # Check if local-tweaks.h file is present in sourceRoot, and
        # if it isn't create it.
        localTweaks = os.path.join(sourceRoot, 'local-tweaks.h')
        try:
            mode = os.stat(localTweaks)[ST_MODE]
            #no need to do more; file exists
        except Exception, err:
            #file does not exist, create it
            localTweaksFile = open(localTweaks,'w')
            print >> localTweaksFile, """\
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** This file is included after the PRODUCT_TWEAKS_FILE and can be used
** to override any standard tweaks for your private build. This file
** is not under verison control.
*/

"""
            localTweaksFile.close()

        # Don't update 'changed' for this.  If it has changed, but
        # none of the headers above, we apparently just changed
        # something insignificant.
        operaDir = os.path.join(hardcoreDir, 'opera')
        if outputRoot is None:
            setupinfoOutputDir = operaDir
        else:
            setupinfoOutputDir = os.path.join(outputRoot, 'modules', 'hardcore', 'opera')
        util.readTemplate(os.path.join(operaDir, 'setupinfo_tweaks_template.cpp'),
                          os.path.join(setupinfoOutputDir, 'setupinfo_tweaks_generated.inl'),
                          HandleTemplateAction(self),
                          hardcoreDir if setupinfoOutputDir == operaDir else None,
                          'opera/setupinfo_tweaks.cpp')

        # Don't update 'changed' for these: it typically just means
        # that every time you've manually changed the generated XML
        # files you need to recompile all the source code.
        apiDir = os.path.join(hardcoreDir, 'api')
        docDir = os.path.join(hardcoreDir, 'documentation')
        tweaksDir = os.path.join(hardcoreDir, 'tweaks')
        baseDir = os.path.join(hardcoreDir, 'base')
        if outputRoot is None:
            docOutputDir = docDir
        else:
            docOutputDir = os.path.join(outputRoot, 'modules', 'hardcore', 'documentation')
        if self.coreVersion() is None:
            apis_xml = 'apis.xml'
            tweaks_xml = 'tweaks.xml'
        else:
            apis_xml = 'apis.%s.xml' % self.coreVersion()
            tweaks_xml = 'tweaks.%s.xml' % self.coreVersion()
        util.readTemplate(os.path.join(apiDir, 'apis_template.xml'),
                          os.path.join(docOutputDir, apis_xml),
                          HandleTemplateAction(self),
                          hardcoreDir if docOutputDir == docDir else None,
                          'documentation/%s' % apis_xml)
        util.readTemplate(os.path.join(tweaksDir, 'tweaks_template.xml'),
                          os.path.join(docOutputDir, tweaks_xml),
                          HandleTemplateAction(self),
                          hardcoreDir if docOutputDir == docDir else None,
                          'documentation/%s' % tweaks_xml)

        if outputRoot is None:
            targetTweaks = baseDir
            targetProfile = tweaksDir
        else:
            targetTweaks = os.path.join(outputRoot, 'modules', 'hardcore', 'base')
            targetProfile = os.path.join(outputRoot, 'modules', 'hardcore', 'tweaks')
            util.makedirs(targetTweaks)
            util.makedirs(targetProfile)

        changed = util.readTemplate(os.path.join(baseDir, 'tweaks_and_apis_template.h'),
                                    os.path.join(targetTweaks, 'tweaks_and_apis.h'),
                                    HandleTemplateAction(self))

        for profile in util.PROFILES:
            changed = util.readTemplate(os.path.join(tweaksDir, 'profile_tweaks_template.h'),
                                        os.path.join(targetProfile, 'profile_tweaks_%s.h' % profile),
                                        ProfileTemplateActionHandler(profile, self)) or changed

        # only update module.generated if we generated the output
        # files relative to the sourceRoot:
        if outputRoot is None or outputRoot == sourceRoot:
            util.updateModuleGenerated(hardcoreDir,
                                       ['base/tweaks_and_apis.h'] +
                                       ['tweaks/profile_tweaks_%s.h' % profile for profile in util.PROFILES])
        return changed

class GenerateTweaksAndApis(basesetup.SetupOperation):
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="Tweaks and APIs system")
        self.updateOperasetupOnchange()

    def __call__(self, sourceRoot, feature_def, outputRoot=None, quiet=True):
        self.startTiming()
        parseStart = time.clock()
        tweaks_parser = TweaksParser(feature_def, sourceRoot)
        tweaks_parser.parseModuleTweaksAndApis()
        parse_time = "%.2gs" % (time.clock() - parseStart)

        if not quiet:
            for warning in tweaks_parser.warnings():
                print warning

        dependStart = time.clock()
        tweaks_parser.resolveDependencies()
        depend_time = "%.2gs" % (time.clock() - dependStart)

        output_time = "none"

        if tweaks_parser.printErrors():
            result = 1
        else:
            outputStart = time.clock()
            changed = tweaks_parser.writeOutputFiles(sourceRoot, outputRoot)
            output_time = "%.2gs" % (time.clock() - outputStart)

            if changed: result = 2
            else: result = 0

        self.setMessage("Tweaks and APIs system (timing: %s parsing, %s calculating dependencies, %s generating code)" % (parse_time, depend_time, output_time))
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        withMainlineConfiguration=True,
        description=" ".join(
            ["Generate the files modules/hardcore/opera/setupinfo_tweaks.cpp,",
             "modules/hardcore/documentation/{apis,tweaks}.{version}.xml,",
             "{outroot}/modules/hardcore/base/tweaks_and_apis.h and",
             "{outroot}/modules/hardcore/tweaks/profile_tweaks_{PROFILES}.h",
             "from modules/hardcore/features/features.{version}.txt,",
             "{adjunct,modules,platforms}/readme.txt,",
             "{adjunct,modules,platforms}/*/module.{tweaks,export,import}",
             "and the template files",
             "modules/hardcore/opera/setupinfo_tweaks_template.cpp,",
             "modules/hardcore/api/apis_template.xml,",
             "modules/hardcore/base/tweaks_and_apis_template.h,",
             "modules/hardcore/tweaks/profile_tweaks_template.h.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    try:
        feature_def = util.getFeatureDef(sourceRoot, options.mainline_configuration)
        generate_tweaks_apis = GenerateTweaksAndApis()
        result = generate_tweaks_apis(sourceRoot=sourceRoot,
                                      feature_def=feature_def,
                                      outputRoot=options.outputRoot,
                                      quiet=options.quiet)
        if options.timestampfile and result != 1:
            util.fileTracker.finalize(options.timestampfile)
        if options.make and result == 2: sys.exit(0)
        else: sys.exit(result)
    except util.UtilError, e:
        print >>sys.stderr, e.value
        sys.exit(1)
