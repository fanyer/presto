import platform
import re
import os.path
import errno
import itertools
import sys

FIELDS = { "system": ["defines",
                      "undefines",
                      "depends on",
                      "required by",
                      "group"],
           "type": ["check if"],
           "macro": ["check if"],
           "feature": ["defines",
                       "undefines",
                       "depends on",
                       "required by",
                       "conflicts with",
                       "enabled for",
                       "disabled for",
                       "attribution",
                       "group"],
            "attribution" : ["text",
                         "force if",
                         "required by"]
           }

PROFILES = frozenset(["desktop", "minimal", "smartphone", "tv", "mini"])


class Line:
    """
    This class represents a single line of a file. It is created by
    specifying the filename, the line number and the text of the
    line.

    f = open(filename)
    lines = [util.Line(filename, nr+1, text.rstrip("\r\n")) for nr,text in enumerate(f)
    f.close()

    @param filename is the filename of the file.
    @param linenr is the line number of the specified text line.
    @param text is one line of text of the file.
    """
    def __init__(self, filename, linenr, text=''):
        self.filename = filename
        self.linenr = linenr
        self.__text = text

    def __str__(self):
        return self.__text

    def __repr__(self):
        return "Line(%r)" % self.__text

    def isComment(self):
        """
        Returns true if the line starts with a comment character '#'
        or if there is only whitespace before that comment character.

        @return true if the line is a comment line.
        """
        return self.__text.lstrip().startswith('#')

    def isEmptyOrWhitespace(self):
        """
        Returns true if the line is empty or if it only contains
        whitespace characters.
        """
        return len(self.__text) == 0 or self.__text.isspace()

    def startsWithSpace(self):
        """
        Returns true if the first character of the line is a space.
        """
        return self.__text[0].isspace()


class SystemMessage:
    """
    This class can be used as a base-class for a warning or error
    message. On converting the message to a string it is formatted
    such that it is recognized by a typical platform ide (e.g. emacs
    on linux and visual studio on windows) as a warning or error and
    the ide can easily load the specified file.

    @param type is the type of message (typically "warning" or
      "error")
    @param filename is the name of the file to which the message
      refers.
    @param linenr is the number of the line in the specified file to
      which the message refers (the number of the first line is 1).
    @param message is the text of the message.
    """
    def __init__(self, type, filename, linenr, message):
        self.__type = type
        self.__filename = filename
        self.__linenr = linenr
        self.__message = message

    def __cmp__(self, other):
        """
        Orders the messages by filename and line number.
        """
        return cmp((self.__filename, self.__linenr), (other.__filename, other.__linenr))

    def format(self):
        """
        Returns the format for printing the message. The format
        depends on the platform.
        """
        if platform.system() in ("Windows", "Microsoft"):
            return "%(filename)s(%(linenr)d) : %(type)s C0000: %(message)s"
        else: return "%(filename)s:%(linenr)d: %(type)s: %(message)s"

    def __str__(self):
        """
        Conversts the message according to the specified format to a
        string.
        """
        return self.format() % { "filename": self.__filename,
                                 "linenr": self.__linenr,
                                 "type": self.__type,
                                 "message": self.__message }

class Warning(SystemMessage):
    """
    This class can be used to print a warning which is formatted like
    a compiler warning.
    """
    def __init__(self, filename, linenr, message):
        SystemMessage.__init__(self, "warning", filename, linenr, message)

    def format(self):
        if platform.system() in ("Windows", "Microsoft"):
            return "%(filename)s(%(linenr)d) : %(type)s: %(message)s"
        else: return "%(filename)s:%(linenr)d: %(type)s: %(message)s"

class Error(SystemMessage):
    """
    This class can be used to print an error message which is
    formatted like a compiler error.
    """
    def __init__(self, filename, linenr, message):
        SystemMessage.__init__(self, "error", filename, linenr, message)


class WarningsAndErrors:
    """
    This class collects warnings and errors.
    """
    def __init__(self, stderr=sys.stderr):
        self.__warnings = []
        self.__errors = []
        self.__stderr = stderr

    def warnings(self): return self.__warnings
    def errors(self): return self.__errors

    def hasErrors(self): return len(self.errors()) > 0
    def hasWarnings(self): return len(self.warnings()) > 0
    def hasWarningsOrErrors(self): return self.hasWarnings() or self.hasErrors()

    def addWarning(self, line, message):
        """
        Adds the specified message to the list of warnings. To print
        all warnings you can call printParserWarnings().

        @param line is the util.Line instance (which contains at least
          a filename and a linenr) to which the message refers.
        @param message is some text describing the warning.
        """
        self.__warnings.append(Warning(line.filename, line.linenr, message))

    def addError(self, line, message):
        """
        Adds the specified message to the list of errors. To print
        all errors you can call printParserErrors().

        @param line is the util.Line instance (which contains at least
          a filename and a linenr) to which the message refers.
        @param message is some text describing the error.
        """
        self.__errors.append(Error(line.filename, line.linenr, message))

    def printWarnings(self):
        """
        If there are warnings, this method prints all warning messages
        to sys.stderr and returns True. If there are no warnings, this
        method returns False immediately.

        Instead of printing the warnings to sys.stderr, a different
        stream can be specified in the class constructor.

        @return True if there is at least one warnings message, False
          if there are no warnings.
        """
        if self.hasWarnings():
            for msg in sorted(self.warnings()): print >>self.__stderr, msg
            return True
        else: return False

    def printErrors(self):
        """
        If there are errors, this method prints all error messages to
        sys.stderr and returns True. If there are no errors, this
        method returns False immediately.

        Instead of printing the errors to sys.stderr, a different
        stream can be specified in the class constructor.

        @return True if there is at least one error message, False if
          there are no errors.
        """
        if self.hasErrors():
            for msg in sorted(self.errors()): print >>self.__stderr, msg
            return True
        else: return False

    def printWarningsAndErrors(self):
        """
        If there are warnings or errors, this method prints all
        warning and error messages to sys.stderr and returns True. If
        there are no warnings and no errors, this method returns False.

        Instead of printing the errors to sys.stderr, a different
        stream can be specified in the class constructor.

        @return True if there is at least one warning or error
          message, False if there are no warnings and no errors.
        """
        if self.hasWarningsOrErrors():
            for msg in sorted(self.errors()+self.warnings()):
                print >>self.__stderr, msg
            return True
        else: return False


class ItemsByDefine:
    """
    This class collects a map of define names to a list of
    module_parser.ModuleItem instances which are responsible for that define.
    The list of ModuleItem instances is generated on parsing various
    different input files (like features*.txt or module.tweaks).
    """
    def __init__(self):
        self.__items_by_define = {}

    def defines(self):
        """
        Returns the sorted list of all defines.
        """
        return sorted(self.__items_by_define.keys())

    def itemsByDefine(self, define):
        """
        Returns a list of items which are responsible for the
        specified define.
        """
        return self.__items_by_define.get(define, set())

    def addItem(self, defines, item):
        """
        For each define in the specified list of the defines, the item
        is added. Thus the item is contained in the list returned by
        itemsByDefine() fore each define.

        @param defines is a lists of defines for which to add the
          specified item.
        @param item is the ModuleItem that is responsible for defining
          all the specified defines.
        """
        for define in defines:
            self.__items_by_define.setdefault(define, set()).add(item)


class UtilError(Exception):
    """Base class for any error in this class"""
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)


class FeatureDefinition:
    def __init__(self, allow_versioned_filename=False):
        """
        @param allow_versioned_filename specifies whether it is
          allowed to use a versioned filename for module.tweaks,
          etc. I.e. module_parser.ModuleParser.parseFile(filename)
          will check for filename+"."+feature_definition.coreVersion() if
          this argument is true.
        """
        self.__allow_versioned_filename=allow_versioned_filename
        # version number, which was specified in load():
        self.__core_version = None
        # milestone number, which was specified in load():
        self.__milestone = None
        # List of all features, i.e. util.FeatureOrSystem objects,
        # sorted by name:
        self.__features = None
        # Dictionary mapping feature names to util.FeatureOrSystem
        # objects:
        self.__featuresByName = None
        # List of all attributions, i.e. util.FeatureOrSystem objects,
        # sorted by name:
        self.__attributions = None
        self.__is_loaded = False
        self.__messages_loaded = False
        self.__tweaks_and_apis_loaded = False
        self.__items_by_define = ItemsByDefine()

    def allowVersionedFilename(self):
        """
        Returns true if it is allowed to use a versioned filename for
        module.tweaks, etc.
        I.e. module_parser.ModuleParser.parseFile(filename) will check
        for filename+"."+feature_definition.coreVersion() if this argument
        is true.
        """
        return self.__allow_versioned_filename

    def defines(self): return self.__items_by_define.defines()
    def itemsByDefine(self, define):
        return self.__items_by_define.itemsByDefine(define)
    def addItemByDefine(self, defines, item):
        self.__items_by_define.addItem(defines, item)

    def isLoaded(self):
        """
        Returns True only if load() was successful.
        """
        return self.__is_loaded

    def messagesLoaded(self):
        """
        Returns True, if the method messages.MessagesParser.loadMessages()
        has reported that the message defines are loaded.

        See setMessagesLoaded()
        """
        return self.__messages_loaded

    def setMessagesLoaded(self, loaded=True):
        """
        messages.MessagesParser.loadMessages() calls this method to
        report that the message defines are loaded.
        """
        self.__messages_loaded = loaded

    def tweaksAndApisLoaded(self):
        """
        Returns True, if the method
        tweaks_and_apis.TweaksParser.parseModuleTweaksAndApis() has
        reported that the tweaks and export defines are loaded.

        See setTweaksAndApisLoaded()
        """
        return self.__tweaks_and_apis_loaded

    def setTweaksAndApisLoaded(self, loaded=True):
        """
        tweaks_and_apis.TweaksParser.parseModuleTweaksAndApis() calls this
        method to report that the tweaks and export defines are loaded.
        """
        self.__tweaks_and_apis_loaded = loaded

    def __partition(self,value,i):
        """
        Split a string at '.' and return the i'th part
        """
        if value is not None:
            parts = value.split('.')
            if len(parts) > i:
                return parts[i]
        return None

    def coreVersion(self):
        """
        Returns the core version number that was specified on calling
        the method load().
        """
        return self.__core_version

    def coreVersionMajor(self):
        """
        Returns the major core version number. That is the number before
        the dot '.'. If the version is '2.4', then this method returns
        '2'.
        """
        return self.__partition(self.__core_version, 0)

    def coreVersionMinor(self):
        """
        Returns the minor core version number. That is the number after
        the dot '.'. If the version is '2.4', then this method returns
        '4'.
        """
        return self.__partition(self.__core_version, 1)

    def operaVersion(self):
        """
        Returns the opera version number that was specified on calling
        the method load().
        """
        return self.__opera_version

    def operaVersionMajor(self):
        """
        Returns the major opera version number. That is the number before
        the dot '.'. If the version is '10.50', then this method returns
        '10'.
        """
        return self.__partition(self.__opera_version, 0)

    def operaVersionMinor(self):
        """
        Returns the minor opera version number. That is the number after
        the dot '.'. If the version is '10.50', then this method returns
        '50'.
        """
        return self.__partition(self.__opera_version, 1)

    def forceVersion(self):
        """
        Returns the forced (overridden) opera version number.
        This might be None
        """
        return self.__force_version;

    def milestone(self):
        """
        Returns the milestone number that was specified on calling
        the method load().
        """
        return self.__milestone

    def featuresWithDefault(self):
        """
        Returns a list of all parsed features,
        i.e. util.FeatureOrSystem objects, which have a default
        value. This is the list of features which are not third party
        features and which are not deprecated.
        """
        return filter(lambda feature:
                          not feature.name().startswith("FEATURE_3P_") and not feature.isDeprecated(),
                      self.__features)

    def features(self):
        """
        Returns a list of all parsed features,
        i.e. util.FeatureOrSystem objects.
        """
        return self.__features

    def featuresByName(self):
        """
        Returns the dictionary that maps the feature names to
        util.FeatureOrSystem objects.
        """
        return self.__featuresByName;

    def attributions(self):
        """
        Returns a list of all parsed attributions,
        i.e. util.Attribution objects.
        """
        return self.__attributions

    def load(self, featuresDir, core_version=None, milestone=None, opera_version=None, force_version=None):
        """
        Loads and parses the features defined in the files
        features.{core_version}.txt and
        features-thirdparty.{core_version}.txt.
        After the features have been loaded, the methods features()
        and featuresWithDefault() return a list of
        util.FeatureOrSystem objects.

        @param featuresDir is the directory in which the files
            features.{core_version}.txt and
            features-thirdparty.{core_version}.txt are located.

        @param core_version is the core version number of the feature-files
            to load. If no version is specified or if the file with the
            specified version does not exist, features.txt or
            features-thirdparty.txt is loaded instead.

        @param milestone is the milestone number

        @param opera_version is the Opera version number, f.ex. 10.50

        @param force_version is the overridden version number, like 9.80
        """
        self.__core_version = core_version
        self.__milestone = milestone
        self.__opera_version = opera_version
        self.__force_version = force_version
        if core_version is not None:
            features_txt = os.path.join(featuresDir, 'features.%s.txt' % core_version)
            features3p_txt = os.path.join(featuresDir, 'features-thirdparty.%s.txt' % core_version)
        if core_version is None or not fileTracker.addInput(features_txt):
            features_txt = os.path.join(featuresDir, 'features.txt')
        if core_version is None or not fileTracker.addInput(features3p_txt):
            features3p_txt = os.path.join(featuresDir, 'features-thirdparty.txt')
        features3p_attributions_txt = os.path.join(featuresDir, 'features-thirdparty_attributions.txt')

        features, attributions, errors, warnings = parseFeaturesAndSystem(
            filenames=[features_txt, features3p_txt],
            attributionfilename=features3p_attributions_txt,
            items_by_define=self.__items_by_define)

        if warnings:
            for msg in warnings:
                print >>sys.stderr, msg
        if errors:
            self.__is_loaded = False
            raise UtilError(
                "Error on loading or parsing %s or %s:\n" % (features_txt, features3p_txt) +
                "\n".join(sorted([str(e) for e in errors])))
        else:
            features.sort(key=FeatureOrSystem.name)
            attributions.sort(key=FeatureOrSystem.name)
            self.__features = features
            self.__attributions = attributions
            self.__featuresByName = dict([(feature.name(), feature) for feature in features])
            self.__is_loaded = True
        return self.isLoaded()


import opera_coreversion

def getFeatureDef(sourceRoot, mainline_configuration="current"):
    """
    This method creates a FeatureDefinition instance and loads the feature
    files for the specified mainline configuration.

    @note If there is an error on loading the feature definition, a UtilError
        exception is raised. The value of the exception includes the
        description of the error.

    @param sourceRoot is the path to the Opera source tree

    @param mainline_configuration is the name of the mainline-configuration
        to load.

    @return a loaded FeatureDefinition
    """
    # It is not allowed to use a versioned filename for the next
    # mainline configuration. Only the current mainline configuration
    # may have a version number. This allows a smooth transition on
    # releasing one mainline version and switching to the next
    # release.
    allow_versioned_filename = "current" == mainline_configuration
    feature_def = FeatureDefinition(allow_versioned_filename)
    core_version, milestone = opera_coreversion.getCoreVersion(sourceRoot, mainline_configuration)
    opera_version, force_version = opera_coreversion.getOperaVersion(sourceRoot, mainline_configuration)
    feature_def.load(
        featuresDir=os.path.join(sourceRoot, 'modules', 'hardcore', 'features'),
        core_version=core_version, milestone=milestone,
        opera_version=opera_version, force_version=force_version)
    return feature_def


import module_parser

from module_parser import ModuleItem

class FeatureOrSystem(module_parser.ModuleItem,
                      module_parser.ItemWithConflictWith):
    def __init__(self, type, filename, linenr, name, owners, thirdparty = False):
        ModuleItem.__init__(self, filename, linenr, name, owners, type)
        module_parser.ItemWithConflictWith.__init__(self)
        self.__type = type
        self.__group = None
        self._thirdparty = thirdparty;
        
        self.__defines = dict()
        self.__undefines = dict()
        self.__enabledFor = set()
        self.__disabledFor = set()
        self.__definesYes = dict()
        self.__definesNo = dict()
        self.__dependsOn = set()
        self.__requiredBy = set()
        self.__checkIf = set()
        self.__attributions = set()

    def __repr__(self): return self.__name

    def type(self): return self.__type
    def isThirdParty(self): return self._thirdparty
    def defines(self): return self.__defines
    def undefines(self): return self.__undefines
    def dependsOn(self): return self.__dependsOn
    def requiredBy(self): return self.__requiredBy
    def checkIf(self): return self.__checkIf
    def attributions(self): return self.__attributions

    def enabledFor(self, profile=None):
        if profile is None: return self.__enabledFor
        else: return profile in self.__enabledFor
    def disabledFor(self, profile=None):
        if profile is None: return self.__disabledFor
        else: return profile in self.__disabledFor

    def setGroup(self, value):
        self.__group = value
    def setDefines(self, value):
        for item in map(str.strip, value.split(",")):
            match = re.match("(\\S+)\\s*=\\s*\"(.*?)\"\\s*/\\s*\"(.*?)\"", item)
            if match:
                self.__defines[match.group(1)] = match.group(2)
                self.__undefines[match.group(1)] = match.group(3)
                continue
            match = re.match("(\\S+)\\s*=\\s*\"(.*?)\"\\s*", item)
            if match:
                self.__defines[match.group(1)] = match.group(2)
                continue
            self.__defines[item] = None
    def setUndefines(self, value):
        for item in map(str.strip, value.split(",")):
            match = re.match("(\\S+)\\s*=\\s*\"(.*?)\"\\s*/\\s*\"(.*?)\"", item)
            if match:
                self.__undefines[match.group(1)] = match.group(2)
                self.__defines[match.group(1)] = match.group(3)
                continue
            match = re.match("(\\S+)\\s*=\\s*\"(.*?)\"\\s*", item)
            if match:
                self.__undefines[match.group(1)] = match.group(2)
                continue
            self.__undefines[item] = None
    def setDependsOn(self, value):
        self.__dependsOn = set(map(str.strip, value.split(",")))
    def setRequiredBy(self, value):
        self.__requiredBy = set(map(str.strip, value.split(",")))
    def setCheckIf(self, value):
        self.__checkIf = set(map(str.strip, value.split(",")))
    def setEnabledFor(self, value):
        self.__enabledFor = set(map(str.strip, value.split(",")))
    def setDisabledFor(self, value):
        self.__disabledFor = set(map(str.strip, value.split(",")))
    def setAttributions(self, value):
        self.__attributions = set(map(str.strip, value.split(",")))
        
    def deprecatedClause(self):
        if self.__type in ("feature", "system") and self.isDeprecated():
            return "#ifdef %s\n#\terror \"%s\"\n#endif // %s\n\n" % (self.name(), self.description(), self.name())
        else:
            return ""

    def controlledDefinesClause(self):
        result = ""

        if not self.isDeprecated():
            for name in sorted(self.__defines.keys()):
                result += "#ifdef %s\n#line %d \"%s\"\n#error \"The define %s should not be defined.  It's controlled by %s.\"\n#endif // %s\n\n" % (name, self.linenr(), self.filename(), name, self.name(), name)
            for name in sorted(self.__undefines.keys()):
                result += "#ifdef %s\n#line %d \"%s\"\n#error \"The define %s should not be defined.  It's controlled by %s.\"\n#endif // %s\n\n" % (name, self.linenr(), self.filename(), name, self.name(), name)

        return result

    def definesClause(self):
        result = ""

        if not self.isDeprecated():
            if self.__defines:
                result += "#if %s == YES\n" % self.name()

                for name in sorted(self.__defines.keys()):
                    if not self.__defines[name]:
                        result += "#\tdefine %s\n" % name
                    else:
                        result += "#\tdefine %s %s\n" % (name, self.__defines[name])

                if not self.__undefines:
                    result += "#endif // %s == YES\n\n" % self.name()

            if self.__undefines:
                if self.__defines:
                    result += "#else // %s == YES\n" % self.name()
                else:
                    result += "#if %s == NO\n" % self.name()

                for name in sorted(self.__undefines):
                    if not self.__undefines[name]:
                        result += "#\tdefine %s\n" % name
                    else:
                        result += "#\tdefine %s %s\n" % (name, self.__undefines[name])

                if self.__defines:
                    result += "#endif // %s == YES\n\n" % self.name()
                else:
                    result += "#endif // %s == NO\n\n" % self.name()

        return result

    def decisionClause(self):
        result = ""

        if not self.isDeprecated():
            result += '#if !defined %s || (%s != YES && %s != NO)\n#error "You need to decide if you want %s or not."\n' % (self.name(), self.name(), self.name(), self.name())

            for line in self.description().split("\n"):
                result += "#\terror \"%s\"\n" % line

            result += "#endif // %s\n\n" % self.name()

        return result

    def dependenciesClause(self):
        result = ""

        if not self.isDeprecated() and self.__dependsOn:
            result += "// %s\n" % self.name()

            if len(self.__dependsOn) == 1:
                dependsOn = iter(self.__dependsOn).next()
                result += "#if %s == YES && %s == NO\n#\terror \"%s depends on %s.\"\n#endif // %s == YES && %s == NO\n\n" % (self.name(), dependsOn, self.name(), dependsOn, self.name(), dependsOn)
            else:
                result += "#if %s == YES\n" % self.name()

                for dependsOn in sorted(self.__dependsOn):
                    result += "#\tif %s == NO\n#\t\terror \"%s depends on %s.\"\n#\tendif // %s == NO\n" % (dependsOn, self.name(), dependsOn, dependsOn)

                result += "#endif // %s == YES\n\n" % self.name()

        return result

    def conflictsWithClause(self):
        result = ""

        if not self.isDeprecated() and self.conflictsWith():
            result += "// %s\n" % self.name()

            if len(self.conflictsWith()) == 1:
                conflictsWith = iter(self.conflictsWith()).next()
                result += "#if %s == YES && %s == YES\n#\terror \"%s conflicts with %s.\"\n#\tendif // %s == YES && %s == YES\n\n" % (self.name(), conflictsWith, self.name(), conflictsWith, self.name(), conflictsWith)
            else:
                result += "#if %s == YES\n" % self.name()

                for conflictsWith in sorted(self.conflictsWith()):
                    result += "#\tif %s == YES\n#\t\terror \"%s conflicts with %s.\"\n#\tendif // %s == YES\n" % (conflictsWith, self.name(), conflictsWith, conflictsWith)

                result += "#endif // %s == YES\n\n" % self.name()

        return result

    def typeCheckClause(self):
        result = ""

        if not self.isDeprecated():
            checkIf = " || ".join(["%s == YES" % name for name in sorted(self.checkIf())])

            if checkIf:
                result += "#if %s\n" % checkIf

            result += "void OperaTestingType_%s(%s param);\n" % (self.name(), self.name())

            if checkIf:
                result += "#endif // %s\n" % checkIf

            result += "\n"

        return result

    def macroCheckClause(self):
        result = ""

        if not self.isDeprecated():
            checkIf = " || ".join(["%s == YES" % name for name in sorted(self.checkIf())])

            if checkIf:
                result += "#if %s\n" % checkIf
                indent = "\t"
            else:
                indent = ""

            if self.hasDescription():
                description = "/*\n%s*/\n" % self.description()
            else: description = ""

            result += "#%sifndef %s\n#%s\terror \"Macro %s not defined.\"\n%s#%sendif // %s\n" % (indent, self.name(), indent, self.name(), description, indent, self.name())

            if checkIf:
                result += "#else // %s\n#\tifdef %s\n#\t\terror \"Macro %s should not be defined unless %s.\"\n#\tendif // %s\n#endif // %s\n" % (checkIf, self.name(), self.name(), checkIf, self.name(), checkIf)

            result += "\n"

        return result

    def enabledCondition(self):
        return " && ".join(["defined %s" % name for name in sorted(self.__defines.keys())] + ["!defined %s" % name for name in sorted(self.__undefines.keys())])

    def __cmp__(self, other): return cmp(self.name(), other.name())
    def __hash__(self): return hash(self.name())

class Attribution(FeatureOrSystem):
    def __init__(self, type, filename, linenr, name, owners):
        FeatureOrSystem.__init__(self,type, filename, linenr, name, owners)
        self.__attributionString = None   
        self.__forceIf = None
        
    def attributionString(self): return self.__attributionString
    def forceIf(self): return self.__forceIf

    def setForceFor(self, value):
        self.__forceIf = value.strip()
    
    def setAttributionString(self, value):
        self.__attributionString = value.strip()
     

def addError(errors, filename, linenr, message):
    """
    Creates an Error message instance from the specified arguments and
    adds it to the specified errors list.
    """
    errors.append(Error(filename, linenr, message))

def parseFeaturesAndSystem(filenames, attributionfilename = None, items_by_define=None):
    parser = module_parser.ModuleParser()

    def createSystemItem(filename, linenr, name, owners):
        return FeatureOrSystem("system", filename, linenr, name, owners)

    def createTypeItem(filename, linenr, name, owners):
        return FeatureOrSystem("type", filename, linenr, name, owners)

    def createMacroItem(filename, linenr, name, owners):
        return FeatureOrSystem("macro", filename, linenr, name, owners)

    def createAttributionItem(filename, linenr, name, owners):
        return Attribution("attribution", filename, linenr, name, owners)

    def createFeatureItem(filename, linenr, name, owners):
        
        if filename.find("features-thirdparty.") >= 0 : 
            thirdparty = True 
        else: 
            thirdparty = False
                    
        return FeatureOrSystem("feature", filename, linenr, name, owners, thirdparty)

    def parseFeatureOrSystemDefinition(line, current, what, value):
        if what == "defines":
            current.setDefines(value)
            if items_by_define is not None:
                items_by_define.addItem(current.defines().keys(), current)
        elif what == "undefines":
            current.setUndefines(value)
            if items_by_define is not None:
                items_by_define.addItem(current.undefines().keys(), current)
        elif what in ("depends on", "required by", "conflicts with", "check if"):
            if not value:
                parser.addParserError(line, "empty value; please use 'nothing' instead")
            elif value != "nothing":
                if current.type() == "feature": prefix = "FEATURE_"
                elif current.type() == "attribution": prefix = "FEATURE_" #item is a feature type for attributions
                else: prefix = "SYSTEM_"
                for item in map(str.strip, value.split(",")):
                    if not item.startswith(prefix):
                        parser.addParserError(line, "%s has ilattribution reference to %s" % (current.name(), item))

                if what == "depends on": current.setDependsOn(value)
                elif what == "required by": current.setRequiredBy(value)
                elif what == "conflicts with": current.addConflictsWith(value)
                elif what == "check if": current.setCheckIf(value)
        elif what == "group": current.setGroup(value)
        elif what == "attribution":
            current.setAttributions(value)
        elif what == "text":
            current.setAttributionString(value)
        elif current.type() == "attribution" and what == "force if":
            current.setForceFor(value)
        else:
            assert what == "enabled for" or what == "disabled for"

            if not value:
                parser.addParserError(line, "empty value; please use 'none' instead")
            elif value != "none":
                profiles = map(str.strip, value.split(","))
                for profile in profiles:
                    if profile not in PROFILES:
                        parser.addParserError(line, "unknown profile: %s" % profile)
                    elif ((what == "enabled for" and current.disabledFor(profile)) or
                          (what == "disabled for" and current.enabledFor(profile))):
                        parser.addParserError(line, "%s is both enabled and disabled for profile %s" % (current.name(), profile))

                if what == "enabled for": current.setEnabledFor(value)
                else: current.setDisabledFor(value)

    parser.setParseDefinition(
        [module_parser.ModuleItemParseDefinition(
                type="SYSTEM",
                definitions=FIELDS["system"],
                create_item=createSystemItem,
                parse_item_definition=parseFeatureOrSystemDefinition),
         module_parser.ModuleItemParseDefinition(
                type="type",
                definitions=FIELDS["type"],
                create_item=createTypeItem,
                parse_item_definition=parseFeatureOrSystemDefinition),
         module_parser.ModuleItemParseDefinition(
                type="macro",
                definitions=FIELDS["macro"],
                create_item=createMacroItem,
                parse_item_definition=parseFeatureOrSystemDefinition),
         module_parser.ModuleItemParseDefinition(
                type="FEATURE",
                definitions=FIELDS["feature"],
                create_item=createFeatureItem,
                parse_item_definition=parseFeatureOrSystemDefinition),
         module_parser.ModuleItemParseDefinition(
                type="LICENSE",
                definitions=FIELDS["attribution"],
                create_item=createAttributionItem,
                parse_item_definition=parseFeatureOrSystemDefinition),
         module_parser.ModuleItemParseDefinition(
                type="ATTRIBUTION",
                definitions=FIELDS["attribution"],
                create_item=createAttributionItem,
                  parse_item_definition=parseFeatureOrSystemDefinition)]
        )
    result = []
    for filename in filenames:
        result.extend(parser.parseFile(filename))
    
    attributions = []
    if attributionfilename:
        attributions.extend(parser.parseFile(attributionfilename))

    table = dict([(item.name(), item) for item in result if item.type() in ("feature", "system")])
    all_attributions = dict([(item.name(), item) for item in attributions])
    
    for item in result:
        if item.type() in ("feature") and item.isThirdParty(): 
            if not item.attributions():
                parser.addParserError(item.line(), "%s is third party. You need to decide if it needs an attribution or attribution" % item.name())
            for attr in item.attributions():
                if attr.lower() != "none":
                    if attr not in all_attributions:
                        parser.addParserError(item.line(), "Attribution %s has not been defined." % attr)
                    elif item.name() not in all_attributions[attr].requiredBy():
                        parser.addParserError(item.line(), "Feature %s needs %s, but is not listed among %s requirements" % (item.name(), attr, attr))
            
    for item in attributions:
        if item.attributionString() == None:
            parser.addParserError(item.line(), "%s needs a attribution text" % item.name())   
        for requiredBy in item.requiredBy():
            if requiredBy not in table:
                if item.type() != "attribution":
                    parser.addParserError(item.line(), "%s is required by unknown %s %s" % (item.name(), item.type(), requiredBy))
                else:
                    parser.addParserError(item.line(), "%s is required by unknown feature %s" % (item.name(),  requiredBy))
            else:
                requiredBy = table[requiredBy]
                if item.type() == "attribution" and item.name() not in requiredBy.attributions():
                    parser.addParserError(item.line(), "%s is required by %s but is not listed among %s's attributions" % (item.name(), requiredBy.name(), requiredBy.name()))

 
    for item in result:
        if item.type() in ("feature", "system") and not item.isDeprecated() and not item.defines() and not item.undefines():
            parser.addParserError(item.line(), "%s neither defines nor undefines anything" % item.name())

        if not item.isDeprecated() and not item.hasDescription():
            parser.addParserWarning(item.line(), "the %s %s has no description" % (item.type(),item.name()) )

        if not item.owners():
            parser.addParserWarning(item.line(), "the %s %s has no owner" % (item.type(),item.name()) )

        for dependsOn in item.dependsOn():
            if dependsOn not in table:
                parser.addParserError(item.line(), "%s depends on unknown %s %s" % (item.name(), item.type(), dependsOn))
            else:
                dependsOn = table[dependsOn]
                if item.name() not in dependsOn.requiredBy():
                    parser.addParserError(item.line(), "%s depends on %s but is not listed among %s's requirements" % (item.name(), dependsOn.name(), dependsOn.name()))

        for requiredBy in item.requiredBy():
            if requiredBy not in table:
                parser.addParserError(item.line(), "%s is required by unknown %s %s" % (item.name(), item.type(), requiredBy))
            else:
                requiredBy = table[requiredBy]
                if item.name() not in requiredBy.dependsOn():
                    parser.addParserError(item.line(), "%s is required by %s but is not listed among %s's dependencies" % (item.name(), requiredBy.name(), requiredBy.name()))

    return result, attributions, parser.errors(), parser.warnings()

def copyrightHeader():
    import time
    return """/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-%d Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
""" % time.localtime()[0]

def updateSetOfLinesInFile(filename, set_of_lines):
    """
    Adds the specified set of lines to the specified file. Only the lines
    that are not yet in the file, are added. So calling this method twice
    with the same line, will only add one line.

    @note The lines are added in random order because they are handled as
    a set.

    @param filename is the path to the file, which should be updated.
        If the files does not exist, a new file is created.

    @param set_of_lines is a list of lines to add to the file. Only those
        lines that are not yet in the file are added.
    """
    existing_lines = set()
    if os.path.exists(filename):
        try:
            f = None
            f = open(filename, "rU")
            for line in f:
                existing_lines.add(line.rstrip())
        finally:
            if f: f.close()
    new_lines = set_of_lines - existing_lines
    if len(new_lines) > 0:
        try:
            f = None
            f = open(filename, "a")
            for line in new_lines:
                print >>f, line
        finally:
            if f: f.close()

def updateModuleGenerated(module_generated_path, module_generated_lines):
    """
    Adds the specified module_generated_lines to the file
    "module.generated" in the specified path. Only the lines of
    module_generated_lines that are not yet in the file, are added. So
    calling this method twice with the same line, will only add one line.

    @param module_generated_path is the path to the directory, which
        contains the "module.generated" file. The specified
        module_generated_lines are added to that file. If the files does
        not exist, a new file is created.

    @param module_generated_lines is a list of lines to add to the
        "module.generated" file. Only those lines that are not yet in
        the file are added.
    """
    makedirs(module_generated_path)
    updateSetOfLinesInFile(os.path.join(module_generated_path, "module.generated"),
                           set(module_generated_lines))

def compareWithFile(output, filename):
    """
    Compares the specifies StringIO object with the contents of the
    specified file.
    @param output is the StringIO object to compare with the
      specified file.
    @param filename is the name of the file to compare with the
      specified StringIO object.
    @return True if both have the same content; False if either
      the file does not exist or if the the content differs.
    """
    if os.path.exists(filename):
        try:
            f = None
            f = open(filename)
            return output.getvalue() == f.read()
        finally:
            if f: f.close()
    else:
        return False

def updateFile(output, filename):
    """
    Compares the specifies StringIO object with the contents of the
    specified file. If it is different, the content of the StringIO
    object is written to the specified file. If it is the same, the
    file remains untouched.
    @param output is the StringIO object to compare with the
      specified file.
    @param filename is the name of the file to update with the
      content of the specified StringIO object. If the file does not
      exist, it is created. If the path to the file does not exist, it
      is created as well.
    @return True if the file was updated, i.e. if either the file did
      not exist or the content of the StringIO object and the file was
      different; False if the content of the StringIO object and the
      content of the file were equal.
    """
    if not compareWithFile(output, filename):
        makedirs(os.path.dirname(filename))
        try:
            out = None
            out = open(filename, "w")
            out.write(output.getvalue())
        finally:
            if out: out.close()
        return True
    else:
        return False

def makedirs(path):
    """
    Ensure existence of the directory path and all its parents,
    creating directories as needed. Like os.makedirs, but does not
    fail if the directory already exists.
    @param path Target directory.
    @return True if directory was created, False if it already existed.
    """
    try:
        os.makedirs(path)
        return True
    except OSError, e:
        if e.errno == errno.EEXIST:
            return False
        else:
            raise

def readTemplate(template_path, output_path, handler, module_generated_path=None, module_generated_line=None):
    import StringIO
    import os

    output = StringIO.StringIO()

    fileTracker.addInput(template_path)
    try:
        f = None
        f = open(template_path)
        for line in f:
            match = re.match("^\\s*//\\s+<(.*?)>\\s+$", line)
            if match:
                action = match.group(1)
                if not handler(action, output):
                    if action == "copyright header":
                        output.write(copyrightHeader())
                    elif action.startswith("autogenerated"):
                        output.write("// Please don't modify this file.\n\n// This file is automatically generated by %s\n" % action.split()[1])
                    elif action.startswith("-*- Mode:"):
                        # ignore an emacs mode-line
                        continue
                    else:
                        output.write(line)
            else:
                output.write(line)
    finally:
        if f: f.close()

    if module_generated_path and module_generated_line:
        updateModuleGenerated(module_generated_path, [module_generated_line])
    return updateFile(output, output_path)

class FileTracker:
    """
    Singleton class accessible as util.FileTracker.
    Tracks the input files that the script uses or attempts to use in
    order to produce correct .d files when --timestamp is specified.
    """
    def __init__(self):
        self.__input_files = {} # Boolean values say whether the files exist

    def addInput(self, filename):
        """
        Register a file that the script reads or attempts to read. If
        the file doesn't exist now and is created later, or exists now
        and disappears later, the dependencies will trigger an update.

        @param filename Path to the input file.

        @return Whether the file exists (to spare another call to
        os.path.exists).
        """
        if filename in self.__input_files:
            return self.__input_files[filename]
        else:
            exists = os.path.exists(filename)
            self.__input_files[filename] = exists
            return exists

    def finalize(self, filename):
        """
        Touch the timestamp and write the dependency file.
        To be called once before the script completes.

        @param filename The path to the timestamp file. (The
        dependency file has the same name + '.d'.)
        """
        import StringIO
        prefix = os.path.abspath(os.path.normpath(os.path.join(sys.modules['util'].__file__, '..', '..', '..')))
        for m in sys.modules:
            if sys.modules[m] and '__file__' in sys.modules[m].__dict__:
                f = sys.modules[m].__file__
                if f.endswith('.pyc'):
                    f = f[:-1]
                if os.path.abspath(f).startswith(prefix):
                    self.addInput(f)
        output = StringIO.StringIO()
        output.write(filename + ':')
        keys = sorted(self.__input_files.keys())
        for f in keys:
            if self.__input_files[f]:
                output.write(' \\\n    ' + os.path.relpath(f))
            else:
                output.write(" \\\n    $(wildcard %s)" % os.path.relpath(f))
        output.write('\n\n')
        for f in keys:
            if self.__input_files[f]:
                output.write("%s:\n" % os.path.relpath(f))
        makedirs(os.path.dirname(filename))
        open(filename, 'a').close()
        os.utime(filename, None)
        return updateFile(output, filename + '.d')

fileTracker = FileTracker()
