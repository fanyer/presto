import sys
import os
import os.path
import platform
import subprocess
import re
import util

# import from modules/hardcore/scripts/
import basesetup

class GenerateViewers(basesetup.SetupOperation):
    """
    This class can be used to execute modules/viewers/src/viewers.py to generate
    source code for the viewers defined in the modules/viewers/module.viewers
    file.

    An instance of this class can be used as one of operasetup.py's
    "system-functions".
    """
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="Viewers system")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True, show_all=False):
        """
        Calling this instance will generate the source files from the
        modules/viewers/module.viewers file.

        @param sourceRoot is the root directory of the source tree
          that was parsed. Some of the output files are always
          generated relative to the sourceRoot.
        @param outputRoot root of the tree for generated files,
          defaults to sourceRoot
        @param quiet if False, print a message if no files were changed.
        @param show_all controls whether to show only modified files or all
          files which are inspected. Default is False.

        @return The convention of the "system-functions" are that the
          return value should be
          - 0 to indicate success
          - 1 to indicate an error
          - 2 to indicate that output files have changed.
        """
        self.startTiming()
        import viewers

        if outputRoot == None:
            outputRoot = sourceRoot

        modulepath = os.path.join(sourceRoot, "modules", "viewers")
        viewersfile = os.path.join(modulepath, "module.viewers")
        destpath = os.path.join(outputRoot, "modules", "viewers", "src")
        enumfile = os.path.join(destpath, "generated_viewers_enum.h")
        datafile = os.path.join(destpath, "generated_viewers_data.inc")
        result = 0
        util.fileTracker.addInput(viewersfile)
        if not os.path.exists(enumfile) or not os.path.exists(datafile) or \
            max(os.path.getmtime(viewersfile), os.path.getmtime(viewers.__file__)) > min(os.path.getmtime(enumfile), os.path.getmtime(datafile)):
            util.makedirs(destpath)
            viewers.BuildViewers(viewersfile, enumfile, datafile)
            result = 2

            # List the generated files in module.generated
            if outputRoot == sourceRoot:
                util.updateModuleGenerated(modulepath,
                                           [enumfile[len(modulepath) + 1:],
                                            datafile[len(modulepath) + 1:]])

        return self.endTiming(result, quiet=quiet)

if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "viewers", "src"))
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Executes BuildViewers() in modules/viewers/src/viewers.py to generate",
             "source files from viewer definitions (module.viewers)."]))

    option_parser.add_option(
        "--show-all", dest="show_all", action="store_true", default=False,
        help="Show all files which are processed, not just the ones which are modified")
    option_parser.add_option(
        "--show-modified", dest="show_all", action="store_false",
        help="Show only modified files which are processed")

    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    myviewers = GenerateViewers()
    result = myviewers(sourceRoot, quiet=options.quiet, show_all=options.show_all)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)

