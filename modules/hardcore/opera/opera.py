import os
import os.path
import re
import sys

# import from modules/hardcore/scripts
if __name__ == "__main__":
    sys.path.insert(1, os.path.join(sys.path[0], "..", "scripts"))
import util
import basesetup
import opera_module
import module_parser

class OperaHTemplateAction:
    """
    This class is used as an action handler in util.readTemplate() by
    GenerateOperaHBase to handle the template actions for the template files
    modules/hardcore/opera/opera_template.h
    modules/hardcore/opera/opera_template.inc
    modules/hardcore/base/capabilities_template.h

    Before using an intance of this class in util.readTemplate() call
    readModules()
    """
    def __init__(self, modules):
        # List of modules, sorted by location in hardcore/opera/modules.txt
        # and name:
        self.__modules = modules
        # Dictionary mapping module names to util.Module objects.
        self.__modulesByName = dict([(module.name(), module) for module in self.__modules])

    def readModules(self, modules_txt, blacklist_txt):
        self.readModulesTxt(modules_txt)
        self.readBlacklist(blacklist_txt)
        self.sortModules()

    def readModulesTxt(self, path):
        if util.fileTracker.addInput(path):
            f = None
            try:
                f = open(path)
                for index, line in enumerate(f):
                    match = re.match("^([A-Za-z0-9_]+)\\s*$", line)
                    if match and match.group(1) in self.__modulesByName:
                        self.__modulesByName[match.group(1)].setIndex(index)
            finally:
                if f: f.close()

    def readBlacklist(self, path):
        if util.fileTracker.addInput(path):
            f = None
            try:
                f = open(path)
                for line in f:
                    match = re.match("^([A-Za-z0-9_]+)\\s*$", line)
                    if match and match.group(1) in self.__modulesByName:
                        self.__modulesByName[match.group(1)].setBlacklisted()
            finally:
                if f: f.close()

    def sortModules(self):
        self.__modules.sort()

    def emitVariables(self, output, type="Module"):
        modulesWithModuleFile = filter(
            lambda module: module.hasModuleFile(), self.__modules)
        counts = []

        for module in modulesWithModuleFile:
            nom, klaz = module.id_name(), module.getModuleClassName(type)
            prefix = '%s_%s' % (nom.upper(), type.upper())
            counts.append('%s_COUNT' % prefix)
            output.writelines([
                    "#ifdef %s_REQUIRED\n" % prefix,
                    "\t%s %s_module;\n" % (klaz, nom),
                    "# define %s_COUNT 1\n" % prefix,
                    # Note: this use of nom is not .upper(), for compatibility
                    # with code written to work with older versions of this
                    # script
                    "# define %s_%s_CLASS_NAME %s\n" % (nom,type.upper(),klaz),
                    "#else // %s_REQUIRED\n" % prefix,
                    "# define %s_COUNT 0\n" % prefix,
                    "#endif // %s_REQUIRED\n" % prefix ])

        plus = ' + \\\n                      ' + ' ' * len(type)
        output.write("#define OPERA_%s_COUNT (%s)\n" % (type.upper(), plus.join(counts)))

    def emitAssignment(self, output, type="MODULE"):
        assert type == type.upper()
        for module in self.__modules:
            if module.hasModuleFile():
                nom = module.id_name()
                prefix = '%s_%s' % (nom.upper(), type)
                output.writelines([
                        "#ifdef %s_REQUIRED\n" % prefix,
                        "\tmodules[i] = &%s_module;\n" % nom,
                        "\tSET_MODULE_NAME(i, \"%s\");\n" % nom,
                        "\t++i;\n",
                        "#endif // %s_REQUIRED\n" % prefix ])

    def __call__(self, action, output):
        if action == "opera variables":
            self.emitVariables(output)
            return True

        elif action == "modules assignment":
            self.emitAssignment(output)
            return True

        elif action == "includes":
            for module in self.__modules:
                if module.hasModuleFile():
                    output.write("#include \"%s\"\n" % module.getModuleFilePath())
            return True

        elif action == "blacklist assignment":
            modulesWithModuleFile = filter(
                lambda module: module.hasModuleFile(), self.__modules)
            byte = 0
            for index, module in enumerate(modulesWithModuleFile):
                if module.blacklisted():
                    byte |= (1 << (index % 8))
                if byte and index and ((index % 8) == 7 or index == len(modulesWithModuleFile) - 1):
                    output.write("\tblack_list[%d] = %d;\n" % (int(index / 8), byte))
                    byte = 0
            return True

        elif action == "capabilities":
            for module in self.__modules:
                if module.name() != "hardcore" and os.path.isdir(module.fullPath()):
                    for name in os.listdir(module.fullPath()):
                        if re.match("^.*capabilit.*\\.h$", name):
                            output.write("#include \"%s/%s\"\n" % (module.includePath(), name))
                            break
            return True

        elif action == "opera modules enum":
            for index, module in enumerate(sorted(self.__modules, key=lambda module: module.name())):
                if index == 0:
                    output.write("\tOPERA_MODULE_FIRST,\n")
                    output.write("\tOPERA_MODULE_%s = OPERA_MODULE_FIRST,\n" % module.id_name().upper())
                elif index == len(self.__modules) - 1:
                    output.write("\tOPERA_MODULE_LAST,\n")
                    output.write("\tOPERA_MODULE_%s = OPERA_MODULE_LAST\n" % module.id_name().upper())
                else:
                    output.write("\tOPERA_MODULE_%s,\n" % module.id_name().upper())
            return True

class GenerateOperaHBase(basesetup.SetupOperation):
    """
    This class can be used as a base-class to generate any of the files
    modules/hardcore/opera/opera.h (see create_opera_h())
    modules/hardcore/opera/hardcore_opera.inc (see create_hardcore_opera_inc())
    modules/hardcore/base/capabilities.h (see create_capabilities_h())

    All these files need an instance of OperaHTemplateAction to handle the
    template actions. If all three files are create the OperaHTemplateAction
    instance can be re-used for all targets and thus the OperaHTemplateAction
    only needs to read and parse the files
    modules/hardcore/opera/{modules,blacklist}.txt once.
    """
    def __init__(self, message):
        basesetup.SetupOperation.__init__(self, message=message)
        self.updateOperasetupOnchange()
        self.__template_action = None

    def getTemplateActionHandler(self, sourceRoot):
        """
        Returns an instance of OperaHTemplateAction which can be used to handle
        opera_template.h, opera_template.inc and capabilities_template.h.
        If an instance is created it is stored as a member and can be re-used
        for the processing the next template file.
        """
        if self.__template_action is None:
            self.__template_action = OperaHTemplateAction(list(opera_module.modules(sourceRoot)))
            hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
            operaDir = os.path.join(hardcoreDir, 'opera')
            self.__template_action.readModules(os.path.join(operaDir, "modules.txt"),
                                               os.path.join(operaDir, "blacklist.txt"))
        return self.__template_action

    def create_opera_h(self, sourceRoot, outputRoot=None):
        """
        Creates modules/hardcore/opera/opera.h from the corresponding template
        file modules/hardcore/opera/opera_template.h. The file is generate
        relative to the specified outputRoot. If the outputRoot is not
        specified, sourceRoot is used. If outputRoot == sourceRoot then
        hardcore's module.generated is updated with the line "opera/opera.h".

        Returns true if the generated file was changed.
        """
        if outputRoot is None: outputRoot = sourceRoot
        hardcoreDir = os.path.join(sourceRoot, "modules", "hardcore")
        template = os.path.join(hardcoreDir, "opera", "opera_template.h")
        opera_h = os.path.join(outputRoot, "modules", "hardcore", "opera", "opera.h")
        changed = util.readTemplate(template, opera_h,
                                    self.getTemplateActionHandler(sourceRoot))
        if sourceRoot == outputRoot:
            util.updateModuleGenerated(hardcoreDir, ["opera/opera.h"])
        return changed

    def create_hardcore_opera_inc(self, sourceRoot, outputRoot=None):
        """
        Creates modules/hardcore/opera/hardcore_opera.inc from the template file
        modules/hardcore/opera/opera_template.inc. The file is generate
        relative to the specified outputRoot. If the outputRoot is not
        specified, sourceRoot is used. If outputRoot == sourceRoot then
        hardcore's module.generated is updated with the line "opera/opera.h".

        Returns true if the generated file was changed.
        """
        if outputRoot is None: outputRoot = sourceRoot
        hardcoreDir = os.path.join(sourceRoot, "modules", "hardcore")
        operaDir = os.path.join(hardcoreDir, "opera")
        template = os.path.join(operaDir, "opera_template.inc")
        hardcore_opera_inc = os.path.join(outputRoot, "modules", "hardcore", "opera", "hardcore_opera.inc")
        changed = util.readTemplate(template, hardcore_opera_inc,
                                    self.getTemplateActionHandler(sourceRoot))
        if sourceRoot == outputRoot:
            util.updateModuleGenerated(hardcoreDir, ["opera/hardcore_opera.inc"])
        return changed

    def create_capabilities_h(self, sourceRoot, outputRoot=None):
        """
        Creates modules/hardcore/base/capabilites.h from the template file
        modules/hardcore/base/capabilities_template.h. The file is generate
        relative to the specified outputRoot. If the outputRoot is not
        specified, sourceRoot is used. If outputRoot == sourceRoot then
        hardcore's module.generated is updated with the line
        'base/capabilities.h'.

        Returns true if the generated file was changed.
        """
        if outputRoot is None: outputRoot = sourceRoot
        hardcoreDir = os.path.join(sourceRoot, "modules", "hardcore")
        template = os.path.join(hardcoreDir, "base", "capabilities_template.h")
        capabilities_h = os.path.join(outputRoot, "modules", "hardcore", "base", "capabilities.h")
        changed = util.readTemplate(template, capabilities_h,
                                    self.getTemplateActionHandler(sourceRoot))
        if sourceRoot == outputRoot:
            util.updateModuleGenerated(hardcoreDir, ["base/capabilities.h"])
        return changed

class GenerateOperaH(GenerateOperaHBase):
    """
    This class can be used to generate modules/hardcore/opera/opera.h from its
    template file modules/hardcore/opera/opera_template.h and the module list
    that is loaded from modules/hardcore/opera/{modules,blacklist}.txt.
    """
    def __init__(self):
        GenerateOperaHBase.__init__(self, message="file opera.h")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        changed = self.create_opera_h(sourceRoot, outputRoot)
        if changed: result = 2
        else: result = 0
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

class GenerateHardcoreOperaInc(GenerateOperaHBase):
    """
    This class can be used to generate modules/hardcore/opera/hardcore_opera.inc
    from its template file modules/hardcore/opera/opera_template.inc and the
    module list that is loaded from
    modules/hardcore/opera/{modules,blacklist}.txt.
    """
    def __init__(self):
        GenerateOperaHBase.__init__(self, message="file hardcore_opera.inc")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        changed = self.create_hardcore_opera_inc(sourceRoot, outputRoot)
        if changed: result = 2
        else: result = 0
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

class GenerateCapabilitiesH(GenerateOperaHBase):
    """
    This class can be used to generate modules/hardcore/base/capabilities.h
    from its template file modules/hardcore/base/capabilities_template.h and
    the module list that is loaded from
    modules/hardcore/opera/{modules,blacklist}.txt.
    """
    def __init__(self):
        GenerateOperaHBase.__init__(self, message="file capabilities.h")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        changed = self.create_capabilities_h(sourceRoot, outputRoot)
        if changed: result = 2
        else: result = 0
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

class GenerateOperaModule(GenerateOperaHBase):
    """
    This class can be used to generate the thre files
    modules/hardcore/opera/opera.h
    modules/hardcore/opera/hardcore_opera.inc
    modules/hardcore/base/capabilities.h
    from its templates and the module list that is loaded from
    modules/hardcore/opera/{modules,blacklist}.txt once.
    """
    def __init__(self):
        GenerateOperaHBase.__init__(self, message="files opera.h, hardcore_opera.inc and capabilities.h")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        changed = self.create_opera_h(sourceRoot, outputRoot)
        changed = self.create_hardcore_opera_inc(sourceRoot, outputRoot) or changed
        chnaged = self.create_capabilities_h(sourceRoot, outputRoot) or changed
        if changed: result = 2
        else: result = 0
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

class ComponentType(module_parser.DependentItem):
    def __init__(self, filename, line, name, owners):
        module_parser.DependentItem.__init__(self, filename, line, name, owners)
        self.klass = ""
        self.include = ""

    def setValue(self, what, value):
        if what == "class":
            self.klass = value
        elif what == "include file":
            self.include = value
        else:
            self.addDependsOnString(value)

class ComponentTemplateActionHandler:
    """
    This class is used as an action handler in util.readTemplate() by
    GenerateComponentsBase to handle the template actions for the template files
    modules/hardcore/opera/components_template.h
    modules/hardcore/component/OpComponentCreate_template.inc
    """
    def __init__(self, components):
        self.components = components

    def defined(self, d):
        if d[0] == '!': return "!" + self.defined(d[1:])
        return "defined(%s)" % d

    def embedCondition(self, c, string):
        """
        Embeds the specified string inside a conditional as
        #ifdef condition
        string
        #endif // condition
        if the specified ComponentType c has a non-empty depends-on condition.
        @param c is a ComponentType
        @param string is the text to embed in the conditional. The string is
          expected to have a final new-line.
        """
        ifdef = endif = "";
        condition = str(c.dependsOn())
        if condition == "<empty>": return string
        else:
            return "#if %s\n%s#endif // %s\n" % (condition, string, condition)

    def componentIdentifier(self, c):
        return '\t%s,\n' % c.name()

    def componentInclude(self, c):
        indent = "";
        if str(c.dependsOn()) != "<empty>": indent = " "
        return self.embedCondition(c, '#%sinclude "%s"\n' % (indent, c.include))

    def componentCase(self, c):
        return self.embedCondition(c, "".join(
                ["\tcase %s:\n" % c.name(),
                 "\t\t*component = OP_NEW(%s, (*address));\n" % c.klass,
                 "\t\treturn *component ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;\n"]));

    def __call__(self, action, output):
        if action == "component identifiers":
            output.write("".join(map(self.componentIdentifier, self.components)))
            return True

        elif action == "component definition include files":
            output.write("".join(map(self.componentInclude, self.components)))
            return True

        elif action == "component construction cases":
            output.write("".join(map(self.componentCase, self.components)))
            return True

class GenerateComponentsBase(basesetup.SetupOperation):
    """
    This class can be used as a base-class to generate any of the files
    modules/hardcore/opera/components.h or
    modules/hardcore/component/OpComponentCreate.inc (see create_components_h()
    and create_components_inc()).

    Both these files are generated with a ComponentTemplateActionHandler
    instance which needs a list of Components instances. This list is created
    from parsing the module.components files of all modules.
    """
    def __init__(self, message):
        basesetup.SetupOperation.__init__(self, message=message)
        self.__components = None
        self.__parsed = None

    def parseComponents(self, sourceRoot):
        """
        Parses all module.components files in all modules that are found in the
        specified sourceRoot. The parsed ComponentType instances can be accessed
        by calling components().

        @return True if parsing was successful and False if there was a parsing
            error (which was printed to stderr).
        """
        if self.__parsed is None:
            # Prepare parser to parse all module.components files
            parser = module_parser.ModuleParser()
            parse_def = module_parser.ModuleItemParseDefinition(
                type="COMPONENT",
                definitions=["class", "include file", "depends on"],
                create_item=ComponentType,
                parse_item_definition=lambda f, c, w, v: c.setValue(w, v))
            parser.setParseDefinition([parse_def])

            # Collect all component definitions.
            components = []
            for module in list(opera_module.modules(sourceRoot)):
                if util.fileTracker.addInput(module.getComponentsFile()):
                    components.extend(parser.parseFile(module.getComponentsFile(),
                                                       allowed_types=set()))

            if parser.printErrors(): self.__parsed = False
            else:
                self.__parsed = True
                self.__components = components
        return self.__parsed

    def components(self):
        """
        Returns a list of ComponentType instances that correspond to the
        module.components files for all modules. These files are only parsed
        once and stored internally. See parseComponents()
        """
        return self.__components

    def create_components_h(self, sourceRoot, outputRoot=None):
        """
        Creates modules/hardcore/opera/components.h from the template file
        modules/hardcore/opera/components_template.h and the module.components
        file from all modules. The file is generated relative to the specified
        outputRoot. If the outputRoot is not specified, sourceRoot is used. If
        outputRoot == sourceRoot then hardcore's module.generated is updated
        with the line 'opera/components.h'.

        Returns true if the generated file was changed.
        """
        if outputRoot is None: outputRoot = sourceRoot
        hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
        operaDir = os.path.join(hardcoreDir, 'opera')
        changed = util.readTemplate(os.path.join(operaDir, 'components_template.h'),
                                    os.path.join(outputRoot, 'modules', 'hardcore', 'opera', 'components.h'),
                                    ComponentTemplateActionHandler(self.components()))
        if sourceRoot == outputRoot:
            util.updateModuleGenerated(hardcoreDir, ["opera/components.h"])
        return changed

    def create_components_inc(self, sourceRoot, outputRoot=None):
        """
        Creates modules/hardcore/component/OpComponentCreate.inc from the
        template modules/hardcore/component/OpComponentCreate_template.inc and
        the module.components file from all modules. The file is generated
        relative to the specified outputRoot. If the outputRoot is not
        specified, sourceRoot is used. If outputRoot == sourceRoot then
        hardcore's module.generated is updated with the line
        'component/OpComponentCreate.inc'.

        Returns true if the generated file was changed.
        """
        if outputRoot is None: outputRoot = sourceRoot
        hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
        componentDir = os.path.join(hardcoreDir, 'component')
        changed = util.readTemplate(os.path.join(componentDir, 'OpComponentCreate_template.inc'),
                                    os.path.join(outputRoot, 'modules', 'hardcore', 'component', 'OpComponentCreate.inc'),
                                    ComponentTemplateActionHandler(self.components()))
        if sourceRoot == outputRoot:
            util.updateModuleGenerated(hardcoreDir, ["component/OpComponentCreate.inc"])
        return changed

class GenerateComponentsH(GenerateComponentsBase):
    """
    This class can be used to generate the file
    modules/hardcore/opera/components.h from its template
    modules/hardcore/opera/components_template.h and the module.components
    files of all modules.
    """
    def __init__(self):
        GenerateComponentsBase.__init__(self, message="file components.h")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        if self.parseComponents(sourceRoot):
            changed = self.create_components_h(sourceRoot, outputRoot)
            if changed: result = 2
            else: result = 0
        else: result = 1 # parse-error
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

class GenerateComponentsInc(GenerateComponentsBase):
    """
    This class can be used to generate the files
    modules/hardcore/component/OpComponentCreate.inc from its template
    modules/hardcore/component/OpComponentCreate_template.inc and the
    module.components files of all modules.
    """
    def __init__(self):
        GenerateComponentsBase.__init__(self, message="file OpComponentCreate.inc")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        if self.parseComponents(sourceRoot):
            changed = self.create_components_inc(sourceRoot, outputRoot)
            if changed: result = 2
            else: result = 0
        else: result = 1 # parse-error
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

class GenerateComponents(GenerateComponentsBase):
    """
    This class can be used to generate the files
    modules/hardcore/opera/components.h and
    modules/hardcore/component/OpComponentCreate.inc from their respective
    template file modules/hardcore/opera/components_template.h and
    modules/hardcore/component/OpComponentCreate_template.inc and the
    module.components files of all modules.
    """
    def __init__(self):
        GenerateComponentsBase.__init__(self, message="components system")

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        if self.parseComponents(sourceRoot):
            changed = self.create_components_h(sourceRoot, outputRoot)
            changed = self.create_components_inc(sourceRoot, outputRoot) or changed
            if changed: result = 2
            else: result = 0
        else: result = 1 # parse-error
        return self.endTiming(result, sourceRoot=sourceRoot, outputRoot=outputRoot, quiet=quiet)

if __name__ == "__main__":
    sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))
    option_parser = basesetup.OperaSetupOption(
        sourceRoot=sourceRoot,
        description=" ".join(
            ["Generates the files modules/hardcore/opera/opera.h,",
             "modules/hardcore/opera/hardcore_opera.inc and",
             "modules/hardcore/base/capabilities.h from the files",
             "modules/hardcore/opera/module.txt,",
             "modules/hardcore/opera/blacklist.txt,",
             "{adjunct,modules,platforms}/readme.txt,",
             "{adjunct,modules,platforms}/*/*_capabilit*.h,",
             "{adjunct,modules,platforms}/*/*_module.h and the templates",
             "modules/hardcore/opera/opera_template.{h,inc},",
             "modules/hardcore/base/capabilities_template.cpp.",
             "Generate the files module/hardcore/opera/components.{h,inc} and",
             "module/hardcore/component/OpComponentCreate.inc from",
             "{adjunct,modules,platforms}/*/module.components and the",
             "templates module/hardcore/opera/components_template.h",
             "and module/hardcore/component/OpComponentCreate_template.inc.",
             "The script exists with status 0 if none of the output files",
             "was changed. The exist status is 2 if at least one of the",
             "output files was changed (see also option '--make').",
             "The exitstatus is 1 if an error was detected."]))

    option_parser.add_option(
        "--generate", action="append",
        choices=["opera.h", "hardcore_opera.inc", "capabilities.h",
                 "components.h", "OpComponentCreate.inc"],
        help = "".join(
            ["Generate the specified target file in its respective target",
             "directory. The default is to generate all files. Multiple",
             "targets can be specified."]))

    (options, args) = option_parser.parse_args(args=sys.argv[1:])

    functions = []

    if options.generate is None:
        functions.append(GenerateOperaModule())
        functions.append(GenerateComponents())
    else:
        if "opera.h" in options.generate:
            functions.append(GenerateOperaH())
        if "hardcore_opera.inc" in options.generate:
            functions.append(GenerateHardcoreOperaCpp())
        if "capabilities.h" in options.generate:
            functions.append(GenerateCapabilitiesH())
        if "components.h" in options.generate:
            functions.append(GenerateComponentsH())
        if "OpComponentCreate.inc" in options.generate:
            functions.append(GenerateComponentsInc())

    errors = False
    changed = False
    for f in functions:
        result = f(sourceRoot, outputRoot=options.outputRoot, quiet=options.quiet)
        if result == 1: errors = True
        elif result == 2: changed = True
    if options.timestampfile and not errors:
        util.fileTracker.finalize(options.timestampfile)
    if errors: sys.exit(1)
    elif changed and not options.make: sys.exit(1)
    else: sys.exit(0)
