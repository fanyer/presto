#!/usr/bin/env python
#
# This script can be used as a standalone script or from operasetup.py to
# generate css_aliases.h, css_properties.h and css_property_strings.h based on
# their respective templates, drawing information from css_properties.txt

import os
import os.path
import sys
import re
import StringIO

if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "hardcore", "scripts"))

# import from modules/hardcore/scripts/:
import util
import basesetup


# Sort property names first by length, then by name
def sort_properties(x, y):
    if len(x) == len(y): return cmp(x, y)
    return len(x)-len(y)

class CssAliasesTemplateActionHandler:
    """
    This class is used to generate css_aliases.h from its template
    file css_property_aliases.txt.
    """
    def __init__(self, aliases):
        self.__aliases = aliases

    def __call__(self, action, output):
        if action == "switch aliased properties":
            for name in sorted(self.__aliases.keys()):
                for a in self.__aliases[name]:
                    output.write("\tcase CSS_PROPERTY_%(alias)s:\n" % {"alias":a.replace("-","_")})
                output.write("\t\treturn CSS_PROPERTY_%(name)s;\n" % {"name":name.replace("-","_")})
            return True
        return False

class PropertyStringsTemplateActionHandler:
    """
    This class is used to generate css_property_strings.h from its template
    file css_property_strings_template.h
    """
    def __init__(self, props):
        self.__props = props

    def __call__(self, action, output):
        if action == "property size index":
            lengths = map(lambda x : len(x), self.__props)
            start = 0
            for i in range(lengths[-1]+1):
                while i > lengths[start]:
                    start += 1
                if i < lengths[start] + 1:
                    output.write("\t%s,\t// start idx size %s\n" % (start, i))
            output.write("\t%s\t// start idx size %s\n" % (len(lengths), lengths[-1]+1))
            return True
        elif action == "property strings array":
            output.write(",\n".join(map(lambda x : ("\tCONST_ENTRY(\"%s\")" % x.upper()), self.__props)) + "\n")
            return True
        return False

class PropertiesTemplateActionHandler:
    """
    This class is used to generate css_properties.h from its template
    file css_properties_template.h
    """
    def __init__(self, props, internal_props):
        self.__props = props
        self.__internal_props = internal_props

    def __call__(self, action, output):
        if action == "property names":
            all_props = self.__props + self.__internal_props
            all_names = map(lambda x : "CSS_PROPERTY_%s" % re.sub("-", "_", x), all_props)
            output.write("\t%s\n" % ",\n\t".join(all_names))
            return True
        elif action == "properties length":
            output.write("#define CSS_PROPERTIES_LENGTH %s\n" % (len(self.__props) + len(self.__internal_props)))
            return True
        elif action == "property name size":
            output.write("#define CSS_PROPERTY_NAME_SIZE %s\n" % len(self.__props))
            return True
        elif action == "property name max size":
            output.write("#define CSS_PROPERTY_NAME_MAX_SIZE %s\n" % len(self.__props[-1]))
            return True
        return False

class GenerateProperties(basesetup.SetupOperation):
    """
    This drives the generation from templates of property and property alias
    related files.
    """
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="css properties")

    def __call__(self, sourceRoot, outputRoot=None, quiet="yes"):
        self.startTiming()
        if outputRoot is None: outputRoot = sourceRoot

        # File names
        css_properties_txt = os.path.join(sourceRoot, "modules", "style", "src", "css_properties.txt")
        css_aliases_template_h = os.path.join(sourceRoot, "modules", "style", "src", "css_aliases_template.h")
        css_aliases_h = os.path.join(outputRoot, "modules", "style", "src", "css_aliases.h")
        css_property_strings_template_h = os.path.join(sourceRoot, "modules", "style", "src", "css_property_strings_template.h")
        css_property_strings_h = os.path.join(outputRoot, "modules", "style", "src", "css_property_strings.h")
        css_properties_template_h = os.path.join(sourceRoot, "modules", "style", "src", "css_properties_template.h")
        css_properties_h = os.path.join(outputRoot, "modules", "style", "src", "css_properties.h")
        css_properties_internal_txt = os.path.join(sourceRoot, "modules", "style", "src", "css_properties_internal.txt")
        atoms_txt = os.path.join(sourceRoot, "modules", "dom", "src", "atoms.txt")

        # Read the property names from css_properties.txt into a set, with
        # their aliases in a dictionary, and the properties which are not
        # aliases into another set
        properties = set([])
        non_alias_properties = set([])
        aliases = dict({})
        try:
            f = None
            util.fileTracker.addInput(css_properties_txt)
            f = open(css_properties_txt)
            for line in f.read().split("\n"):
                if line:
                    props = line.split(",")
                    for p in props:
                        p = p.strip()
                        if p not in properties:
                            properties.add(p)
                        else:
                            self.error("Error: css property '%s' declared multiple times." % p)
                            return self.endTiming(1, quiet=quiet)
                    name = props[0].strip()
                    non_alias_properties.add(name)
                    if len(props) >= 2:
                        for a in props[1:]:
                            a = a.strip()
                            aliases.setdefault(name, []).append(a)
        finally:
            if f: f.close()
        sorted_properties = sorted(properties, cmp=sort_properties)

        # Read internal property names from css_propertiesi_internal.txt into a
        # set
        internal_properties = set([])
        try:
            f = None
            util.fileTracker.addInput(css_properties_internal_txt)
            f = open(css_properties_internal_txt)
            for line in f.read().split("\n"):
                p = line.strip()
                if p:
                    if p not in internal_properties and p not in properties:
                        internal_properties.add(p)
                    else:
                        self.error("Error: css property '%s' declared multiple times." % p)
                        return self.endTiming(1, quiet=quiet)
        finally:
            if f: f.close()
        sorted_internal_properties = sorted(internal_properties, cmp=sort_properties)

        # Regenerate css_properties.txt, with sorted non-alias properties each
        # followed by their alias to css_properties.txt,:
        output = StringIO.StringIO()
        for p in sorted(non_alias_properties, cmp=sort_properties):
            if p in aliases:
                output.write("%s, %s\n" % (p, ", ".join(sorted(aliases[p]))) )
            else:
                output.write("%s\n" % p)
        changed = util.updateFile(output, css_properties_txt)

        # Create css_aliases.h from its template:
        changed = util.readTemplate(css_aliases_template_h, css_aliases_h,
                        CssAliasesTemplateActionHandler(aliases)) or changed

        # Create css_property_strings.h from its template:
        changed = util.readTemplate(css_property_strings_template_h, css_property_strings_h,
                        PropertyStringsTemplateActionHandler(sorted_properties)) or changed

        # Create css_properties.h from its template:
        changed = util.readTemplate(css_properties_template_h, css_properties_h,
                        PropertiesTemplateActionHandler(sorted_properties, sorted_internal_properties)) or changed

        # Check that all properties are mCheck that all properties are in used
        # the atoms.txt file in dom as well.
        try:
            f = None
            util.fileTracker.addInput(atoms_txt)
            f = open(atoms_txt, "r")
            atoms = f.read().lower()
            for p in properties:
                if re.sub("-", "", p.lower()) not in atoms:
                    self.error("Warning: %s is missing from modules/dom/src/atoms.txt" % p)
        finally:
            if f: f.close()

        if changed: result = 2
        else: result = 0
        return self.endTiming(result, quiet=quiet)


if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["This script can be used as a standalone script or from operasetup.py to",
             "generate css_aliases.h, css_properties.h and css_property_strings.h based on",
             "their respective templates, drawing information from css_properties.txt.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exit status is 1 if an error was detected."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    generate_properties = GenerateProperties()
    result = generate_properties(sourceRoot=sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
