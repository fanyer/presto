#!/usr/bin/env python
#
# This script can be used as a standalone script or from operasetup.py
# to generate css_values.h and css_value_strings.h based on their
# respective templates, drawing information from css_values.txt

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

class CSSValue:
    def __init__(self, name, value, internal):
        self.name = name
        self.value = value
        self.internal = internal

    def csymbol(self):
        return "CSS_VALUE_%s" % self.name.replace('-', '_')

    def enum_entry(self):
        return "%s = %s" % (self.csymbol(), self.value)

    def const_entry(self):
        return "CONST_ENTRY(UNI_L(\"%s\"))" % self.name

    def length(self):
        return len(self.name)

class CSSValuesTemplateActionHandler:
    def __init__(self, values, macros, longest_value):
        self.values = values
        self.macros = macros
        self.longest_value = longest_value

    def __call__(self, action, output):
        if action == "CSS values":
            output.write("\t")
            output.write(",\n\t".join(map(CSSValue.enum_entry, self.values)))

        elif action == "CSS value macros":
            for macro in self.macros:
                output.write(macro)

            output.write("\n#define CSS_VALUE_NAME_SIZE %d\n" % len(filter(lambda x: not x.internal, self.values)))
            output.write("#define CSS_VALUE_NAME_MAX_SIZE %d\n" % self.longest_value.length())

class CSSValueStringsTemplateActionHandler:
    def __init__(self, values, lengths, longest_value):
        self.sorted_values = filter(lambda x: not x.internal, values)
        self.sorted_values.sort(key=lambda x: x.name)
        self.sorted_values.sort(key=lambda x: x.length())
        self.lengths = lengths
        self.longest_value = longest_value

    def __call__(self, action, output):
        if action == "CSS value idx":
            accum_idx = 0
            for i in range(0, self.longest_value.length() + 2):
                output.write("\t%d" % accum_idx)
                self.lengths.setdefault(i, 0)
                accum_idx += self.lengths[i]
                output.write(",\t// start idx size %d\n" % i)

        elif action == "CSS value name":
            output.write("\t")
            output.write(",\n\t".join(map(CSSValue.const_entry, self.sorted_values)))
        elif action == "CSS value tok":
            output.write("\t")
            output.write(",\n\t".join(map(CSSValue.csymbol, self.sorted_values)))

class GenerateValues(basesetup.SetupOperation):
    """
    Generate css_values.h and css_value_strings.h from css_values.txt
    """
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="CSS values")

    def __call__(self, sourceRoot, outputRoot=None, quiet="yes"):
        self.startTiming()
        if outputRoot is None: outputRoot = sourceRoot

        css_values_txt = os.path.join(sourceRoot, "modules", "style", "src", "css_values.txt")
        css_values_template_h = os.path.join(sourceRoot, "modules", "style", "src", "css_values_template.h")
        css_values_h = os.path.join(outputRoot, "modules", "style", "src", "css_values.h")

        css_value_strings_template_h = os.path.join(sourceRoot, "modules", "style", "src", "css_value_strings_template.h")
        css_value_strings_h = os.path.join(outputRoot, "modules", "style", "src", "css_value_strings.h")

        longest_value = None
        values = []
        macros = []
        lengths = {}

        re_macro = re.compile("^\#define|^\#ifdef|^\#endif|^\#else")
        re_comment = re.compile("^\/\/")
        re_internal = re.compile("^#internal (.*)")

        util.fileTracker.addInput(css_values_txt)
        f = open(css_values_txt)
        for lineidx, line in enumerate(f):
            if (re_macro.match(line)):
                macros.append(line)
            elif re_comment.match(line) or line == "\n":
                pass
            else:
                internal = False
                m = re_internal.match(line)
                if (m):
                    line = m.group(1)
                    internal = True
                try:
                    kv = line;
                    try:
                        [kv, comment] = re.split("//", kv);
                    except:
                        pass
                    [k, v] = re.split('\s+', kv.strip())
                except ValueError:
                    self.warning("%s:%d: ignoring unknown line '%s'" % (css_values_txt, lineidx + 1, line.strip()))
                    continue

                v = CSSValue(k, v, internal)
                values.append(v)

                if not internal:
                    if (longest_value == None or v.length() > longest_value.length()):
                        longest_value = v

                    lengths.setdefault(v.length(), 0)
                    lengths[v.length()] += 1

        changed = util.readTemplate(css_values_template_h, css_values_h,
                                    CSSValuesTemplateActionHandler(values, macros, longest_value))

        changed = util.readTemplate(css_value_strings_template_h, css_value_strings_h,
                                    CSSValueStringsTemplateActionHandler(values, lengths, longest_value))

        if changed: result = 2
        else: result = 0
        return self.endTiming(result, quiet=quiet)


if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["This script can be used as a standalone script or from operasetup.py to",
             "generate css_values.h and css_value_strings.h based on their",
             "respective templates, drawing information from css_values.txt.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exit status is 1 if an error was detected."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    generate_values = GenerateValues()
    result = generate_values(sourceRoot=sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
