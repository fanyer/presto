#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

import re
import sys
import os
import os.path
import time
import util
from cStringIO import StringIO

def generate(sourceRoot, outputRoot):
    all_prototypes = {}
    groups = []

    re_exclude_from_format = re.compile(r"ifdef|ifndef|if|\(|\)|defined")
    re_whitespace = re.compile(r"\s+")

    def formatCondition(condition):
        condition = re_exclude_from_format.sub("", condition)
        condition = re_whitespace.sub(" ", condition)
        return condition.strip()

    class File:
        def __init__(self, path, root=outputRoot):
            self.__path = os.path.join(root, *path.split("/"))
            self.__file = StringIO()
            self.__updated = False

        def __getattr__(self, name):
            return getattr(self.__file, name)

        def close(self):
            written = self.__file.getvalue()

            try: existing = open(self.__path).read()
            except: existing = None

            if written != existing:
                self.__updated = True
                util.makedirs(os.path.dirname(self.__path))
                open(self.__path, "w").write(written)

        def updated(self):
            return self.__updated

        def __str__(self):
            return self.__path

    class Group:
        def __init__(self, title, condition):
            self.__title = title
            self.__condition = condition
            self.__prototypes = []

        def addPrototype(self, prototype):
            self.__prototypes.append(prototype)

        def getTitle(self): return self.__title

        def getLines(self, for_file):
            lines = ["\t/* %s */\n" % self.__title]

            if self.__condition: lines.append("#%s\n" % self.__condition)

            actual_lines = 0

            current_condition = None
            for index, prototype in enumerate(self.__prototypes):
                prototype_lines = prototype.getLines(for_file)
                if prototype_lines:
                    if prototype.getCondition() != current_condition:
                        if current_condition: lines.append("#endif // %s\n" % formatCondition(current_condition))
                        current_condition = prototype.getCondition()
                        if current_condition: lines.append("#%s\n" % current_condition)

                    lines.extend(prototype_lines)
                    actual_lines += len(prototype_lines)

            if actual_lines == 0: return []

            if current_condition: lines.append("#endif // %s\n" % formatCondition(current_condition))

            if self.__condition: lines.append("#endif // %s\n" % formatCondition(self.__condition))

            lines.append("\n")
            return lines

    class Prototype:
        def __init__(self, name, classname, displayname, functions, functions_with_data, prepare_prototype_function, baseprototype, condition):
            self.__displayname = displayname
            self.__name = name
            self.__classname = classname
            self.__functions = functions
            self.__functions_with_data = functions_with_data
            self.__prepare_prototype_function = prepare_prototype_function
            self.__condition = condition

            if baseprototype:
                if baseprototype == "(ERROR)": self.__baseprototype = -2
                else: self.__baseprototype = "DOM_Runtime::%s_PROTOTYPE" % baseprototype
            else: self.__baseprototype = -1

        def getCondition(self): return self.__condition

        def getLines(self, for_file):
            if self.__name.startswith("(") and self.__name.endswith(")") and for_file in ("domruntime.h", "domruntime.cpp", "domruntime.cpp-constructor-name", "domprototypes.cpp"):
                return None

            if for_file == "domruntime.h":
                return ["\t%s_PROTOTYPE,\n" % self.__name]
            elif for_file == "domruntime.cpp":
                return ["\tDOM_PROTOTYPE_CLASSNAMES_ITEM(\"%s\")\n" % (self.__displayname)];
            elif for_file == "domruntime.cpp-constructor-name":
                return ["\tDOM_CONSTRUCTOR_NAMES_ITEM(\"%s\")\n" % (self.__displayname)];
            elif for_file == "domprototypes.cpp":
                if self.__functions and self.__functions_with_data:
                    return ["\tDOM_PROTOTYPES_ITEM3(%s_PROTOTYPE, %s_functions, %s_functions_with_data, %s)\n" % (self.__name, self.__classname, self.__classname, self.__baseprototype)]
                elif self.__functions:
                    return ["\tDOM_PROTOTYPES_ITEM1(%s_PROTOTYPE, %s_functions, %s)\n" % (self.__name, self.__classname, self.__baseprototype)]
                elif self.__functions_with_data:
                    return ["\tDOM_PROTOTYPES_ITEM2(%s_PROTOTYPE, %s_functions_with_data, %s)\n" % (self.__name, self.__classname, self.__baseprototype)]
                else:
                    return ["\tDOM_PROTOTYPES_ITEM0(%s_PROTOTYPE, %s)\n" % (self.__name, self.__baseprototype)]
            elif for_file == "domprototypes.cpp-prepare-prototype":
                if self.__prepare_prototype_function:
                    return ["\tcase %s_PROTOTYPE:\n" % self.__name, "\t\t%s::%s(object, this);\n" % (self.__classname, self.__prepare_prototype_function), "\t\treturn;\n"]
                else:
                    return None
            else:
                if self.__functions and self.__functions_with_data:
                    return ["\tDOM_FUNCTIONS3(%s)\n" % self.__classname]
                elif self.__functions:
                    return ["\tDOM_FUNCTIONS1(%s)\n" % self.__classname]
                elif self.__functions_with_data:
                    return ["\tDOM_FUNCTIONS2(%s)\n" % self.__classname]

    group_decl = re.compile("^group\\s+\"([^)]+)\"(\\s+{([^}]*)})?$")
    group_start = re.compile("^{$")
    prototype_decl = re.compile("^(\\(?[A-Z0-9_]+\\)?):\\s+([A-Za-z0-9_]+)\\((\S+(?:,\s*\S+)*)?\\)(?:\\s+base=([A-Z0-9_]+|\([A-Z0-9_]+\)))?(?:\\s+{([^}]*)})?$")
    group_end = re.compile(r"^}$")

    pending_group = None
    current_group = None

    def fatal(message):
        print >>sys.stderr, message
        return 1

    prototypes_txt_path = os.path.join(sourceRoot, "modules/dom/src/prototypes.txt")
    util.fileTracker.addInput(prototypes_txt_path)
    for line in open(prototypes_txt_path):
        line = line.strip()

        if not line or line[0] == "#": continue

        match = group_decl.match(line)
        if match:
            title = match.group(1)
            condition = match.group(3)

            if current_group: return fatal("Nested group (\"%s\" inside \"%s\")" % (title, current_group.getTitle()))
            if pending_group: return fatal("Parse error: %s" % line)

            pending_group = Group(title, condition)
            continue

        match = group_start.match(line)
        if match:
            if not pending_group: return fatal("Unexpected {")
            current_group = pending_group
            pending_group = None
            continue

        match = prototype_decl.match(line)
        if match:
            name = match.group(1)
            classname = match.group(2)
            functions_decl = match.group(3)
            baseprototype = match.group(4)
            condition = match.group(5)

            if all_prototypes.has_key(name): return fatal("Duplicate prototype: %s" % name)
            if current_group is None: return fatal("Prototype outside group: %s" % name)


            displayname = None
            functions = False
            functions_with_data = False
            prepare_prototype_function = None

            if functions_decl is not None:
                items = [item.strip() for item in functions_decl.split(",")]
                for item in items:
                    if displayname is None:
                        displayname = item
                    elif item == "functions_with_data":
                        functions_with_data = True
                    elif item == "functions":
                        functions = True
                    elif prepare_prototype_function is None:
                        prepare_prototype_function = item
                    else:
                        return fatal("Failed to grok functions declaration for prototype: %s" % name)

            if not displayname:
                return fatal("Missing display name for prototype: %s" % name)

            current_group.addPrototype(Prototype(name, classname, displayname, functions, functions_with_data, prepare_prototype_function, baseprototype, condition))
            continue

        match = group_end.match(line)
        if match:
            groups.append(current_group)
            current_group = None
            continue

        return fatal("Parse error: %s" % line)

    domruntime_h = File("modules/dom/src/domruntime.h.inc")
    domruntime_h.write("""/* -*- Mode: c++; indent-tabs-mode: t; tab-width: 4 -*- */

enum Prototype
{
""")

    for group in groups:
        domruntime_h.writelines(group.getLines("domruntime.h"))

    domruntime_h.write("""\tPROTOTYPES_COUNT
};
""")

    domruntime_h.close()

    domruntime_cpp = File("modules/dom/src/domruntime.cpp.inc")
    domruntime_cpp.write("""/* -*- Mode: c++; indent-tabs-mode: t; tab-width: 4 -*- */

DOM_PROTOTYPE_CLASSNAMES_START()

""")

    for group in groups:
        domruntime_cpp.writelines(group.getLines("domruntime.cpp"))

    domruntime_cpp.write("\tDOM_PROTOTYPE_CLASSNAMES_ITEM_LAST(\"\")\n\nDOM_PROTOTYPE_CLASSNAMES_END()\n\nDOM_CONSTRUCTOR_NAMES_START()\n\n")

    for group in groups:
        domruntime_cpp.writelines(group.getLines("domruntime.cpp-constructor-name"))

    domruntime_cpp.write("\tDOM_CONSTRUCTOR_NAMES_ITEM_LAST(\"\")\n\nDOM_CONSTRUCTOR_NAMES_END()\n")
    domruntime_cpp.close()

    domprototypes_cpp = File("modules/dom/src/domprototypes.cpp.inc")
    domprototypes_cpp.write("""/* -*- Mode: c++; indent-tabs-mode: t; tab-width: 4 -*- */

DOM_PROTOTYPES_START()

""")

    for group in groups:
        domprototypes_cpp.writelines(group.getLines("domprototypes.cpp"))

    domprototypes_cpp.write("""DOM_PROTOTYPES_END()

void
DOM_Runtime::PreparePrototypeL(ES_Object *object, Prototype type)
{
\tswitch (type)
\t{
""")

    for group in groups:
        domprototypes_cpp.writelines(group.getLines("domprototypes.cpp-prepare-prototype"))

    domprototypes_cpp.write("""\tdefault:
\t\tstatic_cast<void>(object);
\t\t/* No preparation necessary. */
\t\treturn;
\t}
}
""")

    domprototypes_cpp.close()

    domglobaldata = File("modules/dom/src/domglobaldata.inc")
    domglobaldata.write("""/* -*- Mode: c++; indent-tabs-mode: t; tab-width: 4 -*- */

    """)

    written = set()
    for group in groups:
        for line in group.getLines("domglobaldata"):
            if line not in written:
                domglobaldata.write(line)
                if line.strip() and not line.startswith("#"):
                    written.add(line)

    domglobaldata.close()

    if outputRoot == sourceRoot:
        util.updateModuleGenerated(os.path.join(sourceRoot, "modules", "dom"),
                                   ["src/domruntime.h.inc",
                                    "src/domruntime.cpp.inc",
                                    "src/domprototypes.cpp.inc",
                                    "src/domglobaldata.inc"])

    if domruntime_h.updated() or domruntime_cpp.updated() or domprototypes_cpp.updated() or domglobaldata.updated():
        return 2
    else:
        return 0
