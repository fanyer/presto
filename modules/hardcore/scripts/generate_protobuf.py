import sys
import os
import os.path
import platform
import subprocess

# import from modules/hardcore/scripts/
import basesetup
import util

def logmessage(quiet, message):
    if not quiet:
        print message

def error(message):
    print >>sys.stderr, "error:", message

def warning(message):
    print >>sys.stderr, "warning:", message

class GenerateProtobuf(basesetup.SetupOperation):
    """
    """
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="Protobuf system")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True, show_all=False):
        """
        Calling this instance will generate the .cpp files for all .ot files.

        @param sourceRoot is the root directory of the source tree
          that was parsed. Some of the output files are always
          generated relative to the sourceRoot.
        @param outputRoot the top-level directory for generated files
        @param quiet if False print some log messages.
        @param show_all controls whether to show only modified files or all
          files which are inspected. Default is False.

        @return The convention of the "system-functions" are that the
          return value should be
          - 0 to indicate success
          - 1 to indicate an error
          - 2 to indicate that output files have changed.
        """
        self.startTiming()

        from cppgen.script import operasetup as cppgen_operasetup
        logmessage(quiet, "calling cppgen.script")
        result = cppgen_operasetup(sourceRoot, outputRoot=outputRoot, quiet=quiet, show_all=show_all)

        self.endTiming(result, quiet=quiet)

        # Don't report back modifications, this is a minor workaround
        # for operasetup to avoid full recompiles every time the protobuf
        # system is changed.
        # Any changes in those generated files are instead picked up
        # by the regular dependency system.
        if result == 2:
            result = 0

        return result


if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "..", "protobuf"))
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Executes operatorsetup() in modules/protobuf/cppgen/script.py to generate",
             "cpp files from protocol buffer definition files (.proto)."]))

    option_parser.add_option(
        "--show-all", dest="show_all", action="store_true", default=False,
        help="Show all files which are processed, not just the ones which are modified")
    option_parser.add_option(
        "--show-modified", dest="show_all", action="store_false",
        help="Show only modified files which are processed")

    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    protobuf = GenerateProtobuf()
    result = protobuf(sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet, show_all=options.show_all)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
