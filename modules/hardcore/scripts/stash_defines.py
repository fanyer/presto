#!/usr/bin/python
# -*- coding: utf-8 -*-

# System modules.
import os
import sys

# Modules in hardcore/scripts/.
import basesetup
import util

class StashDefines(basesetup.SetupOperation):
    """
    This class generates core/stashed_defines.h from the
    defines given to the compiler via the command line.

    This makes them visible to IDEs that parse the source code
    to find function and macro definitions.
    """

    def __init__(self, defines):
        basesetup.SetupOperation.__init__(self, message = "stashed defines")
        self.__defines = defines


    def __call__(self, sourceRoot, outputRoot = None, quiet = "yes"):
        self.startTiming()

        if outputRoot is None:
            outputRoot = sourceRoot

        template_file = os.path.join(sourceRoot, "modules", "hardcore", "base", "stashed_defines_template.h")
        header_file = os.path.join(outputRoot, "core", "stashed_defines.h")

        # Bind function to this instance.
        handler = self.HandleTemplateAction.__get__(self, StashDefines)

        changed = util.readTemplate(template_file, header_file, handler)

        return self.endTiming(2 if changed else 0)


    def HandleTemplateAction(self, action, output):
        if action != "defines":
            return False

        defines = sorted(self.__defines.items())

        for (key, value) in defines:
            if value is None:
                output.write("#define %s\n" % key)
            else:
                output.write("#define %-24s %s\n" % (key, value))

        return True


def main():
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))

    option_parser = basesetup.OperaSetupOption(sourceRoot = sourceRoot,
        description = "Generates a header file from command-line defines.")

    # for compatibility with operasetup.py, store the defines in a dictionary
    def store_define(option, option_str, value, option_parser):
        if "=" in value:
            key, val = value.split("=", 1)
        else:
            key, val = value, None
        option_parser.values.ensure_value(option.dest, dict()).update({key:val})

    option_parser.add_option("-D", metavar = "DEFINE[=VALUE]", dest = "defines", type = "string",
        action = "callback", callback = store_define, default = {},
        help = "Add the specified define with the specified value.")

    (options, args) = option_parser.parse_args(sys.argv)

    stash_defines = StashDefines(options.defines)
    result = stash_defines(sourceRoot, options.outputRoot, options.quiet)

    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)

    if options.make and result == 2:
        result = 0

    return result

if __name__ == "__main__":
    sys.exit(main())
