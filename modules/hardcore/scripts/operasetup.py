# Builtin modules.
import sys
import os.path
import subprocess
import itertools
import re
import time
import optparse
import StringIO

def selftest():
    try:
        p = subprocess.Popen([sys.executable, os.path.join(sys.path[0], 'tests', 'test.py')],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
        # Note: the test-results are reported on stderr:
        (output, error) = p.communicate()
        retcode = p.returncode
        if retcode < 0:
            print >>sys.stdout, output
            print >>sys.stderr, error
            print >>sys.stderr, "Running tests was terminated by signal", -retcode
            sys.exit(1)
        elif retcode > 0:
            print >>sys.stderr, error
            print >>sys.stderr, "ERROR: Testing my modules failed!"
            print >>sys.stderr, "ERROR: operasetup cannot continue."
            print >>sys.stderr, "Please fix the above printed errors and/or failures."
            sys.exit(1)
        else: # retcode == 0: silently continue
            pass
    except OSError, e:
        print >>sys.stderr, "Execution failed:", e
        sys.exit(1)

# Modules in hardcore/scripts/.
import util
import opera_module
import preprocess
import clexer
import strip_ifdefs as stripper
import opera_coreversion
import opera_xmldocumentation
import stash_defines
import generate_selftests
import generate_strings
import basesetup
import buildidentifier
import generate_protobuf
import generate_prefs
import generate_viewers

sys.path.insert(1, os.path.join(sys.path[0], "..", "base"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "features"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "opera"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "actions"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "keys"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "mh"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "dom", "scripts"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "logdoc", "scripts"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "probetools", "scripts"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "protobuf"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "viewers", "src"))
sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "style", "scripts"))

# Modules in the paths we just added to the module path.
import system
import features
import opera
import tweaks_and_apis
import actions
import messages
import keys
import preprocess_probes
import domsetup
import mkmarkup
import make_properties
import make_values

sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))

def error(message):
    print >>sys.stderr, "error: ",message
    sys.exit(1)

def warning(message):
    print >>sys.stderr, "warning: ",message

class CollectStrings(basesetup.SetupOperation):
    def __init__(self, preprocessor):
        basesetup.SetupOperation.__init__(self, message="file build.strings")
        self.__preprocessor = preprocessor

    def __call__(self, sourceRoot, outputRoot, quiet=True):
        self.startTiming()
        inputTokensSequences = []

        for module in opera_module.modules(sourceRoot):
            filename = os.path.join(module.fullPath(), "module.strings")
            if util.fileTracker.addInput(filename):
                f = None
                try:
                    f = open(filename, "rU")
                    inputTokensSequences.append(clexer.tokenize(clexer.split(f.read(),
                                                                             include_ws=False,
                                                                             include_comments=False),
                                                                filename=filename))
                finally:
                    if f: f.close()

        outputTokens = self.__preprocessor.filterTokens(itertools.chain(*inputTokensSequences))

        build_strings = StringIO.StringIO()

        for token, group in itertools.groupby(sorted(filter(clexer.isidentifier, map(str, outputTokens)))):
            build_strings.write(token + "\n")

        buildStringsPath = os.path.join(outputRoot, "data", "strings", "build.strings")

        if util.updateFile(build_strings, buildStringsPath):
            result = 2 # file has changed
        else: result = 0 # no changes
        return self.endTiming(result, quiet=quiet)

class Preprocessor(preprocess.Preprocessor):
    def __init__(self):
        preprocess.Preprocessor.__init__(self)

        self.__profile = None
        self.__featuresEnabledInProfile = set()
        self.__featuresDisabledInProfile = set()
        self.__featuresEnabled = set()
        self.__featuresDisabled = set()
        self.__featuresMissing = set()
        self.__featureDef = None

    def addDefines(self, defines):
        for name, value in defines.iteritems():
            self[name] = value

    def setFeatureDef(self, feature_def):
        self.__featureDef = feature_def

    def leave(self, directive, path, fullPath):
        match = re.match("modules/hardcore/features/profile_([a-z]+).h", path)
        if match:
            self.__profile = match.group(1)

            for feature in self.__featureDef.features():
                if not feature.isDeprecated():
                    if feature.name() in self:
                        macro = self[feature.name()]
                        if macro.value() == "YES" or macro.value() == "1": self.__featuresEnabledInProfile.add(feature)
                        else: self.__featuresDisabledInProfile.add(feature)

        if directive == "#include PRODUCT_FEATURE_FILE":
            for feature in self.__featureDef.features():
                if not feature.isDeprecated():
                    if feature.name() in self:
                        macro = self[feature.name()]
                        if macro.value() == "YES" or macro.value() == "1": self.__featuresEnabled.add(feature)
                        else: self.__featuresDisabled.add(feature)
                    else: self.__featuresMissing.add(feature)

    def featuresEnabledInProfile(self): return sorted(self.__featuresEnabledInProfile)
    def featuresDisabledInProfile(self): return sorted(self.__featuresDisabledInProfile)

    def featuresEnabled(self): return sorted(self.__featuresEnabled)
    def featuresDisabled(self): return sorted(self.__featuresDisabled)

    def generateHTML(self, path, features_xml):
        """
        path is the output directory in which to create the html file

        features_xml is the file features.xml which contains the
            documentation of the features. The output file will
            contain href links to this file for all features.
        """
        if self.__featureDef.coreVersion() is None:
            version = ""
            version_title = ""
        else:
            version = ".%s" % self.__featureDef.coreVersion()
            version_title = ", core version %s" % self.__featureDef.coreVersion()
        if self.__profile is None:
            profile = "unknown"
            filename = "configuration%s.html" % version
        else:
            profile = self.__profile
            filename = "configuration_%s%s.html" % (profile, version)

        if "PROJECT_TITLE" in self:
            title = self["PROJECT_TITLE"].value().strip('"')
        else: title = "%s profile%s" % (profile, version_title)

        output = None
        try:
            output = open(os.path.join(path, filename), "wct")
            print >>output, "<html>"
            print >>output, "  <head>"
            print >>output, "    <title>Feature configuration: %s</title>" % title
            print >>output, "  </head>"
            print >>output, "  <body>"
            print >>output, "    <h1>Feature configuration: %s</h1>" % title

            def generateFeatureList(title, filter_function):
                def generateFeature(feature):
                    print >>output, "      <li><a href=\"%s#%s\">%s</a></li>" % (features_xml, feature.name(), feature.name())

                print >>output, "    <h2>%s</h2>" % title
                print >>output, "    <ul>"
                map(generateFeature,
                    filter(filter_function, self.__featureDef.features()))
                print >>output, "    </ul>"

            if self.__profile is not None:
                generateFeatureList("Features enabled overriding profile",
                                    lambda feature:
                                        feature in self.__featuresDisabledInProfile and feature in self.__featuresEnabled)
                generateFeatureList("Features disabled overriding profile",
                                    lambda feature:
                                        feature in self.__featuresEnabledInProfile and feature in self.__featuresDisabled)
            generateFeatureList("Enabled features without profile default",
                                lambda feature:
                                    feature not in self.__featuresEnabledInProfile and feature not in self.__featuresDisabledInProfile and feature in self.__featuresEnabled)
            generateFeatureList("Disabled features without profile default",
                                lambda feature:
                                    feature not in self.__featuresEnabledInProfile and feature not in self.__featuresDisabledInProfile and feature in self.__featuresDisabled)

            if self.__profile is not None:
                generateFeatureList("Features enabled via profile",
                                    lambda feature:
                                        feature in self.__featuresEnabledInProfile and feature in self.__featuresEnabled)
                generateFeatureList("Features disabled via profile",
                                    lambda feature:
                                        feature in self.__featuresDisabledInProfile and feature in self.__featuresDisabled)

            print >>output, "  </body>"
            print >>output, "</html>"
        finally:
            if output: output.close()

    def generateHeader(self, path):
        # TODO: add the filename to modules.generated?
        util.makedirs(path)

        output = None
        try:
            output = open(os.path.join(path, "features-configured.h"), "wct")
            for feature in self.__featureDef.features():
                print >>output, "#define %s %s" % (feature.name(), (feature in self.__featuresEnabled) and "YES" or "NO")
        finally:
            if output: output.close()

    def generateGogiHeader(self, outputRoot, target, linenumbers = False):
        templateHeader = os.path.join(sourceRoot, "platforms", "gogi", "include", target + "_template.h")
        if os.path.exists(templateHeader):
            gogiTargetDir = os.path.join(outputRoot, "platforms", "gogi", "include");
            util.makedirs(gogiTargetDir)
            targetHeader = os.path.join(gogiTargetDir, target + ".h")
            result = stripper.removeIfdefs(templateHeader, targetHeader, None, False, linenumbers, self)
            if outputRoot == sourceRoot:
                util.updateModuleGenerated(os.path.join(sourceRoot, 'platforms', 'gogi'),
                                           ["include/%s.h" % target])
            return result
        return True;

class IgnoreChangesResult:
    """
    An instance of this class can be used as a wrapper to the "system-functions"
    from hardcore's sub-applications, which are called in this application.
    The convention of the "system-functions" are that the return value
    - 0 indicates success
    - 1 indicates an error
    - 2 indicates that output files have changed.
    This wrapper will reinterpret a return value of 2 as 0, i.e. the caller
    is not notified about changes. This can be used if changes are not
    significant for rebuilding the sources.

    Example:

    my_function = IgnoreChangesResult(opera_xmldocumentation.GenerateVersionJs())\n"""
    def __init__(self, function):
        self.__function = function

    def __call__(self, sourceRoot, outputRoot, quiet):
        result = self.__function(sourceRoot, outputRoot, quiet)
        if result == 2: return 0
        else: return result

def main(argv):
    """Set up an Opera source tree ready to build.

    Create the following output files:

    with commandline argument --platform_product_config generate
    from modules/hardcore/base/pch_template.h:
    -> core/pch.h,
    -> core/pch_jumbo.h,
    -> core/pch_system_includes.h,

    from sourceRoot/{adjunct,modules,platforms}/readme.txt,
    sourceRoot/{adjunct,modules,platforms}/*/module.sources
    -> sourceRoot/{adjunct,modules,platforms}/*/*_jumbo.cpp
    -> sourceRoot/modules/hardcore/setup/plain/sources/sources.all
    -> sourceRoot/modules/hardcore/setup/plain/sources/sources.nopch
    -> sourceRoot/modules/hardcore/setup/plain/sources/sources.pch
    -> sourceRoot/modules/hardcore/setup/plain/sources/sources.pch_system_includes
    -> sourceRoot/modules/hardcore/setup/plain/sources/sources.pch_jumbo
    -> sourceRoot/modules/hardcore/setup/jumbo/sources/sources.all
    -> sourceRoot/modules/hardcore/setup/jumbo/sources/sources.nopch
    -> sourceRoot/modules/hardcore/setup/jumbo/sources/sources.pch
    -> sourceRoot/modules/hardcore/setup/jumbo/sources/sources.pch_system_includes
    -> sourceRoot/modules/hardcore/setup/jumbo/sources/sources.pch_jumbo

    from sourceRoot/modules/hardcore/base/system.txt,
    sourceRoot/modules/hardcore/base/system_template.h,
    sourceRoot/modules/hardcore/base/system.py
    -> sourceRoot/modules/hardcore/base/system.h

    from sourceRoot/{adjunct,modules,platforms}/*/module.actions,
    sourceRoot/modules/hardcore/actions/actions_template.h,
    sourceRoot/modules/hardcore/actions/actions_enum_template.h,
    sourceRoot/modules/hardcore/actions/actions_strings_template.h
    sourceRoot/modules/hardcore/actions/actions_template_template.h:
    -> sourceRoot/modules/hardcore/actions/generated_actions.h
    -> sourceRoot/modules/hardcore/actions/generated_actions_enum.h
    -> sourceRoot/modules/hardcore/actions/generated_actions_strings.h
    -> sourceRoot/modules/hardcore/actions/generated_actions_template.h

    from sourceRoot/{adjunct,modules,platforms}/*/module.markup,
    -> sourceRoot/modules/logdoc/src/html5/elementtypes.h,
    -> sourceRoot/modules/logdoc/src/html5/elementnames.h,
    -> sourceRoot/modules/logdoc/src/html5/attrtypes.h,
    -> sourceRoot/modules/logdoc/src/html5/attrnames.h,

    from the files sourceRoot/modules/hardcore/opera/module.txt,
    sourceRoot/modules/hardcore/opera/blacklist.txt,
    sourceRoot/{adjunct,modules,platforms}/readme.txt,
    sourceRoot/{adjunct,modules,platforms}/*/*_capabilit*.h,
    sourceRoot/{adjunct,modules,platforms}/*/*_module.h:
    sourceRoot/modules/hardcore/opera/opera_template.{h,inc}
    sourceRoot/modules/hardcore/base/capabilities_template.h
    -> outputRoot/modules/hardcore/opera/opera.h
    -> outputRoot/modules/hardcore/opera/hardcore_opera.inc
    -> outputRoot/modules/hardcore/base/capabilities.h

    from the files sourceRoot/{adjunct,modules/platforms}/module.components
    sourceRoot/modules/hardcore/opera/components_template.h
    sourceRoot/modules/hardcore/component/OpComponentCreate_template.inc
    -> outputRoot/modules/hardcore/opera/components.{h,inc}
    -> outputRoot/modules/hardcore/component/OpComponentCreate.inc

    from sourceRoot/{adjunct,modules,platforms}/readme.txt,
    sourceRoot/{adjunct,modules,platforms}/*/module.messages
    and the template files
    sourceRoot/modules/hardcore/mh/messages_template.h,
    sourceRoot/modules/hardcore/mh/message_strings_template.inc:
    -> outputRoot/modules/hardcore/mh/generated_messages.h
    -> outputRoot/modules/hardcore/mh/generated_message_strings.inc

    from sourceRoot/{adjunct,modules,platforms}/readme.txt,
    sourceRoot/{adjunct,modules,platforms}/*/module.keys and
    sourceRoot/modules/hardcore/keys/opkeys_template.h,
    sourceRoot/modules/hardcore/keys/opkeys_mappings_template.cpp:
    -> sourceRoot/modules/hardcore/keys/opkeys.h
    -> sourceRoot/modules/hardcore/keys/opkeys_mappings.inc

    from sourceRoot/modules/hardcore/features/features.{core_version}.txt
    (only needed to warn about deprecated features),
    sourceRoot/{adjunct,modules,platforms}/readme.txt,
    sourceRoot/{adjunct,modules,platforms}/*/module.{tweaks,export,import}
    -> sourceRoot/modules/hardcore/opera/setupinfo_tweaks.cpp
    -> sourceRoot/modules/hardcore/documentation/{apis,tweaks}.{core_version}.xml
    -> outputRoot/modules/hardcore/base/tweaks_and_apis.h
    -> outputRoot/modules/hardcore/tweaks/profile_tweaks_{PROFILES}.h

    from sourceRoot/modules/hardcore/features/festures.{core_version}.txt:
    -> {outputRoot}/modules/hardcore/features/features.h (should be the
    same for different profiles, but it may be different for different
    {core_version}s ...)
    -> {outputRoot}/modules/hardcore/features/profile_{PROFILES}.h
    -> sourceRoot/modules/hardcore/opera/setupinfo_features.cpp
    (todo: generate this in outputRoot and let
    sourceRoot/modules/hardcore/opera/setupinfo_features.cpp #include
    the generated file - the setupinfo may be different for different
    {core_version}s, but it should be the same for different profiles)
    -> sourceRoot/modules/hardcore/documentation/features.{core_version}.xml

    from defines, include-paths, features.{core_version}.txt:
    (sourceRoot/modules/hardcore/documentation/features.{core_version}.xml)
    -> sourceRoot/documentation/configuration_{PROFILE}.{core_version}.html
    -> outputRoot/modules/hardcore/features/features-configured.h
    -> outputRoot/platforms/gogi/include/gogi_{opera,plugin}_api.h

    from defines, include-paths, features.{core_version}.txt,
    sourceRoot/{adjunct,modules,platforms}/*/module.strings:
    -> outputRoot/data/strings/build.strings

    from outputRoot/data/strings/build.strings:
    -> outputRoot/modules/locale/locale-{enum,map}.inc
    -> outputRoot/modules/locale/locale-dbversion.h

    from sourceRoot/*/*.ot
    -> sourceRoot/modules/selftest/generated/ot_*

    use 'git describe --dirty'
    -> outputRoot/modules/hardcore/base/build_identifier.h

    Produces simple messages while doing so.\n"""
    global sourceRoot

    opera_setup = basesetup.SetupOperation(message="Opera setup")
    opera_setup.startTiming()

    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        withMainlineConfiguration=True,
        withPerl=True,
        description="Generates files that are required to build the opera sources.")

    # the defines are stored in a dictionary, so use a callback action:
    def store_define(option, option_str, value, option_parser):
        if "=" in value:
            key, val = value.split("=", 1)
        else:
            key, val = value, None
        option_parser.values.ensure_value(option.dest, dict()).update({key:val})

    def only(option, option_str, value, option_parser):
        for o in dir(option_parser.values):
            if o.startswith('generate_'):
                setattr(option_parser.values, o, False)
        option_parser.values.selftest = False
        option_parser.values.only = True

    option_parser.add_option(
        "--partial-outroot", action="store_true", dest="partialOutputRoot",
        help=" ".join(
            ["Only use the --outroot setting for output files that depend on",
             "the profile/configuration and generate the rest of the files",
             "under the source root."]))

    option_parser.add_option(
        "-D", metavar="DEFINE[=VALUE]", dest="defines", type="string",
        action="callback", callback=store_define,
        help="Add the specified define with the specified value.")

    option_parser.add_option(
        "-I", metavar="PATH", action="append", dest="includes",
        help=" ".join(["Add the specified path to the list of include paths.",
                       "At least the include path to the source root is",
                       "required such that \"core/pch.h\" is found."]))

    option_parser.add_option(
        "--only", action="callback", callback=only,
        help=" ".join(["Disables all steps by default, so that only the steps",
                       "explicitly specified with --generate_* options AFTER",
                       "--only will be enabled. This option also disables",
                       "self-testing of this script."]))

    option_parser.add_option(
        "--selftest", action="store_true", dest="selftest",
        help=" ".join(
            ["Run selftests for this script before doing anything else.",
             "Enabled by default."]))

    option_parser.add_option(
        "--no-selftest", action="store_false", dest="selftest",
        help=" ".join(
            ["Skip self-testing of this script."]))

    option_parser.add_option(
        "--generate_doc_versionjs", action="store_true", dest="generate_doc_versionjs",
        help=" ".join(
            ["Generate the file modules/hardcore/documentation/version.js",
             "from modules/hardcore/version.ini and the template file",
             "modules/hardcore/documentation/version_template.js. Thus the",
             "documentation files (like features.html) can always produce",
             "links to the corresponding generated files (like",
             "features.{core_version}.xml)."]))

    option_parser.add_option(
        "--no-generate_doc_versionjs", action="store_false", dest="generate_doc_versionjs",
        help=" ".join(
            ["Skip generating the version.js file."]))

    option_parser.add_option(
        "--generate_doc_defines", action="store_true", dest="generate_doc_defines",
        help=" ".join(
            ["Generate the file",
             "modules/hardcore/documentation/defines.{core_version}.xml",
             "from modules/hardcore/features/features.{core_version}.txt,",
             "{adjunct,modules,platforms}/*/module.{messages,tweaks,exports}{,.{core_version}}",
             "and the template file",
             "modules/hardcore/base/defines_template.xml."]))
    option_parser.add_option(
        "--no-generate_doc_defines", action="store_false", dest="generate_doc_defines",
        help=" ".join(
            ["Skip generating the defines documentation file."]))

    option_parser.add_option(
        "--generate_system", action="store_true", dest="generate_system",
        help=" ".join(
            ["Generate modules/hardcore/base/system.h from system.txt and",
             "system_template.h in modules/hardcore/base/. This is",
             "equivalent to calling 'python modules/hardcore/base/system.py'"]))
    option_parser.add_option(
        "--no-generate_system", action="store_false", dest="generate_system",
        help=" ".join(["Skip generating modules/hardcore/base/system.h"]))

    option_parser.add_option(
        "--generate_actions", action="store_true", dest="generate_actions",
        help=" ".join(
            ["Generate the files generated_actions.h, generated_actions_enum.h",
             "generated_actions_strings.h and generated_actions_template.h in",
             "modules/hardcore/actions/ from",
             "{adjunct,modules,platforms}/*/module.actions and",
             "actions_template.h, actions_enum_template.h,",
             "actions_strings_template.h and actions_template_template.h in",
             "modules/hardcore/actions/. This is equivalent to calling",
             "'python modules/hardcore/actions/actions.py'"]))
    option_parser.add_option(
        "--no-generate_actions", action="store_false", dest="generate_actions",
        help=" ".join(["Skip generating the actions files."]))

    option_parser.add_option(
        "--generate_dom_atoms", action="store_true", dest="generate_dom_atoms",
        help=" ".join(
            ["Generate the files opatom.h, opatomdata.cpp and opatom.ot in",
             "modules/logdoc/{src,selftest} from modules/dom/src/atoms.txt.",
             "This is the same as calling 'python modules/dom/scripts/atoms.py'"]))
    option_parser.add_option(
        "--no-generate_dom_atoms", action="store_false", dest="generate_dom_atoms",
        help=" ".join(["Skip generating the DOM atoms files."]))
    option_parser.add_option(
        "--generate_dom_prototypes", action="store_true", dest="generate_dom_prototypes",
        help=" ".join(
            ["Generate the files domruntime.h.inc, domruntime.cpp.inc,",
             "domprototypes.cpp.inc and domglobaldata.inc in modules/dom/src",
             "from modules/dom/src/prototypes.txt.  This is the same as calling",
             "modules/dom/scripts/prototypes.py."]))
    option_parser.add_option(
        "--no-generate_dom_prototypes", action="store_false", dest="generate_dom_prototypes",
        help=" ".join(["Skip generating the DOM prototypes files."]))

    option_parser.add_option(
        "--generate_markup_names", action="store_true", dest="generate_markup_names",
        help=" ".join(
            ["Generate the files elementtypes.h, elementnames.h, attrtypes.h",
             "and attrnames.h in modules/logdoc/src/html5/ from",
             "{adjunct,modules,platforms}/*/module.markup. This is the same",
             "as calling 'python modules/logdoc/scripts/mkmarkup.py'"]))
    option_parser.add_option(
        "--no-generate_markup_names", action="store_false", dest="generate_markup_names",
        help=" ".join(["Skip generating the markup name files."]))

    option_parser.add_option(
        "--generate_opera_h", action="store_true", dest="generate_opera_h",
        help=" ".join(
            ["Generate the files modules/hardcore/opera/opera.h,",
             "modules/hardcore/opera/hardcore_opera.inc and",
             "modules/hardcore/base/capabilities.h from the files",
             "modules/hardcore/opera/module.txt,",
             "modules/hardcore/opera/blacklist.txt,",
             "{adjunct,modules,platforms}/readme.txt,",
             "{adjunct,modules,platforms}/*/*_capabilit*.h,",
             "{adjunct,modules,platforms}/*/*_module.h and the templates",
             "modules/hardcore/opera/opera_template.h,",
             "modules/hardcore/opera/opera_template.inc,"
             "modules/hardcore/base/capabilities_template.cpp.",
             "This option is equivalent to calling",
             "'python modules/hardcore/opera/opera.py'."]))
    option_parser.add_option(
        "--no-generate_opera_h", action="store_false", dest="generate_opera_h",
        help=" ".join(["Skip generating the opera/opera.h files."]))

    option_parser.add_option(
        "--generate_keys", action="store_true", dest="generate_keys",
        help=" ".join(
            ["Generate the files opkeys_mappings.cpp and opkeys.h in",
             "modules/hardcore/keys/ from",
             "{adjunct,modules,platforms}/readme.txt and",
             "{adjunct,modules,platforms}/*/module.keys.",
             "This is equivalent to calling",
             "'python modules/hardcore/keys/keys.py'"]))
    option_parser.add_option(
        "--no-generate_keys", action="store_false", dest="generate_keys",
        help=" ".join(["Skip generating the keys files."]))

    option_parser.add_option(
        "--generate_components", action="store_true", dest="generate_components",
        help=" ".join(
            ["Generate the file modules/hardcore/opera/components.h from the ",
             "information in module.components files."]))
    option_parser.add_option(
        "--no-generate_components", action="store_false", dest="generate_components",
        help="Do not re-generate components.h.")

    option_parser.add_option(
        "--generate_pch", action="store_true", dest="generate_pch",
        help=" ".join(
            ["Generate the files core/pch.h, core/pch_system_includes.h and",
             "core/pch_jumbo.h from modules/hardcore/base/pch_template.h.",
             "If you want to generate the pch files, you may specify the",
             "--platform_product_config command line option as well.",
             "This option is equivalent to calling",
             "'python modules/hardcore/base/pchsetup.py'."]))
    option_parser.add_option(
        "--no-generate_pch", action="store_false", dest="generate_pch",
        help=" ".join(["Skip generating the core/pch files."]))

    option_parser.add_option(
        "--pch_template", metavar="FILENAME",
        help=" ".join(
            ["The filename should be a template file relative to source-root,",
             "pch_xxx.h files will be generated from it."]))

    option_parser.add_option(
        "--platform_product_config", metavar="FILENAME",
        help=" ".join(
            ["The filename should be a platform specific header file,",
             "relative to the source-root, which defines the macros",
             "PRODUCT_FEATURE_FILE, PRODUCT_SYSTEM_FILE, PRODUCT_OPKEY_FILE,",
             "PRODUCT_ACTIONS_FILE and PRODUCT_TWEAKS_FILE.",
             "The generated files modules/hardcore/setup/*/include/core/pch*.h",
             "#include the specified file."]))

    option_parser.add_option(
        "--generate_sources", action="store_true", dest="generate_sources",
        help=" ".join(
            ["Generate the files sources.all, sources.nopch, sources.pch,",
             "sources.pch_system_includes and sources.pch_jumbo in both",
             "modules/hardcore/setup/plain/sources and",
             "modules/hardcore/setup/jumbo/sources.",
             "In addition generate the jumbo compile units of all modules.",
             "This option is equivalent to calling",
             "'python modules/hardcore/scripts/sourcessetup.py'."]))
    option_parser.add_option(
        "--no-generate_sources", action="store_false", dest="generate_sources",
        help=" ".join(["Skip generating the sources files."]))

    option_parser.add_option(
        "--generate_messages", action="store_true", dest="generate_messages",
        help=" ".join(
            ["Generate the files",
             "{outroot}/modules/hardcore/mh/generated_messages.h,",
             "{outroot}/modules/hardcore/mh/generated_message_strings.inc",
             "and modules/hardcore/mh/message_strings.cpp from",
             "{adjunct,modules,platforms}/readme.txt,",
             "{adjunct,modules,platforms}/*/module.messages and the template",
             "files modules/hardcore/mh/messages_template.h,",
             "modules/hardcore/mh/message_strings_template.inc and",
             "modules/hardcore/mh/message_strings_template.cpp.",
             "This option is equivalent to calling",
             "'python modules/hardcore/mh/messages.py'.",
             "Note: You can use the option --outroot to specify the output",
             "root directory for the generated files."]))
    option_parser.add_option(
        "--no-generate_messages", action="store_false", dest="generate_messages",
        help=" ".join(["Skip generating the message files."]))

    option_parser.add_option(
        "--generate_tweaks", action="store_true", dest="generate_tweaks",
        help=" ".join(
            ["Generate the files modules/hardcore/opera/setupinfo_tweaks.cpp,",
             "modules/hardcore/documentation/{apis,tweaks}.{core_version}.xml,",
             "{outroot}/modules/hardcore/base/tweaks_and_apis.h and",
             "{outroot}/modules/hardcore/tweaks/profile_tweaks_{PROFILES}.h",
             "from modules/hardcore/features/features.{core_version}.txt,",
             "{adjunct,modules,platforms}/readme.txt,",
             "{adjunct,modules,platforms}/*/module.{tweaks,export,import}",
             "and the template files",
             "modules/hardcore/opera/setupinfo_tweaks_template.cpp,",
             "modules/hardcore/api/apis_template.xml,",
             "modules/hardcore/base/tweaks_and_apis_template.h,",
             "modules/hardcore/tweaks/profile_tweaks_template.h.",
             "This option is equivalent to calling",
             "'python modules/hardcore/base/tweaks_and_apis.py'.",
             "Note: You can use the option --outroot to specify the output",
             "root directory for the generated files."]))
    option_parser.add_option(
        "--no-generate_tweaks", action="store_false", dest="generate_tweaks",
        help=" ".join(["Skip generating the tweaks files."]))

    option_parser.add_option(
        "--generate_features", action="store_true", dest="generate_features",
        help=" ".join(
            ["Generate the files",
             "{outroot}/modules/hardcore/features/features.h,",
             "{outroot}/modules/hardcore/features/profile_{PROFILES}.h,",
             "modules/hardcore/opera/setupinfo_features.cpp and",
             "modules/hardcore/documentation/features.{core_version}.xml",
             "from modules/hardcore/features/features.{core_version}.txt,",
             "and the template files",
             "modules/hardcore/features/features_template.h,",
             "modules/hardcore/features/profile_template.h,",
             "modules/hardcore/opera/setupinfo_features_template.cpp and",
             "modules/hardcore/features/features_template.xml.",
             "This option is equivalent to calling",
             "'python modules/hardcore/features/features.py'.",
             "Note: You can use the option --outroot to specify the output",
             "root directory for the generated files."]))
    option_parser.add_option(
        "--no-generate_features", action="store_false", dest="generate_features",
        help=" ".join(["Skip generating the features files."]))

    option_parser.add_option(
        "--generate_probes", action="store_true", dest="generate_probes",
        help=" ".join(
            ["Generate the files",
             "{outputRoot}/modules/probetools/generated/probedefines.h",
             "and {outputRoot}/modules/probetools/generated/probedefines.cpp",
             "from {adjunct,modules,platforms}/*/module.probes.",
             "Note: You can use the option --outroot to specify the output",
             "root directory for the generated files."]))
    option_parser.add_option(
        "--no-generate_probes", action="store_false", dest="generate_probes",
        help=" ".join(["Skip generating the probedefines files."]))

    option_parser.add_option(
        "--generate_selftests", action="store_true", dest="generate_selftests",
        help=" ".join(
            ["Generate the all selftest source files from all .ot files.",
             "This option is equivalent to calling",
             "'python modules/hardcore/scripts/generate_selftests.py'.",
             "Note: You can use the option --pike to specify the pike",
             "executable."]))
    option_parser.add_option(
        "--no-generate_selftests", action="store_false", dest="generate_selftests",
        help=" ".join(["Skip generating the selftest sources files."]))

    option_parser.add_option(
        "--generate_protobuf", action="store_true", dest="generate_protobuf",
        help=" ".join(
            ["Generate all protocol buffer source files from .proto files.",
             "This option is equivalent to calling",
             "'python modules/hardcore/scripts/generate_protobuf.py'.",
             ]))
    option_parser.add_option(
        "--no-generate_protobuf", action="store_false", dest="generate_protobuf",
        help=" ".join(["Skip generating the protocol buffer source files."]))

    option_parser.add_option(
        "--generate_misc_headers", action="store_true", dest="generate_misc_headers",
        help=" ".join(
            ["Generate miscellaneous headers:",
             "modules/hardcore/features/features-configured.h,",
             "gogi_opera_api.h, gogi_plugin_api.h."
             ]))
    option_parser.add_option(
        "--no-generate_misc_headers", action="store_false", dest="generate_misc_headers",
        help=" ".join(["Skip generating miscellaneous headers."]))

    option_parser.add_option(
        "--generate_prefs", action="store_true", dest="generate_prefs",
        help=" ".join(
            ["Generate all preferences source files from .txt files.",
             "This option is equivalent to calling",
             "'python modules/hardcore/scripts/generate_prefs.py'.",
             ]))
    option_parser.add_option(
        "--no-generate_prefs", action="store_false", dest="generate_prefs",
        help=" ".join(["Skip generating the preferences source files."]))

    option_parser.add_option(
        "--generate_viewers", action="store_true", dest="generate_viewers",
        help=" ".join(
            ["Generate all viewers source files from module.viewers files.",
             "This option is equivalent to calling",
             "'python modules/hardcore/scripts/generate_viewers.py'.",
             ]))
    option_parser.add_option(
        "--no-generate_viewers", action="store_false", dest="generate_viewers",
        help=" ".join(["Skip generating the viewers source files."]))

    option_parser.add_option(
        "--generate_properties", action="store_true", dest="generate_properties",
        help=" ".join(
            ["Generate a few source files in the style module relating to css",
			 "properties and their aliases.",
             "This option is equivalent to calling",
             "'python modules/style/scripts/make_properties.py'.",
             ]))
    option_parser.add_option(
        "--no-generate_properties", action="store_false", dest="generate_properties",
        help=" ".join(["Skip generating the css properties related files."]))

    option_parser.add_option(
        "--generate_stashed_defines", action="store_true", dest="generate_stashed_defines",
        help=" ".join(
            ["Generate the stashed defines header file.",
             "This option is equivalent to calling",
             "'python modules/hardcore/scripts/stash_defines.py'.",
             ]))
    option_parser.add_option(
        "--no-generate_stashed_defines", action="store_false", dest="generate_stashed_defines",
        help=" ".join(["Skip generating the stashed defines header file."]))

    option_parser.add_option(
        "--generate_css_values", action="store_true", dest="generate_css_values",
        help=" ".join(
            ["Generate a two source files in the style module relating to CSS",
			 "values.",
             "This option is equivalent to calling",
             "'python modules/style/scripts/make_values.py'.",
             ]))
    option_parser.add_option(
        "--no-generate_css_values", action="store_false", dest="generate_css_value",
        help=" ".join(["Skip generating the css values related files."]))


    option_parser.add_option(
        "--pike", metavar="PATH_TO_EXECUTABLE",
        help=" ".join(
            ["Specifies the full path to the Pike executable to use on",
             "running parse_tests.pike. If the option is not specified, pike",
             "is expected to be found in one of the directories specified in",
             "the PATH environment variable."]))

    option_parser.add_option(
        "--generate_strings", action="store_true", dest="generate_strings",
        help=" ".join(
            ["Executes data/strings/scripts/makelang.pl to generate the files",
             "locale-enum.inc, locale-map.inc and locale-dbversion.h in",
             "{outputRoot}/modules/locale/ from",
             "{outputRoot}/data/strings/build.strings and the specified",
             "language database files (see --makelang_db).",
             "This option is equivalent to calling",
             "'python modules/hardcore/scripts/generate_strings.py'.",
             "See also options --makelang_db and --makelang_opt.",
             "Note: You can use the option --perl to specify the perl",
             "executable."]))
    option_parser.add_option(
        "--no-generate_strings", action="store_false", dest="generate_strings",
        help=" ".join(["Skip generating the strings files."]))

    option_parser.add_option(
        "--makelang_db", metavar="MAKELANGDB", action="append", dest="makelangDBs",
        help=" ".join(
            ["Specifies the path to the language database file relative to the",
             "source directory. The language database file is passed to",
             "makelang.pl. Multiple language database files can be specified.",
             "If no database file is specified, the default value",
             "data/strings/english.db is used."]))

    option_parser.add_option(
        "--makelang_opt", metavar="OPTION", action="append", dest="makelangOptions",
        help=" ".join(
            ["The specified option is added to the command line arguments on",
             "calling makelang.pl. Makelang is called as 'perl makelang.pl",
             "-d {outputRoot}/modules/locale",
             "-b {outputRoot}/data/strings/build.strings",
             "-c 9 -q -H {OPTION} {MAKELANGDB}'."]))

    option_parser.add_option(
        "--generate_buildid", action="store_true", dest="generate_buildid",
        help=" ".join(
            ["Generate the header file modules/hardcore/base/build_identifier.h",
             "that defines the macro VER_BUILD_IDENTIFIER as the string that",
             "contains the result of 'git describe --dirty'. The generated",
             "header file can be used as a value for",
             "TWEAK_ABOUT_BUILD_NUMBER_HEADER."]))
    option_parser.add_option(
        "--no-generate_buildid", action="store_false", dest="generate_buildid",
        help=" ".join(["Skip generating modules/hardcore/base/build_identifier.h"]))
    option_parser.add_option(
        "--buildid_output", metavar="PATH_TO_FILE",
        help=" ".join(
            ["Use the specified filename for the generated header file",
             "(relative to the outputRoot) instead of the default value",
             "modules/hardcore/base/build_identifier.h.",
             "See option --generate_buildid."]))

    option_parser.add_option(
        "--buildid", metavar="ID",
        help=" ".join(
            ["Use the specified ID instead of the output from 'git describe",
             "--dirty' on genereting the build_identifier.h header file. This",
             "may be useful if you don't want the default output from 'git",
             "describe --dirty' or if you don't have the git repository",
             "available on creating the header file."]))

    option_parser.add_option(
        "--buildid_fallback", metavar="ID",
        help=" ".join(
            ["If neither a --buildid is specified nor a git repository is",
             "available to run 'git describe --dirty', then this ID is used",
             "to generate the build identfier. Specifying this option is",
             "useful if you want to use 'git describe --dirty', but you also"
             "want to cover the case where the build is created outside a git"
             "repository (e.g. on bauhaus)."]))

    option_parser.add_option(
        "--git", metavar="PATH_TO_EXECUTABLE",
        help=" ".join(
            ["Specifies the full path to the git executable to use on running",
             "'git describe --dirty'. If the option is not specified, git is",
             "expected to be found in one of the directories specified in the",
             "PATH environment variable."]))

    option_parser.add_option(
        "--with-linenumbers", action="store_true", dest="linenumbers",
        help="Generate #line directives in generated headers.")

    option_parser.add_option(
        "-v", "--verbose", action="store_true",
        help="Print warnings on parsing the core header files.")

    option_parser.add_option(
        "-b", "--big-endian", action="store_true", dest="bigendian",
        help=" ".join(
            ["Force the byte order to big-endian. The default is to use the",
             "byte order returned by sys.byteorder on the building platform,"
             "but when cross-compiling for a big-endian platform this flag",
             "must be specified."]))

    option_parser.set_defaults(partialOutputRoot=False,
                               defines={},
                               includes=[],
                               makelangOptions=[],
                               only=False,
                               selftest=True,
                               generate_doc_versionjs=True,
                               generate_doc_defines=True,
                               generate_doc_conf=True,
                               generate_system=True,
                               generate_actions=True,
                               generate_dom_atoms=True,
                               generate_dom_prototypes=True,
                               generate_markup_names=True,
                               generate_opera_h=True,
                               generate_keys=True,
                               generate_components=True,
                               generate_pch=False,
                               generate_messages=True,
                               generate_tweaks=True,
                               generate_features=True,
                               generate_probes=True,
                               generate_selftests=True,
                               generate_protobuf=True,
                               generate_misc_headers=True,
                               generate_prefs=True,
                               generate_viewers=True,
                               generate_stashed_defines=True,
                               generate_sources=True,
                               generate_strings=True,
                               generate_buildid=False,
                               generate_properties=True,
                               generate_css_values=True,
                               platform_product_config=None,
                               linenumbers=False,
                               verbose=False,
                               bigendian=False,
                               pch_template=None)

    # It is also allowed to specify the include and define options
    # starting with a slash "/" instead of a dash "-". This keeps
    # compatibility to visual studio like command line arguments. So
    # we have to convert all args "/D..." and "/I..." to "-D..."
    # resp. "-I...":
    def slash_to_dash_arg(arg):
        if arg.startswith("/D") or arg.startswith("/I"):
            return arg.replace("/", "-", 1)
        else:
            return arg

    (options, args) = option_parser.parse_args(args=map(slash_to_dash_arg, argv))

    if options.selftest:
        selftest()
        # Exit if there is nothing more to do, so that it's possible
        # to run --only --selftest without specifying any -I options.
        haveTasks = False
        for o in dir(options):
            if o.startswith('generate_') and getattr(options, o):
                haveTasks = True
                break
        if not haveTasks:
            sys.exit(0)

    # Sanity check that at least the sourceRoot is specified as include path:
    def hasUserIncludePath(sourceRoot):
        sourceRootAbs = os.path.normcase(os.path.abspath(sourceRoot))
        for inc in options.includes:
            if os.path.normcase(os.path.abspath(inc)) == sourceRootAbs:
                return True
        return False

    if not hasUserIncludePath(sourceRoot):
        error("""\
You must specify the same include paths on calling operasetup.py that
you specify on compiling the sources; at least the include path to the
source root is required; otherwise I am not able to find 'core/pch.h'
correctly; e.g. specify\n-I%s\nas command-line argument on calling operasetup.py
""" % sourceRoot);

    try:
        feature_def = util.getFeatureDef(sourceRoot,
                                         options.mainline_configuration)
    except util.UtilError, e:
        error(e.value)

    errors = False
    changed = False

    systemsStart = time.clock()

    # First create the files that do not depend on
    # mainline-configuration nor outputRoot:
    system_functions = [];

    if options.generate_doc_versionjs:
        # GenerateVersionJs generates from modules/hardcore/version.ini
        # -> modules/hardcore/documentation/version.js
        system_functions.append(
            IgnoreChangesResult(opera_xmldocumentation.GenerateVersionJs()))

    if options.generate_pch:
        # opera_module.GeneratePchSetup() generates
        # from modules/hardcore/base/pch_template.h:
        # -> core/pch.h,
        # -> core/pch_jumbo.h,
        # -> core/pch_system_includes.h,
        system_functions.append(
            IgnoreChangesResult(opera_module.GeneratePchSetup(options.platform_product_config, options.pch_template)))

    if options.generate_system:
        # modules/hardcore/base/system.py: system.GenerateSystemH()
        # generates from modules/hardcore/base/system.txt and
        # modules/hardcore/base/system_template.h
        # -> modules/hardcore/base/system.h
        # this is equivalent to calling modules/hardcore/base/system.py
        system_functions.append(system.GenerateSystemH())

    if options.generate_actions:
        # modules/hardcore/actions/actions.py: actions.GenerateActions()
        # generates from {adjunct,modules,platforms}/*/module.actions,
        # modules/hardcore/actions/actions_template.h,
        # modules/hardcore/actions/actions_enum_template.h,
        # modules/hardcore/actions/actions_strings_template.h
        # modules/hardcore/actions/actions_template_template.h:
        # -> modules/hardcore/actions/generated_actions.h
        # -> modules/hardcore/actions/generated_actions_enum.h
        # -> modules/hardcore/actions/generated_actions_strings.h
        # -> modules/hardcore/actions/generated_actions_template.h
        # this is equivalent to calling modules/hardcore/actions/actions.py
        system_functions.append(
            IgnoreChangesResult(actions.GenerateActions()))

    if options.generate_dom_atoms:
        # modules/dom/scripts/domsetup.py: domsetup.AtomsOperation()
        # generates from modules/dom/src/atoms.txt,
        # -> modules/dom/src/opatom.h
        # -> modules/dom/src/opatomdata.cpp
        # -> modules/dom/selftest/opatom.ot
        # this is equivalent to calling modules/dom/scripts/atoms.py
        system_functions.append(
            IgnoreChangesResult(domsetup.AtomsOperation()))

    if options.generate_dom_prototypes:
        # modules/dom/scripts/domsetup.py: domsetup.PrototypesOperation()
        # generates from modules/dom/src/prototypes.txt,
        # -> modules/dom/src/domruntime.h.inc
        # -> modules/dom/src/domruntime.cpp.inc
        # -> modules/dom/src/domprototypes.cpp.inc
        # -> modules/dom/src/domglobaldata.inc
        # this is equivalent to calling modules/dom/scripts/prototypes.py
        system_functions.append(
            IgnoreChangesResult(domsetup.PrototypesOperation()))

    if options.generate_markup_names:
        # modules/logdoc/scripts/mkmarkup.py: mkmarkup.ParseMarkupNames()
        # generates from {adjunct,modules,platforms}/*/module.markup,
        # -> modules/logdoc/src/html5/elementtypes.h,
        # -> modules/logdoc/src/html5/elementnames.h,
        # -> modules/logdoc/src/html5/attrtypes.h,
        # -> modules/logdoc/src/html5/attrnames.h,
        # this is equivalent to calling modules/logdoc/scripts/mkmarkup.py
        system_functions.append(
            IgnoreChangesResult(mkmarkup.ParseMarkupNames(bigendian=options.bigendian)))

    if options.generate_opera_h:
        # modules/hardcore/opera/opera.py: opera.GenerateOperaModule()
        # generates from the files
        # modules/hardcore/opera/module.txt,
        # modules/hardcore/opera/blacklist.txt,
        # {adjunct,modules,platforms}/readme.txt,
        # {adjunct,modules,platforms}/*/*_capabilit*.h,
        # {adjunct,modules,platforms}/*/*_module.h,
        # and the templates modules/hardcore/opera/opera_template.h
        # modules/hardcore/opera/opera_template.inc
        # modules/hardcore/base/capabilities_template.cpp
        # -> modules/hardcore/opera/opera.h
        # -> modules/hardcore/opera/hardcore_opera.inc
        # -> modules/hardcore/base/capabilities.h
        # this may also be done by calling modules/hardcore/opera/opera.py
        system_functions.append(opera.GenerateOperaModule())

    if options.generate_components:
        # modules/hardcore/opera/opera.py: opera.GenerateComponents()
        # generates from the files {adjunct,modules/platforms}/module.components
        # and the templates modules/hardcore/opera/components_template.h
        # and modules/hardcore/component/OpComponentCreate_template.inc
        # -> modules/hardcore/opera/components.h
        # -> modules/hardcore/component/OpComponentCreate.inc
        # this may also be done by calling modules/hardcore/opera/opera.py
        system_functions.append(opera.GenerateComponents())

    if options.generate_protobuf:
        # modules/hardcore/scripts/generate_protobuf.py
        # class GenerateProtobuf calls operasetup() in modules/scope/cppgen/script.py
        # to generate .cpp/h files in modules with module.protobuf
        protobuf = generate_protobuf.GenerateProtobuf()
        system_functions.append(protobuf)

    if options.generate_prefs:
        # modules/hardcore/scripts/generate_prefs.py
        # class GeneratePrefs generates prefs-collection inline sources in
        # modules with module.prefs
        prefs = generate_prefs.GeneratePrefs()
        system_functions.append(prefs)

    if options.generate_viewers:
        # modules/hardcore/scripts/generate_viewers.py
        # class GenerateViewers calls BuildViewers() in modules/viewers/src/viewers.py
        # to generate include files for the viewers module
        viewers = generate_viewers.GenerateViewers()
        system_functions.append(viewers)

    if options.generate_keys:
        # modules/hardcore/keys/keys.py: keys.GenerateOpKeys() generates from
        # {adjunct,modules,platforms}/readme.txt,
        # {adjunct,modules,platforms}/*/module.keys,
        # modules/hardcore/keys/opkeys_template.h,
        # modules/hardcore/keys/opkeys_mappings_template.cpp:
        # -> modules/hardcore/keys/opkeys.h
        # -> modules/hardcore/keys/opkeys_mappings.cpp
        # this is equivalent to calling modules/hardcore/keys/keys.py
        system_functions.append(keys.GenerateOpKeys())

    if options.generate_buildid:
        # modules/hardcore/base/buildidentifier.py:
        # buildidentifier.GenerateBuildIdentifierH() generates from
        # modules/hardcore/base/build_identifier_template.h and
        # `git describe --dirty`:
        # -> modules/hardcore/base/build_identifier.h
        # this is equivalent to calling modules/hardcore/base/buildidentifier.py
        system_functions.append(buildidentifier.GenerateBuildIdentifierH(
                git=options.git,
                buildid=options.buildid,
                fallbackid=options.buildid_fallback,
                output=options.buildid_output))

    if options.generate_properties:
		# modules/style/scripts/make_properties.py:
		# make_properties.GenerateProperties() generates from
		# modules/style/src/css_properties.txt,
		# modules/style/src/css_properties_internal.txt,
		# modules/style/src/css_{aliases,properties,property_strings}_template.h
		# ->
		# modules/style/src/css_{aliases,properties,property_strings}.h
		# this is equivalent to calling modules/style/scripts/make_properties.py
		system_functions.append(make_properties.GenerateProperties())

    if options.generate_css_values:
		# modules/style/scripts/make_values.py:
		# make_values.GenerateValues() generates from
		# modules/style/src/css_values.txt,
		# modules/style/src/css_{values,value_strings}_template.h
		# ->
		# modules/style/src/css_{values,value_strings}.h
		# this is equivalent to calling modules/style/scripts/make_values.py
		system_functions.append(make_values.GenerateValues())

    for systemfn in system_functions:
        result = systemfn(sourceRoot,
                          outputRoot=sourceRoot if options.partialOutputRoot else options.outputRoot,
                          quiet=options.quiet)
        if result == 1: errors = True
        elif result == 2: changed = True

    if options.generate_stashed_defines:
        # modules/hardcore/scripts/stash_defines.py
        # from command line options generate
        # -> core/stashed_defines.h
        stash = stash_defines.StashDefines(options.defines)
        result = stash(sourceRoot, options.outputRoot, quiet=options.quiet)
        if result == 1: errors = True
        elif result == 2: changed = True

    # Now we look at the files, that depend on mainline-configuration:
    system_functions = []

    if options.generate_messages:
        # modules/hardcore/mh/messages.py: function messages.main
        # generates from
        # {adjunct,modules,platforms}/readme.txt,
        # {adjunct,modules,platforms}/*/module.messages
        # and the template files
        # modules/hardcore/mh/messages_template.h,
        # modules/hardcore/mh/message_strings_template.inc and
        # modules/hardcore/mh/message_strings_template.cpp:
        # -> {outputRoot}/modules/hardcore/mh/generated_messages.h
        # -> {outputRoot}/modules/hardcore/mh/generated_message_strings.inc
        # -> modules/hardcore/mh/message_strings.cpp
        # this is equivalent to calling modules/hardcore/mh/messages.py
        system_functions.append(messages.GenerateMessages())

    if options.generate_tweaks:
        # modules/hardcore/base/tweaks_and_apis.py: function
        # tweaks_and_apis.main generates from
        # modules/hardcore/base/features/features.{core_version}.txt,
        # {adjunct,modules,platforms}/readme.txt,
        # {adjunct,modules,platforms}/*/module.{tweaks,export,import}
        # and the template files
        # modules/hardcore/opera/setupinfo_tweaks_template.cpp,
        # modules/hardcore/api/apis_template.xml,
        # modules/hardcore/base/tweaks_and_apis_template.h,
        # modules/hardcore/tweaks/profile_tweaks_template.h:
        # -> modules/hardcore/opera/setupinfo_tweaks.cpp
        # -> modules/hardcore/documentation/{apis,tweaks}.{core_version}.xml
        # -> {outputRoot}/modules/hardcore/base/tweaks_and_apis.h
        # -> {outputRoot}/modules/hardcore/tweaks/profile_tweaks_{PROFILES}.h
        system_functions.append(tweaks_and_apis.GenerateTweaksAndApis())

    if options.generate_features:
        # modules/hardcore/features/features.py: function features.main
        # generates from
        # modules/hardcore/features/festures.{core_version}.txt
        # and the template files
        # modules/hardcore/features/features_template.h
        # modules/hardcore/features/profile_template.h
        # modules/hardcore/opera/setupinfo_features_template.cpp
        # modules/hardcore/features/features_template.xml
        # -> {outputRoot}/modules/hardcore/features/features.h
        # -> {outputRoot}/modules/hardcore/features/profile_{PROFILES}.h
        # -> modules/hardcore/opera/setupinfo_features.cpp
        # (todo: generate this in outputRoot and let
        # opera/setupinfo_features.cpp #include the generated file)
        # -> modules/hardcore/documentation/features.{core_version}.xml
        system_functions.append(features.GenerateFeaturesH())

    if options.generate_probes:
        # modules/probetools/scripts/preprocess_probes.py
        # generated from module.probes
        # -> {outputRoot}/modules/probetools/generated/probedefines.h
        # -> {outputRoot}/modules/probetools/generated/probedefines.cpp
        system_functions.append(preprocess_probes.main)

    if options.generate_doc_defines:
        # modules/hardcore/scripts/opera_xmldocumentation.GenerateDefinesXml
        # generates from
        # modules/hardcore/features/features.{core_version}.txt,
        # {adjunct,modules,platforms}/*/module.{messages,tweaks,exports}{,.{core_version}}
        # and the template file
        # modules/hardcore/base/defines_template.xml
        # -> modules/hardcore/documentation/defines.{core_version}.xml
        system_functions.append(opera_xmldocumentation.GenerateDefinesXml())

    for systemfn in system_functions:
        result = systemfn(sourceRoot,
                          feature_def=feature_def,
                          outputRoot=options.outputRoot,
                          quiet=options.quiet)
        if result == 1: errors = True
        elif result == 2: changed = True
        elif not result: continue # to skip the verbose output
        if options.verbose: # Support for debugging spurious "changes":
            print >>sys.stderr, systemfn.__module__, 'returned', result

    systemsEnd = time.clock()
    systemsTime = "%.2gs" % (systemsEnd - systemsStart)

    ppTime = "no time"
    pp = None
    if not errors and (options.generate_strings or options.generate_doc_conf or options.generate_misc_headers):
        ppStart = time.clock()

        pp = Preprocessor()
        pp.addDefines(options.defines)
        # TODO: preprocessing depends on the set of include-paths,
        # i.e. if multiple mainline-configurations should be processed
        # in one call, we need to be able to specify at least one
        # include path as a template or switch between different
        # include paths on preprocessing core/pch.h:

        # Warn about not-existing include paths (do that after we may have
        # created an include path by generating a file in it):
        for path in options.includes:
            if not os.path.isdir(path):
                warning("The include path '%s' specified as an -I or /I argument to operasetup.py does not exist." % path)

        pp.addUserIncludePaths(options.includes)
        pp.setFeatureDef(feature_def)
        if options.verbose:
            pp.setVerbose()
        else:
            pp.setQuiet()

        # TODO: add a flag to control this.  Just don't conflate it "verbose" !
        # TODO: fix the many errors that we get if we omit this.
        pp.ignoreErrors()

        cachedPath = os.path.join(options.outputRoot, "modules", "hardcore", "scripts", "corepch.cached")
        if not pp.restoreFrom(cachedPath):
            pchPath = os.path.join(options.outputRoot, "core", "pch.h")
            if not os.path.exists(pchPath):
                pchPath = os.path.join(sourceRoot, "core", "pch.h")
            if pp.preprocess(pchPath):
                pp.storeTo(cachedPath)
            else:
                errors = True
                pp = None

        ppEnd = time.clock()
        ppTime = "%.2gs" % (ppEnd - ppStart)

    if pp and options.generate_doc_conf:
        # generate modules/hardcore/documentation/configuration_{profile}.html
        if feature_def.coreVersion() is None:
            features_xml = 'features.xml'
        else:
            features_xml = 'features.%s.xml' % feature_def.coreVersion()
        pp.generateHTML(os.path.join(sourceRoot, "modules", "hardcore", "documentation"), features_xml)

    if pp and options.generate_misc_headers:
        # generate modules/hardcore/features/features-configured.h
        pp.generateHeader(os.path.join(options.outputRoot, "modules", "hardcore", "features"))
        if not pp.generateGogiHeader(options.outputRoot, "gogi_opera_api", options.linenumbers):
            errors = True
        if not pp.generateGogiHeader(options.outputRoot, "gogi_plugin_api", options.linenumbers):
            errors = True

    stringsTime = "no time"
    if pp and options.generate_strings:
        stringsStart = time.clock()
        collect_strings = CollectStrings(pp)
        needToRunStrings = collect_strings(sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet) == 2
        if needToRunStrings:
            strings = generate_strings.GenerateStrings(
                perl = options.perl,
                makelangDBs = options.makelangDBs,
                makelangOptions = options.makelangOptions)

            result = strings(sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
            if result == 1: errors = True
            elif result == 2: changed = True
        stringsEnd = time.clock()
        stringsTime = "%.2gs" % (stringsEnd - stringsStart)

    system_functions = []

    if not errors and options.generate_selftests:
        if pp and "SELFTEST" not in pp:
            # Skip generating selftests if FEATURE_SELFTEST is disabled:
            print "FEATURE_SELFTEST is disabled."
        else:
            system_functions.append(generate_selftests.GenerateSelftests(options.pike))

    if options.generate_sources:
        # opera_module.generateSourcesSetup needs to be called after
        # generating selftest files, because new selftests means new
        # source file, so update sources.* as the last step:

        # scripts/opera_module.py:
        # opera_module.generateSourcesSetup generates from
        # {adjunct,modules,platforms}/readme.txt,
        # {adjunct,modules,platforms}/*/module.sources
        # -> {adjunct,modules,platforms}/*/*_jumbo.cpp
        # -> modules/hardcore/setup/plain/sources/sources.all
        # -> modules/hardcore/setup/plain/sources/sources.nopch
        # -> modules/hardcore/setup/plain/sources/sources.pch
        # -> modules/hardcore/setup/plain/sources/sources.pch_system_includes
        # -> modules/hardcore/setup/plain/sources/sources.pch_jumbo
        # -> modules/hardcore/setup/jumbo/sources/sources.all
        # -> modules/hardcore/setup/jumbo/sources/sources.nopch
        # -> modules/hardcore/setup/jumbo/sources/sources.pch
        # -> modules/hardcore/setup/jumbo/sources/sources.pch_system_includes
        # -> modules/hardcore/setup/jumbo/sources/sources.pch_jumbo
        system_functions.append(opera_module.generateSourcesSetup)

    if len(system_functions) > 0:
        systems2Start = time.clock()
        for systemfn in system_functions:
            result = systemfn(sourceRoot,
                              outputRoot=sourceRoot if options.partialOutputRoot else options.outputRoot,
                              quiet=options.quiet)
            if result == 1: errors = True
            elif result == 2: changed = True

        systems2End = time.clock()
        systemsTime = "%.2gs" % (systemsEnd - systemsStart + systems2End - systems2Start)

    if errors: result=1
    elif changed: result=2
    elif not opera_setup.hasOperaSetup(sourceRoot, options.outputRoot):
        # force updating operasetup.h if the file was not yet written:
        result=2
    else: result=0

    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)

    if only:
        # Suppress total timing when steps are run selectively
        return result
    else:
        opera_setup.setMessage("Opera setup (%s processing basic systems, %s preprocessing core/pch.h, %s processing language strings)" % (systemsTime, ppTime, stringsTime))
        return opera_setup.endTiming(result, sourceRoot, options.outputRoot, options.quiet)

if __name__ == "__main__":
    result = main(sys.argv[1:])
    if result == 2: sys.exit(0)
    else: sys.exit(result)
