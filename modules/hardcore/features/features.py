import os
import os.path
import sys
import time

# import from modules/hardcore/scripts
if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))

import util
import basesetup


def error(errors):
    for msg in errors:
        print >>sys.stderr, msg

def fixDescription(description):
    description = repr(description)[1:-1].replace(r'"', r'\"')
    if len(description) > 1023:
        return description[0:1020] + "..."
    else:
        return description

class ProfileTemplateActionHandler:
    def __init__(self, profile, features_with_default):
        self.__profile = profile
        self.__features = features_with_default
        self.__define = "MODULES_HARDCORE_FEATURES_PROFILE_%s_H" % profile.upper()

    def __call__(self, action, output):
        import util
        if action == "inclusion guard":
            output.write("#ifndef %s\n#define %s\n" % (self.__define, self.__define))
            return True

        elif action == "inclusion guard end":
            output.write("#endif // %s\n" % self.__define)
            return True

        elif action == "enable profile features":
            longest = max(*map(len, map(util.FeatureOrSystem.name, self.__features)))

            for feature in self.__features:
                if feature.enabledFor(self.__profile):
                    output.write("#define %s %sYES\n" % (feature.name(), " " * (longest - len(feature.name()))))
                elif feature.disabledFor(self.__profile):
                    output.write("#define %s %sNO\n" % (feature.name(), " " * (longest - len(feature.name()))))

            return True

class LegalTemplateActionHandler:
    def __init__(self, feature_def):
        self.__feature_def = feature_def

    def __call__(self, action, output):
        result = ""
        if action == "feature attribution strings":
            # Get all attributions
            all_attributions = dict([(item.name(), item) for item in self.__feature_def.attributions() if item.type() in ("attribution")])


            # Get all attributions without 3 party feature
            attributions_without_3p_feature = dict([(item.name(), item) for item in self.__feature_def.attributions() if item.type() in ("attribution") and item.forceIf() is not None])

            class UsedAttribution:
                def __init__(self, feature, attribution):
                    self.features = []
                    self.attribution = attribution
                    if (feature is not None):
                        self.features.append(feature)
            
            usedAttributions = dict()
            
            #Associate attributions with their features.
            for feature in self.__feature_def.features():
                if feature.isThirdParty():
                    for attributionKey in feature.attributions():
                        if attributionKey != "none":
                            if attributionKey in usedAttributions:
                                usedAttributions[attributionKey].features.append(feature)
                            else:
                                usedAttributions[attributionKey] = UsedAttribution(feature, all_attributions[attributionKey])
            
            #Add attributions that does not have 3 party feature (uses "force if" keyword).
            for attributionKey in attributions_without_3p_feature:
                if attributionKey != "none":
                    usedAttributions[attributionKey] = UsedAttribution(None, attributions_without_3p_feature[attributionKey])

            for usedAttributionKey in sorted(usedAttributions, key=lambda x: len(usedAttributions[x].attribution.attributionString())):
                ifdefString = ""
                ifdefString += "#if "
                
                if usedAttributions[usedAttributionKey].attribution.forceIf() is None:
                    ifdefString += "("            
                    firstFeature = True
                    for feature in usedAttributions[usedAttributionKey].features:
                        if firstFeature == False:
                            ifdefString += " || "
                        firstFeature = False
                        ifdefString += "("
                        firstDefine = True
                        for name in sorted(feature.defines().keys()):
                            if firstDefine == False:
                                ifdefString += " && "
                            ifdefString += "defined(%s)" % name
                            firstDefine = False
                        ifdefString += ")"
                    ifdefString += ") ||"
                                
                if usedAttributions[usedAttributionKey].attribution.forceIf() is not None:
                    ifdefString += "( %s )" %  usedAttributions[usedAttributionKey].attribution.forceIf() + " || "

                ifdefString += "defined(SHOW_ALL_ATTRIBUTIONS)"

                result += ifdefString;
                result +="\n\"<li>\"\n"

                for attributionLine in usedAttributions[usedAttributionKey].attribution.attributionString().split("\\ "):
                    result +="\"%s\"\n" % attributionLine

                result += "\"</li>\"\n"
                result += ifdefString.replace("#if", "#endif //")
                result += "\n"

            output.write(result)
            return True

class HandleTemplateAction:
    def __init__(self, feature_def):
        self.__feature_def = feature_def

    def __call__(self, action, output):
        if action == "deprecated features":
            for feature in self.__feature_def.features():
                output.write(feature.deprecatedClause())
            return True

        elif action == "feature choice check":
            for feature in self.__feature_def.features():
                output.write(feature.decisionClause())
            return True

        elif action == "feature conflict check":
            for feature in self.__feature_def.features():
                output.write(feature.conflictsWithClause())
            return True

        elif action == "feature dependency check":
            for feature in self.__feature_def.features():
                output.write(feature.dependenciesClause())
            return True

        elif action == "feature defines":
            for feature in self.__feature_def.features():
                output.write(feature.controlledDefinesClause())
            for feature in self.__feature_def.features():
                output.write(feature.definesClause())
            return True

        elif action == "feature undefines":
            for feature in self.__feature_def.features():
                output.write("#undef %s\n" % feature.name())
            return True

        elif action == "core version":
            output.write("#define CORE_VERSION_MAJOR %s\n" %
                         self.__feature_def.coreVersionMajor())
            output.write("#define CORE_VERSION_MINOR %s\n" %
                         self.__feature_def.coreVersionMinor())
            if self.__feature_def.milestone() is not None:
                output.write("#define CORE_VERSION_MILESTONE %s\n" %
                             self.__feature_def.milestone())
                output.write("#define CORE_VERSION_STRING \"%s.%s\"\n" %
                             (self.__feature_def.coreVersion(),
                              self.__feature_def.milestone()))
            else:
                output.write("#define CORE_VERSION_STRING \"%s\"\n" %
                             self.__feature_def.coreVersion())
            return True

        elif action == "opera version":
            output.write("#ifndef HC_PLATFORM_DEFINES_VERSION\n")

            if self.__feature_def.forceVersion() is not None:
                output.write("# define VER_NUM_FORCE_STR \"%s\"\n" %
                             self.__feature_def.forceVersion())

            (major,minor) = (self.__feature_def.operaVersionMajor(),
                             self.__feature_def.operaVersionMinor())

            output.write("# define VER_NUM %s\n" % self.__feature_def.operaVersion())
            output.write("# define VER_NUM_MINOR %s\n" % self.__feature_def.operaVersionMinor())
            output.write("# define VER_NUM_MAJOR %s\n" % self.__feature_def.operaVersionMajor())
            output.write("# define VER_NUM_STR \"%s\"\n" % self.__feature_def.operaVersion())
            output.write("# define VER_NUM_INT_STR \"%s%s\"\n" % (major,minor))
            output.write("# define VER_NUM_STR_UNI UNI_L(\"%s\")\n" % self.__feature_def.operaVersion())
            output.write("# define VER_NUM_INT_STR_UNI UNI_L(\"%s%s\")\n" % (major,minor))
            output.write("#endif // HC_PLATFORM_DEFINES_VERSION\n")
            return True

        elif action == "module profiling":
            output.write("#ifndef HC_MODULE_INIT_PROFILING\n")
            output.write("# undef PLATFORM_HC_PROFILE_INCLUDE\n")
            output.write("# define PLATFORM_HC_PROFILE_INIT_START(module_name)\n")
            output.write("# define PLATFORM_HC_PROFILE_INIT_STOP(module_name)\n")
            output.write("# define PLATFORM_HC_PROFILE_DESTROY_START(module_name)\n")
            output.write("# define PLATFORM_HC_PROFILE_DESTROY_STOP(module_name)\n")
            output.write("#endif // HC_MODULE_INIT_PROFILING\n")
            return True

        elif action == "features xml":
            if self.__feature_def.coreVersion() is None:
                core_version_attr = ""
            else:
                core_version_attr = " core-version=\"%s\"" % self.__feature_def.coreVersion()
            output.write("<features%s>\n" % core_version_attr)
            for feature in self.__feature_def.features():
                output.write("\t\t<feature %s>\n" % feature.nameToXMLAttr())
                output.write(feature.descriptionToXML("\t", 2))
                output.write(feature.ownersToXML("\t", 2))

                if feature.defines():
                    output.write("\t\t<defines>\n")
                    for name in sorted(feature.defines().keys()):
                        if feature.defines()[name]:
                            output.write("\t\t\t<define name='%s' value='%s'/>\n" % (name, feature.defines()[name]))
                        else:
                            output.write("\t\t\t<define name='%s'/>\n" % name)
                    output.write("\t\t</defines>\n")

                if feature.undefines():
                    output.write("\t\t<undefines>\n")
                    for name in sorted(feature.undefines().keys()):
                        if feature.undefines()[name]:
                            output.write("\t\t\t<undefine name='%s' value='%s'/>\n" % (name, feature.undefines()[name]))
                        else:
                            output.write("\t\t\t<undefine name='%s'/>\n" % name)
                    output.write("\t\t</undefines>\n")

                if feature.enabledFor() or feature.disabledFor():
                    output.write("\t\t<profiles>\n")
                    for profile in ["desktop", "tv", "smartphone", "minimal", "mini"]:
                        if feature.enabledFor(profile):
                            output.write("\t\t\t<profile name='%s' enabled='yes'/>\n" % profile)
                        elif feature.disabledFor(profile):
                            output.write("\t\t\t<profile name='%s' enabled='no'/>\n" % profile)
                    output.write("\t\t</profiles>\n")

                if feature.dependsOn():
                    output.write("\t\t<dependencies>\n")
                    for dependsOn in sorted(feature.dependsOn()):
                        output.write("\t\t\t<depends-on name='%s'/>\n" % dependsOn)
                    output.write("\t\t</dependencies>\n")

                if feature.requiredBy():
                    output.write("\t\t<dependencies>\n")
                    for requiredBy in sorted(feature.requiredBy()):
                        output.write("\t\t\t<depends-on name='%s'/>\n" % requiredBy)
                    output.write("\t\t</dependencies>\n")

                if feature.conflictsWith():
                    output.write("\t\t<dependencies>\n")
                    for conflictsWith in sorted(feature.conflictsWith()):
                        output.write("\t\t\t<depends-on name='%s'/>\n" % conflictsWith)
                    output.write("\t\t</dependencies>\n")

                output.write("\t</feature>\n")
            output.write("</features>\n")
            return True

        elif action == "GetFeatureCount":
            output.write("\treturn %d;\n" % len(filter(lambda feature: not feature.isDeprecated(), self.__feature_def.features())))
            return True

        elif action == "GetFeature":
            for index, feature in enumerate(filter(lambda feature: not feature.isDeprecated(), self.__feature_def.features())):
                if not feature.isDeprecated():
                    output.write("\tcase %d:\n" % index)
                    output.write("\t\tf.name = UNI_L(\"%s\");\n" % feature.name())
                    output.write("#if %s\n" % feature.enabledCondition())
                    output.write("\t\tf.enabled = TRUE;\n")
                    output.write("#endif // %s\n" % feature.enabledCondition())
                    output.write("\t\tf.description = UNI_L(\"%s\");\n" % fixDescription(feature.description()))
                    output.write("\t\tbreak;\n")

            return True

class GenerateFeaturesH(basesetup.SetupOperation):
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="Feature system")
        self.updateOperasetupOnchange()

    def __call__(self, sourceRoot, feature_def, outputRoot=None, quiet=True):
        """
        Generates the following files in the specified output root:
        - modules/hardcore/features/features.h
        - modules/hardcore/features/features-thirdparty_attributions.inl
        - modules/hardcore/features/profile_{profile}.h for all profiles
        Generates the following files in the specified source root:
        - modules/hardcore/opera/setupinfo_features.cpp
        - modules/hardcore/documentation/features.{core_version}.xml for
          the core_version as given by the specified feature_def.

        @param sourceRoot is the path to the Opera source tree

        @param feature_def specifies a loaded instance of the class
          util.FeatureDefinition

        @param outputRoot is the path in which to generate the output files
          modules/hardcore/features/features.h
          modules/hardcore/features/profile_{profile}.h
          If this argument is None, then
          {sourceRoot}/modules/hardcore/features/ is used.
        """
        self.startTiming()
        if feature_def is None or not feature_def.isLoaded():
            error("the argument feature_def must be a loaded util.FeatureDefinition instance")
            result = 1
        else:
            hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
            featuresDir = os.path.join(hardcoreDir, 'features')
            if outputRoot is None:
                targetDir = featuresDir
            else:
                targetDir = os.path.join(outputRoot, 'modules', 'hardcore', 'features')

            changed = util.makedirs(targetDir)

            # TODO: instead of creating a copy of "features.h" for each
            # profile in
            # opera/profiles/{profile}.[next]/include/modules/hardcore/features/
            # with the same content, try to find a place for that include file
            # which only depends on "current" or "next" mainline-configuration:
            changed = util.readTemplate(
                os.path.join(featuresDir, 'features_template.h'),
                os.path.join(targetDir, 'features.h'),
                HandleTemplateAction(feature_def)) or changed

            changed = util.readTemplate(os.path.join(featuresDir, 'features-thirdparty_attributions_template.inl'),
                                        os.path.join(targetDir, 'features-thirdparty_attributions.inl'),
                                        LegalTemplateActionHandler(feature_def)) or changed

            # TODO: instead of creating profile_XXX.h for all profiles XXX in
            # opera/profile/{profile}.[next]/include/modules/hardcore/features/,
            # only create profile_{profile}.h for the one profile that is
            # needed:
            for profile in util.PROFILES:
                changed = util.readTemplate(
                    os.path.join(featuresDir, 'profile_template.h'),
                    os.path.join(targetDir, 'profile_%s.h' % profile),
                    ProfileTemplateActionHandler(profile, feature_def.featuresWithDefault())) or changed
            if targetDir == featuresDir:
                util.updateModuleGenerated(hardcoreDir,
                                           ['features/features.h',
                                            'features/features-thirdparty_attributions.inl'] +
                                           ['features/profile_%s.h' % profile for profile in util.PROFILES])

            # Ignore changes in setupinfo_features.cpp and documentation files:
            setupinfoDir = os.path.join(hardcoreDir, 'opera')
            documentationDir = os.path.join(hardcoreDir, 'documentation')
            if outputRoot is None:
                setupinfoOutputDir = setupInfoDir
                documentationOutputDir = documentationDir
            else:
                setupinfoOutputDir = os.path.join(outputRoot, 'modules', 'hardcore', 'opera')
                documentationOutputDir = os.path.join(outputRoot, 'modules', 'hardcore', 'documentation')
            # TODO: create a different output file
            # setupinfo_features.{core_version}.cpp for different
            # core-versions - note: this is only required if new features are
            # added or the documentation of a feature is changed.
            util.readTemplate(
                os.path.join(setupinfoDir, 'setupinfo_features_template.cpp'),
                os.path.join(setupinfoOutputDir, 'setupinfo_features_generated.inl'),
                HandleTemplateAction(feature_def),
                hardcoreDir if setupinfoDir == setupinfoOutputDir else None,
                'opera/setupinfo_features.cpp')
            if feature_def.coreVersion() is None:
                features_xml = 'features.xml'
            else: features_xml = 'features.%s.xml' % feature_def.coreVersion()
            util.readTemplate(
                os.path.join(featuresDir, 'features_template.xml'),
                os.path.join(documentationOutputDir, features_xml),
                HandleTemplateAction(feature_def),
                hardcoreDir if documentationDir == documentationOutputDir else None,
                'documentation/%s' % features_xml)

            if changed: result = 2
            else: result = 0
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        withMainlineConfiguration=True,
        description=" ".join(
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
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])

    try:
        feature_def = util.getFeatureDef(sourceRoot, options.mainline_configuration)
        generate_features = GenerateFeaturesH()
        result = generate_features(sourceRoot=sourceRoot,
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
