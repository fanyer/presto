import os.path
import re
import sys

# import from modules/hardcore/scripts
sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))
import opera_module
import basesetup

sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
option_parser = basesetup.OperaSetupOption(
    sourceRoot=sourceRoot,
    description=" ".join(
        ["Generate the files core/pch.h, core/pch_system_includes.h and",
         "core/pch_jumbo.h in modules/hardcore/setup/plain/include/ and",
         "modules/hardcore/setup/jumbo/include/ from",
         "modules/hardcore/base/pch_template.h.",
         "The script exits with status 0 if none of the output files was",
         "changed. The exit status is 2 if at least one of the output files",
         "was changed (see also option '--make'). The exit status is 1 if",
         "an error was detected."]))

option_parser.add_option(
    "--platform_product_config", metavar="FILENAME",
    help=" ".join(
        ["The filename should be a platform specific header file, relative to",
         "the source-root, which defines the macros PRODUCT_FEATURE_FILE,",
         "PRODUCT_SYSTEM_FILE, PRODUCT_OPKEY_FILE, PRODUCT_ACTIONS_FILE and",
         "PRODUCT_TWEAKS_FILE.",
         "The generated files modules/hardcore/setup/*/include/core/pch*.h",
         "#include the specified file."]))
(options, args) = option_parser.parse_args(args=sys.argv[1:])
generate_pch_setup = opera_module.GeneratePchSetup(options.platform_product_config)
result = generate_pch_setup(sourceRoot, options.outputRoot, options.quiet)
if options.timestampfile and result != 1:
    util.fileTracker.finalize(options.timestampfile)
if options.make and result == 2: sys.exit(0)
else: sys.exit(result)
