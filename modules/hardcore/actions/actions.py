import os
import os.path
import re
import sys

if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))

# import from modules/hardcore/scripts/:
import util
import basesetup
import module_parser
from module_parser import DependentItem, ModuleParser

class Action(module_parser.DependentItem):
    def __init__(self, filename, linenr, name, owners):
        DependentItem.__init__(self, filename, linenr, name, owners)
        self.__modules = set()
        self.__string = None

    def symbols(self): return set([self.name(), self.defineToUse()])
    def modules(self): return self.__modules
    def addModule(self, module): self.__modules.add(module)
    def string(self): return self.__string
    def setString(self, value): self.__string = value
    def defineToUse(self): return self.name() + "_ENABLED"

class HandleTemplateAction:
    def __init__(self, actions_parser):
        self.__actions_parser = actions_parser

    def generateCode(self, output):
        for action in self.__actions_parser.actions():
            define = action.defineToUse()

            if action.isDeprecated():
                output.write("#ifdef %s\n#\terror \"%s\"\n#endif // %s\n\n" % (define, action.description(), define))
            else:
                condition = str(action.dependsOn())
                if condition == "<empty>": condition = None

                if condition:
                    output.write("#if %s\n" % condition)
                    indent = "\t"
                else:
                    indent = ""

                output.write("#%sif !defined %s || (%s != YES && %s != NO)\n" % (indent, define, define, define))
                output.write("#%s\terror \"You must decide if you want action %s.\"\n" % (indent, action.name()))
                output.write("#%selse // !defined %s || (%s != YES && %s != NO)\n" % (indent, define, define, define))
                output.write("#%s\tif %s == NO\n" % (indent, define))
                output.write("#%s\t\tundef %s\n" % (indent, define))
                output.write("#%s\tendif // %s == NO\n" % (indent, define))
                output.write("#%sendif // !defined %s || (%s != YES && %s != NO)\n" % (indent, define, define, define))

                if condition:
                    output.write("#else // %s\n" % condition)
                    output.write("#\tundef %s\n" % define)
                    output.write("#endif // %s\n" % condition)

                output.write("\n")

    def generateEnum(self, output):
        for action in self.__actions_parser.actions():
            if not action.isDeprecated():
                define = action.defineToUse()
                output.write("#ifdef %s\n\t%s,\n#endif // %s\n" % (define, action.name(), define))

    def generateStrings(self, output):
        for action in self.__actions_parser.actions():
            if not action.isDeprecated():
                define = action.defineToUse()
                output.write("#ifdef %s\n\t,CONST_ENTRY(%s)\n#endif // %s\n" % (define, action.string(), define))

    def generateStringsUni(self, output):
        for action in self.__actions_parser.actions():
            if not action.isDeprecated():
                define = action.defineToUse()
                output.write("#ifdef %s\n\t,CONST_ENTRY(UNI_L(%s))\n#endif // %s\n" % (define, action.string(), define))

    def generateTemplate(self, output):
        for action in self.__actions_parser.actions():
            if not action.isDeprecated():
                condition = str(action.dependsOn())
                if condition == "<empty>": condition = None

                if condition: output.write("// depends on %s\n" % condition)
                else: output.write("// depends on nothing\n")
                output.write("#define %-60s\tYES/NO\n\n" % action.defineToUse())

    def __call__(self, action, output):
        if action == "actions": self.generateCode(output)
        elif action == "actions_enum": self.generateEnum(output)
        elif action == "actions_strings": self.generateStrings(output)
        elif action == "actions_strings_uni": self.generateStringsUni(output)
        elif action == "actions_template": self.generateTemplate(output)
        else: return False
        return True

class ActionsParser(module_parser.ModuleParser):
    def __init__(self, feature_def=None):
        module_parser.ModuleParser.__init__(self, feature_def)
        self.__module = None
        self.__actions = []
        self.__actionsByName = {}

    def actions(self): return self.__actions
    def actionsByName(self): return self.__actionsByName

    def createAction(self, filename, linenr, name, owners):
        if name in self.__actionsByName:
            action = self.__actionsByName[name]
            action.addOwners(owners)
        else:
            action = Action(filename, linenr, name, owners)
            self.__actionsByName[name] = action
            self.__actions.append(action)
        action.addModule(self.__module)
        return action

    def parseActionDefinition(self, line, current, what, value):
        if what == "depends on":
            current.addDependsOnString(value)
        else:
            assert what == "string"
            if current.string() is not None and current.string().lower() != value.lower():
                self.addParserError(line, "conflicting action strings defined in different module.actions files; previously defined by %s" % ", ".join(sorted([module.name() for module in current.modules()])))
            else:
                current.setString(value)

    def loadActions(self, sourceRoot):
        import opera_module
        self.setParseDefinition([
                module_parser.ModuleItemParseDefinition(
                    type="ACTION",
                    definitions=["depends on", "string"],
                    create_item=self.createAction,
                    parse_item_definition=self.parseActionDefinition)
                ])
        for module in opera_module.modules(sourceRoot):
            self.__module = module
            self.parseFile(module.getActionsFile())
            self.__module = None

        # validate the result:
        for action in self.actions():
            if not action.isDeprecated() and not action.string():
                self.addParserError(action.line(), "%s has no string set" % action.name())

        if self.printParserErrors():
            return False
        else:
            self.__actions.sort(key=Action.name)
            return True

    def writeOutputFiles(self, sourceRoot, outputRoot=None):
        import util
        hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
        actionsDir = os.path.join(hardcoreDir, 'actions')
        if outputRoot is None: targetDir = actionsDir
        else:
            targetDir = os.path.join(outputRoot, 'modules', 'hardcore', 'actions')
            util.makedirs(targetDir)
        changed = util.readTemplate(os.path.join(actionsDir, 'actions_template.h'),
                                    os.path.join(targetDir, 'generated_actions.h'),
                                    HandleTemplateAction(self))

        changed = util.readTemplate(os.path.join(actionsDir, 'actions_enum_template.h'),
                                    os.path.join(targetDir, 'generated_actions_enum.h'),
                                    HandleTemplateAction(self)) or changed
        changed = util.readTemplate(os.path.join(actionsDir, 'actions_strings_template.h'),
                                    os.path.join(targetDir, 'generated_actions_strings.h'),
                                    HandleTemplateAction(self)) or changed
        # Ignore changes in this; this is just a template for a
        # platforms' actions.h and not used when building Opera:
        util.readTemplate(os.path.join(actionsDir, 'actions_template_template.h'),
                          os.path.join(targetDir, 'generated_actions_template.h'),
                          HandleTemplateAction(self))
        if targetDir == actionsDir:
            util.updateModuleGenerated(
                hardcoreDir,
                ['actions/generated_actions.h',
                 'actions/generated_actions_enum.h',
                 'actions/generated_actions_strings.h',
                 'actions/generated_actions_template.h'])
        return changed

class GenerateActions(basesetup.SetupOperation):
    def __init__(self):
        basesetup.SetupOperation.__init__(self, message="actions system")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        parser = ActionsParser()
        if parser.loadActions(sourceRoot):
            changed = parser.writeOutputFiles(sourceRoot, outputRoot)
            if changed: result = 2
            else: result = 0
        else: result = 1 # error
        return self.endTiming(result, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Generates from {adjunct,modules,platforms}/*/module.actions,",
             "actions_template.h, actions_enum_template.h,",
             "actions_strings_template.h and actions_template_template.h in",
             "modules/hardcore/actions the files generated_actions.h,",
             "generated_actions_enum.h, generated_actions_strings.h and ",
             "generated_actions_template.h in modules/hardcore/actions/.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))
    (options, args) = option_parser.parse_args(args=sys.argv[1:])
    generate_actions = GenerateActions()
    result = generate_actions(sourceRoot=sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
    if options.timestampfile and result != 1:
        util.fileTracker.finalize(options.timestampfile)
    if options.make and result == 2: sys.exit(0)
    else: sys.exit(result)
