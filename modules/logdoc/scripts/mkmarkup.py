import os
import os.path
import re
import sys
import subprocess

markup_debug = False

def logmessage(s):
    global markup_debug
    if markup_debug:
        print >>sys.stdout, s

# is_standalon is True if the script is not called from the build script
is_standalone = __name__ == "__main__"

sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "svg", "scripts"))

import make_pres_attr_table

if is_standalone:
    sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "hardcore", "scripts"))

# import from modules/hardcore/scripts/:
import util
import basesetup
import opera_module


def hashString(string, const):
    hash = const;
    for c in string:
        hash = (hash * 33) % 4294967296
        hash = (hash + (ord(c) | 0x20)) % 4294967296
    return hash

class MarkupParserError(Exception):
    def __init__(self, file, line, col, msg):
        self.file_name = file
        self.line_num = line
        self.col_num = col
        self.error_msg = msg
    def __str__(self):
        return "%s:%d %s" % (self.file_name, self.line_num, self.error_msg)

class HashTestingError(Exception):
    def __init__(self, file):
        self.file_name = file
        self.error_msg = """
No fitting hash base value found to avoid too many collisions.
You must manually sort out how to make the hashing produce less collisions.
Contact the logdoc module owner."""
    def __str__(self):
        return "%s: %s" % (self.file_name, self.error_msg)

class MarkupNameParser(util.WarningsAndErrors):
    """
    Parses module.markup and generates the element and attribute tables.
    """
    def __init__(self, sourceRoot, outputRoot, bigendian, debug, quiet):
        global markup_debug
        markup_debug = debug
        util.WarningsAndErrors.__init__(self, sys.stderr)
        self.error_value = 0
        self.parser = None
        self.bigendian = bigendian
        self.src_root = sourceRoot
        self.elmHashBase = 0
        self.elmHashSize = 0
        self.attrHashBase = 0
        self.attrHashSize = 0

        if outputRoot is None:
            outputRoot = sourceRoot
        self.out_root = outputRoot

        self.hash_file_path = os.path.join(sourceRoot, "modules", "logdoc", "src", "html5")
        self.template_file_path = os.path.join(sourceRoot, "modules", "logdoc", "scripts")
        if markup_debug:
            self.out_file_path = self.template_file_path
        else:
            self.out_file_path = os.path.join(outputRoot, "modules", "logdoc", "src", "html5")
        self.svg_script_path = os.path.join(sourceRoot, "modules", "svg", "scripts")

        self.type_file_pattern = "%stypes%s.inl"
        self.name_file_pattern = "%snames%s.h"
        self.base_file_pattern = "%shashbase%s.h"

        self.markup_files = []
        self.in_markup = False
        self.in_elements = False
        self.in_attributes = False
        self.current_ns = ""
        self.current_prefix = ""
        self.elements = []
        self.special_elements = []
        self.attributes = []
        self.special_attributes = []

    def startElmHandler(self, name, attrs):
        if self.in_elements:
            if name == "elm":
                if "str" in attrs:
                    self.elements.append((attrs["str"], self.current_ns, self.current_prefix))
                elif "name" in attrs:
                    self.special_elements.append((attrs["name"], self.current_ns, self.current_prefix))
                else:
                    raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "<elm> needs str or name attribute")
            else:
                raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "Expecting <elm>")
        elif self.in_attributes:
            if name == "attr":
                if "str" in attrs:
                    self.attributes.append((attrs["str"], self.current_ns, self.current_prefix))
                elif "name" in attrs:
                    self.special_attributes.append((attrs["name"], self.current_ns, self.current_prefix))
                else:
                    raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "<attr> needs str or name attribute")
            else:
                raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "Expecting <attr>")
        elif self.in_markup:
            if name == "elements":
                self.in_elements = True
                if "ns" in attrs and "prefix" in attrs:
                    self.current_ns = attrs["ns"]
                    self.current_prefix = attrs["prefix"]
                else:
                    raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "<elements> needs both ns and prefix attributes")
            elif name == "attributes":
                self.in_attributes = True
                if "ns" in attrs and "prefix" in attrs:
                    self.current_ns = attrs["ns"]
                    self.current_prefix = attrs["prefix"]
                else:
                    raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "<attributes> needs both ns and prefix attributes")
            else:
                raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "Expecting <elements> or <attributes>")
        else:
            if name == "markup":
                self.in_markup = True
            else:
                raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "Expecting <markup>")

    def endElmHandler(self, name):
        if self.in_elements:
            if name == "elements":
                self.in_elements = False
                self.current_ns = ""
            elif name != "elm":
                raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "Expecting </elm> or </elements>")
        elif self.in_attributes:
            if name == "attributes":
                self.in_attributes = False
                self.current_ns = ""
            elif name != "attr":
                raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "Expecting </attr> or </attributes>")
        elif self.in_markup:
            if name == "markup":
                self.in_markup = False
            else:
                raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "Expecting </markup>")
        else:
            raise MarkupParserError(self.current_file, self.parser.CurrentLineNumber, 0, "Expecting <markup>")

    def parseMarkupNames(self):
        import xml.parsers.expat

        logmessage("Parsing module.markup files.")
        for filename in self.markup_files:
            logmessage("  parsing: %s" % filename)
            self.current_file = filename
            file = open(filename, "r")
            self.parser = xml.parsers.expat.ParserCreate()
            self.parser.StartElementHandler = self.startElmHandler
            self.parser.EndElementHandler = self.endElmHandler
            try:
                self.parser.ParseFile(file)
            except xml.parsers.expat.ExpatError, xe:
                raise MarkupParserError(filename, xe.lineno, xe.offset, xml.parsers.expat.ErrorString(xe.code))
            finally:
                self.parser = None
            file.close()

    def readHashConstants(self):
        logmessage("Reading new hash constants:")

        hashFileName = os.path.join(self.hash_file_path, self.base_file_pattern % ("element", ""))
        hashFile = open(hashFileName, "r")

        for line in hashFile:
            entry = line.strip()
            [dummy, name, val] = entry.split(" ")
            if name == "HTML5_TAG_HASH_BASE":
                self.elmHashBase = int(val)
            elif name == "HTML5_TAG_HASH_SIZE":
                self.elmHashSize = int(val)

        hashFile.close()

        hashFileName = os.path.join(self.hash_file_path, self.base_file_pattern % ("attr", ""))
        hashFile = open(hashFileName, "r")

        for line in hashFile:
            entry = line.strip()
            [dummy, name, val] = entry.split(" ")
            if name == "HTML5_ATTR_HASH_BASE":
                self.attrHashBase = int(val)
            elif name == "HTML5_ATTR_HASH_SIZE":
                self.attrHashSize = int(val)

        hashFile.close()
        logmessage(" Found elm(base:%d, size:%d) and attr(base:%d, size:%d)\n" % (self.elmHashBase, self.elmHashSize, self.attrHashBase, self.attrHashSize))

    def testHashing(self, list, constant, size):
        numEntries = len(list)
        logmessage(" Testing hashing with constant=%d and size=%d. %d entries" % (constant, size, numEntries))
        collisions = 0
        maxCollisions = 0
        slots = {}

        for name in list:
            hashVal = hashString(name, constant)
            slot = hashVal % size

            if slot in slots:
                count = slots[slot]
                count += 1
                slots[slot] = count

                if count == 2:
                    collisions += 1
                else:
                    if count > maxCollisions:
                        maxCollisions = count
            else:
                slots[slot] = 1

        return (collisions, maxCollisions)

    def writeHashConstants(self, newBase, isAttr):
        logmessage(" Writing new hash base (%d):" % newBase)

        if markup_debug:
            fileSuffix = ".debug"
            filePath = self.out_file_path
        else:
            fileSuffix = ""
            filePath = self.hash_file_path

        if isAttr:
            hashFileName = os.path.join(filePath, self.base_file_pattern % ("attr", fileSuffix))
            content = "#define HTML5_ATTR_HASH_BASE %d\n#define HTML5_ATTR_HASH_SIZE %d\n" % (newBase, self.attrHashSize)
        else:
            hashFileName = os.path.join(filePath, self.base_file_pattern % ("element", fileSuffix))
            content = "#define HTML5_TAG_HASH_BASE %d\n#define HTML5_TAG_HASH_SIZE %d\n" % (newBase, self.elmHashSize)

        hashFile = open(hashFileName, "w")
        hashFile.write(content)
        hashFile.close()
        logmessage(" Wrote %s\n" % hashFileName)

    def tuneHashing(self, list, isAttr):
        logmessage(" Tuning hashing constants:")

        bestBase = -1
        fewestCollisions = 99999
        base = 0
        if isAttr:
            size = self.attrHashSize
        else:
            size = self.elmHashSize

        while base < 10000:
            result = self.testHashing(list, base, size)
            if result[1] == 0:
                logmessage("  Found max-2 base: %d" % base)
                if result[0] < fewestCollisions:
                    bestBase = base
                    fewestCollisions = result[0]
            else:
                logmessage("  Exceeded max-2 collisions: %d" % result[1])
            base += 1

        if bestBase == -1:
            logmessage(" No appropriate hashing found!\n")
            if isAttr:
                fileName = os.path.join(self.hash_file_path, self.base_file_pattern % ("attr", ""))
            else:
                fileName = os.path.join(self.hash_file_path, self.base_file_pattern % ("element", ""))

            raise HashTestingError(fileName)
        else:
            logmessage(" Best base found [%d], collisions(%d)\n" % (bestBase, fewestCollisions))
            self.writeHashConstants(bestBase, isAttr)

    def getTidyNameList(self, origList):
        nameMap = {}
        for entry in origList:
            nameMap[entry[0]] = 1
        return nameMap.keys()

    def checkHashing(self):
        did_tuning = False

        logmessage("\nChecking element hashing for collisions:")
        nameList = self.getTidyNameList(self.elements)
        result = self.testHashing(nameList, self.elmHashBase, self.elmHashSize)
        logmessage(" Collisions(%d), Max collisions(%d)" % result)
        if result[1] > 0:
            self.tuneHashing(nameList, False)
            did_tuning = True

        logmessage("Checking attribute hashing for collisions:")
        nameList = self.getTidyNameList(self.attributes)
        result = self.testHashing(nameList, self.attrHashBase, self.attrHashSize)
        logmessage("  Collisions(%d), Max collisions(%d)" % result)
        if result[1] > 0:
            self.tuneHashing(nameList, True)
            did_tuning = True

        return did_tuning

    class MarkupTypeTemplateAction:
        def __init__(self, is_attr, normals, specials, name_map, max_len):
            self.is_attr = is_attr
            self.normals = normals
            self.specials = specials
            self.name_map = name_map
            if is_attr:
                self.name_type = "A"
            else:
                self.name_type = "E"
            self.max_len = max_len
            self.first_name = None
            self.first_special = None

        def __call__(self, action, output):
            if action == "lengthconsts":
                if self.is_attr:
                    max_len_const = "kMaxAttributeNameLength"
                    table_len_const = "kUpperAttrNameTableLength"
                else:
                    max_len_const = "kMaxTagNameLength"
                    table_len_const = "kUpperTagNameTableLength"
                output.write("static const unsigned\t%s = %d;\n" % (max_len_const, self.max_len))
                output.write("static const unsigned\t%s = %d;\n" % (table_len_const, len(self.name_map)))
                return True
            elif action == "normaltypes":
                aggregation = ""

                for [name, ns, prefix] in self.normals:
                    name_esc = name.replace('-', '_').upper()
                    type_str = "%s%s_%s" % (prefix.upper(), self.name_type, name_esc)
                    if ns != self.name_map[name][0]:
                        aggregation += "\t, %s = %s%s_%s\n" % (type_str, self.name_map[name][1].upper(), self.name_type, name_esc)
                    else:
                        if self.first_name is None:
                            self.first_name = type_str

                        if aggregation != "":
                            output.write("\t, %s__PLACEHOLDER // placeholder for next non-duplicate\n" % type_str)
                            output.write(aggregation)
                            output.write("\t, %s = %s__PLACEHOLDER\n" % (type_str, type_str))
                            aggregation = ""
                        else:
                            output.write("\t, %s\n" % type_str)

                last_name = "HTE_UNKNOWN"
                if self.is_attr:
                    last_name = "HA_XML"

                if aggregation != "":
                    output.write("\t, %s__PLACEHOLDER\n" % last_name)
                    output.write(aggregation)
                    output.write("\t, %s = %s__PLACEHOLDER\n" % (last_name, last_name))
                else:
                    output.write("\t, %s\n" % last_name)
                return True
            elif action == "specialtypes":
                aggregation = ""
                self.specials.sort()

                for [name, ns, prefix] in self.specials:
                    name_str = "%s%s_%s" % (prefix.upper(), self.name_type, name.replace('-', '_').upper())
                    if self.first_special is None:
                        self.first_special = name_str

                    if aggregation:
                        output.write("\t, %s = %s\n" % (name_str, aggregation))
                        aggregation = ""
                    else:
                        output.write("\t, %s\n" % name_str)
                return True
            elif action == "delimiters":

                if self.is_attr:
                    output.write("\t, HA_FIRST = %s\n" % self.first_name)
                    output.write("\t, HA_LAST = HA_XML\n")
                    output.write("\t, HA_FIRST_SPECIAL = %s\n" % self.first_special)
                else:
                    output.write("\t, HTE_FIRST = %s\n" % self.first_name)
                    output.write("\t, HTE_LAST = HTE_UNKNOWN\n")
                    output.write("\t, HTE_LAST_WITH_LAYOUT = HTE_TEXTGROUP\n")
                    output.write("\t, HTE_FIRST_SPECIAL = %s\n" % self.first_special)
                return True
            return False

    class MarkupNameTemplateAction:
        def __init__(self, is_attr, list, name_map, bigendian):
            self.is_attr = is_attr
            self.list = list
            self.name_map = name_map
            self.bigendian = bigendian
            self.indices = []
            self.upper_names = ""

        def __call__(self, action, output):
            if action == "nametable":
                index = 0
                first = True
                for [name, ns, prefix] in self.list:
                    if ns == self.name_map[name][0]:
                        self.indices.append(index)
                        result = self.makeArray(name, ns)
                        index += result[1]
                        if first:
                            output.write("\t%s\n" % result[0])
                            self.upper_names += "\tCONST_ENTRY(UNI_L(\"%s\"))\n" % name.upper()
                            first = False
                        else:
                            output.write("\t, %s\n" % result[0])
                            self.upper_names += "\t, CONST_ENTRY(UNI_L(\"%s\"))\n" % name.upper()
                return True
            elif action == "indextable":
                for index in self.indices:
                    if index == 0:
                        output.write("\t%d\n" % index)
                    else:
                        output.write("\t, %d\n" % index)
                return True
            elif action == "uppernametable":
                output.write(self.upper_names);
                return True
            return False

        def makeArray(self, s, ns):
            # textArea is a special case since it has different case in
            # HTML and SVG. In order to get just one entry for it in the table,
            # we pretend that HTML has the same string but use this trick to
            # use the lowercase version for any namespace but SVG.
            if s == "textArea":
                ns = "SVG"

            index_increment = 1
            str_len = len(s)
            add_flattened = s != s.lower()
            if add_flattened:
                out_array = [str_len + 1, "Markup::%s" % ns.upper()]
            else:
                out_array = [0, 0]

            index_increment += str_len + 1
            if self.bigendian:
                for c in s:
                    out_array.extend([0, "'%s'" % c])
            else:
                for c in s:
                    out_array.extend(["'%s'" % c, 0])
            out_array.extend([0, 0])

            if add_flattened:
		index_increment += str_len + 1
                if self.bigendian:
                    for c in s:
                        out_array.extend([0, "'%s'" % c.lower()])
                else:
                    for c in s:
                        out_array.extend(["'%s'" % c.lower(), 0])
                out_array.extend([0, 0])

            return [", ".join(map(str, out_array)), index_increment * 2]

    def generateTables(self):
        max_name_len = 0
        name_map = {}
        normals = None
        specials = None
        type_filename = ""
        name_filename = ""
        file_name_suffix = ""
        has_changed = False

        if markup_debug:
            file_name_suffix = ".debug"

        #
        # Generate element tables
        #
        normals = self.elements
        specials = self.special_elements
        type_filename = self.type_file_pattern % ("element", "%s")
        name_filename = self.name_file_pattern % ("element", "%s")

        normals.sort()
        # Scan through the list to find duplicate names and
        # the length of the longest name
        for [name, ns, prefix] in normals:
            if not name in name_map:
		name_map[name] = [ns, prefix]
            if len(name) > max_name_len:
		max_name_len = len(name)

        type_template_file = os.path.join(self.template_file_path, type_filename % "_template")
        name_template_file = os.path.join(self.template_file_path, name_filename % "_template")
        type_out_file = os.path.join(self.out_file_path, type_filename % file_name_suffix)
        name_out_file = os.path.join(self.out_file_path, name_filename % file_name_suffix)

        logmessage("\nGenerating element type file:\n  %s\nfrom template:\n  %s\n" % (type_out_file, type_template_file))
        if util.readTemplate(type_template_file, type_out_file, self.MarkupTypeTemplateAction(False, normals, specials, name_map, max_name_len)):
            has_changed = True

        logmessage("\nGenerating element name file:\n  %s\nfrom template:\n  %s\n" % (name_out_file, name_template_file))
        if util.readTemplate(name_template_file, name_out_file, self.MarkupNameTemplateAction(False, normals, name_map, self.bigendian)):
            has_changed = True

        #
        # Generate attribute tables
        #
        name_map = {}
        normals = self.attributes
        specials = self.special_attributes
        type_filename = self.type_file_pattern % ("attr", "%s")
        name_filename = self.name_file_pattern % ("attr", "%s")

        normals.sort()
        # Scan through the list to find duplicate names and
        # the length of the longest name
        for [name, ns, prefix] in normals:
            if not name in name_map:
		name_map[name] = [ns, prefix]
            if len(name) > max_name_len:
		max_name_len = len(name)

        type_template_file = os.path.join(self.template_file_path, type_filename % "_template")
        name_template_file = os.path.join(self.template_file_path, name_filename % "_template")
        type_out_file = os.path.join(self.out_file_path, type_filename % file_name_suffix)
        name_out_file = os.path.join(self.out_file_path, name_filename % file_name_suffix)

        logmessage("\nGenerating attribute type file:\n  %s\nfrom template:\n  %s\n" % (type_out_file, type_template_file))
        if util.readTemplate(type_template_file, type_out_file, self.MarkupTypeTemplateAction(True, normals, specials, name_map, max_name_len)):
            has_changed = True

        logmessage("\nGenerating attribute name file:\n  %s\nfrom template:\n  %s\n" % (name_out_file, name_template_file))
        if util.readTemplate(name_template_file, name_out_file, self.MarkupNameTemplateAction(True, normals, name_map, self.bigendian)):
            has_changed = True

        return has_changed

    def regenerateSVGpresentationAttrs(self):
        global markup_debug
        logmessage("Calling SVG script")
        svg_generator = make_pres_attr_table.SVGPresentationAttrGenerator()
        return svg_generator(sourceRoot=self.src_root, outputRoot=self.out_root, quiet=True, debug=markup_debug)

    def processMarkupFiles(self, sourceRoot):
        self.readHashConstants()

        logmessage("Searching for module.markup files in %s" % sourceRoot)
        # Find the module.markup files in the source tree
        for module in opera_module.modules(sourceRoot):
            filename = module.getModuleFile("markup")
            if util.fileTracker.addInput(filename):
                self.markup_files.append(filename)

        logmessage("  found %d files.\n" % len(self.markup_files))

        # Generate the files from the module.markup data
        try:
            self.error_value = 0

            self.parseMarkupNames()

            if self.checkHashing():
                self.error_value = 2

            if self.generateTables():
                self.error_value = 2

            if self.regenerateSVGpresentationAttrs() == 2:
                self.error_value = 2
        except MarkupParserError, mpe:
            logmessage("  FATAL: bad markup at line %d, column %d! %s." % (mpe.line_num, mpe.col_num, mpe.error_msg))
            self.addError(util.Line(mpe.file_name, mpe.line_num), "FATAL: bad markup!\n%s" % mpe.error_msg)
            self.error_value = 1
        except HashTestingError, hte:
            logmessage("  FATAL: testing hashing failed!%s" % hte.error_msg)
            self.addError(util.Line(hte.file_name, 1), "FATAL: hash tuning failed!\n%s" % hte.error_msg)
            self.error_value = 1

        self.printWarningsAndErrors()
        return self.error_value

class ParseMarkupNames(basesetup.SetupOperation):
    def __init__(self, bigendian):
        if bigendian:
            self.bigendian = True
        else:
            self.bigendian = sys.byteorder == "big"
        basesetup.SetupOperation.__init__(self, message="markup names")

    def __call__(self, sourceRoot, outputRoot, quiet, debug=False):
        self.startTiming()
        parser = MarkupNameParser(sourceRoot, outputRoot, self.bigendian, debug, quiet)
        result = parser.processMarkupFiles(sourceRoot)
        return self.endTiming(result, quiet=quiet)

if is_standalone:
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))

    option_parser = basesetup.OperaSetupOption(
        sourceRoot = sourceRoot,
        description=" ".join(
            ["Generates elementtypes.h, elementnames.h, attrtypes.h,",
             "attrnames.h in modules/logdoc/src/html5 from ",
             "{adjunct,modules,platforms}/*/module.markup and ",
             "templates in modules/logdoc/scripts/. ",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))
    option_parser.add_option("-d", "--debug", dest="debug_mode", action="store_true")

    option_parser.add_option(
        "-b", "--big-endian", action="store_true", dest="bigendian",
        help=" ".join(
            ["Force the byte order to big-endian. The default is to use the",
             "byte order returned by sys.byteorder on the building platform,"
             "but when cross-compiling for a big-endian platform this flag",
             "must be specified."]))

    option_parser.set_defaults(debug_mode=False, bigendian=False)

    (options, args) = option_parser.parse_args(args=sys.argv[1:])

    parse_markup = ParseMarkupNames(options.bigendian)
    result = parse_markup(sourceRoot=sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet, debug=options.debug_mode)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2:
        sys.exit(0)
    else:
        sys.exit(result)
