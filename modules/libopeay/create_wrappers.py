#!/usr/bin/env python

import os.path
import sys
import StringIO
import optparse

sys.path.insert(1, os.path.join(sys.path[0], '..', '..', 'modules', 'hardcore', 'scripts'))
import module_sources
import util

class WrapperModuleSources(module_sources.ModuleSources):
    """
    This class extends the parser of module.sources by two new options:
    - "cpp" (default disabled) can be used for all source-files
      starting with "opera_" to indicate that the original source-file
      is a cpp-file and not a c-file; so the wrapper can create the
      correct include statement.
    - forcecompileif=statement can be used to compile the source-code
      if the specifed preprocessor statement is true.
    """
    def __init__(self):
        module_sources.ModuleSources.__init__(self, 'modules', 'libopeay')

    def cpp(self, source):
        """
        This option is used for all source-files starting with
        "opera_" to indicate that the original source-file is a
        cpp-file and not a c-file. So the wrapper can create the
        correct include statement.
        @param source is the source file to check.
        @return True if the specified source-file has the
          option cpp set and False otherwise (False is default).
        """
        return self.getSourceOption(source, "cpp", "0") == "1"

    def forcecompileif(self, source):
        """
        The forcecompileif statement is used to compile the
        source-code if the specifed preprocessor statement is true.

        @param source is the source file to check.
        @return the preprocessor statement or None if the option is
          not set.
        """
        return self.getSourceOption(source, "forcecompileif", None)

    def external_ssl_only(self, source):
        """
        The external-ssl-only statement is used to compile the
        source-code with an "#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION"
        statement. Without this option, the source-code is compiled
        with an "#ifdef _SSL_USE_OPENSSL_" statement.

        @param source is the source file to check.
        @return True if the specified source-file has the
          option external-ssl-only set and False otherwise (False is
          default).
        """
        return self.getSourceOption(source, "external-ssl-only", "0") == "1"

    def include_with_system_lib(self, source):
        """
        The include-with-system-lib statement is used for source-files
        that should be compiled even if
        TWEAK_LIBOPEAY_USE_SYSTEM_OPENSSL_LIBRARY is enabled (i.e. an
        external openssl library is used). Source files without this
        option will be wrapped inside an
        "#if !defined(_USE_SYSTEM_LIBCRYPTO_)" statement.

        @param source is the source file to check.
        @return True if the specified source-file has the
          option include-with-system-lib set and False otherwise (False is
          default).
        """
        return self.getSourceOption(source, "include-with-system-lib", "0") == "1"

    def getSourceOptionString(self, source):
        """
        Returns the options string for the specified source file. The
        options string can be used to update the file module.sources.
        """
        options = []
        if self.nopch(source) and self.system_includes(source):
            options.append("winnopch")
        if self.nopch(source): options.append("no-pch")
        if self.system_includes(source):
            options.append("system_includes")
        if self.cpp(source):
            options.append("cpp")
        if self.external_ssl_only(source):
            options.append("external-ssl-only")
        if self.include_with_system_lib(source):
            options.append("include-with-system-lib")
        forcecompileif = self.forcecompileif(source)
        if forcecompileif:
            options.append("forcecompileif=%s" % forcecompileif)
        return ";".join(options)

class HandleTemplateAction:
    def __init__(self, cpp, module_sources, source_file, original_filename):
        self.__cpp = cpp
        self.__system_includes = module_sources.system_includes(source_file)
        self.__forcecompileif = module_sources.forcecompileif(source_file)
        self.__include_with_system_lib = module_sources.include_with_system_lib(source_file)
        if module_sources.external_ssl_only(source_file):
            self.__define = "EXTERNAL_SSL_OPENSSL_IMPLEMENTATION"
        else:
            self.__define = "_SSL_USE_OPENSSL_"
        self.__original_filename = original_filename

    def __call__(self, action, output):
        if action == "header":
            if self.__system_includes:
                output.write("#include \"core/pch_system_includes.h\"\n")
            else:
                output.write("#include \"core/pch.h\"\n")
            if not self.__cpp:
                output.write("#include \"modules/libopeay/core_includes.h\"\n")
            return True

        elif action == "compileif start":
            output.write("#if defined(%s)" % self.__define)
            if self.__forcecompileif:
                output.write(" || %s" % self.__forcecompileif)
            output.write("\n")
            if not self.__include_with_system_lib:
                output.write("# if !defined(_USE_SYSTEM_LIBCRYPTO_)\n")
            return True

        elif action == "compileif end":
            if not self.__include_with_system_lib:
                output.write("# endif // !_USE_SYSTEM_LIBCRYPTO_\n")
            output.write("#endif // %s" % self.__define)
            if self.__forcecompileif:
                output.write(" || %s" % self.__forcecompileif)
            output.write("\n")
            return True

        elif action == "extern C start":
            if not self.__cpp: output.write('extern "C" {\n')
            return True

        elif action == "extern C end":
            if not self.__cpp: output.write('}\n')
            return True

        elif action == "original filename":
            output.write('#include "modules/libopeay/%s"\n' % self.__original_filename)
            return True

option_parser = optparse.OptionParser(
    description="(Re-)Generates the wrapper files for the openssl source files.")
option_parser.parse_args()

module_sources_out = StringIO.StringIO()
module_sources_out.write("# [no-jumbo]\n")
comments = ["Note: don't use jumbo-compile for libopeay, because this code",
            "contains a lot of wrappers around external C-sources which",
            "contain a lot of #define macros and static local functions which",
            "generate a lot of conflicting declarations...",
            "Note: this file is automatically generated by create_wrappers.py",
            "source-files and options are preserved, but comments are not ..."
            ]
for line in comments:
    module_sources_out.write("# %s\n" % line)
module_sources_out.write("\n")

sourceRoot = os.path.normpath(os.path.join(sys.path[0], '..', '..'))
libopeayDir = os.path.join(sourceRoot, 'modules', 'libopeay')
wrapper_template = os.path.join(libopeayDir, 'wrapper_template.cpp')
module_sources = WrapperModuleSources()
module_sources.load(os.path.join(libopeayDir, 'module.sources'))
changed_files = []
for source_file in module_sources.sources_all():
    file_path = source_file.split('/')
    filename = file_path[-1]
    original_filename = filename
    (sourcename, dot, extension) = filename.partition(".")
    if sourcename.startswith("opera_"):
        extension = "c"
        if module_sources.cpp(source_file):
            extension = "cpp"
        original_filename = sourcename[6:] + "." + extension
    else:
        extension = "c"
        filename = "opera_" + sourcename + ".cpp"
    file_path[-1] = filename;

    options = module_sources.getSourceOptionString(source_file)
    if len(options) == 0:
        module_sources_out.write("%s\n" % '/'.join(file_path))
    else:
        module_sources_out.write("%s # [%s]\n" % ('/'.join(file_path).ljust(34), options))
    if util.readTemplate(wrapper_template,
                         os.path.join(libopeayDir, *file_path),
                         HandleTemplateAction(extension=="cpp",
                                              module_sources,
                                              source_file,
                                              "/".join(file_path[0:-1] + [original_filename]))):
        changed_files.append(os.path.join(*file_path))

if util.updateFile(module_sources_out,
                   os.path.join(libopeayDir, "module.sources")):
    print "module.sources updated"
else:
    print "module.sources not changed"
if len(changed_files):
    print "%d wrapper files updated:" % len(changed_files), changed_files
else:
    print "No wrapper file changed"
