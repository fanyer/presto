import sys
import os.path
import re
import time

# import modules from modules/hardcore/scripts/
import util
from util import WarningsAndErrors
import module_sources
import basesetup
import errno

class Module:
    def __init__(self, sourceRoot, outputRoot, type, name, version=None):
        """
        @param sourceRoot is the path to the Opera source tree
        @param type is usually one of "adjunct", "modules" or
          "platforms".
        @param name is the directory name of the module.
        @param version is the current cvs tag used for the module.
        """
        self.__type = type
        self.__name = name
        self.__version = version
        self.__fullPath = os.path.join(sourceRoot, type, name)
        self.__generatedPath = os.path.join(outputRoot, type, name)
        self.__blacklisted = False
        self.__index = None

        self.__tweaks = []
        self.__apiImports = []
        self.__apiExports = []
        self.__module_sources = None
        self.__hasModuleFile = os.path.exists(os.path.join(sourceRoot, self.getModuleFilePath()))

    def __cmp__(self, other):
        """
        Compares this instance with the specified other instance. If
        no index is set (see setIndex()) the modules are compared by
        name. A module with index set is always lower than a module
        without index. If both modules have an index set, then the
        modules are compared by the index value.

        Note: the index value is determined on reading
        modules/hardcore/opera/modules.txt. That defines the
        initialization order of the modules.
        """
        if self is other: return 0
        elif self.__index is None and other.__index is None:
            return cmp(self.__name, other.__name)
        elif self.__index is None: return 1
        elif other.__index is None: return -1
        else: return cmp(self.__index, other.__index)

    def __hash__(self): return hash(self.__name)

    def type(self): return self.__type
    def name(self): return self.__name
    def version(self): return self.__version
    def fullPath(self): return self.__fullPath
    def generatedPath(self): return self.__generatedPath
    def id_name(self):
        """
        Returns the module name as a valid C identifier. The name of
        the module may be alphanumeric characters [a-zA-Z0-9_] plus
        dot '.', dash '-' and slash '/' (to be able to add modules
        like 'lingogi/share/net') - see also load(). The characters
        dot '.', dash '-' and slash '/' are converted to an underscore
        '_' (so the id_name() of 'lingogi/share/net' is
        'lingogi_share_net').
        """
        return re.sub("[\\.\\/-]", "_", self.name())

    def relativePath(self):
        """
        Returns the path to the module directory relative to the
        source-root specified in the constructor. The relative path is
        something like "modules/hardcore".
        """
        return os.path.join(self.type(), self.name())

    def includePath(self):
        """
        Returns the path to the module directory relative to the
        source-root specified in the constructor. This path is joined
        by a slash '/' instead of the system path separator. The value
        returned by this method can be used to generate an include
        statement like
        @code
        print >>output, '#include \'%s/foo.h\'' % module.includePath()
        @endcode
        """
        return '/'.join([self.__type, self.__name])

    def hasModuleFile(self):
        """
        Returns True if the module has a {type}/{name}/{name}_module.h
        file. Such a file should define the module's initialization
        class which is derived from OperaModule.
        """
        return self.__hasModuleFile

    def getModuleFilePath(self):
        """
        Returns the path to the module's <name>_module.h file relative
        to the root directory that was specified on initializing the
        instance. The directory names are separated by a slash '/'.
        The returned value can be used e.g. to generate an include
        statement to include that header file.
        """
        return "%s/%s/%s_module.h" % (self.__type, self.__name, self.id_name())

    def getModuleClassName(self, suffix="Module"):
        """
        Returns the class name of the module's initialization class
        which is derived from OperaModule. The class name is assumed
        to be "{Name}{suffix}", where {Name} is the capitalized name (as
        returned by id_name()) of the module and {suffix} is the specified
        argument suffix.
        """
        if self.name() == "ecmascript": return "EcmaScript%s" % suffix
        else: return "%s%s" % ("".join(map(str.capitalize, self.id_name().split("_"))), suffix)

    def blacklisted(self): return self.__blacklisted
    def setBlacklisted(self): self.__blacklisted = True

    def setIndex(self, index):
        """
        Sets the index of the module to the specified value.

        The index is used to define the initialization order of the
        OperaModule instance of all modules. If no index is set the
        modules are compared by name. A module with index set is
        always lower than a module without index. If both modules have
        an index set, then the modules are compared by the index
        value.

        Note: the index value is determined on reading
        modules/hardcore/opera/modules.txt. That defines the
        initialization order of the modules.

        @param index is the index of the module. This is the
          line-number of line in modules/hardcore/opera/modules.txt
          where the module is listed.

        @see __cmp__()
        """
        self.__index = index

    def getModuleFile(self, type):
        """
        Returns the full path to the module's module.{type} file.
        Note: this method does not check if the file exists.
        @param type is the type of the filename to return.
        """
        return os.path.join(self.fullPath(), "module.%s" % type)

    def getTweaksFile(self):
        """
        Returns the full path to the module's module.tweaks file.
        Note: this method does not check if the file exists.
        """
        return self.getModuleFile("tweaks")

    def getAPIExportFile(self):
        """
        Returns the full path to the module's module.export file.
        Note: this method does not check if the file exists.
        """
        return self.getModuleFile("export")

    def getAPIImportFile(self):
        """
        Returns the full path to the module's module.import file.
        Note: this method does not check if the file exists.
        """
        return self.getModuleFile("import")

    def getMessagesFile(self):
        """
        Returns the full path to the module's module.messages file.
        Note: this method does not check if the file exists.
        """
        return self.getModuleFile("messages")

    def getActionsFile(self):
        """
        Returns the full path to the module's module.actions file.
        Note: this method does not check if the file exists.
        """
        return self.getModuleFile("actions")

    def getSourcesFile(self):
        """
        Returns the full path to the module's module.actions file.
        Note: this method does not check if the file exists.
        """
        override = self.getModuleFile("sources.override")
        if os.path.exists(override): return override
        else: return self.getModuleFile("sources")

    def hasSourcesFile(self):
        """
        Returns true if the module has a module.sources file.
        """
        return os.path.exists(self.getSourcesFile())

    def getAboutFile(self):
        """
        Returns the full path to the module's module.about file.
        Note: this method does not check if the file exists.
        """
        return self.getModuleFile("about")

    def hasComponentsFile(self):
        """
        Returns true if the module has a module.components file.
        """
        return os.path.exists(self.getComponentsFile())

    def getComponentsFile(self):
        """
        Returns the full path to the module's module.components file.
        Note: this method does not check if the file exists.
        """
        return self.getModuleFile("components")

    def tweaks(self): return self.__tweaks
    def setTweaks(self, tweaks): self.__tweaks = tweaks

    def apiImports(self): return self.__apiImports
    def setAPIImports(self, api_imports): self.__apiImports = api_imports

    def apiExports(self): return self.__apiExports
    def setAPIExports(self, api_exports): self.__apiExports = api_exports

    def getModuleSources(self):
        """
        Returns the ModuleSources instances that has loaded the
        module's module.sources file.
        """
        if self.__module_sources is None:
            self.__module_sources = module_sources.ModuleSources(self.type(), self.name())
            self.__module_sources.load(self.getSourcesFile())

            # Look for additional module.sources files and append the results to the __module_sources
            root = self.generatedPath()
            if os.path.exists(root):
                for fname in os.listdir(root):
                    if fname.startswith("module.sources_"):
                        generated_source = module_sources.ModuleSources(self.type(), self.name())
                        generated_source.load(os.path.join(root, fname))
                        generated_source.printWarningsAndErrors()
                        self.__module_sources.extend(generated_source)

            self.__module_sources.printWarningsAndErrors()
        return self.__module_sources

    def files(self):
        """
        Returns a list of source file names that are specified in the
        module's module.sources file.
        """
        return self.getModuleSources().sources_all()


class ModuleSet(WarningsAndErrors):
    """
    An instance of the class ModuleSet keeps a list of all Module
    instances which are listed in the readme.txt file of the
    "modules", "adjunct" and "platforms" directories.
    """
    def __init__(self, sourceRoot, outputRoot, types=["modules", "adjunct", "platforms"], stderr=sys.stderr):
        """
        @param sourceRoot is the path to the Opera source tree. The
          directories specified in the argument types are expected
          relative to the source root.
        @param types is a list of directories realtive to the
          specified source root in which the modules are
          located. Usually this list contains "adjunct", "modules" and
          "platforms".
        @param stderr can be set to the error-stream to which to print
          warnings and errors. The default is sys.stderr.
        """
        WarningsAndErrors.__init__(self, stderr)
        self.__sourceRoot = sourceRoot
        self.__outputRoot = outputRoot
        self.__types = types
        self.__modules = []
        self.__plain_sources = None
        self.__jumbo_sources = None

    def modules(self): return self.__modules

    def load(self):
        """
        Loads all modules which are listed in the readme.txt files of
        the sub-directories specified in the constructor's types
        argument.

        @return the list of Module instances
        """
        re_EmptyLine = re.compile("^\\s*(|#.*)$")
        # Note: the name of the module may be alphanumeric characters
        # [a-zA-Z0-9_] = \w plus dot '.', dash '-' and slash '/' (to
        # be able to add modules like 'lingogi/share/net').
        # The optional tag-name may be alphanumeric characters
        # [a-zA-Z0-9_] = \w plus dot '.' and dash '-' (no slash);
        # other characters are not allowed:
        re_ModuleLine = re.compile("^"+
                                   # the name of the module
                                   "([\\w\.\/-]+)" +
                                   # followed by optional tag-name
                                   "(?:\\s+([\\w\.-]+))?",
                                   re.IGNORECASE)
        for type in self.__types:
            readme_txt = os.path.join(self.__sourceRoot, type, "readme.txt")
            if util.fileTracker.addInput(readme_txt):
                try:
                    f = None
                    f = open(readme_txt)
                    for nr,text in enumerate(f):
                        line = util.Line(readme_txt, nr+1, text)
                        if re_EmptyLine.match(str(line)): continue
                        match = re_ModuleLine.match(str(line))
                        if match:
                            module = Module(self.__sourceRoot, self.__outputRoot, type,
                                            name=match.group(1),
                                            version=match.group(2))
                            self.__modules.append(module)
                        else:
                            self.addError(line, "warning unrecognized line '%s'" % str(line))
                finally:
                    if f: f.close()
        return self.__modules

    def generateSourcesSetup(self, verify_sources=True):
        """
        Generates the files sources.all, sources.nopch, sources.pch,
        sources.pch_system_includes and sources.pch_jumbo in both
        modules/hardcore/setup/plain/sources and
        modules/hardcore/setup/jumbo/sources. It also generates the
        jumbo compile units of all modules.
        @return True if any of the generated files has changed and
          False if none has changed.
        """
        changed = False
        self.__plain_sources = module_sources.SourcesSet()
        self.__jumbo_sources = module_sources.SourcesSet()
        for module in self.__modules:
            if util.fileTracker.addInput(module.getSourcesFile()):
                sources = module.getModuleSources()
                changed = sources.generateJumboCompileUnits(self.__sourceRoot, self.__outputRoot) or changed
                self.__plain_sources.extend(sources.plain_sources(),
                                            module.type(), module.name())
                self.__jumbo_sources.extend(sources.jumbo_sources(),
                                            module.type(), module.name())

        # verify that all files exist:
        if verify_sources:
            for filename in set(self.__plain_sources.all()) | set(self.__jumbo_sources.all()):
                if not os.path.exists(os.path.join(self.__sourceRoot, filename)) and not os.path.exists(os.path.join(self.__outputRoot, filename)):
                    self.addError(util.Line("", 0), "file '%s' not found" % filename)

        hardcoreDir = os.path.join(self.__outputRoot, 'modules', 'hardcore')
        plainDir = os.path.join(hardcoreDir, 'setup', 'plain', 'sources')
        jumboDir = os.path.join(hardcoreDir, 'setup', 'jumbo', 'sources')
        sources = ['all', 'nocph', 'pch', 'pch_system_includes', 'pch_jumbo']
        changed = self.__plain_sources.generateSourcesListFiles(plainDir) or changed
        util.updateModuleGenerated(hardcoreDir,
                                   map(lambda s: '/'.join(['setup', 'plain', 'sources', "sources.%s" % s]), sources))
        changed = self.__jumbo_sources.generateSourcesListFiles(jumboDir) or changed
        util.updateModuleGenerated(hardcoreDir,
                                   map(lambda s: '/'.join(['setup', 'jumbo', 'sources', "sources.%s" % s]), sources))

        return changed

def modules(root, type=["modules", "adjunct", "platforms", "data"]):
    if isinstance(type, str): types = [type]
    else: types = map(str, type)

    module_set = ModuleSet(root, root, types)
    module_set.load()
    module_set.printWarningsAndErrors()
    return module_set.modules()


def generateSourcesSetup(sourceRoot, quiet=True, outputRoot=None, verify_sources=True):
    """
    Generates from {adjunct,modules,platforms}/readme.txt,
    {adjunct,modules,platforms}/*/module.sources all jumbo compile
    units:
    - {adjunct,modules,platforms}/*/*_jumbo.cpp
    and the files listing all sources:
    - modules/hardcore/setup/plain/sources/sources.all
    - modules/hardcore/setup/plain/sources/sources.nopch
    - modules/hardcore/setup/plain/sources/sources.pch
    - modules/hardcore/setup/plain/sources/sources.pch_system_includes
    - modules/hardcore/setup/plain/sources/sources.pch_jumbo
    - modules/hardcore/setup/jumbo/sources/sources.all
    - modules/hardcore/setup/jumbo/sources/sources.nopch
    - modules/hardcore/setup/jumbo/sources/sources.pch
    - modules/hardcore/setup/jumbo/sources/sources.pch_system_includes
    - modules/hardcore/setup/jumbo/sources/sources.pch_jumbo

    @param sourceRoot is the path to the Opera source tree.
    @param outputRoot is the path for output files
    @param quiet is ignored by this method
    """
    outputStart = time.clock()
    module_set = ModuleSet(sourceRoot, outputRoot or sourceRoot)
    module_set.load()
    changed = module_set.generateSourcesSetup(verify_sources=verify_sources)
    outputEnd = time.clock()
    timing = "%.2gs" % (outputEnd - outputStart)
    if module_set.printWarningsAndErrors():
        sys.exit(1)

    elif changed:
        print "Sources setup system modified (%s)." % timing
        return 3

    else:
        print "No changes to the sources setup system (%s)." % timing
        return 0

def generatePchFiles(sourceRoot, outputRoot, platform_product_config=None, pch_template=None):
    """
    Generates from modules/hardcore/base/pch_template.h the header
    files core/pch.h, core/pch_jumbo.h, core/pch_system_includes.h
    @param sourceRoot is the path to the Opera source tree.
    @param outputRoot is the output directory in which to generate the
      header files. If outputRoot is None, the header files are
      generated relative to the specified sourceRoot.
    @param platform_product_config is the platform's config file
      that #defines the PRODUCT_* macros. That file is included by the
      generated pch header files. If no config file is specified, an
      #error statement is generated to indicate that these macros are
      needed on compiling the sources.
    @param pch_template is the template file
      that should be used when generating pch files
    @return True if any of the generated files was updated and False
      if none of the files were changed.
    """

    class CreatePchFromTemplate:
        """
        Creates a core/pch...h file frome the template file
        modules/hardcore/base/pch_template.h.
        """
        def __init__(self, filename, platform_product_config=None):
            """
            @param filename is the name of the header file to
              create. This name is used to generate the
              #ifndef...#define...#endif statements.
            @param platform_product_config is the path to the
              platform's config file which #defines the PRODUCT_*
              macros. If no config file is specified, an #error
              statement is generated to indicate that these macros are
              needed on compiling the sources.
            """
            self.__define = "_CORE_%s_" % filename.replace('.', '_').upper()
            self.__platform_product_config = platform_product_config
            if filename == "pch_system_includes.h":
                # only core/pch_system_includes.h should add a
                # #define ALLOW_SYSTEM_INCLUDES
                self.__allow_system_includes = True
            else:
                self.__allow_system_includes = False

        def __call__(self, action, output):
            if action == "ifndef file_h":
                output.write("#ifndef %s\n" % self.__define)
                return True

            elif action == "define file_h":
                output.write("#define %s\n" % self.__define)
                return True

            elif action == "define ALLOW_SYSTEM_INCLUDES":
                if self.__allow_system_includes:
                    # enclose in ifndef, because sometimes it's defined globally
                    # (in Opera.vcxproj generated by vcxproj_update.py)
                    # and in this case redefinition gives a lot of compilation warnings
                    output.write("#ifndef ALLOW_SYSTEM_INCLUDES\n")
                    output.write("#define ALLOW_SYSTEM_INCLUDES\n")
                    output.write("#endif // ALLOW_SYSTEM_INCLUDES\n\n")
                return True

            elif action == "endif file_h":
                output.write("#endif // %s\n" % self.__define)
                return True

            elif action == "include platform config":
                if self.__platform_product_config:
                    output.write("#include \"%s\"\n" % self.__platform_product_config)
                else:
                    output.write("#ifdef PRODUCT_CONFIG_FILE\n")
                    output.write("// The platform may define a command line define PRODUCT_CONFIG_FILE:\n")
                    output.write("// 'PRODUCT_CONFIG_FILE=\"platforms/my_platform/product_config.h\"' which\n")
                    output.write("// defines a header file that is included here. The header file has the\n")
                    output.write("// possibility to define the PRODUCT_*_FILE macros e.g. like\n")
                    output.write("// #define PRODUCT_FEATURE_FILE \"platforms/my_platform/features.h\"\n")
                    output.write("// #define PRODUCT_SYSTEM_FILE \"platforms/my_platform/system.h\"\n")
                    output.write("// #define PRODUCT_OPKEY_FILE \"platforms/my_platform/opkey.h\"\n")
                    output.write("// #define PRODUCT_ACTIONS_FILE \"platforms/my_platform/actions.h\"\n")
                    output.write("// #define PRODUCT_TWEAKS_FILE \"platforms/my_platform/tweaks.h\"\n")
                    output.write("# include PRODUCT_CONFIG_FILE\n")
                    output.write("#endif // PRODUCT_CONFIG_FILE\n")
                return True

    changed = False
    hardcoreDir = os.path.join(sourceRoot, 'modules', 'hardcore')
    if not pch_template:
        baseDir = os.path.join(hardcoreDir, 'base')
        pch_template = os.path.join(baseDir, 'pch_template.h')
    elif not os.path.isabs(pch_template):
        pch_template = os.path.join(sourceRoot, pch_template)
    if not os.path.exists(pch_template):
        raise IOError(errno.ENOENT, "Template file '%s' (passed with --pch_template) not found" % pch_template)
    for pch in ('pch.h', 'pch_jumbo.h', 'pch_system_includes.h'):
        changed = util.readTemplate(
            pch_template, os.path.join(outputRoot, 'core', pch),
            CreatePchFromTemplate(pch, platform_product_config)) or changed

    # if the pch files are generated relative to the modules/hardcore
    # dir, update hardcore's module.generated:
    match = re.match("^%s(.*)$" % re.escape(hardcoreDir+os.sep), outputRoot)
    if match:
        base = match.group(1).replace(os.sep, '/')
        util.updateModuleGenerated(os.path.join(sourceRoot, 'modules', 'hardcore'),
                                   map(lambda h: '/'.join([base, 'core', h]),
                                       ['pch.h', 'pch_jumbo.h',
                                        'pch_system_includes.h']))
    return changed

class GeneratePchSetup(basesetup.SetupOperation):
    """
    This class is a simple wrapper class which is used by
    operasetup.py to call the function generatePchFiles() for a
    platform_product_config file that was specified on operasetup.py's
    commandline. The pch files are generated in the sourceRoot.
    """
    def __init__(self, platform_product_config, pch_template=None):
        basesetup.SetupOperation.__init__(self, "pch header files")
        self.__platform_product_config = platform_product_config
        self.__pch_template = pch_template

    def __call__(self, sourceRoot, outputRoot=None, quiet=True):
        self.startTiming()
        changed = False
        for type in ('plain', 'jumbo'):
            changed = generatePchFiles(sourceRoot, outputRoot or sourceRoot,
                                       self.__platform_product_config,
                                       self.__pch_template) or changed
        if changed: result = 2
        else: result = 0
        return self.endTiming(result, quiet=quiet)
