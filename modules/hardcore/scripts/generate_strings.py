import sys
import os
import os.path
import subprocess
import stat

# import from modules/hardcore/scripts/
import basesetup
import util

class GenerateStrings(basesetup.SetupOperation):
    """
    This class can be used to execute data/strings/scripts/makelang.pl to
    generate the files locale-enum.inc, locale-dbversion.h and
    locale-map.inc in {outputRoot}/modules/locale/ from the specified
    database files (makelangDBs).

    The output files are only re-generated if any of the files
    {outputRoot}/data/strings/build.strings or the database files are newer
    than the generated files.

    TODO: also store the arguments and re-generate the outputfiles if any
    of the arguments has changed.

    makelang.pl is called as

    perl -I{sourceRoot}/data/strings/scripts {sourceRoot}/data/strings/scripts/makelang.pl -d {outputRoot}/modules/locale -b {outputRoot}/data/strings/build.strings -c 9 -q -H {makelangOptions} {makelangDBs}

    where {makelangOptions} is an optional list of additional command line
    options, {makelangDBs} defaults to {sourceRoot}/data/strings/english.db
    {sourceRoot} is specified in __call__() and {outputRoot} specifies the
    destination root directory.

    The file build.strings is expected to be located relative to the same
    output root directory as the generated output files.

    An instance of this class can be used as one of operasetup.py's
    "system-functions".
    """
    def __init__(self, perl=None, makelangDBs=None, makelangOptions=None):
        """
        @param perl is the path to a Perl executable or None. If this value
          is None, the Perl executable is searched in either the PERL
          environment variable or in the list of paths set by the PATH
          environment variable.

        @param makelangDBs is a list of database files (like english.db)
          which should be used for generating the output files. The path of
          the databases is expected to be relative to the sourceRoot (as
          specified in method __call__()). If no list is specified, the file
          data/strings/english.db is used.

        @param makelangOptions specifies a list of additional command line
          arguments that is passed to makelang.pl.
        """
        basesetup.SetupOperation.__init__(self, message="locale setup", perl=perl)
        if makelangDBs is None:
            self.__makelangDBs = [os.path.join("data", "strings", "english.db")]
        else: self.__makelangDBs = makelangDBs
        self.__makelangOptions = makelangOptions

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        """
        Calling this instance will generate the locale output files.

        @param sourceRoot is the root directory of the source tree
          that was parsed. If no outputRoot was specified in the constructor,
          this directory is used as outputRoot. The makelang script
          data/strings/scripts/makelang.pl is expected to be relative to
          this directory. The database files (specified in the constructor)
          are expected to be relative to this directory
        @param outputRoot specifies the root for the generated files. The
          files locale-enum.inc, locale-dbversion.h and locale-map.inc are
          generated in {outputRoot}/modules/locale/.
        @param quiet if False print some log messages.

        @return The convention of the "system-functions" are that the
          return value should be
          - 0 to indicate success
          - 1 to indicate an error
          - 2 to indicate that output files have changed.
        """
        self.startTiming()
        result = 0
        perl = self.findPerl(quiet)
        if perl is None: result = 1
        else:
            # Now perl should be the path to a Perl executable
            if outputRoot is None:
                outputRoot = sourceRoot

            # Try to do some dependency checking for calling makelang.pl:
            # makelang.pl always writes its output files, so we only want to
            # call it, if some of the input files has changed.
            # @todo The dependency checks should better be placed inside
            # makelang.pl or inside the makefile.
            # @todo Add a dependency check for changed arguments: we should
            # call makelang.pl as well if any command line argument (like
            # a version number) has changed.
            needToRunStrings = False
            localeDirectory = os.path.join(outputRoot, "modules", "locale")
            if util.makedirs(localeDirectory):
                self.logmessage(quiet, "Need to call makelang.pl because locale directory %s does not yet exist" % localeDirectory)
                needToRunStrings = True

            # we need to run makelang.pl if any of the generated files
            # is missing:
            generated_mtime = None
            if not needToRunStrings:
                for filename in [os.path.join(localeDirectory, filename)
                                 for filename in ["locale-enum.inc", "locale-dbversion.h", "locale-map.inc"]]:
                    if not os.path.exists(filename):
                        self.logmessage(quiet, "Need to call makelang.pl because destination file %s does not yet exist" % filename)
                        needToRunStrings = True
                        break
                    elif generated_mtime is None:
                        generated_mtime = os.stat(filename)[stat.ST_MTIME]
                    else:
                        generated_mtime = min(generated_mtime, os.stat(filename)[stat.ST_MTIME])

            # we need to run makelang.pl if any of the generated files is older
            # than build.strings or any of the makelangDBs:
            buildStringsPath = os.path.join(outputRoot, "data", "strings", "build.strings")
            if not os.path.exists(buildStringsPath):
                self.error("File %s not found, cannot call makelang.pl" % buildStringsPath)
                result = 1

        if result == 0:
            build_mtime = os.stat(buildStringsPath)[stat.ST_MTIME]
            if generated_mtime < build_mtime:
                self.logmessage(quiet, "Need to call makelang because build.strings is newer than the destination files: %s > %s" % (str(build_mtime), str(generated_mtime)))
                needToRunStrings = True

            makelangDBs = [os.path.join(sourceRoot, path) for path in self.__makelangDBs]
            if not needToRunStrings:
                for db in makelangDBs:
                    if not os.access(db, os.F_OK):
                        self.error("makelang database %s not found; cannot call makelang.pl" % db)
                        result = 1

                    elif generated_mtime < os.stat(db)[stat.ST_MTIME]:
                        self.logmessage(quiet, "Need to call makelang because database is newer than the generated files")
                        needToRunStrings = True
                        break

        if result == 0:
            if not needToRunStrings:
                self.logmessage(quiet, "generated files are up to date")
                result == 0 # no changes

            else:
                # need to add the path to data/strings/scripts to Perls include
                # directory list, so makelang.pl can load its modules:
                perlArgs = ["-I"+os.path.join(sourceRoot, "data", "strings", "scripts")]
                makelang_pl = os.path.join(sourceRoot, "data", "strings", "scripts", "makelang.pl")
                makelangArgs = ["-d", localeDirectory, # output directory
                                "-b", buildStringsPath, # path to build.strings
                                "-c", "9", # compatibility version 9
                                "-q", # be quiet
                                "-H"] # Do not create LNG file
                if self.__makelangOptions is not None:
                    makelangArgs += self.__makelangOptions
                makelangArgs += makelangDBs
                command = [perl] + perlArgs + [makelang_pl] + makelangArgs
                self.logmessage(quiet, "Calling %s" % " ".join(command))
                if 0 == subprocess.call(command):
                    result = 2 # output files changed
                else: result = 1 # errors

        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)


if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Executes data/strings/scripts/makelang.pl to generate the files",
             "locale-enum.inc, locale-map.inc and locale-dbversion.h in",
             "{outputRoot}/modules/locale/ from",
             "{outputRoot}/data/strings/build.strings and the specified",
             "language database files."]),
        withPerl=True)

    option_parser.add_option(
        "--makelang_db", metavar="MAKELANGDB", action="append", dest="makelangDBs",
        help=" ".join(
            ["Specifies the path to the language database file relative to the",
             "source directory. The language database file is passed to",
             "makelang.pl. Multiple language database files can be specified.",
             "If no database file is specified, the default value",
             "data/strings/english.db is used."]))

    option_parser.add_option(
        "--makelang_opt", metavar="OPTION", action="append", dest="makelangOptions",
        help=" ".join(
            ["The specified option is added to the command line arguments on",
             "calling makelang.pl. Makelang is called as 'perl makelang.pl",
             "-d {outputRoot}/modules/locale",
             "-b {outputRoot}/data/strings/build.strings",
             "-c 9 -q -H {OPTION} {MAKELANGDB}'."]))

    option_parser.set_defaults(makelangOptions = [])
    (options, args) = option_parser.parse_args(args=sys.argv[1:])

    result = 0
    strings = GenerateStrings(perl = options.perl,
                              makelangDBs = options.makelangDBs,
                              makelangOptions = options.makelangOptions)
    result = strings(sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
