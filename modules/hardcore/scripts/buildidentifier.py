#! /usr/bin/python
#
# This script can be used as a standalone script or from operasetup.py to
# generate a header file which defines the VER_BUILD_IDENTIFIER to be the
# string that is returned by 'git describe --dirty'.
# The generated header file can then be used as the value for
# TWEAK_ABOUT_BUILD_NUMBER_HEADER - see modules/about/module.tweaks
#
import os
import os.path
import sys
import platform
import subprocess

if __name__ == "__main__":
    sys.path.insert(1, sys.path[0])

import basesetup
import util

def logmessage(quiet, message):
    if not quiet:
        print message

def error(message):
    print >>sys.stderr, "error:", message

def warning(message):
    print >>sys.stderr, "warning:", message

class HandleTemplateAction:
    """
    An instance of this class can handle a template to insert the macro
    definition for VER_BUILD_IDENTIFIER.
    Any line with content '// <build_identifier>' will be replaced by
    '#define VER_BUILD_IDENTIFIER "build_identifier"',
    any line with content '// <build_identifier_uni>' will be replaced by
    '#define VER_BUILD_IDENTIFIER UNI_L("build_identifier")',
    where build_identifier is the value of the specified argument.

    An instance of this class can be used with util.readTemplate().
    """
    def __init__(self, build_identifier):
        self.__build_identifier = build_identifier

    def __call__(self, action, output):
        if action == "build_identifier":
            output.write("#define VER_BUILD_IDENTIFIER \"%s\"\n" % self.__build_identifier)
        elif action == "build_identifier_uni":
            output.write("#define VER_BUILD_IDENTIFIER UNI_L(\"%s\")\n" % self.__build_identifier)
        else: return False
        return True

class GenerateBuildIdentifierH(basesetup.SetupOperation):
    def __init__(self, template=None, output=None, git=None, buildid=None, fallbackid=None):
        """
        template is the path to the template file to read - relative to the
            sourceRoot that is specified in the argument to __call__(). If the
            value is None, modules/hardcore/base/build_identifier_template.h
            is used.

        output is the path to the file to generate - relative to the outputRoot
            that is specified in the argument to __call__(). If the value
            is None, modules/hardcore/base/build_identifier.h will be used.

        git is the full path of the git executable to use on calling
            'git describe --dirty'. If the value is None, then the git
            executable will be searched in the environment GIT or the paths
            specified in the PATH environment.

        buildid if not None, this value is used instead of calling
            'git describe --dirty'.

        fallbackid if not None and neither buildid is specified nor the source
            tree is in a git-repository (thus 'git describe --dirty' cannot be
            called), then this id is used as a fallback. If this id is None in
            such a case, the operation will fail.
        """
        basesetup.SetupOperation.__init__(self, message="build identifier")
        if template is None:
            self.__template = os.path.join("modules", "hardcore", "base", "build_identifier_template.h")
        else: self.__template = template
        if output is None:
            self.__output = os.path.join("modules", "hardcore", "base", "build_identifier.h")
        else: self.__output = output
        self.__git = git
        self.__buildid = buildid
        self.__fallbackid = fallbackid

    def FindGit(self, quiet=True):
        """
        Find and return the full path of the git executable.
        If the path was specified as command-line argument (see argument --git)
        (and the path is executable), the specified path is returned.
        If no path was specified, look for an environment variable GIT and if
        specified and the value is the filename of an executable, that value is
        returned.
        Otherwise search for a git executable in the and directory of the PATH
        environment and return the first executable file.
        """
        if self.__git is None:
            if "GIT" in os.environ and os.access(os.environ["GIT"], os.F_OK | os.X_OK):
                self.__git = os.environ["GIT"]
                logmessage(quiet, "use git from environment GIT=%s" % self.__git)

            else:
                # search for a git executable in the PATH environment
                if platform.system() in ("Windows", "Microsoft"):
                    git_executables = ["git.exe", "git.cmd"]
                    pathSeparator = ";"
                else:
                    git_executables = ["git"]
                    pathSeparator = ":"

                path_env = os.environ.get("PATH", "")
                for path in filter(None, map(str.strip, path_env.split(pathSeparator))):
                    for git_executable in git_executables:
                        if os.access(os.path.join(path, git_executable), os.F_OK | os.X_OK):
                            logmessage(quiet, "found git in path '%s'" % path)
                            self.__git = os.path.join(path, git_executable)
                            return self.__git
                else:
                    warning("can't find suitable git executable; set the environment variable GIT to the git executable or adjust your PATH (current value='%s') to include the path to the git executable or use --git argument" % path_env)
                    return None

        elif os.access(self.__git, os.F_OK | os.X_OK):
            logmessage(quiet, "use git from commandline argument --git=%s" % self.__git)

        else:
            error("invalid --git argument; path is not an executable file")
            return None

        return self.__git

    def getBuildIdentifier(self, sourceRoot, quiet=True):
        """
        Determines the build-identifier (either using the one specified in
        __init__, or calling 'git describe --dirty') and returns it.

        If it cannot determine a build-identifier (e.g. because git returns an
        error, or neither buildid nor fallbackid are specified nor the
        sourceRoot is not a git repository), None is returned.
        """
        if self.__buildid is not None:
            # use the specified build-id:
            return self.__buildid

        elif os.path.exists(os.path.join(sourceRoot, ".git")):
            # call 'git describe --dirty' to get the build identifier
            git = self.FindGit(quiet)
            if git is None:
                # Use fallback-id if we can't execute git
                warning("using fallback-id '%s', because we could not find the git executable" % self.__fallbackid)

            else:
                # call 'git describe --dirty':
                run_git = subprocess.Popen([git, "describe", "--dirty"],
                                           cwd=sourceRoot,
                                           stdout=subprocess.PIPE,
                                           stderr=subprocess.PIPE)
                (output, output_err) = run_git.communicate()
                if run_git.returncode == 0:
                    return output.strip()
                else:
                    warning("using fallback-id '%s', because running git caused an error:\n%s" % (self.__fallbackid, output_err))

        return self.__fallbackid

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        """
        Determine a build-identifier (either use the one specified in __init__,
        or call 'git describe --dirty' to get a build identifier) and use the
        value on handling the template file.
        """
        self.startTiming()
        if outputRoot is None: outputRoot = sourceRoot
        result = 0
        build_identifier = self.getBuildIdentifier(sourceRoot, quiet)
        if build_identifier is None:
            result = 1 # error on getting the build-identifier
            error("Cannot determine build-identifier, please specify a --buildid argument or --fallbackid argument")

        elif util.readTemplate(os.path.join(sourceRoot, self.__template),
                               os.path.join(outputRoot, self.__output),
                               HandleTemplateAction(build_identifier)):
            result = 2 # output file was changed

        self.endTiming(result, quiet=quiet)
        # don't let operasetup regenerate operasetup.h if this file was changed:
        if result == 2: return 0
        else: return result

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Generate a header file that defines the VER_BUILD_IDENTIFIER",
             "macro as the string that contains the result of 'git describe",
             "--dirty'. That generated header file can be used as a value for",
             "the TWEAK_ABOUT_BUILD_NUMBER_HEADER. See also option --output.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))

    option_parser.add_option(
        "--git", metavar="PATH_TO_EXECUTABLE",
        help=" ".join(
            ["Specifies the full path to the git executable to use on running",
             "'git describe --dirty'. If the option is not specified, git is",
             "expected to be found in one of the directories specified in the",
             "PATH environment variable."]))

    option_parser.add_option(
        "--template", metavar="PATH_TO_FILE",
        help=" ".join(
            ["Specifies the path relative to the source-root to the template",
             "file that shall be used to generate the build identifier header.",
             "The template file must be a valid header file and include a line",
             "'// <build_identifier>'. That line will be replaced by the",
             "correct build-identifier define."]))

    option_parser.add_option(
        "--output", metavar="PATH_TO_FILE",
        help=" ".join(
            ["Specifies the path relative to the output-root to the header",
             "file that shall be generated from the template file.",
             "The generated header file contains the build-identifier macro.",
             "This file can be used for the TWEAK_ABOUT_BUILD_NUMBER_HEADER's",
             "value ABOUT_BUILD_NUMBER_HEADER:",
             "#define ABOUT_BUILD_NUMBER_HEADER \"<PATH_TO_FILE>\"."]))

    option_parser.add_option(
        "--buildid", metavar="ID",
        help=" ".join(
            ["Use the specified ID instead of the output from 'git describe",
             "--dirty'. This may be useful if you don't want the default",
             "output from 'git describe --dirty'."]))

    option_parser.add_option(
        "--fallbackid", metavar="ID",
        help=" ".join(
            ["If neither a --buildid is specified nor a git repository is",
             "available to run 'git describe --dirty', then this ID is used",
             "to generate the build identfier. Specifying this option is",
             "useful if you want to use 'git describe --dirty', but you also"
             "want to cover the case where the build is created outside a git"
             "repository (e.g. on bauhaus)."]))


    option_parser.add_option(
        "--print", action="store_true", dest="print_buildid",
        help=" ".join(
            ["Instead of generating the output file, simply print the build-id",
             "that would be used in the generated header file. This option can",
             "be used together with --buildid and --fallbackid. (Note: no",
             "output file is generated, so any --output argument is",
             "ignored.)"]))

    hardcoreDir = os.path.join("modules", "hardcore")
    baseDir = os.path.join(hardcoreDir, "base")
    option_parser.set_defaults(
        git=None,
        template=os.path.join(baseDir, "build_identifier_template.h"),
        output=os.path.join(baseDir, "build_identifier.h"),
        print_buildid=False)

    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    generate_build_id = GenerateBuildIdentifierH(git=options.git,
                                                 buildid=options.buildid,
                                                 fallbackid=options.fallbackid,
                                                 template=options.template,
                                                 output=options.output)
    if options.print_buildid:
        build_identifier = generate_build_id.getBuildIdentifier(sourceRoot=sourceRoot)
        if build_identifier is not None: print build_identifier
        else: sys.exit(1)
    else:
        result = generate_build_id(sourceRoot=sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
        if options.make and result == 2: sys.exit(0)
        else: sys.exit(result)
