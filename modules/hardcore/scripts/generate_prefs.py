import sys
import os.path
import subprocess
import re

# import from modules/hardcore/scripts/
import basesetup
import util

class PrefsCollectionTemplateActionHandler:
    """
    An instance of this class can be used by util.readTemplate() to convert
    both the prefs_template.h and prefs_template.cpp into pc_XXX_h.inl resp.
    pc_XXX_c.inl for any prefs collection.
    """
    def __init__(self, header, items):
        """
        header is an instance of PrefsCollectionParser.HeaderItem as returened
               by PrefsCollectionParser.header()
        items  is a list of PrefsCollectionParser.PrefItem instances as returned
               by PrefsCollectionParser.items()
        """
        self.__header = header
        self.__items = items

    def itemsFor(self, dotype):
        """
        Filters and returns a list of all PrefItem instances that have the
        specified type. The returned list may be empty.
        """
        return sorted(filter(lambda x: x.isType(dotype), self.__items),
                      key=lambda item: item.identifier())

    def typeFor(self, dotype):
        """
        Returns the name of the enum that is used for the prefs of the specified
        type. The enum name is stringpref, integerpref, ...
        """
        if dotype == "color": return "customcolorpref";
        else: return "%spref" % dotype

    def typeNameFor(self, dotype):
        """
        Returns the human readable name for the specified type. This is usually
        the same as the type.
        """
        if dotype == "color": return "custom color";
        else: return dotype

    def numberOfFor(self, dotype):
        """
        Returns the second part of the name of the macro that defines the number
        of prefs for a given collection and type. The resulting macro name
        will be PCXXX_NUMBEROFYYYPREFS where XXX depends on the collection and
        YYY depends on the type.
        """
        if dotype == "string":      return "NUMBEROFSTRINGPREFS"
        elif dotype == "integer":   return "NUMBEROFINTEGERPREFS"
        elif dotype == "file":      return "NUMBEROFFILEPREFS"
        elif dotype == "directory": return "NUMBEROFFOLDERPREFS"
        elif dotype == "color":     return "NUMBEROFCUSTOMCOLORPREFS"
        else: return None

    def dummyLastFor(self, dotype):
        """
        Returns the name of the last enum for a prefs-collection of some type.
        """
        if dotype == "string":      return "DummyLastStringPref"
        elif dotype == "integer":   return "DummyLastIntegerPref"
        elif dotype == "file":      return "DummyLastFilePref"
        elif dotype == "directory": return "DummyLastFolderPref"
        elif dotype == "color":     return "DummyLastCustomColorPref"
        else: return None

    def conditionalFor(self, dotype):
        """
        Returns a conditional that is used to protect some type.
        """
        if dotype == "directory": return "#ifdef PREFS_READ"
        else: return None

    def initFor(self, dotype):
        """
        Returns the macro-name that is used in the pc_XXX_c.inl to start the
        initialisation of the specified type.
        """
        if dotype == "string":      return "INITSTRINGS"
        elif dotype == "integer":   return "INITINTS"
        elif dotype == "file":      return "INITFILES"
        elif dotype == "directory": return "".join(
            ["/** The directories that can be configured here are defined by\n",
             "  * OpFolderManager's OpFileFolder enumeration.*/\n",
             "INITFOLDERS"])
        elif dotype == "color":     return "INITCUSTOMCOLORS"
        else: return None

    def initSentinelFor(self, dotype):
        """
        Returns the invokation of the intialisation macro for the sentinel
        prefs entry for the specified type.
        """
        if dotype == "string":      return "P(SNone, NULL, NULL)"
        elif dotype == "integer":   return "I(SNone, NULL, 0, prefssetting::prefssettingtypes(0))"
        elif dotype == "file":      return "F(SNone, NULL, OpFileFolder(-1), NULL, FALSE)"
        elif dotype == "directory": return "D(OpFileFolder(-1), SNone, NULL)"
        elif dotype == "color":     return "CC(SNone, NULL, 0)"
        else: return None

    def initCommentFor(self, dotype):
        """
        Returns the content of the comment that is inserted before invoking the
        initialisation macros for each pref.
        """
        if dotype == "file":
            return "Section, Key, Default folder, Default filename, Use default if file is not found"
        elif dotype == "directory":
            return "Folder, Section, Key"
        else: return "Section, Key, Default"

    def collectionTypes(self):
        """
        Returns a list of collection types that is defined for the current
        prefs-collection.
        The default collection types are "string" and "integer".
        """
        if self.__header.isFiles(): return ["file", "directory"]
        elif self.__header.isFontColor(): return ["font", "color"]
        else: return ["string", "integer"]

    def updateIfdef(self, output, current_ifdef, item):
        """
        Closes the current ifdef and opens a new ifdef (in the generated output)
        if the current conditional and the conditional of the specified item
        differ.
        output        The output stream to write into.
        current_ifdef The current conditional (which was used for the last item)
                      or None if no conditional is used.
        item          The PrefItem which is currently handled.

        Returns the conditional of the specified item.
        """
        if current_ifdef != item.ifdef():
            if current_ifdef is not None: output.write("#endif\n")
            if item.ifdef() is not None: output.write("%s\n" % item.ifdef())
        return item.ifdef()

    def writeEnums(self, output):
        """
        Creates the code to define all enums for the collection.
        """
        for dotype in self.collectionTypes():
            # TODO: Implement support for custom font settings
            if dotype == "font": continue

            args = {
                'collection': self.__header.collection(),
                'macro_name': self.__header.macro_name(),
                'type name':  self.typeNameFor(dotype),
                'type':       self.typeFor(dotype),
                'NUMBEROF':   self.numberOfFor(dotype),
                'DummyLast':  self.dummyLastFor(dotype),
                'note': ''
                }

            if dotype == "directory":
                args['note'] = "".join(
                    ["\n",
                     "  * Please note that the preferences are accessed by using\n",
                     "  * OpFolderManager's OpFileFolder enumeration."])

            # Output headers
            if self.conditionalFor(dotype) is not None:
                output.write("%s\n" % self.conditionalFor(dotype))
            output.write("".join(
                    ["/** Enumeration of all %(type name)s preferences in this collection.%(note)s */\n",
                     "enum %(type)s\n",
                     "{\n"]) % args)

            # Get all entries, sorted alphabetically by section and key
            current_ifdef = None
            for item in self.itemsFor(dotype):
                current_ifdef = self.updateIfdef(output, current_ifdef, item)
                item_args = {
                    'enum':        item.enum(),
                    'description': item.description(),
                    }
                if dotype == "directory":
                    item_args['enum'] = "dummy_" + item.enum()
                output.write("\t%(enum)s, ///< %(description)s\n" % item_args)
            if current_ifdef is not None: output.write("#endif\n")

            # Output footers
            output.write("".join(
                    ["\n",
                     "\t%(DummyLast)s\n",
                     "};\n",
                     "#define %(macro_name)s_%(NUMBEROF)s static_cast<int>(%(collection)s::%(DummyLast)s)\n"]) % args)
            if self.conditionalFor(dotype) is not None: output.write("#endif\n")
            output.write("\n")

    def writeInitStaticData(self, output):
        """
        Creates the code to initialise the collection's static data.
        """
        for dotype in self.collectionTypes():
            # TODO: Implement support for custom font settings
            if dotype == "font": continue

            args = {
                'collection':    self.__header.collection(),
                'macro_name':    self.__header.macro_name(),
                'init comment':  self.initCommentFor(dotype),
                'NUMBEROF':      self.numberOfFor(dotype),
                'INIT':          self.initFor(dotype),
                'init sentinel': self.initSentinelFor(dotype),
                }

            # output header
            if self.conditionalFor(dotype) is not None:
                output.write("%s\n" % self.conditionalFor(dotype))
            output.write("".join(
                    ["%(INIT)s(%(collection)s, %(macro_name)s_%(NUMBEROF)s)\n",
                     "{\n",
                     "\tINITSTART\n",
                     "\t/* %(init comment)s */\n"]) % args)

            # Get all entries, sorted alphabetically by section and key
            item_template = {
                'string'    : "\tP(%(sectionenum)s, \"%(key)s\", %(default)s),\n",
                'integer'   : "\tI(%(sectionenum)s, \"%(key)s\", %(default)s, prefssetting::%(inttype)s),\n",
                'file'      : "\tF(%(sectionenum)s, \"%(key)s\", %(directory)s, %(default)s, %(fallback)s),\n",
                'directory' : "\t// %(description)s\n\tD(%(enum)s, %(sectionenum)s, \"%(key)s\"),\n",
                'font'      : None, # TODO
                'color'     : "\tCC(%(sectionenum)s, \"%(key)s\", %(colorint)s),\n",
                }

            current_ifdef = None
            for item in self.itemsFor(dotype):
                current_ifdef = self.updateIfdef(output, current_ifdef, item)
                item_args = {
                    'conditional': item.ifdef(),
                    'sectionenum': item.sectionenum(),
                    'enum':        item.enum(),
                    'key':         item.key(),
                    'default':     item.default(),
                    'description': item.description(),
                    'directory':   item.directory(),
                    'fallback':    item.fallback(),
                    'inttype':     item.inttype(),
                    'colorint':    item.colorint(),
                    }
                output.write(item_template[dotype] % item_args)
            if current_ifdef is not None: output.write("#endif\n")

            # output footer
            output.write("".join(
                    ["\n",
                     "\t// Sentinel, always the last entry\n",
                     "\t%(init sentinel)s\n",
                     "\tINITEND\n",
                     "};\n"]) % args)
            if self.conditionalFor(dotype) is not None: output.write("#endif\n")
            output.write("\n")

    def writeDebugOperator(self, output):
        """
        Creates the code to add a Debug& operator<<() for the collection's enum
        """
        for dotype in self.collectionTypes():
            # TODO: Implement support for custom font settings
            if dotype == "font": continue

            # output header
            if self.conditionalFor(dotype) is not None:
                output.write("%s\n" % self.conditionalFor(dotype))
            args = {
                'collection': self.__header.collection(),
                'type':       self.typeFor(dotype),
                }
            template = "".join(
                ["Debug& operator<<(Debug& dbg, enum %(collection)s::%(type)s e)\n",
                 "{\n",
                 "\tswitch (e) {\n"])
            output.write(template % args)

            # Get all entries, sorted alphabetically by section and key
            current_ifdef = None
            for item in self.itemsFor(dotype):
                current_ifdef = self.updateIfdef(output, current_ifdef, item)
                item_args = {
                    'collection': args['collection'],
                    'enum':       item.enum(),
                    'enum_name':  item.enum(),
                    }
                if dotype == "directory":
                    item_args['enum'] = "dummy_" + item.enum()
                template = "".join(
                    ["\tcase %(collection)s::%(enum)s:\n",
                     "\t\treturn dbg << \"%(collection)s::%(enum_name)s\";\n"])
                output.write(template % item_args)
            if current_ifdef is not None: output.write("#endif\n")

            # output footer
            template = "".join(
                ["\tdefault:\n",
                 "\t\treturn dbg << \"%(collection)s::%(type)s(unknown:\" << (int)e << \")\";\n",
                 "\t}\n",
                 "}\n"])
            output.write(template % args)
            if self.conditionalFor(dotype) is not None: output.write("#endif\n")
            output.write("\n")

    def __call__(self, action, output):
        if action == "PrefsCollection enums":
            self.writeEnums(output)
            return True

        elif action == "PrefsCollection InitStaticData":
            self.writeInitStaticData(output)
            return True

        elif action == "PrefsCollection debug operator implementation":
            self.writeDebugOperator(output)
            return True

class PrefsCollectionParser(util.WarningsAndErrors):
    """
    The parser can be used to parse a prefs-collection's txt-file which defines
    all prefs of the collection.

    Call parse() to parse a txt-file.
    Call header() to retrieve the HeaderItem for the collection.
    Call items() to retrieve all list of all PrefItem for the collection.
    """
    def __init__(self, stderr=sys.stderr):
        util.WarningsAndErrors.__init__(self, stderr)
        self.__header = None
        self.__items = []

    def header(self): return self.__header
    def items(self): return self.__items

    def addParserWarning(self, line, message):
        """
        Adds the specified message to the list of warnings. To print
        all warnings you can call printParserWarnings().

        @param line is the util.Line instance (which contains at least
          a filename and a linenr) to which the message refers.
        @param message is some text describing the warning.
        """
        self.addWarning(line, message)

    def addParserError(self, line, message):
        """
        Adds the specified message to the list of errors. To print
        all errors you can call printParserErrors().

        @param line is the util.Line instance (which contains at least
          a filename and a linenr) to which the message refers.
        @param message is some text describing the error.
        """
        self.addError(line, message)

    def printParserWarnings(self):
        """
        If there are warnings, this method prints all warning messages
        to sys.stderr and returns True. If there are no warnings, this
        method returns False immediately.

        Instead of printing the warnings to sys.stderr, a different
        stream can be specified in the class constructor.

        @return True if there is at least one warnings message, False
          if there are no warnings.
        """
        return self.printWarnings()

    def printParserErrors(self):
        """
        If there are errors, this method prints all error messages to
        sys.stderr and returns True. If there are no errors, this
        method returns False immediately.

        Instead of printing the errors to sys.stderr, a different
        stream can be specified in the class constructor.

        @return True if there is at least one error message, False if
          there are no errors.
        """
        return self.printErrors()

    class HeaderItem:
        """
        Contains the information from the header of the parsed txt-file.
        """
        def __init__(self):
            self.__collection = None
            self.__description = None
            self.__type = None
            self.__macro_name = None

        def isFiles(self): return self.__type == "files"
        def isFontColor(self): return self.__type == "fontcolor"

        def collection(self): return self.__collection
        def macro_name(self): return self.__macro_name

        def parse_line(self, parser, line):
            tokens = str(line).split(':', 1)
            if len(tokens) == 2:
                (key, value) = (tokens[0].strip(), tokens[1].strip())
                if   key == "Collection":  self.__collection = value
                elif key == "Description": self.__description = value
                elif key == "Type":        self.__type = value
                elif key == "Macro name":  self.__macro_name = value
                else: parser.addParserError(line, "Unknown field '%s'!" % key)
            else: parser.addParserError(line, "Cannot parse line '%s'!" % str(line))

    class PrefItem:
        """
        Contains the information for a single pref as specified in the txt-file.
        """
        def __init__(self):
            self.__line = None
            self.__enum = None
            self.__ifdef = None
            self.__section = None
            self.__key = None
            self.__type = None
            self.__description = None
            self.__default = None
            self.__directory = None
            self.__fallback = None
            self.__isboolean = False

        def identifier(self):  return "%s|%s" % (self.__section, self.__key)
        def enum(self):        return self.__enum
        def ifdef(self):       return self.__ifdef
        def section(self):     return self.__section
        def sectionenum(self): return "S"+self.__section.replace(" ", "")
        def key(self):         return self.__key
        def type(self):        return self.__type
        def isType(self, type): return self.__type == type
        def description(self): return self.__description
        def default(self):     return self.__default
        def directory(self):   return self.__directory
        def fallback(self):    return self.__fallback
        def inttype(self):
            if self.isType("integer"):
                if self.__isboolean: return "boolean"
                else: return "integer"
            else: return None
        def colorint(self):
            if self.isType("color"):
                return re.sub(r'^#(..)(..)(..)', r'OP_RGB(0x\1,0x\2,0x\3)',
                              self.__default)
            else: return None

        def setIfdef(self, condition):
            if condition != "":
                if "(" in condition or "defined" in condition:
                    self.__ifdef = "#if %s" % condition
                else: self.__ifdef = "#ifdef %s" % condition
            else: self.__ifdef = None

        def setDefault(self, default):
            if self.__type in ["string", "file"] and default.startswith("\""):
                self.__default = "UNI_L(%s)" % default
            else: self.__default = default

        def parse_line(self, parser, line):
            if self.__line is None: self.__line = line
            tokens = str(line).split(':', 1)
            if len(tokens) == 2:
                (key, value) = (tokens[0].strip(), tokens[1].strip())
                if   key == "Preference":  self.__enum = value
                elif key == "Depends on":  self.setIfdef(value)
                elif key == "Section":     self.__section = value
                elif key == "Key":         self.__key = value
                elif key == "Type":        self.__type = value
                elif key == "Description": self.__description = value
                elif key == "Default":     self.setDefault(value)
                elif key == "Directory":   self.__directory = value
                elif key == "Fall back":   self.__fallback = value
                else: parser.addParserError(line, "Unknown field '%s'!" % key)
            else: parser.addParserError(line, "Cannot parse line '%s'!" % str(line))

        def validate_key(self, parser, key, value):
            if value is None or value == "":
                parser.addParserError(self.__line, "Key \"%s\" missing" % key)

        def validate(self, parser):
            self.validate_key(parser, "Preference",  self.__enum)
            self.validate_key(parser, "Section",     self.__section)
            self.validate_key(parser, "Key",         self.__key)
            self.validate_key(parser, "Type",        self.__type)
            if not self.__type in ["boolean", "integer", "string", "file", "directory", "color"]:
                parser.addParserError(self.__line, "Unknown type \"%s\"" % self.__type)
            self.validate_key(parser, "Description", self.__description)
            if self.__type != "directory":
                self.validate_key(parser, "Default", self.__default)
            if self.__type == "boolean":
                self.__type = "integer"
                self.__isboolean = True
                if not (self.__default in ["FALSE", "TRUE", "-1"] or
                        self.__default.startswith("DEFAULT")):
                    parser.addParserError(self.__line, "Invalid boolean: %s" % self.__default)
            elif self.__type == "file":
                self.validate_key(parser, "Fall back", self.__fallback)
                if not self.__fallback in ["FALSE", "TRUE"]:
                    parser.addParserError(self.__line, "Invalid fallback: %s" % self.__fallback)
                self.validate_key(parser, "Directory", self.__directory)

    def parse(self, filename):
        """
        Parses the specified prefs-collection definition file.

        Call printParserErrors() and printParserWarnings() to print any error
        or warning that was detected while parsing the specified file.

        filename is the txt-file which contains the definition of a
                 prefs-collection.
        """
        f = None
        eof = False
        try:
            f = open(filename)
            util.fileTracker.addInput(filename)
            current_item = self.HeaderItem()
            self.__header = current_item
            for nr,text in enumerate(f):
                line = util.Line(filename, nr+1, text.rstrip())
                if line.isComment(): pass
                elif line.isEmptyOrWhitespace():
                    current_item = None
                elif text.strip() == ".eof":
                    eof = True
                    break
                else:
                    if current_item is None:
                        current_item = self.PrefItem()
                        self.__items.append(current_item)
                    current_item.parse_line(self, line)
        finally:
            if f is not None: f.close()

        for item in self.__items: item.validate(self)
        if not eof: self.addParserError(util.Line(filename, 0, ""), "Missing \".eof\" marker")

def find_prefs_files(opera_root):
    from opera_module import modules as find_modules
    modules = find_modules(opera_root)
    comment_re = re.compile(r"#.*$")

    def find_prefs(path):
        prefs_index = os.path.join(path, "module.prefs")
        if not util.fileTracker.addInput(prefs_index) or not os.path.isfile(prefs_index):
            return
        f = None
        try:
            f = open(prefs_index)
            for nr,line in enumerate(f):
                line = comment_re.sub("", line).strip()
                if line:
                    prefs_entry = os.path.join(path, line)
                    (base, ext) = prefs_entry.rsplit(".", 1)
                    if not ext == "txt":
                        print "warning: %s:%d: '%s' does not have a .txt suffix, ignored" % (prefs_index, nr+1, line)
                    elif os.path.exists(prefs_entry):
                        yield base
                    else:
                        print "error: %s:%d: The file '%s' does not exist" % (prefs_index, nr+1, line)
                        sys.exit(1)
        finally:
            if f is not None: f.close()

    for module in modules:
        files = list(find_prefs(module.fullPath()))
        if files:
            yield module, files

class GeneratePrefs(basesetup.SetupOperation):
    """
    This class can be used to execute
    modules/prefs/prefsmanager/collections/make-prefs.pl for each collection
    listed in a module.prefs file to generate preferences sources from their
    corresponding .txt files.

    An instance of this class can be used as one of operasetup.py's
    "system-functions".
    """
    def __init__(self, perl=None):
        basesetup.SetupOperation.__init__(self, message="Prefs system", perl=perl)

    def __call__(self, sourceRoot, outputRoot=None, quiet=True, show_all=False):
        """
        Calling this instance will generate the source files from all prefs .txt
        files.

        @param sourceRoot is the root directory of the source tree
          that was parsed. Some of the output files are always
          generated relative to the sourceRoot.
        @param outputRoot root of the tree for generated files,
          defaults to sourceRoot
        @param quiet if False, print some log messages about executing the perl
          script.
        @param show_all controls whether to show only modified files or all
          files which are inspected. Default is False.

        @return The convention of the "system-functions" are that the
          return value should be
          - 0 to indicate success
          - 1 to indicate an error
          - 2 to indicate that output files have changed.
        """
        self.startTiming()

        if outputRoot == None:
            outputRoot = sourceRoot

        perl = self.findPerl(quiet)
        if perl is None: return self.endTiming(1, quiet=quiet)
        # Now perl should be the path to a perl executable

        # Scan for modules and prefs files and build the structure
        allfiles = []
        changed = False
        for module, files in find_prefs_files(sourceRoot):
            modfiles = []
            for filename in files:
                self.logmessage(quiet, "Creating prefs enum for %s" % (filename))

                parser = PrefsCollectionParser()
                parser.parse("%s.txt" % filename)
                if not quiet: parser.printParserWarnings()
                if parser.printParserErrors():
                    return self.endTiming(1, quiet=quiet)

                handler = PrefsCollectionTemplateActionHandler(
                    parser.header(), parser.items())

                stem = os.path.relpath(filename, sourceRoot)

                outdir = os.path.dirname(os.path.join(outputRoot, stem))
                basename = os.path.basename(filename)
                prefs_collection_dir = os.path.join("modules", "prefs", "prefsmanager", "collections")
                pc_template_h = os.path.join(sourceRoot, prefs_collection_dir,
                                             "pc_template.h")
                output_h = os.path.join(outdir, "%s_h.inl" % basename)
                changed = util.readTemplate(pc_template_h, output_h, handler) or changed

                pc_template_c = os.path.join(sourceRoot, prefs_collection_dir,
                                             "pc_template.cpp")
                output_c = os.path.join(outdir, "%s_c.inl" % basename)
                changed = util.readTemplate(pc_template_c, output_c, handler) or changed
                modfiles += [output_c, output_h]

                allfiles.append(stem)
                util.fileTracker.addInput(stem + ".cpp") # Will be used by extract-documentation.pl

            # List the generated files in module.generated
            if outputRoot == sourceRoot:
                util.updateModuleGenerated(module.fullPath(), modfiles)

        # Update documentation
        for f in ('modules/prefs/prefsmanager/opprefscollection.cpp',
                  'modules/prefs/prefsmanager/opprefscollection.h',
                  'modules/prefs/module.tweaks',
                  'modules/prefs/prefsmanager/collections/erasettings.h',
                  'modules/pi/OpSystemInfo.h'):
            util.fileTracker.addInput(f) # Will be used by extract-documentation.pl
        if changed:
            documentation_dir = os.path.join(sourceRoot, "modules", "prefs")
            documentation_output_dir = os.path.join(outputRoot, "modules", "prefs")
            documentation_path = os.path.join(documentation_dir, "extract-documentation.pl")
            util.makedirs(os.path.join(documentation_output_dir, "documentation"))
            self.logmessage(quiet, "calling script %s" % (documentation_path))
            subprocess.call([perl, documentation_path, "-o", outputRoot] + allfiles, cwd=sourceRoot)
            if outputRoot == sourceRoot:
                util.updateModuleGenerated(documentation_output_dir, ["documentation/settings.html"])

        if changed: result = 2
        else: result = 0
        return self.endTiming(result, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Generates source files from preferences definition files (.txt)."]),
        withPerl=True)

    option_parser.add_option(
        "--show-all", dest="show_all", action="store_true", default=False,
        help="Show all files which are processed, not just the ones which are modified")
    option_parser.add_option(
        "--show-modified", dest="show_all", action="store_false",
        help="Show only modified files which are processed")

    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    prefs = GeneratePrefs(perl=options.perl)
    result = prefs(sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet, show_all=options.show_all)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)

