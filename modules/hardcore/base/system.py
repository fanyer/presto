import os
import sys
import time

# import from modules/hardcore/scripts/
if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))

import util
import basesetup

# List of all systems, sorted by name.
systems = None

# List of all macros, sorted by name.
macros = None

# List of all types, sorted by name.
types = None

# Dictionary mapping system names to util.FeatureOrSystem objects.
systemsByName = None

def handleTemplateAction(action, output):
    if action == "deprecated systems":
        for system in systems:
            output.write(system.deprecatedClause())
        for macro in macros:
            output.write(macro.deprecatedClause())
        return True

    elif action == "system choice check":
        for system in systems:
            output.write(system.decisionClause())
        return True

    elif action == "system dependency check":
        for system in systems:
            output.write(system.dependenciesClause())
        return True

    elif action == "system defines":
        for system in systems:
            output.write(system.controlledDefinesClause())
        for system in systems:
            output.write(system.definesClause())
        return True

    elif action == "check system types":
        for type in types:
            output.write(type.typeCheckClause())
        return True

    elif action == "check macros":
        for macro in macros:
            output.write(macro.macroCheckClause())
        return True

    elif action == "system undefines":
        for system in systems:
            output.write("#undef %s\n" % system.name())
        return True

def loadSystem(sourceRoot):
    global systems
    global macros
    global types
    global systemsByName

    import util
    items, attributions, errors, warnings = util.parseFeaturesAndSystem([os.path.join(sourceRoot, "modules", "hardcore", "base", "system.txt")])

    if warnings:
        for warning in sorted(warnings):
            print >>sys.stderr, warning

    if errors:
        for error in sorted(errors):
            print >>sys.stderr, error
        return False
    else:
        items.sort(key=util.FeatureOrSystem.name)

        systems = filter(lambda item: item.type() == "system", items)
        macros = filter(lambda item: item.type() == "macro", items)
        types = filter(lambda item: item.type() == "type", items)

        systemsByName = dict([(system.name(), system) for system in systems])
        return True

class GenerateSystemH(basesetup.SetupOperation):
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="system system")
        self.updateOperasetupOnchange()

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        outputRoot = outputRoot or sourceRoot
        if loadSystem(sourceRoot):
            hardcoreDir = os.path.join('modules', 'hardcore')
            baseDir = os.path.join(hardcoreDir, 'base')
            changed = util.readTemplate(os.path.join(sourceRoot, baseDir, 'system_template.h'),
                                        os.path.join(outputRoot, baseDir, 'system.h'),
                                        handleTemplateAction,
                                        os.path.join(outputRoot, hardcoreDir) if outputRoot == sourceRoot else None,
                                        'base/system.h')
            if changed: result = 2
            else: result = 0
        else: result = 1 # errors
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Generates modules/hardcore/base/system.h from system.txt and",
             "system_template.h. The script exists with status 0 if system.h",
             "was not changed. The exist status is 2 if system.h was changed",
             "(see also option '--make'). The exitstatus is 1 if an error",
             "was detected."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    generate_system_h = GenerateSystemH()
    result = generate_system_h(
        sourceRoot=sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
