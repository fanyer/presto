import sys
import os
import time
from copy import copy, deepcopy
import time
import re
import cPickle as pickle

def setupImports():
    opera_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
    cppgen_root = os.path.join(opera_root, "modules", "protobuf")
    hardcore_root = os.path.join(opera_root, "modules", "hardcore", "scripts")
    sys.path.append(cppgen_root)
    sys.path.append(hardcore_root)
    # Make sure it finds the bundled hob package
    sys.path.insert(1, os.path.join(cppgen_root, "cppgen", "ext"))

setupImports()
import util

class ProgramError(Exception):
    """
    The main exception for end-user program errors.
    The exception will bubble up to the top of the program and will
    be displayed to the end-user.
    """
    pass

globalOptions = {
    # If True the .py files will be included in the dependency list
    "includePyDependency": True,
}

def hash_md5():
    """
    Create an md5 object and return it.
    It creates the md5 object from the best module available (hashlib or md5)
    """
    try:
        import hashlib
        return hashlib.md5()
    except ImportError:
        import md5 as md5lib
        return md5lib.new()

def readCppConf(cpp_conf_file):
    """Reads the cpp config entries from the specified file and returns a ConfigParser object"""
    import ConfigParser
    cpp_conf = ConfigParser.SafeConfigParser()
    if util.fileTracker.addInput(cpp_conf_file):
        cpp_conf.read([cpp_conf_file])
    return cpp_conf

def find_proto_files(opera_root):
    """
    Scans all opera modules for defined proto files and yields each result.
    The module is checked for a module.protobuf file, if found it will
    be parsed and all proto files extracted from it.
    The yield value is the tuple (module, files) where module is the
    module object from operasetup and files is a list of proto files
    with absolute paths.

    Modules without any protobuf files will be skipped.
    """
    from opera_module import modules as find_modules
    modules = find_modules(opera_root)

    proto_re = re.compile(r".*\.proto$", re.I)
    comment_re = re.compile(r"#.*$")

    def find_proto(path):
        """Iterators over all defined proto files in a given path"""
        protobuf_index = os.path.join(path, "module.protobuf")
        if not util.fileTracker.addInput(protobuf_index):
            return
        for line in open(protobuf_index).read().splitlines():
            line = comment_re.sub("", line).strip()
            if line:
                protobuf_entry = os.path.join(path, line)
                if not util.fileTracker.addInput(protobuf_entry):
                    continue
                if os.path.isdir(protobuf_entry):
                    for root, dirs, files in os.walk(protobuf_entry):
                        util.fileTracker.addInput(root)
                        for f in files:
                            if proto_re.match(f):
                                filename = os.path.join(root, f)
                                util.fileTracker.addInput(filename)
                                yield filename
                else:
                    yield protobuf_entry

    for module in modules:
        files = list(find_proto(module.fullPath()))
        if files:
            yield module, files

def find_protobuf_sources(opera_root):
    """
    Finds all module.source_protobuf files in opera modules iterates over the result.
    Returns module, absolute sources file and relative source file.

    @param str opera_root: Path to the root of the opera source tree.
    """
    from opera_module import modules as find_modules
    modules = find_modules(opera_root)

    for module in modules:
        path = module.fullPath()
        sources_file = os.path.join(path, "module.sources_protobuf")
        if os.path.exists(sources_file):
            yield module, sources_file, os.path.join(module.relativePath(), "module.sources_protobuf")

def hobDependencies():
    """
    Returns all dependencies for the hob modules (ext/hob).
    """
    from cppgen.cpp import DependencyList, get_python_src
    dependencies = DependencyList()
    if globalOptions["includePyDependency"]:
        import hob, hob.parser, hob.proto, hob.ui, hob.utils
        for module in (hob, hob.parser, hob.proto, hob.ui, hob.utils):
            dependencies.add(os.path.abspath(get_python_src(module.__file__)))
    return dependencies

def cacheDependencies():
    """
    Returns all dependencies needed for working on cache files.
    """
    from cppgen.cpp import DependencyList, get_python_src
    dependencies = hobDependencies()
    if globalOptions["includePyDependency"]:
        import cppgen, cppgen.script, cppgen.cpp
        for module in (cppgen, cppgen.script, cppgen.cpp):
            dependencies.add(os.path.abspath(get_python_src(module.__file__)))
    return dependencies

def scriptDependencies():
    """
    Returns all dependencies for the main script.
    """
    from cppgen.cpp import DependencyList, get_python_src
    dependencies = hobDependencies()
    if globalOptions["includePyDependency"]:
        import cppgen, cppgen.cpp, cppgen.script, cppgen.utils
        for module in (cppgen, cppgen.cpp, cppgen.script, cppgen.utils):
            dependencies.add(os.path.abspath(get_python_src(module.__file__)))
    return dependencies

def serviceDependencies():
    """
    Returns all dependencies for the scope service generator.
    """
    from cppgen.cpp import DependencyList, get_python_src
    dependencies = messageDependencies()
    if globalOptions["includePyDependency"]:
        import cppgen.scope_service
        for module in (cppgen.scope_service, ):
            dependencies.add(os.path.abspath(get_python_src(module.__file__)))
    return dependencies

def managerDependencies():
    """
    Returns all dependencies for the scope manager generator.
    """
    from cppgen.cpp import DependencyList, get_python_src
    dependencies = scriptDependencies()
    if globalOptions["includePyDependency"]:
        import cppgen.scope_manager
        for module in (cppgen.scope_manager, ):
            dependencies.add(os.path.abspath(get_python_src(module.__file__)))
    return dependencies

def messageDependencies():
    """
    Returns all dependencies for the message generator.
    """
    from cppgen.cpp import DependencyList, get_python_src
    dependencies = scriptDependencies()
    if globalOptions["includePyDependency"]:
        import cppgen.message
        for module in (cppgen.message, ):
            dependencies.add(os.path.abspath(get_python_src(module.__file__)))
    return dependencies

def opMessageDependencies():
    """
    Returns all dependencies for the OpMessage generator.
    """
    from cppgen.cpp import DependencyList, get_python_src
    dependencies = messageDependencies()
    if globalOptions["includePyDependency"]:
        import cppgen.op_message
        for module in (cppgen.op_message, ):
            dependencies.add(os.path.abspath(get_python_src(module.__file__)))
    return dependencies

class File(object):
    """Lightweight wrapper for a file.

    Provides methods for getting the text contents and updating the file with new text.
    It also provides some convencience functions for checking difference and printing a diff.
    """
    def __init__(self, path):
        self.path = path
        self.file = None
        self.contents = None

    def _open(self, arg=None):
        """
        Opens the file if not already open and returns the file object.

        :param arg: Optional argument which is passed to open().
        """
        if not self.file:
            if arg:
                self.file = open(self.path, arg)
            else:
                self.file = open(self.path)
        return self.file

    def _close(self):
        """
        Closes the open file object, does nothing if file is not opened yet.
        """
        if self.file:
            self.file.close()
            self.file = None

    def open(self, arg=None):
        """
        Opens the file and returns the file object.
        If the file is already open closes the files first.

        :param arg: Optional argument which is passed to open().
        """
        if self.file:
            self._close()
        return self._open(arg)

    def close(self):
        """
        Closes the open file object, does nothing if file is not opened yet.
        """
        self._close()

    def exists(self):
        """
        Returns True if the file exists, False otherwise.
        """
        return os.path.exists(self.path)

    def mtime(self):
        """
        Returns the modification time as an int for the current file, or None if the file does not exist.
        """
        if os.path.exists(self.path):
            return int(os.stat(self.path).st_mtime)
        else:
            return None

    def text(self, default=None):
        """
        Returns the text contents of the file.
        The text is read with universal newlines (rU).

        If the file does not exist it returns the parameter default.
        """
        if not self.contents:
            if not self.exists() and default is not None:
                return default
            f = self._open("rU")
            try:
                self.contents = f.read()
            finally:
                self._close()
        return self.contents

    def touch(self, atime=None, mtime=None):
        """
        Touches the file and updates modification and access time.
        If either atime or mtime is not set it uses the current time
        for that parameter.
        """
        atime = atime or time.time()
        mtime = mtime or time.time()
        os.utime(self.path, (atime, mtime))

    def update(self, data):
        """
        Updates the file with text data if the data differes from what is present on the file.
        If the file does not exist it will be created, any missing parent
        directories will also be created.
        """
        text = None
        if self.exists():
            text = self.text()
        if text != data:
            self._close()
            self.makedirs()
            f = self._open("w")
            try:
                f.write(data)
                self.contents = data
            finally:
                self._close()
            return True
        return False

    def makedirs(self):
        """
        Creates any missing parent directories for the file.
        """
        util.makedirs(os.path.dirname(self.path))

    def isSame(self, data):
        """
        Returns True if the file contains the same data as the parameter data,
        False if not or the file does not exist.
        """
        if not self.exists():
            return False
        text = self.text()
        return text == data

    def diff(self, data):
        """
        Performs a unified diff on the file contents versus the parameter data.
        The function will iterate over each line in the diff.
        """
        text = self.text(default="")
        import difflib
        for line in difflib.unified_diff(text.splitlines(True), data.splitlines(True), self.path, self.path + ".new"):
            yield line

class Generator(object):
    """
    Base class for all generators, provides common code for processing dependencies and calling sub-generators.

    :param opera_root: The path to the root of the Opera source tree.
    :param output_root: The path to the root of the output directory.
    :param base_path: Relative path (from opera_root) to the current module.
    :param cache_file: Optional path to the cache file to use for this generator, if not supplied generates one in the protobuf module.
    """
    def __init__(self, opera_root, output_root, base_path, cache_file=None):
        self.opera_root = opera_root
        self.output_root = output_root
        self.base_path = base_path
        self.root = os.path.join(opera_root, base_path)
        self.cache_file = cache_file or os.path.join(self.output_root, "modules/protobuf", ".cppgen_rules.cache")
        self.update_files = []
        self.moduleType = os.path.dirname(base_path)
        self.moduleName = os.path.basename(base_path)
        from cppgen.utils import makeRelativeFunc
        self.relativeFunc = makeRelativeFunc(self.opera_root)

    def makeRelative(self, fname):
        """Returns the path of fname relative to the opera_root"""
        return self.relativeFunc(fname)

    def printRule(self, root_rule):
        """
        Helper method for printing all rules in a dependency, generates a tree-structure of all dependencies.
        """
        from cppgen.cpp import RuleDependency
        ui = self.ui
        ui.outl("rule:")
        marks = ("U", "M")
        def process(rule, level):
            prefix = " "*(level*1)
            ui.outl("%s %s target: %s: mtime=%s" % (prefix, marks[rule.needsUpdate()], rule.target, rule.targetMtime()))
            for dep in rule.dependencies.iterDependencies():
                if isinstance(dep, RuleDependency):
                    ui.outl("%s    dependency: rule" % prefix)
                    for dep_rule in dep.rules:
                        process(dep_rule, level + 2)
                else:
                    ui.outl("%s  %s dependency: %s: mtime=%s" % (prefix, marks[dep.isModified(rule.targetMtime())], dep, dep.mtime()))
        process(root_rule, 0)

    def process(self, generator, mtimes=None):
        """
        Processes all sub-generators by first checking for dependencies
        to see if an update is needed and then calls the generator if
        needed.

        :param generator: An object which returns either a single generator file, or a list generator files.
        :param mtimes: Currently unused.
        """
        from cppgen.cpp import mtimeCache

        ui = self.ui

        def processFile(generated_file):
            """
            Process a single generated file.

            Checks if the file needs an update by checking the dependency
            rule on it (needsUpdate). The update may also be forced
            with the global option "force".

            If the dependencies has changed the actual code generator is
            called, this either the "callback" method on the generator
            file, pre-computed "text" on the generator file or the
            "generator" method on the generator parameter passed in
            process(). The callback is the most up-to-date way of
            working with code generators.

            Once the new code has been generated it will check this
            against what is already present on the filesystem. If
            nothing has changed it will leave the file as it is to avoid
            unecessary re-compiles.

            This function also provides output for the end-user depending
            on what options was provided to the script.
            """
            rule = generated_file.rule
            if self.options.print_deps:
                self.printRule(rule)

            if not generated_file.jumbo_component and generated_file.package:
                # Individual proto files may specify cpp_component=<value>, indicating whether they should
                # exist in all binaries ('framework'), some particular selection, or core components only (None).
                if "cpp_component" in generated_file.package.options:
                    generated_file.jumbo_component = generated_file.package.options["cpp_component"].value

            self.generated_files.append(generated_file)
            if generated_file.build_module:
                generated_file.build_module.generated_files.append(generated_file)

            if self.options.force:
                rule.setNeedsUpdate(True)

            if not rule.needsUpdate():
                if not self.options.show_only_modified:
                    ui.outl("  %s" % self.makeRelative(rule.target))
                return
            try:
                start = time.time()
                try:
                    if generated_file.callback:
                        out = generated_file.callback(generated_file)
                    elif generated_file.text:
                        out = generated_file.text
                    else:
                        out = generator.generate()
                finally:
                    delta = time.time() - start
                    if self.options.show_timing:
                        ui.outl("cpp: %s" % delta)
            except Exception, e:
                error = str(e).splitlines()[0]
                ui.warnl("E %s" % self.makeRelative(generated_file.path))
                ui.warnl("  `- %s" % error)
                if hasattr(ui, "pdb") and ui.pdb: # Only for hob>=0.2
                    ui.postmortem()
                elif verbose:
                    import traceback
                    ui.warnl(traceback.format_exc())
                if self.options.stop:
                    raise ProgramError("Error while executing generator")
            else:
                mtime = rule.dependenciesMtime()
                f = File(generated_file.path)
                if self.options.print_stdout:
                    ui.outl(out)
                elif rule.needsUpdate():
                    if self.options.diff:
                        for line in f.diff(out):
                            ui.out(line)
                        self.update_files.append(generated_file.path)
                    elif self.options.replace:
                        if f.isSame(out):
                            # The regenerated content is the same as the file
                            # Instead of updating the file we just record the new mtime for future checks
                            mtimeCache.updateTime(generated_file.path)
                            if not self.options.show_only_modified:
                                ui.outl("m %s" % self.makeRelative(generated_file.path))
                        else:
                            generated_file.modified = True
                            self.modified_files.append(generated_file.path)
                            f.update(out)
                            ui.outl("M %s" % self.makeRelative(generated_file.path))
                    else:
                        if f.isSame(out):
                            mtimeCache.updateTime(generated_file.path)
                            if not self.options.show_only_modified:
                                ui.outl("m %s" % self.makeRelative(generated_file.path))
                        else:
                            ui.outl("U %s" % self.makeRelative(generated_file.path))
                            self.update_files.append(generated_file.path)
                else:
                    if not diff:
                        ui.outl("  %s" % self.makeRelative(generated_file.path))

        if hasattr(generator, "generate"):
            processFile(generator.createFile())
        elif hasattr(generator, "generateFiles"):
            generated_files = generator.generateFiles()
            for generated_file in generated_files:
                processFile(generated_file)

class GeneratorOptions(object):
    """Options for generator classes.

    The GeneratorOptions is initialized with:
    verbose_level - How much information should printed, 0 is silent
    replace - Whether to replace the contents of generated files or not
    stop - If true then it will stop on the first file with an error
    diff - If true then it will output the difference between the contents of the existing file and the new code. Overrides replace.
    print_stdout - If true then it output the generated code to stdout. Overrides replace and diff.
    show_all - Whether to show all files which are process or just the ones which are modified.
    print_deps - If true it will print dependency information for each file it processes
    force - If true it will regenerate all files even if no change was detected
    output_root - Destination directory for output files
    """
    def __init__(self, verbose_level=1, replace=False, stop=False, diff=False, print_stdout=False, show_all=False, print_deps=False, force=False, output_root=None):
        self.replace = replace
        self.verbose_level = verbose_level
        self.verbose = verbose_level >= 1
        self.stop = stop
        self.diff = diff
        self.print_stdout = print_stdout
        self.show_only_modified = not show_all
        self.show_all = show_all
        self.show_timing = False
        self.print_deps = print_deps
        self.force = force
        self.output_root = output_root

class ProtofileGenerator(Generator):
    """Generates C++ files from .proto files, either a service file or a plain descriptor file.

    This class is reponsible for generating C++ code for services in a given module.
    It will iterate over all services in a module and generate C++ files for each of them.
    The generator keeps track of dependencies per generated file and is able to determine
    if there is a need to update the files or not. This helps to reduce the time it
    takes to rerun the generator.

    The generator is initialized with:
    ui - UI object reponsible for keeping a configuration and outputting text to the user
    cpp_conf_path - Path to the cpp.conf file to use
    hob_conf_path - Path to the hob.conf file to use
    cpp_conf - The parsed cpp.conf file, use readCppConf() to get one
    opera_root - The absolute path to the opera source directory
    output_root - Destination path for generated files
    base_path - The relative path to the module, e.g. "modules/scope"
    files - List of files to generate, use an empty list to generate all files
    cache_file - Optional path to the cache file to use for this generator, if not supplied generates one in the protobuf module.

    Note: The generator will use a file called .cppgen_cached_rules.pickle to cache dependency information.
    """
    def __init__(self, build_module, ui, cpp_conf_path, hob_conf_path, cpp_conf, opera_root, output_root, base_path, options, files, cache_file=None):
        Generator.__init__(self, opera_root, output_root, base_path, cache_file)
        self.build_module = build_module
        self.ui = ui
        self.cpp_conf_path = cpp_conf_path
        self.hob_conf_path = hob_conf_path
        self.cpp_conf = cpp_conf
        self.root = os.path.join(opera_root, base_path)
        self.files = files
        self.options = options
        self.modified_files = []
        self.generated_files = []
        self.dependencies = []
        # Any services which are found are placed here, may be empty if nothing was changed
        self.services = []

    def generate(self):
        """Generates C++ code based on the current configuration"""
        ui = self.ui
        cpp_conf = self.cpp_conf
        root = self.root
        files = self.files

        if not files:
            return

        # TODO: Check if these lines are really needed anymore, the proto files is loaded before this is called
        from cppgen.cpp import setCurrentConfig
        # Adjust base for all proto files
        ui.config.base = root
        setCurrentConfig(ui.config, cpp_conf)

        from cppgen.cpp import DependencyList, RuleDependency
        from cppgen.scope_service import CppScopeServiceGenerator
        from cppgen.message import CppProtobufGenerator
        from cppgen.op_message import CppOpMessageGenerator

        services = []
        for build_proto in self.build_module.iterProtos():
            proto_file = build_proto.path()
            proto_dependencies = DependencyList([RuleDependency([build_proto.cache_rule])])
            modified = False
            if self.options.force:
                modified = True

            pkg = build_proto.package

            if pkg.services:
                gen = CppScopeServiceGenerator(self.output_root, self.build_module, self.base_path, pkg, pkg.services[0], dependencies=proto_dependencies | serviceDependencies())
                for generator in gen.generators():
                    self.process(generator)
            else:
                gen = CppProtobufGenerator(self.output_root, self.build_module, self.base_path, pkg, dependencies=proto_dependencies | messageDependencies())
                for generator in gen.generators():
                    self.process(generator)
                message_enums = []

            if pkg.cpp.needsOpMessageClass():
                gen = CppOpMessageGenerator(self.output_root, self.build_module, self.base_path, pkg, dependencies=proto_dependencies | opMessageDependencies())
                for generator in gen.generators():
                    self.process(generator)

class ProtobufDescriptorGenerator(Generator):
    """
    Generator for the global descriptor class containing all proto descriptors.

    This class is reponsible for generating C++ code for the protobuf descriptor
    classes.
    It will iterate over all protobuf files and create a descriptor class for
    each one. Currently files with services are managed entirely by the
    scope module so they will be skipped.

    The generator is initialized with:
    :param ui: UI object reponsible for keeping a configuration and outputting text to the user
    :param str cpp_conf_path: Path to the cpp.conf file to use
    :param ConfigParser cpp_conf: The parsed cpp.conf file, use readCppConf() to get one
    :param str opera_root: The absolute path to the opera source directory
    :param str output_root: The path to the root of the output directory.
    :param str base_path: The relative path to the module, e.g. "modules/scope"
    :param GeneratorOptions options: Options for the generators.
    :param BuildRoot build_root: Build object which specifies where the build occurs.
    :param build_module: The module object for the module which should contain the generated files.
    :param str cache_file: Optional path to the cache file to use for this generator, if not supplied generates one in the protobuf module.
    """
    def __init__(self, ui, cpp_conf_path, cpp_conf, opera_root, output_root, base_path, options, build_root, build_module, cache_file=None):
        Generator.__init__(self, opera_root, output_root, base_path, cache_file)
        self.ui = ui
        self.cpp_conf_path = cpp_conf_path
        self.cpp_conf = cpp_conf
        self.modified_files = []
        self.generated_files = []
        self.dependencies = []
        self.options = options
        self.build_root = build_root
        self.build_module = build_module

    def generate(self):
        """
        Generates the descriptors if they need updating.
        """
        from cppgen.message import CppProtobufDescriptorGenerator

        manager_modified = True
        if manager_modified:
            # TODO: Should use a .dat file for the data needed by the descriptors file and make it dependend on that
            dependencies = messageDependencies()
            for build_proto in self.build_root.iterProtos():
                if not build_proto.package.services:
                    dependencies |= build_proto.cache_dependencies
            proto_list = []
            for build_proto in self.build_root.iterProtos():
                package = build_proto.package

                # Services manage descriptors separately
                if package.services:
                    continue

                descriptorClass = package.cpp.descriptorClass()

                # TODO: Instead of passing data, send the entire build_proto object
                data = {"filename": package.filename,
                        "services": [],
                        "descriptor_id": package.cpp.descriptorIdentifier(),
                        "descriptor_path": descriptorClass.symbol(),
                        "defines": package.cpp.defines(),
                        "includes": package.cpp.descriptorIncludes(),
                        "has_service": bool(package.services),
                        "options" : package.options,
                        "package": package,
                        }
                proto_list.append(data)

            proto_list.sort(key=lambda p: p["descriptor_id"])

            gen = CppProtobufDescriptorGenerator(self.output_root, self.build_module, "modules/protobuf", proto_list, dependencies=dependencies, jumbo_component="framework")
            for generator in gen.generators():
                self.process(generator)

class OpMessageGenerator(Generator):
    """
    Generator for the global OpMessage information.

    This class is reponsible for generating the global C++ code for the
    OpMessage system.
    It will iterate over all protobuf files and create enums for files
    which contain messages marked for OpMessage generation.

    The generator is initialized with:
    :param ui: UI object reponsible for keeping a configuration and outputting text to the user
    :param str cpp_conf_path: Path to the cpp.conf file to use
    :param ConfigParser cpp_conf: The parsed cpp.conf file, use readCppConf() to get one
    :param str opera_root: The absolute path to the opera source directory
    :param str output_root: The path to the root of the output directory.
    :param str base_path: The relative path to the module, e.g. "modules/scope"
    :param GeneratorOptions options: Options for the generators.
    :param list message_enums: List of OpMessage enums, defined by OpMessageSystem (currently unused).
    :param BuildRoot build_root: Build object which specifies where the build occurs.
    :param build_module: The module object for the module which should contain the generated files.
    :param str cache_file: Optional path to the cache file to use for this generator, if not supplied generates one in the protobuf module.
    """
    def __init__(self, ui, cpp_conf_path, cpp_conf, opera_root, output_root, base_path, options, message_enums, build_root, build_module, cache_file=None):
        Generator.__init__(self, opera_root, output_root, base_path, cache_file)
        self.ui = ui
        self.cpp_conf_path = cpp_conf_path
        self.cpp_conf = cpp_conf
        self.modified_files = []
        self.generated_files = []
        self.dependencies = []
        self.options = options
        self.message_enums = message_enums
        self.build_root = build_root
        self.build_module = build_module

    def generate(self):
        from cppgen.op_message import CppOpMessageGlobalGenerator
        from hob.proto import iterTree

        manager_modified = True
        if self.options.force:
            manager_modified = True

        if manager_modified:
            # TODO: Should use a .dat file for the data needed by the descriptors file and make it dependend on that
            dependencies = opMessageDependencies()
            for build_proto in self.build_root.iterProtos():
                if build_proto.package.cpp.needsOpMessageClass():
                    dependencies |= build_proto.cache_dependencies
            # TODO: Remove message_enums
            message_enums = []
            messages = []
            for build_proto in self.build_root.iterProtos():
                package = build_proto.package
                if package.cpp.needsOpMessageClass():
                    for message in iterTree(package.messages):
                        if message.cpp.needsOpMessageClass():
                            messages.append(message)
                            message_enums.append((message.cpp.opMessageEnum().symbol(), message.cpp.opMessageEnum().name, ".".join(message.path()),
                                                  package.filename,
                                                  message.cpp.opMessage().declarationName(),
                                                  message.cpp.opMessage().symbol(),
                                                  package.cpp.opMessageIncludes()))

            message_enums.sort(key=lambda i: i[0])

            jumbo_component = "framework" # OpTypedMessage must compile in framework component
            gen = CppOpMessageGlobalGenerator(self.output_root, self.build_module, self.base_path, jumbo_component, messages, message_enums, dependencies=dependencies)
            for generator in gen.generators():
                self.process(generator)

class ServiceManagerGenerator(Generator):
    """
    Generator for the global scope manager.

    This class is reponsible for generating the C++ code for the
    scope manager in the scope module.
    It will iterate over all protobuf files and find service entries.
    All services are then sent to the C++ generator to create the
    descriptors and other code needed by the manager.

    The generator is initialized with:
    :param ui: UI object reponsible for keeping a configuration and outputting text to the user
    :param str cpp_conf_path: Path to the cpp.conf file to use
    :param ConfigParser cpp_conf: The parsed cpp.conf file, use readCppConf() to get one
    :param str opera_root: The absolute path to the opera source directory
    :param str output_root: The path to the root of the output directory.
    :param str base_path: The relative path to the module, e.g. "modules/scope"
    :param GeneratorOptions options: Options for the generators.
    :param BuildRoot build_root: Build object which specifies where the build occurs.
    :param build_module: The module object for the module which should contain the generated files.
    :param str cache_file: Optional path to the cache file to use for this generator, if not supplied generates one in the protobuf module.
    """
    def __init__(self, ui, cpp_conf_path, cpp_conf, opera_root, output_root, base_path, options, build_root, build_module, cache_file=None):
        Generator.__init__(self, opera_root, output_root, base_path, cache_file)
        self.ui = ui
        self.cpp_conf_path = cpp_conf_path
        self.cpp_conf = cpp_conf
        self.modified_files = []
        self.generated_files = []
        self.dependencies = []
        self.options = options
        self.build_root = build_root
        self.build_module = build_module

    def generate(self):
        ui = self.ui

        from cppgen.cpp import CppServiceBuilder, CppBuildOptions
        from cppgen.scope_manager import CppScopeManagerGenerators, MetaOptions, MetaOptionValue, MetaService

        # TODO: Check if these lines are really needed anymore
        from cppgen.cpp import setCurrentConfig
        setCurrentConfig(ui.config, self.cpp_conf)

        manager_modified = True
        if manager_modified:
            dependencies = managerDependencies()
            for build_proto in self.build_root.iterProtos():
                if build_proto.package.services:
                    dependencies |= build_proto.cache_dependencies

            build_options = CppBuildOptions(self.moduleType, self.moduleName)
            # TODO: Remove the meta service objects (and class) and just send the real service object
            services = []
            for build_proto in self.build_root.iterProtos():
                package = build_proto.package
                # Only include packages with services
                if not package.services:
                    continue
                for service in package.services:
                    options = MetaOptions()
                    meta_service = MetaService(service.name, options, filename=package.filename)
                    services.append(meta_service)
                    for key, opt in service.options.iteritems():
                        options[key] = MetaOptionValue(opt.value)
                    meta_service.cpp = CppServiceBuilder(meta_service, build_options)

            services.sort(key=lambda s: s.name.lower())

            gen = CppScopeManagerGenerators(self.output_root, self.build_module, self.base_path, services, dependencies=dependencies)
            for generator in gen.generators():
                self.process(generator)

class ScopeRepo(object):
    """Represents a scope repository in the source tree

    The repository is defined by module_type and module_name and is relative
    to the opera_root.

    The repository object is initialized with:
    ui - UI object reponsible for keeping a configuration and outputting text to the user
    opera_root - The absolute path to the opera source directory
    output_root - Destination path for generated files
    module_type - The type of module, e.g. "modules" or "platforms"
    module_name - The name of the module, e.g. "scope"
    cpp_conf - Path to the cpp.conf file to use
    hob_conf - Path to the hob.conf file to use
    files -
    """
    def __init__(self, ui, opera_root, output_root, module_type, module_name, hob_conf, cpp_conf, files=None):
        from cppgen.cpp import DependencyList
        self.ui = ui
        self.opera_root = opera_root
        self.output_root = output_root
        self.module_type = module_type
        self.module_name = module_name
        self.hob_conf = hob_conf
        self.cpp_conf = cpp_conf
        self.files = set(files or [])
        self.dependencies = DependencyList()
        self.generated_files = None
        self.modified_files = None
        self.cpp_conf_obj = None

    def modulePath(self):
        """Returns the relative path to the module, e.g. "modules/scope"."""
        return os.path.join(self.module_type, self.module_name)

    def moduleAbsPath(self):
        """Returns the absolute path to the module, ie. the root and modulePath(), e.g. "/home/user/src/opera/modules/scope"."""
        return os.path.join(self.opera_root, self.module_type, self.module_name)

    def cppConfigObject(self):
        """
        Return the ConfigParser object for the cpp.conf file in the current module.
        """
        if not self.cpp_conf_obj:
            self.cpp_conf_obj = readCppConf(self.cpp_conf)
        return self.cpp_conf_obj

    def createGenerator(self, build_module, files=None, options=None, cache_file=None):
        """Creates a new generator object for the scope repository and returns it.

        files: List of files to generate, or None to generate all files.
        See ProtofileGenerator for details on replace, stop, diff and print_stdout
        """
        if self.hob_conf:
            self.ui.config.read([self.hob_conf])
        update_files = self.files
        if files:
            update_files &= set([os.path.abspath(f) for f in (files or [])])
        cpp_conf = self.cppConfigObject()
        gen = ProtofileGenerator(build_module, self.ui, self.cpp_conf, self.hob_conf, cpp_conf, self.opera_root, self.output_root, self.modulePath(), files=update_files, options=options or GeneratorOptions(), cache_file=cache_file)
        return gen

class Protobuf(object):
    """
    Container for a protobuf package and file path.
    The package is scanned for Message and OpMessage objects and
    store in the attributes messages and opmessages.
    Services are stored in the attribute services.

    :param Package package: The protobuf package supplied by hob.
    :param str path: Path to the protobuf file.
    """
    def __init__(self, package, path):
        self.package = package
        self.path = path
        self.messages = []
        self.opmessages = []
        self.services = []

        from cppgen.cpp import iterOpMessages
        from hob.proto import iterTree

        for message in iterTree(package.messages):
            self.messages.append(message)

        if package.cpp.needsOpMessageClass():
            for message in iterOpMessages(package):
                self.opmessages.append(message)

        self.services = package.services[:]

    def __repr__(self):
        return "Protobuf(%r, %r)" % (self.package, self.path)

class MetaMessage(object):
    """
    Short representation of Message objects with just name, path and identifier.
    """
    def __init__(self, name, path, identifier):
        self.name = name
        self.path = path
        self.identifier = identifier

class OpMessageSystem(object):
    """
    System for keeping track of all OpMessage objects in the source tree.
    The system will generate a unique hash for each OpMessage and store
    them in the attribute hashes.
    In addition it generates enums for all OpMessages and stores them
    in the attribute message_enums.
    """
    def __init__(self, options, opera_root, ui):
        self.opera_root = opera_root
        self.hashes = {}
        self.message_enums = []

    def addProto(self, proto):
        import binascii
        from cppgen.utils import makeRelativeFunc
        makeRelativePath = makeRelativeFunc(self.opera_root)
        protoPath = makeRelativePath(proto.path)
        hashes = self.hashes
        for message in proto.opmessages:
            if message.cpp.opMessageID is None:
                data = "%s:%s" % (protoPath, ".".join(message.path()))
                identifier = binascii.crc32(data) & 0xffffffff
                while identifier in hashes:
                    identifier = (identifier * 33 + ord(data[-1])) & 0xffffffff
                message.cpp.opMessageID = identifier
            else:
                identifier = message.cpp.opMessageID
            meta_message = MetaMessage(message.name, message.path(), identifier)
            hashes[identifier] = meta_message

            package = proto.package
            self.message_enums.append((message.cpp.opMessageEnum().symbol(), message.cpp.opMessageEnum().name, ".".join(message.path()),
                                       os.path.normpath(package.filename).replace("\\", "/"),
                                       message.cpp.opMessage().declarationName(),
                                       message.cpp.opMessage().symbol(),
                                       package.cpp.opMessageIncludes()))

    def postProcess(self):
        self.message_enums.sort(key=lambda i: i[0])

def abs_path(root, path):
    """
    Returns the absolute path for `path` relative to `root`.
    If `path` is already an absolute path it returns that instead.
    """
    if not os.path.isabs(path):
        path = os.path.join(root, path)
    return path

def add_dep_path(dep_list, root, path):
    """
    Adds the absolute path for `root` and `path` to the dependency list `dep_list`.
    """
    dep_list.add(abs_path(root, path))

class BuildRoot(object):
    """
    Represents the root of the current build.

    :param path opera_root: The root of the opera source tree.
    :param str output_root: The path to the root of the output directory.
    :param ui: The ui object used for end-user output.

    :attribute str cache_path: The path to the cache directory.
    :attribute dict modules: Dictionary of module objects, the key is the relative path of the module.
    """
    def __init__(self, opera_root, output_root, ui):
        from cppgen.utils import cachePath
        self.opera_root = opera_root
        self.ui = ui
        self.cache_path = os.path.join(output_root, cachePath)
        self.modules = {}

    def path(self):
        """
        Return the path of the opera source tree
        """
        return self.opera_root

    def addModule(self, module):
        """
        Registers a new module in the build.
        """
        self.modules[module.path()] = module

    def iterModules(self):
        """
        Iterates over all registered modules in alphabetical order.
        """
        for module in sorted(self.modules.values(), key=lambda m: m.name):
            yield module

    def iterProtos(self):
        """
        Iterates over all protobuf files that have been found.
        """
        for module in self.iterModules():
            for proto in module.iterProtos():
                yield proto

class BuildModule(object):
    """
    Represents the root of an opera module in the build.

    :param module: The module object.
    :param ScopeRepo repo: The module repository object.
    :param BuildRoot root: Root of the build.
    :param str hob_conf_path: The path to the hob.conf file.
    :param str cpp_conf_path: The pat to the cpp.conf file.
    """
    def __init__(self, module, repo, root, hob_conf_path, cpp_conf_path):
        from cppgen.cpp import DependencyList
        self.module = module
        self.repo = repo
        self.root = root
        self.hob_conf_path = hob_conf_path
        self.cpp_conf_path = cpp_conf_path
        self.protos = {}
        self.conf_dependencies = DependencyList()
        for path in (self.hob_conf_path, self.cpp_conf_path):
            if util.fileTracker.addInput(path):
                add_dep_path(self.conf_dependencies, self.root.opera_root, path)
        self.generated_files = []

    def name(self):
        """
        Returns the name of the module
        """
        return self.repo.module_name

    def path(self):
        """
        Returns the relative path of the module.
        """
        return self.repo.modulePath()

    def absPath(self):
        """
        Returns the absolute path of the module.
        """
        return self.repo.moduleAbsPath()

    def addProto(self, proto):
        """
        Adds a new proto file to the module.
        """
        self.protos[proto.path()] = proto

    def iterProtos(self):
        """
        Iterates over all proto files in the module.
        """
        for proto in self.protos.values():
            yield proto

class ProtoFile(object):
    """
    Represents a protobuf file.

    This representation is a layer over the actual protobuf structure
    returned by the hob package.
    It handles caching of the protobuf structure to speed up the build
    process.

    :param str proto_path: Path to the proto file.
    :param BuildModule module: The module object.
    """
    def __init__(self, proto_path, module):
        from cppgen.cpp import DependencyList, Rule, RuleDependency
        self.proto_path = proto_path
        self.proto_file = File(self.proto_path)
        self.module = module
        self.cache_path = os.path.join(self.module.root.cache_path, self.proto_path + ".dat")
        self.cache_file = File(self.cache_path)
        self.package = None
        self.proto = None
        self.proto_dependencies = cacheDependencies() | self.module.conf_dependencies
        self.proto_rule = Rule(target=self.makeAbsPath(self.proto_path), dependencies=self.proto_dependencies)
        self.cache_dependencies = DependencyList([self.makeAbsPath(self.proto_rule.target)]) | self.proto_dependencies
        self.cache_rule = Rule(target=self.makeAbsPath(self.cache_path), dependencies=self.cache_dependencies)

    def path(self):
        """
        Return the path to the proto file.
        """
        return self.proto_path

    def absPath(self):
        """
        Return the absolute path to the proto file.
        """
        return os.path.join(self.module.root.path(), self.proto_path)

    def makeAbsPath(self, path):
        """
        Helper method for generating an absolute path relative from the opera root.
        """
        return abs_path(self.module.root.opera_root, path)

    def needUpdate(self):
        """Return True if the proto file or any dependencies has been modified or there is no cache information available, False otherwise"""
        if self.cache_rule.needsUpdate():
            return True
        if self.cache_file.exists():
            return self.cache_file.mtime() < self.proto_file.mtime()
        return True

    def loadCache(self):
        """Load the protobuf structure from a cache file, returns True if successful, False otherwise"""
        try:
            package = pickle.load(self.cache_file.open("rb"))
        except Exception, e:
            # If it fails to load the cache then ignore the error and load the .proto file as normal
            return False

        import hob.proto as proto
        quantifiers = {}
        for q in dir(proto.Quantifier):
            q = getattr(proto.Quantifier, q)
            if isinstance(q, proto.QuantifierType):
                quantifiers[q.id] = q
        types = {}
        for p in dir(proto.Proto):
            p = getattr(proto.Proto, p)
            if isinstance(p, proto.ProtoType):
                types[p.id] = p

        # The cached version does not have the same object id() as the
        # the parsed one so some post-processing is needed.
        #
        # 1. Fixup field types and quantifers to point to global objects
        # 2. Make sure any use of Default message points to the globale message
        for message in proto.iterTree(package.messages):
            for field in message.fields:
                field.q = quantifiers[field.q.id]
                field.type = types[field.type.id]
                if field.message and isinstance(field.message, proto.Message) and not field.message.fields and field.message.name == "Default":
                    field.message = proto.defaultMessage
        for service in package.services:
            for command in service.commands:
                if not command.message.fields and command.message.name == "Default":
                    command.message = proto.defaultMessage
                if isinstance(command, proto.Request):
                    if not command.response.fields and command.response.name == "Default":
                        command.response = proto.defaultMessage

        self.package = package
        if self.package.cpp.isBuilt():
            print "isbuilt=%s" % self.package.cpp.isBuilt()
            import pdb; pdb.set_trace()
        return True

    def saveCache(self):
        """Saves the protobuf structure to a cache file"""
        if not self.package:
            raise Exception("No package found, cannot save cache")
        self.cache_file.makedirs()
        pickle.dump(self.package, self.cache_file.open("wb"))

    def loadPackage(self):
        """Load protobuf structure from .proto file"""
        from cppgen.cpp import setCurrentConfig, CppBuildOptions, applyOptions
        from hob.proto import loadPackage

        # Adjust base for all proto files
        ui = self.module.root.ui
        ui.config.base = self.module.absPath()

        setCurrentConfig(ui.config, self.module.repo.cpp_conf)

        def load(path, base, cpp_config=None):
            pkg = loadPackage(path, "")
            build_options = CppBuildOptions(os.path.dirname(base), os.path.basename(base))
            applyOptions(pkg, os.path.splitext(os.path.basename(path))[0], base=base, cpp_config=cpp_config, build_options=build_options)
            return pkg

        self.package = load(self.absPath(), base=self.module.repo.modulePath(), cpp_config=self.module.repo.cppConfigObject())
        if self.package.cpp.isBuilt():
            print "isbuilt=%s" % self.package.cpp.isBuilt()
            import pdb; pdb.set_trace()

    def scan(self):
        """Scan the loaded protobuf structure and return the result object"""
        from cppgen.cpp import buildCppElements
        buildCppElements(self.package)
        self.proto = Protobuf(self.package, self.absPath())
        return self.proto

class ProtobufSystem(object):
    """
    System that scans the opera root for modules with protobuf files
    and calls the necessary generators to create descriptors, OpMessage
    code, scope services and the global scope manager.

    :param GeneratorOptions options: Global options for all generators.
    :param str opera_root: Path to the root of the opera source tree.
    :param ui: The ui object used to output to the end-user, if not supplied a new one is created.
    """
    def __init__(self, options, opera_root, ui=None):
        self.options = options
        self.opera_root = opera_root
        self.output_root = options.output_root or opera_root
        self.removed_modules = []
        self.ui = ui
        self.total_modified = 0
        self.total_generated = 0
        # Dictionary mapping proto files to dictionarys of options specified in the proto files.
        self.optionMap = {}
        from hob.ui import UserInterface
        ui = self.ui
        if not ui:
            ui = UserInterface([], [])
            ui.pdb = True
            ui.verbose_level = options.verbose_level
        self.build_root = BuildRoot(opera_root, self.output_root, ui)
        self.opmessage_system = None
        self.modules = {}
        self.generated_files = set()

        # Pre-make modules which contain global files
        for module_type, module_name in (("modules", "protobuf"),
                                         ("modules", "hardcore"),
                                         ("modules", "scope")):
            build_module = self.modules[(module_type, module_name)] = self.makeNamedModule(module_type, module_name)
            self.build_root.addModule(build_module)

    def makeNamedModule(self, module_type, module_name):
        """
        Create an OperaModule, ScopeRepo and BuildModule objects from the module path and name, returns the BuildModule.
        :param str module_type: The path of the module.
        :param str module_name: The name of the module.
        """
        from opera_module import Module as OperaModule
        module = OperaModule(self.opera_root, self.opera_root, module_type, module_name)
        return self.makeModule(module)

    def makeModule(self, module):
        """
        Creates a ScopeRepo and BuildModule object from the OperaModule object `module`.
        :param OperaModule module:
        """
        opera_root = self.opera_root
        ui = self.build_root.ui

        hob_conf = os.path.join(opera_root, module.type(), module.name(), "hob.conf")
        cpp_conf = os.path.join(opera_root, module.type(), module.name(), "cpp.conf")
        ui_copy = copy(ui)
        ui_copy.config = copy(ui_copy.config)

        if hob_conf:
            ui_copy.config.read([hob_conf])

        repo = ScopeRepo(ui_copy, opera_root, self.output_root, module.type(), module.name(), hob_conf, cpp_conf, [])
        build_module = BuildModule(module, repo, self.build_root, hob_conf, cpp_conf)
        return build_module

    def scan(self):
        """Scans the system for modules containing proto files and stores them in self.build_root.
        Once all files are found it scans for global information such as OpMessage generation
        and scope services and stores Python structures for this information which is
        is used by the code generators later on.

        The information it generates can be seen as a cache as it can all be regenerated
        from the proto and config files. The system tries to minimize repeated work
        by only updating information when the proto files have changed.

        The structure of the proto files is as follows:
        BuildRoot -
                   `- BuildModule -
                                   `- ProtoFile

        BuildRoot is the container for the entire build and contains
        BuildModule entries.
        BuildModule is the container for a module and all ProtoFile objects
        in it.
        ProtoFile is the representation of one .proto file, contains parsed
        information which is need for global processing.

        Global systems:
        OpMessage - Generates sub-classes of OpTypedMessage for messages which
                    are marked with cpp_opmessage = true
        Scope service - Generates C++ service classes if the proto file contains
                        a service definition.
        Descriptor set - Creates a global descriptor set which contains the
                         protobuf information needed to serialize/deserialize
                         protobuf, xml, json and ES data.
        """
        modules = {}
        files = {}

        opmessages = {}
        services = {}
        descriptors = {}

        opera_root = self.opera_root
        options = self.options

        ui = self.build_root.ui

        from cppgen.utils import makeRelativeFunc, cachePath
        makeRelative = makeRelativeFunc(opera_root)

        from cppgen.cpp import setCurrentConfig, loadPackage, currentConfig, Rule, DependencyList, RuleDependency, iterOpMessages, OptionError

        if options.verbose_level >= 1:
            print "Scanning for proto files"

        storage_path = os.path.join(self.output_root, cachePath)
        util.makedirs(storage_path)
        opmessage_storage_file = os.path.join(storage_path, "opmessage.dat")

        # Scan for modules and proto files and build the structure

        for module, files in find_proto_files(opera_root):
            if (module.type(), module.name()) not in self.modules:
                self.modules[(module.type(), module.name())] = build_module = self.makeModule(module)
            else:
                build_module = self.modules[(module.type(), module.name())]
            build_module.repo.files.update(files)
            self.build_root.addModule(build_module)

            for proto_file in files:
                build_proto = ProtoFile(makeRelative(proto_file), build_module)
                build_module.addProto(build_proto)

        # Build the module list to keep compatibility with older cache code.
        # With this list the old code will delete any module.source_protobuf files which are no longer in use
        def create_old_cache():
            cache_file = os.path.join(self.output_root, "modules/protobuf", ".cppgen_rules.cache")
            compat_cache = {"files": [],
                            "modules": {"path": []}}
            compat_modules = compat_cache["modules"]["path"]
            for module_type, module_name in self.modules.iterkeys():
                compat_modules.append((module_type, module_name))

            # A magic ID for the older cache file
            rule_cache_magic = "3"
            def save_rules(data, path):
                import cPickle as pickle

                data_file = open(path, "wb")
                try:
                    print >>data_file, rule_cache_magic
                    pickle.dump(data, data_file)
                finally:
                    data_file.close()
            save_rules(compat_cache, cache_file)
        create_old_cache()

        # Process proto files, either by loading cache files or parsing .proto files

        self.opmessage_system = OpMessageSystem(options, opera_root, ui)
        has_error = False

        message_count = 0
        service_count = 0
        for build_module in self.build_root.iterModules():
            if options.verbose_level >= 1:
                print " In %s:" % makeRelative(build_module.path())

            module_root = build_module.absPath()
            makeRelativeModule = makeRelativeFunc(opera_root, module_root)

            for build_proto in build_module.iterProtos():
                proto_relative = makeRelativeModule(build_proto.path())

                try:
                    mark = ""
                    # Try and load the cache file if it exists
                    # TODO: Use dependency system and make sure .py scripts are included
                    if not build_proto.needUpdate():
                        if build_proto.loadCache():
                            mark = " [C]"

                    if not build_proto.package:
                        build_proto.loadPackage()
                        build_proto.saveCache()

                    # Scan the resulting protobuf structure
                    proto = build_proto.scan()
                    self.opmessage_system.addProto(proto)

                    if options.verbose_level >= 1:
                        print "  %s%s" % (proto_relative, mark)
                    message_count += len(proto.messages)
                    service_count += len(proto.services)
                except OptionError, e:
                    has_error = True
                    ui.warnl("Error loading proto file %s" % proto_relative)
                    ui.warnl("`- %s" % e)
                    if self.options.stop:
                        raise ProgramError("Error loading proto file %s" % proto_relative)
                    continue
                except Exception, e:
                    has_error = True
                    ui.warnl("Error loading proto file %s" % proto_relative)
                    ui.warnl("`- %s" % e)
                    if hasattr(ui, "pdb") and ui.pdb: # Only for hob>=0.2
                        ui.postmortem()
                    elif verbose:
                        import traceback
                        ui.warnl(traceback.format_exc())
                    if self.options.stop:
                        raise ProgramError("Error loading proto file %s" % proto_relative)
                    continue

        if has_error:
            raise ProgramError("Error while loading proto files")

        self.opmessage_system.postProcess()

        if options.verbose_level >= 1:
            print "Found %d messages" % message_count
            print "Found %d services" % service_count
            print "Found %d OpMessage classes" % len(self.opmessage_system.hashes)
        pickle.dump(self.opmessage_system, open(opmessage_storage_file, "wb"))

        if options.verbose_level >= 1:
            print "Scan finished"

    def process(self):
        """
        Process the scanned proto files by generating the necessary C++ code.

        Generates the following code:
        - C++ classes for all messages. Services also generate commands and events.
        - Global descriptor classes for messages and fields.
        - Global scope manager which keeps track of services.
        - Global OpMessage information such as unique ID and enums.
        """
        options = self.options
        opera_root = self.opera_root

        from hob.ui import UserInterface
        ui = self.ui
        if not ui:
            ui = UserInterface([], [])
            ui.pdb = True
            ui.verbose_level = options.verbose_level

        self.total_modified = 0
        self.total_generated = 0
        for build_module in self.build_root.iterModules():
            module = build_module.module
            if options.verbose_level >= 1:
                print "Updating files in module %s" % module.relativePath()

            repo = build_module.repo

            gen = repo.createGenerator(build_module, options=options)
            repo.optionMap = gen.generate()
            if options.verbose_level >= 1:
                print "Updated %d of %d files" % (len(gen.modified_files), len(gen.generated_files))
            self.total_modified += len(gen.modified_files)
            self.total_generated += len(gen.generated_files)
            repo.dependencies.update(gen.dependencies)
            repo.generated_files = gen.generated_files
            repo.modified_files = gen.modified_files

            self.generated_files.update(gen.generated_files)

        # Generate global descriptor class
        if options.verbose_level >= 1:
            print "Updating descriptor class"
        cpp_conf = os.path.join(opera_root, "modules", "protobuf", "cpp.conf")
        ui_copy = copy(ui)
        ui_copy.config = copy(ui_copy.config)
        gen = ProtobufDescriptorGenerator(ui_copy, cpp_conf, readCppConf(cpp_conf), opera_root, self.output_root, "modules/protobuf", options, self.build_root, self.modules[("modules", "protobuf")])
        gen.generate()
        self.total_modified += len(gen.modified_files)
        self.total_generated += len(gen.generated_files)

        self.generated_files.update(gen.generated_files)

        if options.verbose_level >= 1:
            print "Updated %d of %d files" % (len(gen.modified_files), len(gen.generated_files))

        if options.verbose_level >= 1:
            print "Total: Updated %d of %d files" % (self.total_modified, self.total_generated)

        # Generate scope manager
        if options.verbose_level >= 1:
            print "Updating scope manager"
        cpp_conf = os.path.join(opera_root, "modules", "scope", "cpp.conf")
        ui_copy = copy(ui)
        ui_copy.config = copy(ui_copy.config)
        gen = ServiceManagerGenerator(ui_copy, cpp_conf, readCppConf(cpp_conf), opera_root, self.output_root, "modules/scope", options, self.build_root, self.modules[("modules", "scope")])
        gen.generate()
        self.total_modified += len(gen.modified_files)
        self.total_generated += len(gen.generated_files)

        self.generated_files.update(gen.generated_files)

        # Generate global OpMessage files
        if options.verbose_level >= 1:
            print "Updating OpMessage system"
        cpp_conf = os.path.join(opera_root, "modules", "protobuf", "cpp.conf")
        ui_copy = copy(ui)
        ui_copy.config = copy(ui_copy.config)
        gen = OpMessageGenerator(ui_copy, cpp_conf, readCppConf(cpp_conf), opera_root, self.output_root, "modules/hardcore", options, self.opmessage_system.message_enums, self.build_root, self.modules[("modules", "hardcore")])
        gen.generate()
        self.total_modified += len(gen.modified_files)
        self.total_generated += len(gen.generated_files)

        self.generated_files.update(gen.generated_files)

        if options.verbose_level >= 1:
            print "Updated %d of %d files" % (len(gen.modified_files), len(gen.generated_files))

        if options.verbose_level >= 1:
            print "Total: Updated %d of %d files" % (self.total_modified, self.total_generated)

class JumboFile(object):
    """
    Represents a jumbo file used in the opera source tree.

    :param str module_type: The relative path of the module.
    :param str module_name: The name of the module.
    :param str name: The name of the jumbo file.
    """
    def __init__(self, module_type, module_name, name):
        self.module_type = module_type
        self.module_name = module_name
        self.name = name

    def path(self):
        """
        Return the relative path of the jumbo file.
        """
        return os.path.join(self.module_type, self.module_name, self.name)

    def __repr__(self):
        return "JumboFile(%r,%r,%r)" % (self.module_type, self.module_name, self.name)

class JumboFiles(object):
    """
    Contains a list of JumboFile objects.
    """
    def __init__(self):
        self.jumbo_files = []

    def iterJumboFiles(self):
        """
        Iterates over all JumboFile objects.
        """
        for jumbo_file in self.jumbo_files:
            yield jumbo_file

    def add(self, jumbo_file):
        """
        Adds a new JumboFile to the container.
        """
        self.jumbo_files.append(jumbo_file)

    def iterRemoved(self, update_files):
        """
        Iterates over all JumboFile objects which no longer exists.
        """
        removed = []
        files = set([jumbo_file.path() for jumbo_file in update_files.iterJumboFiles()])
        for jumbo_file in self.iterJumboFiles():
            if jumbo_file.path() not in files:
                yield jumbo_file

    def loadCache(self, cache_file):
        """
        Loads the the JumboFile list from the cache.
        """
        try:
            files = pickle.load(cache_file.open("rb"))
        except Exception, e:
            # If it fails to load the cache then ignore the error and continue as normal
            return False

        self.jumbo_files = files
        return True

    def saveCache(self, cache_file):
        """
        Saves the current JumboFile list to the cache file.
        """
        cache_file.makedirs()
        pickle.dump(self.jumbo_files, cache_file.open("wb"))


def operasetup(opera_root, outputRoot=None, quiet=False, show_all=False):
    """Helper function used by the operasetup code"""
    # Make sure it finds the bundled hob package
    sys.path.insert(1, os.path.abspath(os.path.join(os.path.dirname(__file__), "ext")))

    show_all=True
    from cppgen.cpp import mtimeCache, DependencyList
    from module_sources import ModuleSources, JumboCompileUnit
    mtimeCache.setRoot(outputRoot or opera_root)
    mtimeCache.load()
    try:
        from cppgen.cpp import Rule
        verbose_level = 1
        if quiet:
            verbose_level = 0

        options = GeneratorOptions(replace=True, verbose_level=verbose_level, show_all=show_all, stop=True, output_root=outputRoot)
        from hob.ui import UserInterface
        ui = UserInterface([], [])
        ui.verbose_level = verbose_level
        system = ProtobufSystem(options, opera_root, ui=ui)
        system.scan()
        system.process()

        result = 0
        modified = 0
        generated = 0

        # Update module.sources_protobuf files in each module if necessary
        if verbose_level >= 1:
            print "Updating generated module.sources files"

        jumbo_files = JumboFiles()

        for build_module in system.build_root.iterModules():
            module = build_module.module
            module_key = (module.type(), module.name())

            dependencies = DependencyList()
            for build_proto in build_module.iterProtos():
                dependencies |= build_proto.cache_dependencies

            module_sources_name = "module.sources_protobuf"
            module_sources = os.path.join(module.type(), module.name(), module_sources_name)
            module_sources_abs = os.path.join(outputRoot or opera_root, module_sources)
            module_sources_rule = Rule(module_sources_abs, dependencies=dependencies)
            module_sources_file = File(module_sources_abs)

            if build_module.generated_files:
                generated += 1
                jumbo_unit = JumboCompileUnit(module.type(), module.name(), "protobuf_%s_jumbo.cpp" % module.name(), pch=True, system_includes=False)
                sources = ModuleSources(module.type(), module.name())
                for gen_file in build_module.generated_files:
                    path = gen_file.path
                    if path.endswith(".cpp"):
                        jumbo_args = {}
                        if gen_file.jumbo_component:
                            jumbo_args["component"]  = gen_file.jumbo_component
                        jumbo_unit.addSourceFile(path, jumbo_args)
                sources.addJumboCompileUnit(jumbo_unit)
                if sources.save(module_sources_abs):
                    if verbose_level >= 1:
                        print "M %s" % module_sources
                    module_sources_file.touch(module_sources_rule.mtime())
                    modified += 1
                    result = 2
                else:
                    if options.show_all and verbose_level >= 1:
                        print "  %s" % module_sources
                jumbo_files.add(JumboFile(module.type(), module.name(), module_sources_name))

        from cppgen.utils import cachePath
        jumboCacheFile = File(os.path.join(outputRoot or opera_root, cachePath, "jumbo_files.dat"))

        # See if any jumbo files should be removed by scanning for module.sources_protobuf
        # and seeing which ones are no longer in use

        jumbo_map = {} # maps (module_type,module_name) to module
        for jumbo_file in jumbo_files.iterJumboFiles():
            jumbo_map[(jumbo_file.module_type, jumbo_file.module_name)] = jumbo_file

        for module, sources_file_abs, sources_file in find_protobuf_sources(opera_root):
            module_key = (module.type(), module.name())
            if module_key in jumbo_map:
                continue
            if util.fileTracker.addInput(module_sources_abs):
                if verbose_level >= 1:
                    print "R %s" % sources_file
                generated += 1
                modified += 1
                os.unlink(sources_file_abs)
                result = 2

        # Update the outdated jumbo-cache file, so that downgrading to old code will still work
        jumbo_files.saveCache(jumboCacheFile)

        if system.total_modified:
            result = 2

        if verbose_level >= 1:
            print "Updated %d of %d files" % (modified, generated)

        return result
    except Exception, e:
        if not quiet:
            print >> sys.stderr, "Failed with exception: %s" % e
            import traceback
            traceback.print_exc(e)
        return 1
    finally:
        mtimeCache.save()

def hobExtension(ui, cpp_conf, files, replace, stop, diff, print_stdout, show_all):
    """Helper function used by the hob extension (extension.py)"""

    opera_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))

    from cppgen.cpp import mtimeCache
    mtimeCache.setRoot(opera_root)
    mtimeCache.load()

    try:
        options = GeneratorOptions(replace=replace, verbose_level=ui.verbose_level, stop=stop, diff=diff, print_stdout=print_stdout, show_all=show_all)
        system = ProtobufSystem(options, opera_root, ui=ui)
        system.process()
    finally:
        mtimeCache.save()

def main():
    """Main function for use as a script"""
    # Detect root and update import path to allow this file to be run as a script
    opera_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))

    from optparse import OptionParser
    parser = OptionParser()

    parser.add_option("--outroot", metavar="PATH", action="store", dest="outputRoot", default=opera_root,
                      help="Base directory in which to put output files, defaults to source root")
    parser.add_option("--quiet", action="store_true", dest="quiet", default=False,
                      help="Increase verbosity level")
    parser.add_option("--profile", dest="profile_filename", metavar="FILE",
                      help="Run Python profiler and store result in file")
    parser.add_option("--diff", dest="show_diff", action="store_true", default=False,
                      help="Display unified diff")
    parser.add_option("-r", "--replace", dest="replace", action="store_true", default=False,
                      help="Updated files with new content, if the content is the same nothing is written")
    parser.add_option("--show-all", dest="show_all", action="store_true", default=False,
                      help="Show all files which are processed, not just the ones which are modified")
    parser.add_option("--show-modified", dest="show_all", action="store_false",
                      help="Show only modified files which are processed")
    parser.add_option("--show-deps", dest="show_deps", action="store_true",
                      help="Show dependency information")
    parser.add_option("-f", "--force", dest="force", action="store_true",
                      help="Force generation of all files even if there is no change")
    parser.add_option("--print", dest="print_stdout", action="store_true",
                      help="Print the output to stdout")

    # Options for debugging/development purposes
    parser.add_option("--ignore-py-dependency", dest="ignore_py_dependency", action="store_true", default=False,
                      help="Ignore .py files when checking for changes in dependency list. Mainly meant for development or debugging purposes.")

    opts, args = parser.parse_args()

    globalOptions["includePyDependency"] = not opts.ignore_py_dependency

    verbose_level = 1
    if opts.quiet:
        verbose_level = 0

    p = None
    if opts.profile_filename:
        from cProfile import Profile
        p = Profile()

    def profiler_wrap():
        from cppgen.cpp import mtimeCache
        mtimeCache.setRoot(opts.outputRoot or opera_root)
        mtimeCache.load()
        try:
            options = GeneratorOptions(replace=opts.replace, verbose_level=verbose_level, stop=False, diff=opts.show_diff, print_stdout=opts.print_stdout, show_all=opts.show_all, print_deps=opts.show_deps, force=opts.force, output_root=opts.outputRoot)
            system = ProtobufSystem(options, opera_root)
            system.scan()
            system.process()
        finally:
            mtimeCache.save()
    try:
        if p:
            ret = p.runcall(profiler_wrap)
        else:
            ret = profiler_wrap()
    finally:
        if opts.profile_filename:
            p.dump_stats(opts.profile_filename)
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print >> sys.stderr, "Aborted"
        sys.exit(1)
    except ProgramError, e:
        print >> sys.stderr, e
        sys.exit(1)
    except SystemExit:
        pass
    except Exception, e:
        print >> sys.stderr, "Failed with exception: %s" % e
        import traceback
        traceback.print_exc(e)
        import pdb
        pdb.post_mortem(sys.exc_traceback)
        sys.exit(1)
