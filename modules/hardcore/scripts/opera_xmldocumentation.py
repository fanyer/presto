import sys
import os.path

# import from modules/hardcore/scripts
import opera_coreversion
import util
import basesetup

# import from modules/hardcore/...
sys.path.insert(1, os.path.join(sys.path[0], "..", "mh"))
import messages
sys.path.insert(1, os.path.join(sys.path[0], "..", "base"))
import tweaks_and_apis

class GenerateVersionJs(basesetup.SetupOperation):
    """
    This class generates the file modules/hardcore/documentation/version.js
    from modules/hardcore/version.ini and the template file
    modules/hardcore/documentation/version_template.js. Thus the
    documentation files (like features.html) can always produce links
    to the corresponding generated files (like features.{version}.xml).
    """
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="file version.js")

    class TemplateAction:
        def __init__(self, current_version, next_version):
            self.__current_version = current_version
            self.__next_version = next_version

        def __call__(self, action, output):
            if action == "define current_version":
                output.write("var current_version = \"%s\";\n" % self.__current_version)
                return True

            elif action == "define next_version":
                output.write("var next_version = \"%s\";\n" % self.__next_version)
                return True

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        current_version, milestone = opera_coreversion.getCoreVersion(sourceRoot, "current")
        next_version, milestone = opera_coreversion.getCoreVersion(sourceRoot, "next")
        hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
        documentationDir = os.path.join(hardcoreDir, 'documentation')
        version_js_template = os.path.join(documentationDir, 'version_template.js')
        changes = util.readTemplate(
            version_js_template, os.path.join(documentationDir, 'version.js'),
            GenerateVersionJs.TemplateAction(current_version, next_version),
            hardcoreDir, "documentation/version.js")
        if changes: return 2
        else: return 0


class GenerateDefinesXml(basesetup.SetupOperation):
    """
    This class can be used to generate the file
    modules/hardcore/documentation/defines.{version}.xml from
    modules/hardcore/features/features.{version}.txt,
    {adjunct,modules,platforms}/*/module.{messages,tweaks,exports}{,.{version}}
    and the template file modules/hardcore/base/defines_template.xml.

    An instance class of this instance can be used as one of operasetup.py's
    "system-functions".
    """
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="define documentation")

    class DefinesTemplateAction:
        def __init__(self, feature_def):
            self.__feature_def = feature_def

        def coreVersion(self):
            if self.__feature_def is None: return None
            else: return self.__feature_def.coreVersion()

        def defines(self):
            if self.__feature_def is None: return []
            else: return self.__feature_def.defines()

        def itemsByDefine(self, define):
            if self.__feature_def is None: return []
            else: return self.__feature_def.itemsByDefine(define)

        def __call__(self, action, output):
            if action == "defines xml":
                if self.coreVersion() is None:
                    core_version_attr = ""
                else:
                    core_version_attr = " core-version=\"%s\"" % self.coreVersion()
                output.write("<defines%s>\n" % core_version_attr)
                for define in self.defines():
                    output.write("\t<define name=\"%s\">\n" % define)
                    for item in sorted(self.itemsByDefine(define),
                                       key=lambda x: x.name()):
                        if item.type() is None: type = "unknown"
                        else: type = item.type()
                        output.write("\t\t<item type=\"%s\" name=\"%s\"/>\n" % (type, item.name()))
                    output.write("\t</define>\n")
                output.write("</defines>\n")
                return True

    def __call__(self, sourceRoot, feature_def, outputRoot=None, quiet=True):
        """
        Calling this instance will generate the file
        modules/hardcore/documentation/defines.{version}.xml. It is
        expected that the specified util.FeatureDefinition instance has
        already loaded the corresponding features.{version}.txt file.
        If necessary this method loads and parses also all message,
        tweaks and apis to obtain a complete list of all defines.

        @param sourceRoot is the root directory of the source tree
          that was parsed. Some of the output files are always
          generated relative to the sourceRoot.
        @param feature_def is a loaded instance of the class
          util.FeatureDefinition. That instance collects all defines from
          features.{version}.txt and all module.tweaks and module.exports.
        @param outputRoot is the root directory relative to which to
          generate the output files that depend on the mainline
          configuration that is given in the specified FeatureDefinition
          instance. This class ignores the outputRoot as all output files
          are generated in modules/hardcore/documentation/ relative to the
          sourceRoot.
        @param quiet is ignored.

        @return The convention of the "system-functions" are that the
          return value should be
          - 0 to indicate success
          - 1 to indicate an error
          - 2 to indicate that output files have changed.
        """
        if not feature_def.messagesLoaded():
            messages_parser = messages.MessagesParser(feature_def)
            messages_parser.loadMessages(sourceRoot)
            messages_parser.printWarningsAndErrors()
            if messages_parser.hasErrors(): return 1
        if not feature_def.tweaksAndApisLoaded():
            tweaks_parser = tweaks_and_apis.TweaksParser(feature_def, sourceRoot)
            tweaks_parser.parseModuleTweaksAndApis()
            tweaks_parser.resolveDependencies()
            tweaks_parser.printWarningsAndErrors()
            if tweaks_parser.hasErrors(): return 1

        hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
        if feature_def.coreVersion() is None: defines_xml = 'defines.xml'
        else: defines_xml = 'defines.%s.xml' % feature_def.coreVersion()
        changes = util.readTemplate(
            os.path.join(hardcoreDir, 'base', 'defines_template.xml'),
            os.path.join(hardcoreDir, 'documentation', defines_xml),
            GenerateDefinesXml.DefinesTemplateAction(feature_def),
            hardcoreDir, 'documentation/%s' % defines_xml)
        if changes: return 2
        else: return 0


if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        withMainlineConfiguration=True,
        description=" ".join(
            ["Reads and prints the core-version of the specified mainline",
             "configuration from the file modules/hardcore/version.ini."]))

    option_parser.add_option(
        "--generate_doc_versionjs", action="store_true", dest="generate_doc_versionjs",
        help=" ".join(
            ["Generate the file modules/hardcore/documentation/version.js",
             "from modules/hardcore/version.ini and the template file",
             "modules/hardcore/documentation/version_template.js. Thus the",
             "documentation files (like features.html) can always produce",
             "links to the corresponding generated files (like",
             "features.{version}.xml)."]))
    option_parser.add_option(
        "--no-generate_doc_versionjs", action="store_false", dest="generate_doc_versionjs",
        help=" ".join(
            ["Skip generating the version.js file."]))

    option_parser.add_option(
        "--generate_doc_defines", action="store_true", dest="generate_doc_defines",
        help=" ".join(
            ["Generate the file",
             "modules/hardcore/documentation/defines.{version}.xml",
             "from modules/hardcore/features/features.{version}.txt,",
             "{adjunct,modules,platforms}/*/module.{messages,tweaks,exports}{,.{version}}",
             "and the template file",
             "modules/hardcore/base/defines_template.xml."]))
    option_parser.add_option(
        "--no-generate_doc_defines", action="store_false", dest="generate_doc_defines",
        help=" ".join(
            ["Skip generating the defines documentation file."]))

    option_parser.set_defaults(generate_doc_versionjs=True,
                               generate_doc_defines=True)
    (options, args) = option_parser.parse_args(args=sys.argv[1:])

    result = 0
    if options.generate_doc_versionjs:
        thisresult = GenerateVersionJs().callWithTiming(
            sourceRoot=sourceRoot, quiet=options.quiet)
        if thisresult != 0 and result != 1:
            # only update the exit code in case of an error (thisresult==1)
            # or if the output files have changed (thisresult==2)
            # and never overwrite an error (result=1)
            result = thisresult

    if options.generate_doc_defines:
        try:
            feature_def = util.getFeatureDef(sourceRoot,
                                             options.mainline_configuration)
        except util.UtilError, e:
            print >>sys.stder, e.value
            sys.exit(1)
        thisresult = GenerateDefinesXml().callWithTiming(
            sourceRoot=sourceRoot, feature_def=feature_def, quiet=options.quiet)
        if thisresult != 0 and result != 1:
            # only update the exit code in case of an error (thisresult==1)
            # or if the output files have changed (thisresult==2)
            # and never overwrite an error (result=1)
            result = thisresult

    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
