import sys
import os
import os.path
import platform
import subprocess

# import from modules/hardcore/scripts/
import basesetup

def logmessage(quiet, message):
    if not quiet:
        print message

def error(message):
    print >>sys.stderr, "error:", message

def warning(message):
    print >>sys.stderr, "warning:", message

class GenerateSelftests(basesetup.SetupOperation):
    """
    This class can be used to execute modules/selftest/parser/parse_tests.pike
    to generate all selftest source files from all .ot files.

    An instance of this class can be used as one of operasetup.py's
    "system-functions".
    """
    def __init__(self, pike=None):
        basesetup.SetupOperation.__init__(self, message="Selftest system")
        self.__pike = pike

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        """
        Calling this instance will generate the .cpp files for all .ot files.

        @param sourceRoot is the root directory of the source tree
          that was parsed. Some of the output files are always
          generated relative to the sourceRoot.
        @param outputRoot is the directory relative to which all files
          are generated
        @param quiet if False print some log messages.

        @return The convention of the "system-functions" are that the
          return value should be
          - 0 to indicate success
          - 1 to indicate an error
          - 2 to indicate that output files have changed.
        """
        self.startTiming()
        result = 0
        if self.__pike is None:
            if "PIKE" in os.environ and os.access(os.environ["PIKE"], os.F_OK | os.X_OK):
                self.__pike = os.environ["PIKE"]
                logmessage(quiet, "use pike from environment PIKE=%s" % self.__pike)

            else:
                # search for a pike executable in the PATH environment
                if platform.system() in ("Windows", "Microsoft"):
                    pike_executable = "pike.exe"
                    pathSeparator = ";"
                else:
                    pike_executable = "pike"
                    pathSeparator = ":"

                path_env = os.environ.get("PATH", "")
                for path in filter(None, map(str.strip, path_env.split(pathSeparator))):
                    if os.access(os.path.join(path, pike_executable), os.F_OK | os.X_OK):
                        logmessage(quiet, "found pike in path '%s'" % path)
                        self.__pike = os.path.join(path, pike_executable)
                        break
                else:
                    error("can't find suitable pike executable; set the environment variable PIKE to the pike executable or adjust your PATH (current value='%s') to include the path to the pike executable or use --pike argument" % path_env)
                    result = 1

        elif os.access(self.__pike, os.F_OK | os.X_OK):
            logmessage(quiet, "use pike from commandline argument --pike=%s" % self.__pike)

        else:
            error("invalid --pike argument; path is not an executable file")
            result = 1

        if result == 0:
            # Now self.__pike should be the path to a pike executable

            parser_dir = os.path.join(sourceRoot, "modules", "selftest", "parser")
            parser_path = os.path.join(parser_dir, "parse_tests.pike")
            logmessage(quiet, "working dir is %s; calling script %s" % (parser_dir, parser_path))
            result = subprocess.call([self.__pike, parser_path] + (['--outroot', outputRoot] if outputRoot else []),
                                     cwd=parser_dir, stdout=sys.stdout, stderr=sys.stderr)
        return self.endTiming(result, quiet=quiet)


if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Executes modules/selftest/parser/parse_tests.pike to generate",
             "all selftest source files from all .ot files."]))

    option_parser.add_option(
        "--pike", metavar="PATH_TO_EXECUTABLE",
        help=" ".join(
            ["Specifies the full path to the Pike executable to use on",
             "running parse_tests.pike. If the option is not specified, pike",
             "is expected to be found in one of the directories specified in",
             "the PATH environment variable."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    selftests = GenerateSelftests(options.pike)
    result = selftests(sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
