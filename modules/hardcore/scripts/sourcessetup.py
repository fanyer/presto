import os.path
import re
import sys

# import from modules/hardcore/scripts
if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))

import opera_module
import basesetup

class SourcesSetup(basesetup.SetupOperation):
    def __init__(self, verify_sources=True):
        basesetup.SetupOperation.__init__(self, message="sources setup")
        self.__verify_sources = verify_sources

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        if opera_module.generateSourcesSetup(sourceRoot, quiet, outputRoot=outputRoot, verify_sources=self.__verify_sources):
            result = 2 # changed
        else: result = 0 # no changes
        return self.endTiming(result, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Generate the files sources.all, sources.nopch, sources.pch,",
             "sources.pch_system_includes, sources.pch_jumbo and",
             "sources.component_* in both",
             "modules/hardcore/setup/plain/sources and",
             "modules/hardcore/setup/jumbo/sources.",
             "In addition generate the jumbo compile units of all modules.",
             "The script exits with status 0 if none of the output files",
             "was changed. The exit status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exit status is 1 if an error was detected."]))


    option_parser.add_option(
        "--ignore-missing-sources", action="store_false", dest="verify_sources",
        help=" ".join(
            ["Ignore missing source files. The default is to print an",
             "error message when a source file is not found and exit with",
             "status 1."]))

    option_parser.set_defaults(verify_sources=True)

    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    sources_setup = SourcesSetup(verify_sources=options.verify_sources)
    result = sources_setup(sourceRoot, options.outputRoot, options.quiet)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
