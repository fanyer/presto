import os.path
import sys
import re
import StringIO
from copy import deepcopy

# import modules from modules/hardcore/scripts/
import util
from util import WarningsAndErrors

class SourcesSet:
    """
    An instance of this class keeps a set of source-files with
    different options:
    - All source-files are returned by the method all()
    - Source-files that need to be compiled without a precompiled
      header are returned by nopch()
    - Source-files that need to be compiled with the precompiled
      header "core/pch.h" are returned by the method "pch".
    - Source-files that need to be compiled with the precompiled
      header "core/pch_system_includes.h" are returned by the method
      pch_system_includes().
    - Source-files that need to be compiled with the precompiled
      header "core/pch_jumbo.h" are returned by the method
      pch_jumbo().

    Adding a source-file to this set is done by calling
    addSourceFile().
    """
    def __init__(self):
        self.__all = []
        self.__nopch = []
        self.__pch = []
        self.__pch_system_includes = []
        self.__pch_jumbo = []
        self.__components = {}

    def all(self): return self.__all
    def nopch(self): return self.__nopch
    def pch(self): return self.__pch
    def pch_system_includes(self): return self.__pch_system_includes
    def pch_jumbo(self): return self.__pch_jumbo
    def components(self): return self.__components
    def component(self, component_name): return self.__components[component_name]

    def addSourceFile(self, source, pch, system_includes, jumbo, components=None):
        """
        Adds the specified source-file to this set.
        @param source is the name of the source-file to add.
        @param pch is true if the option "pch" is set for the
          specified source-file.
        @param system_includes is true if the option "system_includes"
          is set for the specified source-file.
        @param jumbo is true if the specified source-file is a
          jumbo-compile unit.
        @param components are the names of a components the source-file is
          part of.
        """
        self.__all.append(source)
        if pch:
            if system_includes:
                if jumbo:
                    # todo: currently there is no pch.h for
                    # jumbo-compile plus system_includes, so use nopch
                    # for this source-file:
                    self.__nopch.append(source)
                else: self.__pch_system_includes.append(source)
            elif jumbo:
                self.__pch_jumbo.append(source)
            else: self.__pch.append(source)
        else:
            self.__nopch.append(source)

        if components is not None:
            for component in components:
                if component not in self.__components:
                    self.__components[component] = []
                self.__components[component].append(source)

    def extend(self, other, type=None, module=None):
        """
        Extends each list of sources in this set with the
        corresponding list of sources of the other set.

        If type and module are not None, then each the other set is
        assumed to contain filenames relative to the module in the
        directory "root/type/module" while this set is should contain
        filenames relative to "root", so each filename in the other
        set is prepended with "type/module" before adding it to this
        set.

        @param other is the other SourcesSet to add to this set.
        @param type is usually one of "adjunct", "modules" or
          "platforms".
        @param module is the directory name of the module from which
          the associated module.sources file was loaded into the other
          set.
        """
        parts = []
        if type is not None: parts.append(type)
        if module is not None: parts.append(module)
        self.all().extend('/'.join(parts + [f]) for f in other.all())
        self.nopch().extend('/'.join(parts + [f]) for f in other.nopch())
        self.pch().extend('/'.join(parts + [f]) for f in other.pch())
        self.pch_system_includes().extend('/'.join(parts + [f]) for f in other.pch_system_includes())
        self.pch_jumbo().extend('/'.join(parts + [f]) for f in other.pch_jumbo())
        for component in other.components():
            if component not in self.__components:
                self.__components[component] = []
            self.__components[component].extend('/'.join(parts + [f]) for f in other.component(component))

    def generateSourcesListFiles(self, output_root):
        """
        Generates the files 'sources.all', 'sources.nopch',
        'sources.pch', 'sources.pch_system_includes',
        'sources.pch_jumbo' and 'sources.component_*' in the
        specified output_root directory.

        Each generated file contains the names of the source files in
        the corresponding list. Each filename is on a new line.

        If any of the files exist and if the content has not changed,
        the file is not written, i.e. its modification date remains
        unchanged.

        @param output_root is the directory name in which to generate
          the files. If the directory does not yet exist, it is
          generated.
        @return True if any of the files has changed; False if none of
          the files have changed.
        """
        def updateFile(list, filename):
            content = StringIO.StringIO()
            for f in list:
                print >>content, f
            return util.updateFile(content, filename)

        changed = updateFile(self.all(), os.path.join(output_root, 'sources.all'))
        changed = updateFile(self.nopch(), os.path.join(output_root, 'sources.nopch')) or changed
        changed = updateFile(self.pch(), os.path.join(output_root, 'sources.pch')) or changed
        changed = updateFile(self.pch_system_includes(), os.path.join(output_root, 'sources.pch_system_includes')) or changed
        changed = updateFile(self.pch_jumbo(), os.path.join(output_root, 'sources.pch_jumbo')) or changed
        for component in self.components():
            changed = updateFile(self.component(component), os.path.join(output_root, 'sources.component_' + component)) or changed
        return changed

class JumboCompileUnit:
    """
    The class JumboCompileUnit is used to collect all files that are
    supposed to be joined in one compilation unit.

    The method addSourceFile() is used to add source-files to a
    compile-unit.

    The class can generate the source-file for the jumbo-compile-unit
    from a template by calling generateSourceFile()
    """
    def __init__(self, type, module, name, pch, system_includes, sources=None):
        """
        @param type is usually one of "adjunct", "modules" or
          "platforms".
        @param module is the directory name of the module from which
          the associated module.sources file is loaded.
        @param name is the filename of this jumbo-compile unit. The
          filename should be specified relative to the
          module-directory and path-separator should be a slash "/",
          so this name can be added to the file module.generated as it
          is.
        @param pch is True if the jumbo compile unit should be able to
          use a precompiled header. False otherwise.
        @param system_includes is True if the jumbo compile unit
          should #include core/pch_system_includes.h. False if the
          jumbo compile unit should #include core/pch.h.
        """
        self.__type = type # (= "adjunct", "modules", "platforms")
        self.__module = module
        self.__name = name
        self.__pch = pch
        self.__system_includes = system_includes
        self.__source_files = list(sources or [])
        # Map from source files to an dictionary of option strings, such as {'component' : 'plugin'}, to be added to module.sources.
        self.__source_file_options = {}

    def __repr__(self):
        extra = ""
        if self.source_files():
            extra += ",sources=%r" % self.source_files()
        return "JumboCompileUnit(type=%r,module=%r,name=%r,pch=%r,system_includes=%r%s)" % (self.type(), self.module(), self.name(), self.pch(), self.system_includes(), extra)

    def extend(self, other):
        existing = set(self.__source_files)
        for source in other.source_files():
            if source not in existing:
                existing.add(source)
                self.__source_files.append(source)
                self.__source_file_options[source] = other.source_file_options(source)

    def type(self): return self.__type
    def module(self): return self.__module
    def name(self): return self.__name
    def pch(self): return self.__pch
    def system_includes(self): return self.__system_includes
    def source_files(self): return self.__source_files
    def source_file_options(self, source_file):
        if self.__source_file_options.has_key(source_file):
            return self.__source_file_options[source_file]
        return []

    def getModuleDir(self):
        """
        Returns the path to the module directory relative to the
        source root.
        """
        return os.path.join(self.type(), *(self.module().split('/')))

    def getFilepath(self):
        """
        Returns the filename of this jumbo compilation unit relative
        to the source root of the Opera source tree
        """
        return os.path.join(self.getModuleDir(), *(self.name().split('/')))

    def addSourceFile(self, source_file, source_file_options={}):
        """
        Adds the specified source-file to this compile-unit.

        @param source_file is the path to the source-file to add to
          this jumbo-compile unit. The path should be as specified in
          the corresponding module.sources file.

        @param source_file_options A dictionary of option strings that will,
          if non-empty, be written to the module.sources file.
        """
        self.__source_files.append(source_file)
        self.__source_file_options[source_file] = source_file_options

    class TemplateAction:
        """
        This class is used to generate the jumbo compile unit's source
        file from the corresponding template. The template actions
        "include pch" and "include files" are known.
        """
        def __init__(self, jumbo_compile_unit):
            self.__jumbo_compile_unit = jumbo_compile_unit

        def __call__(self, action, output):
            if action == "include pch":
                # the pch header file depends on the option system_includes
                if self.__jumbo_compile_unit.system_includes():
                    print >>output, "#include \"core/pch_system_includes.h\""
                else:
                    print >>output, "#include \"core/pch_jumbo.h\""
                return True

            elif action == "include files":
                # include all source files of this compilation unit
                for include_file in self.__jumbo_compile_unit.source_files():
                    print >>output, "#include \"%s/%s/%s\"" % (
                        self.__jumbo_compile_unit.type(),
                        self.__jumbo_compile_unit.module(),
                        include_file)
                return True

            else: return False

    def generateSourceFile(self, sourceRoot, outputRoot):
        """
        Generates the source file for the jumbo compile unit.
        @param sourceRoot is the path to the Opera source tree
        @param outputRoot is the path for output files
        @return True if the generated source file has changed and
          False if it has not changed.
        """
        return util.readTemplate(os.path.join(sourceRoot, 'modules', 'hardcore', 'base', 'jumbo_compile_template.cpp'),
                                 os.path.join(outputRoot, self.getFilepath()),
                                 JumboCompileUnit.TemplateAction(self),
                                 os.path.join(outputRoot, self.getModuleDir()),
                                 self.name())


class ModuleSources(WarningsAndErrors):
    """
    The class ModuleSources represents the module.sources file of one
    module.

    The module.sources files is loaded and parsed by calling
    load(). Warnings and errors during the loading are collected and
    can be printed to the error-stream that is specified in the
    constructor.

    After module.sources is loaded, the list of source-files (with
    different options) can be accessed via the methods
    - sources(), sources_nopch(), sources_pch() and
      sources_pch_system_includes() for a plain compilation;
    - jumbo_sources(), jumbo_sources_nopch(), jumbo_sources_pch(),
      jumbo_sources_pch_jumbo() and sources_pch_system_includes() for
      a jumbo compilation.

    Usage:

    @code
    module_sources = ModuleSources('modules', 'hardcore')
    module_sources.load(os.path.join(source_root, 'modules', 'hardcore', 'module.sources'))
    module_sources.generateJumboCompileUnits(source_root, output_root)
    @endcode
    """
    def __init__(self, type, module_name, stderr=sys.stderr):
        """
        @param type is usually one of "adjunct", "modules" or
          "platforms".
        @param module_name is the directory name of the module from
          which to load the module.sources file.
        @param stderr can be set to the error-stream to which to print
          warnings and errors. The default is sys.stderr.
        """
        WarningsAndErrors.__init__(self, stderr)
        self.__type = type # (= "adjunct", "modules", "platforms")
        self.__module_name = module_name
        self.__plain_sources = SourcesSet()
        self.__jumbo_sources = SourcesSet()
        self.__source_options = {}
        self.__jumbo_compile_units = {}

    def jumboCompileUnits(self):
        """
        Returns the list of all jumbo-compile-units, i.e. a list of
        JumboCompileUnit instances.
        """
        return self.__jumbo_compile_units.values()

    def addJumboCompileUnit(self, unit):
        name = unit.name()
        if name in self.__jumbo_compile_units:
            cur_unit = self.__jumbo_compile_units[name]
            if cur_unit.name() == unit.name() and cur_unit.pch() == unit.pch() and cur_unit.system_includes() == unit.system_includes():
                cur_unit.extend(unit)
            else:
                raise ValueError("Conflict in JumboCompileUnit objects, the attributes name, pch and system_includes does not match")
        else:
            self.__jumbo_compile_units[name] = deepcopy(unit)

    def plain_sources(self): return self.__plain_sources
    def jumbo_sources(self): return self.__jumbo_sources

    def sources_all(self):
        """
        Returns the list of all source filenames for a plain compile.
        The list is empty until load() is called.
        """
        return self.__plain_sources.all()

    def sources_nopch(self):
        """
        Returns the list of all source filenames for a plain compile
        which don't use a precompiled header.
        The list is empty until load() is called.
        """
        return self.__plain_sources.nopch()

    def sources_pch(self):
        """
        Returns the list of all source filenames for a plain compile
        which use "core/pch.h" as precompiled header.
        The list is empty until load() is called.
        """
        return self.__plain_sources.pch()

    def sources_pch_system_includes(self):
        """
        Returns the list of all source filenames for a plain compile
        which use "core/pch_system_include.h".
        The list is empty until load() is called.
        """
        return self.__plain_sources.pch_system_includes()

    def jumbo_sources_all(self):
        """
        Returns the list of all source filenames for a jumbo compile.
        The list is empty until load() is called.
        """
        return self.__jumbo_sources.all()

    def jumbo_sources_nopch(self):
        """
        Returns the list of all source filenames for a jumbo compile
        which don't use a precompiled header.
        The list is empty until load() is called.
        """
        return self.__jumbo_sources.nopch()

    def jumbo_sources_pch(self):
        """
        Returns the list of all source filenames for a jumbo compile
        which use "core/pch.h" as precompiled header.
        The list is empty until load() is called.
        """
        return self.__jumbo_sources.pch()

    def jumbo_sources_pch_jumbo(self):
        """
        Returns the list of all source filenames for a jumbo compile
        which use "core/pch_jumbo.h" as precompiled header.
        The list is empty until load() is called.
        """
        return self.__jumbo_sources.pch_jumbo()

    def jumbo_sources_pch_system_includes(self):
        """
        Returns the list of all source filenames for a jumbo compile
        which use "core/pch_system_include.h".
        The list is empty until load() is called.
        """
        return self.__jumbo_sources.pch_system_includes()

    def getSourceOption(self, source, option, default_value=None):
        """
        Returns the value of the specified option for the specified
        source file. If the specified option is not set for the
        specified source file, the specified default value is
        returned.

        @param source is the source file for which to return the value
          of the option.
        @param option is the name of the option to return.
        @param default_value is the default value to return if the
          option is not set.
        """
        if (source in self.__source_options and
            option in self.__source_options[source]):
            return self.__source_options[source][option][0]
        else: return default_value

    def getSourceOptions(self, source, option, default_value=None):
        """
        Returns the values of the specified option for the specified
        source file. If the specified option is not set for the
        specified source file, the specified default value is
        returned.

        @param source is the source file for which to return the values
          of the option.
        @param option is the name of the option to return.
        @param default_value is the default value to return if the
          option is not set.
        """
        if (source in self.__source_options and
            option in self.__source_options[source]):
            return self.__source_options[source][option]
        else:
            return default_value

    def pch(self, source):
        """
        Returns True if the specified source-file has the option
        "pch" with value "1" (wich is the default value) and False if
        the option "pch" is set to "0".
        """
        return self.getSourceOption(source, "pch", "1") == "1"

    def nopch(self, source):
        """
        Returns True if the specified source-file has the option
        "pch" with value "0" (or "no-pch") and False if the option
        "pch" is set to "1" (wich is the default value).
        """
        return not self.pch(source)

    def system_includes(self, source):
        """
        Returns True if the specified source-file has the option
        "system_includes" with value "1" and False if the option
        "system_includes" is set to "0" (wich is the default value).
        """
        return self.getSourceOption(source, "system_includes", "0") == "1"

    def jumbo(self, source):
        """
        Returns the value of the option "jumbo". This is the name of
        the jumbo-compile-unit for the specified source file or "1" if
        the default filename should be used or "0" if jumbo-compile is
        disabled for that source-file.
        """
        return self.getSourceOption(source, "jumbo", "1")

    def component(self, source):
        """
        Returns the value of the option "component". This is the name
        of the component that the source file belongs to, or None if
        the file is not part of a component. Build systems may choose
        to compile files that are parts of components into separate
        libraries or binaries.
        """
        return self.getSourceOption(source, "component", None)

    def components(self, source):
        """
        Returns the values of the option "component". This are the names
        of the component that the source file belongs to, or None if
        the file is not part of any component. Build systems may choose
        to compile files that are parts of components into separate
        libraries or binaries.
        """
        return self.getSourceOptions(source, "component", None)

    def parseOptions(self, option_string):
        """
        Parse the options string and returns a dictionary with the
        key-value pairs which are defined in the specified
        option_string.

        @param option_string is a semicolon separated list of
          key=value pairs. Example: "option_1;option_2=value_1"
        @return a dictionary representing the key,value pairs.
        """
        result = {}
        if option_string is not None:
            for option in option_string.split(';'):
                parts = option.split('=', 1)
                key = parts[0]
                if "winnopch" == key:
                    result["pch"] = "0"
                    result["system_includes"] = "1"
                elif key:
                    if len(parts) > 1:
                        value = parts[1]
                    elif key.startswith("no-"):
                        key = key[3:]
                        value = "0"
                    else:
                        value = '1'

                    if key in result and type(result[key]) == list:
                        result[key].append(value)
                    else:
                        result[key] = [value]
        return result

    def extend(self, module_source):
        self.__plain_sources.extend(module_source.__plain_sources)
        self.__jumbo_sources.extend(module_source.__jumbo_sources)
        self.__source_options.update(module_source.__source_options)
        for unit in module_source.__jumbo_compile_units.itervalues():
            self.addJumboCompileUnit(unit)

    def load(self, filename):
        """
        Loads the file 'module.sources' from the specified path and
        parses its content.

        @param filename is the path and filename to the
          'module.sources' file to load.
        """
        self.__jumbo_compile_units = {}
        try:
            f = None
            f = open(filename, "rU")
            util.fileTracker.addInput(filename)
            current_options = None
            # regular expression to match a single line which only
            # contains the options for the following source files:
            re_Options = re.compile("^\\s*#\\s*\[(.*?)\]")
            # regular expression to match a single line which contains
            # the name of a source file with an optional set of
            # options for this file:
            re_Source = re.compile("^([\\w./-]+)\\s*(#.*)?$")
            for nr,text in enumerate(f):
                line = util.Line(filename, nr+1, text)
                if line.isEmptyOrWhitespace(): continue # ignore empty lines

                match = re_Options.match(str(line))
                if match:
                    # This line contains options for the source files
                    # on the next lines
                    current_options = self.parseOptions(match.group(1))
                    continue

                match = re_Source.match(str(line))
                if match:
                    # This line contains a new source file
                    source = match.group(1)
                    if current_options is not None:
                        source_options = current_options.copy()
                    else: source_options = None
                    comment = match.group(2)
                    if comment is not None:
                        match = re_Options.match(comment)
                        if match:
                            new_options = self.parseOptions(match.group(1))
                            if source_options is None:
                                source_options = new_options
                            else: source_options.update(new_options)

                    if source_options is not None and len(source_options) > 0:
                        self.__source_options[source] = source_options

                    pch = self.pch(source)
                    system_includes = self.system_includes(source)
                    jumbo = self.jumbo(source)
                    components = self.components(source)
                    self.__plain_sources.addSourceFile(
                        source, pch=pch, system_includes=system_includes,
                        # Note: for a plain compile, jumbo is always disabled
                        jumbo=False, components=components)
                    if jumbo == "0" or components is not None:
                        # Note: files with a component can not be in the jumbo compile (component information would be lost)
                        self.__jumbo_sources.addSourceFile(source, pch=pch, system_includes=system_includes, jumbo=False, components=components)
                    elif jumbo in self.__jumbo_compile_units:
                        # if the jumbo-unit exists, add the
                        # source-file to the jumbo-unit, but the
                        # jumbo-unit was already added to
                        # self.__jumbo_sources
                        jumbo_unit = self.__jumbo_compile_units[jumbo]
                        if (pch != jumbo_unit.pch() or
                            system_includes != jumbo_unit.system_includes()):
                            self.addError(line, "the source-file %s must have the same 'pch' and 'system_includes' option as the other files in its jumbo-compile-unit" % source)
                        else:
                            jumbo_unit.addSourceFile(source, source_options)

                    else:
                        # this is a new jumbo compile unit, so create
                        # a new JumboCompileUnit instance and add it
                        # to self.__jumbo_sources
                        if jumbo == "1":
                            # Note: the module name may contain
                            # slashes (e.g. "lingogi/share/net"), so
                            # convert them to underscores:
                            jumbo_name = "%s_jumbo.cpp" % self.__module_name.replace("/", "_")
                        else: jumbo_name = jumbo
                        jumbo_unit = JumboCompileUnit(
                            type=self.__type, module=self.__module_name,
                            name=jumbo_name,
                            pch=pch, system_includes=system_includes)
                        jumbo_unit.addSourceFile(source, source_options)
                        self.__jumbo_compile_units[jumbo] = jumbo_unit
                        self.__jumbo_sources.addSourceFile(jumbo_name, pch=pch, system_includes=system_includes, jumbo=True)

                elif line.isComment(): continue # ignore empty lines
                else: self.addWarning(line, "unrecognized line '%s'" % str(line))
        finally:
            if f: f.close()
        return self.__plain_sources.all()

    def save(self, filename):
        """Saves the content to the specified filename.

        @param filename is the path and filename to the
          'module.sources' file to save.

        If the filename already exists and saved content is the same as the
        filename then it will not update file. This avoids unecessary updates
        to files used in build systems.

        Return True if the file was updated, False otherwise.
        """
        out = ""
        abs_root = os.path.normpath(os.path.dirname(os.path.abspath(filename)))
        def make_relative(fname):
            fname = os.path.normpath(os.path.abspath(fname))
            if fname.startswith(abs_root):
                fname = fname[len(abs_root)+1:]
            else:
                fname = fname
            return fname.replace(os.path.sep, "/")

        all = set(self.__plain_sources.all())
        pch = set(self.__plain_sources.pch())
        nopch = set(self.__plain_sources.nopch())
        pch_system_includes = set(self.__plain_sources.pch_system_includes())
        pch_jumbo = set(self.__plain_sources.pch_jumbo())

        jumbo_units = self.__jumbo_compile_units
        for jumbo_unit in jumbo_units.itervalues():
            out += "# [pch=%d;system_includes=%d;jumbo=%s]\n" % (int(jumbo_unit.pch()), int(jumbo_unit.system_includes()), jumbo_unit.name())
            for fname in jumbo_unit.source_files():
                options = ""
                if jumbo_unit.source_file_options(fname):
                    options = " # [%s]" % ",".join(map(lambda (k, v): '%s=%s' % (k, v),
                                                       zip(jumbo_unit.source_file_options(fname).keys(),
                                                           jumbo_unit.source_file_options(fname).values())))
                out += make_relative(fname) + options + "\n"
                for src_set in (all, pch, nopch, pch_system_includes, pch_jumbo):
                    src_set.discard(fname)

        plain = all - pch - nopch - pch_system_includes - pch_jumbo

        if pch_jumbo:
            out += "# [pch=1;jumbo=1]\n"
            for fname in pch_jumbo:
                out += make_relative(fname) + "\n"
        if pch:
            out += "# [pch=1]\n"
            for fname in pch:
                out += make_relative(fname) + "\n"
        if nopch:
            out += "# [pch=0]\n"
            for fname in nopch:
                out += make_relative(fname) + "\n"
        if pch_system_includes:
            out += "# [pch=1;system_includes=0]\n"
            for fname in pch_system_includes:
                out += make_relative(fname) + "\n"
        if plain:
            out += "# [pch=0;jumbo=0]\n"
            for fname in plain:
                out += make_relative(fname) + "\n"

        output = StringIO.StringIO()
        output.write(out)
        return util.updateFile(output, filename)

    def generateJumboCompileUnits(self, sourceRoot, outputRoot):
        """
        Updates all jumbo compile units.
        @param sourceRoot is the path to the Opera source tree
        @param outputRoot is the path for output files
        @return True if any of the generated jumbo compile units has
          changed and False if none has changed.
        """
        changed = False
        for jumbo_unit in self.__jumbo_compile_units.values():
            changed = jumbo_unit.generateSourceFile(sourceRoot, outputRoot) or changed
        return changed
