import os.path
import sys
# In Python 3.0 the module ConfigParser has been renamed to configparser:
try: import ConfigParser
except ImportError: import configparser

import util

def getCoreVersion(sourceRoot, mainline_configuration):
    """
    Returns the core version number and the milestone number of the
    specified mainline-configuration from the file
    modules/hardcore/version.ini. If version.ini does not exist, an
    empty string is returned.

    @param sourceRoot is the path to the Opera source tree

    @param mainline_configuration is the name of the
        mainline-configuration for which to return the core version
        number. A section with the specified name is expected in the
        version.ini file.

    @return core_version, milestone
    """
    version_ini = os.path.join(sourceRoot, 'modules', 'hardcore', 'version.ini')
    if util.fileTracker.addInput(version_ini):
        config = ConfigParser.RawConfigParser()
        config.read(version_ini);
        if mainline_configuration is None: mainline_configuration = 'current'
        core_version = config.get(mainline_configuration, 'coreversion')
        milestone = config.get(mainline_configuration, 'milestone')
        return core_version,  milestone
    else:
        return '', None

def getOperaVersion(sourceRoot, mainline_configuration):
    """
    Returns the opera version number and (if set) the forced version number
    from the file modules/hardcore/version.ini.
    The forced version might be None

    @param sourceRoot is the path to the Opera source tree

    @param mainline_configuration is the name of the
        mainline-configuration for which to return the opera version
        number. A section with the specified name is expected in the
        version.ini file.

    @return opera_version, force_version
    """
    version_ini = os.path.join(sourceRoot, 'modules', 'hardcore', 'version.ini')
    if util.fileTracker.addInput(version_ini):
        config = ConfigParser.RawConfigParser()
        config.read(version_ini);
        if mainline_configuration is None: mainline_configuration = 'current'
        opera_version = config.get(mainline_configuration, 'operaversion')
        force_version = config.get(mainline_configuration, 'forceversion')
        if not len(force_version): force_version = None
        return opera_version, force_version
    else:
        return '', None

if __name__ == "__main__":
    import optparse

    option_parser = optparse.OptionParser(
        description=" ".join(
            ["Reads and prints the core-version of the specified mainline",
             "configuration from the file modules/hardcore/version.ini."]))

    option_parser.add_option(
        "--mainline_configuration", metavar="CONFIGURATION",
        choices=["current", "next"],
        help = " ".join(
            ["Use the specified mainline configuration. Valid configurations",
             "are \"next\" and \"current\". If no configuration is specified,",
             "\"%default\" is assumed. The file modules/hardcore/version.ini",
             "defines a version number for the specified mainline",
             "configuration."]))

    option_parser.add_option(
        "--version", dest="version", choices=["core", "milestone", "opera", "all"],
        help = "".join(
            ["Prints the specified version number. Possible values are",
             "'core': prints the core version number;",
             "'opera': prints the Opera version number;"
             "'milestone': prints the milestone number;"
             "'all': print all three values."]))

    option_parser.add_option(
        "-q", "--quiet", dest="quiet", action="store_true")

    option_parser.add_option(
        "--no-quiet", dest="quiet", action="store_false")

    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser.set_defaults(quiet=True, make=False,
                               mainline_configuration="current",
                               version="core",
                               outputRoot=sourceRoot)
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    core_version, milestone = getCoreVersion(sourceRoot, options.mainline_configuration)
    opera_version, force_version = getOperaVersion(sourceRoot, options.mainline_configuration)
    if options.version == "core":
        print core_version
    elif options.version == "milestone":
        print milestone
    elif options.version == "opera":
        print opera_version
    else:
        print "CoreVersion:", core_version
        print "Milestone:", milestone
        print "OperaVersion:", opera_version
