#!/usr/bin/env python

import xml.dom.minidom
import xml.sax.saxutils
import re
import os
import os.path
import stat
import sys
import cStringIO
import traceback
import shutil
import time
import optparse
import fnmatch

# The script is in sourceRoot/modules/hardcore/tools:
sourceRoot = os.path.normpath(os.path.join(sys.path[0], "..", "..", ".."))

class Timing:
    """
    A simple class which can be used for timing a function.

    In Python 2.5 or newer this can be used in combination with the
    "with" statement like:
    @code
    with Timing() as my_timer:
        call_some_function()
    print "Time used: %s" % my_timer.timestr()
    @endcode

    This class can also be used without the "with" statement:
    @code
    my_timer = Timing()
    my_timer.start()
    call_some_function()
    my_timer.stop()
    print "Time used: %s" % my_timer.timestr()
    """
    def __init__(self):
        self.__start = None
        self.__stop = None

    def __enter__(self):
        self.start()
        return self
    def __exit__(self, exc_type, exc_value, traceback): self.stop()

    def start(self): self.__start = time.clock()
    def stop(self): self.__stop = time.clock()

    def time(self):
        if self.__start is None: return None
        elif self.__stop is None: return time.clock() - self.__start
        else: return self.__stop - self.__start

    def timestr(self):
        time = self.time()
        if time is None: return "None"
        else: return "%.2gs" % time

def error(message):
    print >>sys.stderr, message
    sys.exit(1)

def escapeattr(value):
    return xml.sax.saxutils.quoteattr(value, { '"': "&quot;" })

class VCProjectUpdater:
    def __init__(self, sourceRoot, jumbo_compile, remove_excluded_files, sources_filter, output, exclude_files_pattern="", quiet=True):
        self.__sourceRoot = sourceRoot
        self.__jumbo_compile = jumbo_compile
        self.__remove_excluded_files = remove_excluded_files
        self.__quiet = quiet
        self.__sources_filter = sources_filter
        self.__exclude_files_pattern = exclude_files_pattern
        # the relative path is used in projRelativePath():
        self.__relpath_projectfile_sourceRoot = None
        if output is not None:
            # if there is an output file, then the relative path from the
            # output project file to the sourceRoot is calculated in here
            self.__relpath_projectfile_sourceRoot = self.calculateRelPathProjectfile2SourceRoot(output)
            # if output is None, then the input file and output file are
            # the same and this is calculated in parseVCProjectFile()
        # the root node is set in parseVCProjectFile()
        self.__root_node = None
        # the name of the project file is set in parseVCProjectFile()
        self.__project_file = None
        # the list of all sources is loaded the first time
        # sourcesAll() is called
        self.__sources_all = None
        # the set of all sources is loaded on loadSourcesConfigurations()
        self.__sources = {
            'plain' : { 'all' : None,
                        'nopch' : None,
                        'pch' : None,
                        'pch_jumbo' : None,
                        'pch_system_includes' : None },
            'jumbo' : { 'all' : None,
                        'nopch' : None,
                        'pch' : None,
                        'pch_jumbo' : None,
                        'pch_system_includes' : None }
            }
        # the list of all configuration names is calculated the first
        # time getConfigurations() is called:
        self.__configurations = None
        self.__pch_file = {}
        # for each branch_name there may be a list of modules to
        # include; if such a list is specified, only the modules in
        # that list are added to the project file
        self.__include_modules_for_branch = {}
        # for each branch_name there may be a list of modules to
        # exclude; if such a list is specified, the modules in that
        # list are removed from the project file
        self.__exclude_modules_for_branch = {}

    def calculateRelPathProjectfile2SourceRoot(self, project_file):
        """
        Calculates the relative path from the specified project file
        to the source root (that was specified in the constructor.

        @param project_file path to the project file from which the
          relative path to the source root is requested.

        @return the relative path from the directory of the specified
          project_file to the source root.
        """
        try:
            # os.path.relpath new in python 2.6, exists only on Windows and Unix
            return os.path.relpath(self.__sourceRoot, os.path.dirname(os.path.abspath(project_file))).replace("/", "\\")
        except AttributeError:
            # os.path.relpath not available, so try to solit the path
            # into its directory names and compare each level:
            proj = os.path.abspath(project_file).split(os.sep)
            del proj[len(proj)-1] # remove filename
            root = os.path.abspath(self.__sourceRoot).split(os.sep)
            # remove the common start directories:
            while (len(proj) and len(root) and proj[0].lower() == root[0].lower()):
                del proj[0:1]
                del root[0:1]
            if len(proj) == 0: root[0:0] = ['.']
            else: root[0:0] = [".."] * len(proj)
            return '\\'.join(root)


    def setIncludeModules(self, branch_name, modules):
        self.__include_modules_for_branch[branch_name] = modules

    def setExcludeModules(self, branch_name, modules):
        self.__exclude_modules_for_branch[branch_name] = modules

    def isModuleIncluded(self, branch_name, module):
        # First check if the module is included:
        if branch_name in self.__include_modules_for_branch:
            included = module in self.__include_modules_for_branch[branch_name]
        else: included = True
        # then check if the module is excluded (i.e. an exclusion can
        # override the inclusion):
        if (included and branch_name in self.__exclude_modules_for_branch and
            module in self.__exclude_modules_for_branch[branch_name]):
            included = False
        return included


    def setPchFile(self, pch, file):
        self.progressMsg("pch file for '%s' is '%s'" % (pch, file))
        self.__pch_file[pch] = file

    def getPchFile(self, pch):
        if pch in self.__pch_file: return self.__pch_file[pch]
        else: return None

    def progressMsg(self, msg):
        if not self.__quiet: print >>sys.stdout, msg

    def sourceRoot(self): return self.__sourceRoot
    def root_node(self): return self.__root_node
    def vcproj_file(self): return self.__project_file
    def jumbo_compile(self): return self.__jumbo_compile
    def sources_filter(self): return self.__sources_filter
    def exclude_files_pattern(self): return self.__exclude_files_pattern
    def remove_excluded_files(self): return self.__remove_excluded_files

    def getFirstChildNode(self, parent, node_name):
        """
        Returns the first child of node with the specified node name.
        @param parent is the xml node in which to look for the first
          child with the specified node name.
        @param node_name is a node name. The first child with this
          node name is returned.
        @param node_name is the name of the child node to return
        @return the xml node with the specified name or None if no
          such node exists.
        """
        if parent is not None:
            for node in parent.childNodes:
                if node.nodeName == node_name: return node
        return None

    def getChildNodes(self, parent, node_name):
        """
        Returns all children of the node with the specified node
        name.
        @param parent is the xml node in which to look for the
          children with the specified node name.
        @param node_name is a node name. Only children with this node
          name are returned.
        @return the xml node with the specified name or None if no
          such node exists.
        """
        if parent is not None:
            for node in parent.childNodes:
                if node.nodeName == node_name: yield node

    def getFirstChildNodeWithAttr(self, parent, node_name, attr, attr_value):
        """
        Returns the first child of the parent node with the specified
        node name and where the specified attribute has the specified
        value.
        @param parent is the parent node to search for the child.
        @param node_name is a node name. Only children with this node
          name are returned.
        @param attr is the attribute to test for the attr_value.
        @param attr_value is the requested value of the attribute. The
          first child node where the specified attribute has this
          value is returned.
        @return the xml node with the specified name and attribute or
          None if no such node exists.
        """
        if parent is not None:
            for node in parent.childNodes:
                if (node.nodeName == node_name and
                    node.getAttribute(attr) == attr_value):
                    return node
        return None

    def getFirstChildNodeWithNameAttr(self, parent, node_name, name_attr):
        """
        Returns the first child of the parent node with the specified
        node name and which has a "Name" attribute with the specified
        value.
        @param parent is the parent node to search for the child.
        @param node_name is a node name. Only children with this node
          name are returned.
        @param name_attr is the value of the "Name" attribute. The
          first child node which has a "Name" with this value is
          returned.
        @return the xml node with the specified name and attribute or
          None if no such node exists.
        """
        return self.getFirstChildNodeWithAttr(parent, node_name, "Name", name_attr)

    def getFilterNode(self, parent, filter_name):
        """
        Returns the child xml-node "Filter" from the specified parent
        node, which has the "Name" attribute with the specified
        filter_name. If no such node exists, a new node is created and
        inserted into the parent.
        @param parent is the parent node in which to find the Filter
          node.
        @param filter_name is the value of the "Name" attribute of the
          requested Filter node.
        @return the "Filter" node with the specified name.
        """
        filter = self.getFirstChildNodeWithNameAttr(parent, "Filter", filter_name)
        if not filter:
            filter = parent.ownerDocument.createElement("Filter")
            filter.setAttribute("Name", filter_name)
            parent.appendChild(filter)

        if not self.getFirstChildNodeWithNameAttr(parent, "Filter", filter_name):
            raise Exception, "Can not find added group\n"
        return filter

    def removeNodeAttributes(self, node, attributes):
        """
        Removes the specified list of attributes from the specified
        node.
        @param node is the xml node from which to remove the
          attributes.
        @param attributes is a list of attribute names to remove.
        @return True if the node has changed; False if the node was
          not changed, i.e. if it did not have any of the specified
          attributes.
        """
        changed = False
        for attribute in attributes:
            if node.hasAttribute(attribute):
                node.removeAttribute(attribute)
                changed = True
        return changed

    def sortNodeChildren(self, node_to_sort):
        """
        Sorts the children of the specified xml-node. The children are
        sorted by type (Filter or File) and within the same type they
        are sorted by name (i.e. the Filter nodes are sorted by
        Filter-name and the File nodes are sorted by the filename of
        the RelativePath). If the first child of the specified node is
        a Filter, than all Filters are placed at the top. If the first
        child of the specified node is a File, than all Filters are
        placed at the bottom.

        The children of each Filter node are also sorted.

        If a Filter node is empty, i.e. if it has no File nodes as
        children or grand-children, then the Filter node is removed.

        If the children of the specified node are already sorted, then
        the node is not changed and False is returned. Otherwise True
        is returned.

        @param node_to_sort is the xml-node to sort.
        @return True if the specified node was changed; False if the
          specified node was not changed.
        """
        changed = False
        folder_children = {}
        file_children = {}

        files_first = False
        filters_first = False

        current_order = []
        for child in node_to_sort.childNodes:
            if child.nodeName == "Filter":
                current_order.append(child)
                if not files_first: filters_first = True
                filter_name = child.getAttribute("Name")
                if not filter_name: raise Exception, "Null filter name?!?\n"

                # sort the Filter:
                changed = self.sortNodeChildren(child) or changed

                def hasContent(node):
                    for child in node.childNodes:
                        if (child.nodeName == "File" or
                            (child.nodeName == "Filter" and hasContent(child))):
                            return True
                    return False

                # only use the Filter if it has File nodes
                if hasContent(child): folder_children[filter_name] = child

            elif child.nodeName == "File":
                current_order.append(child)
                if not filters_first: files_first = True
                file_name = child.getAttribute("RelativePath").split("\\")[-1]
                file_children[file_name] = child

        sorted_children = []
        if filters_first:
            for folder_name in sorted(folder_children.keys()):
                sorted_children.append(folder_children[folder_name])

        for file_name in sorted(file_children.keys()):
            file_node = file_children[file_name]
            sorted_children.append(file_node)
            if len(file_node.childNodes) == 0:
                raise Exception, "Missing text in file node\n"

        if files_first:
            for folder_name in sorted(folder_children.keys()):
                sorted_children.append(folder_children[folder_name])

        if current_order != sorted_children:
            changed = True
            while node_to_sort.firstChild:
                node_to_sort.removeChild(node_to_sort.firstChild)
            for child in sorted_children:
                node_to_sort.appendChild(child)
        return changed

    def getSourceFilesNode(self):
        """
        Returns the xml node "/VisualStudioProject/Files/Source Files".
        """
        sources_filter = self.sources_filter()
        vis_node = self.getFirstChildNode(self.root_node(), "VisualStudioProject")
        files_node = self.getFirstChildNode(vis_node, "Files")
        if len(sources_filter) < 1:
            return files_node
        source_files = self.getFirstChildNodeWithNameAttr(files_node, "Filter", sources_filter)
        if not source_files:
            raise Exception, "No \"Filter\" with name \"" + sources_filter + "\" in the vcproj file\n"
        return source_files

    def getModuleNode(self, branch_name, module_name):
        """
        Returns the module node
        "/VisualStudioProject/Files/Source Files/@branch_name/@module_name".
        The returned node corresponds to the module with the specified
        module_name in the specified branch. If the module node does not
        exist, it is created and inserted into the xml tree.

        @param branch_name is typically 'adjunct', 'modules' or
          'platforms'. The node with the specified module_name is expected
          to be a child of that branch.
        @param module_name is the name of the module for which to get the
          xml-node. The module is expected to be a child of the specified
          branch_name.
        @return the node of the specified module_name.
        """
        modules = self.getModulesNode(branch_name)
        if not modules:
            modules = self.root_node().createElement("Filter")
            modules.setAttribute("Name", branch_name)
            self.getSourceFilesNode().appendChild(modules)

        return self.getFilterNode(modules, module_name)

    def getModulesNode(self, branch_name):
        """
        Returns the node "/VisualStudioProject/Files/Source Files/@branch_name"
        from the root_node. The returned node corresponds to the
        adjunct, modules or platforms directory (depending on the
        branch_name) and the children of the returned node are typically
        the nodes corresponding to each adjunct-, modules- or
        platform-module.

        @param branch_name is the name of the branch for which to return
          the node. The branch_name is typically 'adjunct', 'modules' or
          'platforms'.

        @return the node of the specified branch_name or None if such a
          node does not exist.
        """
        return self.getFirstChildNodeWithNameAttr(
            self.getSourceFilesNode(), "Filter", branch_name)

    def getConfigurations(self):
        """
        Returns the list of configuration names of the project file.
        """
        if self.__configurations is None:
            # List of configurations is requested the first time:
            vis_node = self.getFirstChildNode(self.root_node(), "VisualStudioProject")
            conf_node = self.getFirstChildNode(vis_node, "Configurations")
            self.__configurations = map(
                lambda c: c.getAttribute("Name"),
                self.getChildNodes(conf_node, "Configuration"))
        return self.__configurations

    def loadSourcesFile(self, type, name):
        """
        Loads the contents of the sourceslist with the specified name.
        @param type can be 'plain' or 'jumbo'
        @param name can be one of 'all', 'nopch', 'pch', 'pch_jumbo' or
          'pch_system_includes'.
        @return a list of source-files.
        """
        filename = os.path.join(self.__sourceRoot, 'modules', 'hardcore', 'setup', type, 'sources', 'sources.%s' % name)
        sources = []
        f = None
        try:
            f = open(filename)
            for source in f:
                sources.append(source.strip())
        finally:
            if f: f.close()
        return sources

    def sourcesAll(self):
        """
        Returns a sorted list of all source files, i.e. both the
        source files needed for a jumbo-compilation and the source
        files needed for a plain compilation.
        """
        if self.__sources_all is None:
            if self.remove_excluded_files():
                # keep only the files that are needed for the
                # specified compile-configuration, i.e. only the jumbo
                # files or only the plain files:
                if self.jumbo_compile():
                    self.__sources_all = sorted(self.__sources['jumbo']['all'])
                else:
                    self.__sources_all = sorted(self.__sources['plain']['all'])
            else:
                # all files are added to the project file, jumbo and
                # plain files and depending on the configuration, some
                # of them are excluded from the build:
                self.__sources_all = sorted(set(self.__sources['plain']['all']) |
                                            set(self.__sources['jumbo']['all']))
            # filter out files that shouldn't be included.
            if len(self.exclude_files_pattern()) > 0:
                self.__sources_all = [f for f in self.__sources_all if not fnmatch.fnmatch(f, self.exclude_files_pattern())];
        return self.__sources_all

    def loadSourcesConfigurations(self):
        for type in ('plain', 'jumbo'):
            for configuration in ('all', 'nopch', 'pch', 'pch_jumbo', 'pch_system_includes'):
                self.__sources[type][configuration] = self.loadSourcesFile(type, configuration)

    def findPrecompiledHeaderFiles(self):
        """
        This method searches in the project tree for any File which
        has a configuration "UsePrecompiledHeader=1", i.e. the
        precompiled header file is generated. Such an entry should
        also have the attributes "PrecompiledHeaderThrough" and
        "PrecompiledHeaderFile". The value from
        "PrecompiledHeaderThrough" is used as pch and the value from
        "PrecompiledHeaderFile" is used as pch_file on calling
        "setPchFile(pch, pch_file)" to associate the generated
        precompiled header file with the include statement.
        """
        source_files_node = self.getSourceFilesNode()
        all_files = source_files_node.getElementsByTagName("File")
        for file_node in all_files:
            # assume that all configurations have the same
            # "UsePrecompiledHeader" options
            config_node = self.getFirstChildNode(file_node, "FileConfiguration")
            tool_node = self.getFirstChildNodeWithNameAttr(config_node, "Tool", "VCCLCompilerTool")
            if (tool_node and
                tool_node.getAttribute("UsePrecompiledHeader") == "1"):
                if tool_node.hasAttribute("PrecompiledHeaderThrough"):
                    pch = tool_node.getAttribute("PrecompiledHeaderThrough")
                else: pch = "core/pch.h"
                if tool_node.hasAttribute("PrecompiledHeaderFile"):
                    pch_file = tool_node.getAttribute("PrecompiledHeaderFile")
                else: pch_file = None
                self.setPchFile(pch, pch_file)

    def parseVCProjectFile(self, project_file):
        self.progressMsg("parsing %s ..." % project_file)
        self.__project_file = project_file
        if self.__relpath_projectfile_sourceRoot is None:
            self.__relpath_projectfile_sourceRoot = self.calculateRelPathProjectfile2SourceRoot(project_file)
        self.__root_node = xml.dom.minidom.parse(project_file)
        self.findPrecompiledHeaderFiles()
        self.progressMsg("finished parsing %s" % project_file)

    def add_hardcoded_attributes(self, hardcoded_attributes, node, attrq, indentation, data):
        for a in hardcoded_attributes:
            if node.hasAttribute(a):
                data.write("\n%s%s=%s" % (indentation, a, attrq(node.getAttribute(a))))
                node.removeAttribute(a)

    def vcproj_render_xml(self, data, n, textq, attrq, indentation):
        tool_hardcoded_attributes = [ "Name",
                                      "AdditionalOptions",
                                      "AdditionalDependencies",
                                      "OutputFile",
                                      "LinkIncremental",
                                      "Optimization",
                                      "InlineFunctionExpansion",
                                      "AdditionalIncludeDirectories",
                                      "PreprocessorDefinitions",
                                      "MkTypLibCompatible",
                                      "Culture",
                                      "StringPooling",
                                      "ExceptionHandling",
                                      "BasicRuntimeChecks",
                                      "RuntimeLibrary",
                                      "StructMemberAlignment",
                                      "BufferSecurityCheck",
                                      "EnableFunctionLevelLinking",
                                      "UsePrecompiledHeader",
                                      "PrecompiledHeaderThrough",
                                      "PrecompiledHeaderFile",
                                      "AssemblerOutput",
                                      "AssemblerListingLocation",
                                      "ObjectFile",
                                      "ProgramDataBaseFileName",
                                      "BrowseInformation",
                                      "WarningLevel",
                                      "SuppressStartupBanner",
                                      "GenerateDebugInformation",
                                      "ProgramDatabaseFile",
                                      "GenerateMapFile",
                                      "MapFileName",
                                      "DebugInformationFormat",
                                      "CompileAs",
                                      "TreatWChar_tAsBuiltInType",
                                      "RuntimeTypeInfo",
                                      "UndefinePreprocessorDefinitions",
                                      "SubSystem",
                                      "ShowProgress",
                                      "TargetMachine",
                                      "StackReserveSize",
                                      "OptimizeReferences",
                                      "TargetEnvironment",
                                      "TypeLibraryName" ]

        hardcoded_attributes = [ "ProjectType",
                                 "Version",
                                 "Name",
                                 "OutputDirectory",
                                 "IntermediateDirectory",
                                 "ConfigurationType",
                                 "UseOfMFC",
                                 "ATLMinimizesCRunTimeLibraryUsage",
                                 "CharacterSet",
                                 "SccProjectName",
                                 "SccLocalPath",
                                 "AdditionalOptions",
                                 "AdditionalDependencies",
                                 "OutputFile",
                                 "LinkIncremental",
                                 "Optimization",
                                 "InlineFunctionExpansion",
                                 "PreprocessorDefinitions",
                                 "Culture",
                                 "AdditionalIncludeDirectories",
                                 "StringPooling",
                                 "BasicRuntimeChecks",
                                 "RuntimeLibrary",
                                 "StructMemberAlignment",
                                 "EnableFunctionLevelLinking",
                                 "UsePrecompiledHeader",
                                 "PrecompiledHeaderThrough",
                                 "PrecompiledHeaderFile",
                                 "AssemblerOutput",
                                 "AssemblerListingLocation",
                                 "ObjectFile",
                                 "ProgramDataBaseFileName",
                                 "BrowseInformation",
                                 "WarningLevel",
                                 "SuppressStartupBanner",
                                 "GenerateDebugInformation",
                                 "ProgramDatabaseFile",
                                 "GenerateMapFile",
                                 "MapFileName",
                                 "SubSystem",
                                 "StackReserveSize",
                                 "OptimizeReferences",
                                 "DebugInformationFormat",
                                 "CompileAs",
                                 "UndefinePreprocessorDefinitions",
                                 "RootNamespace",
                                 "ProjectGUID" ]

        if n.nodeType == n.TEXT_NODE:
            if n.nodeValue.strip() != "":
                raise Exception, "Project file contains text which wouldn't be preserved\n"
        elif n.nodeType == n.ELEMENT_NODE:
            data.write("%s<%s" % (indentation, n.nodeName))
            if n.nodeName == "Tool":
                self.add_hardcoded_attributes(tool_hardcoded_attributes, n, attrq, indentation + "\t", data)
            else:
                self.add_hardcoded_attributes(hardcoded_attributes, n, attrq, indentation + "\t", data)

            for a in sorted(n.attributes.keys()):
                data.write("\n%s\t%s=%s" % (indentation, a, attrq(n.getAttribute(a))))

            if len(n.childNodes):
                data.write(">\n")
            else:
                data.write("/>\n")
        elif n.nodeType == n.PROCESSING_INSTRUCTION_NODE:
            if n.data: data.write("<?%s %s?>" % (n.target, n.data))
            else: data.write("<?%s?>" % n.target)
        elif n.nodeType == n.COMMENT_NODE:
            data.write("<!--%s-->" % n.nodeValue)

        if n.nodeType == n.ELEMENT_NODE:
            new_indentation = indentation + "\t"
        else:
            new_indentation = indentation

        for child in n.childNodes:
            self.vcproj_render_xml(data, child, textq, attrq, new_indentation)

        if n.nodeType == n.ELEMENT_NODE:
            if len(n.childNodes):
                data.write("%s</%s>\n" % (indentation, n.nodeName))

    def updateVCProjFile(self, output_filename):
        """
        Writes the xml document to a new file, compares it to the
        specified output_filename and replaces the output_filename if the
        new file is different.

        @param output_filename is the name of the output file to write. If the
          the value is None, the name of the input file vcproj_file() is used.

        @returns False if the old file is kept,
          True if a new file is installed.
        """
        if output_filename is None: output_filename = self.vcproj_file()
        data = cStringIO.StringIO()
        try:
            self.vcproj_render_xml(data, self.root_node(), xml.sax.saxutils.escape, escapeattr, "")
        except:
            print >>sys.stderr, "VCProjectUpdater::updateVCProjFile(): vcproj_render_xml() FAILED!\n"
            traceback.print_exc()
            sys.exit(1)

        try:
            f = None
            try:
                f = open(os.path.join(output_filename + ".tmp"), "wct")
                f.write(data.getvalue())
            finally:
                if f: f.close()
        except:
            print >>sys.stderr, "VCProjectUpdater::updateVCProjFile(): could not write to temporary file!\n"
            traceback.print_exc()
            sys.exit(1)

        try:
            proj_f = None
            tmp_f = None
            if os.path.exists(output_filename):
                try:
                    proj_f = open(output_filename)
                    tmp_f = open(output_filename+".tmp")
                    need_to_update = proj_f.read() != tmp_f.read()
                finally:
                    if proj_f: proj_f.close()
                    if tmp_f: tmp_f.close()
            else: need_to_update = True
            if need_to_update:
                shutil.copyfile(output_filename+".tmp", output_filename)
                return True
            else:
                return False
        except:
            print >>sys.stderr, "VCProjectUpdater::updateVCProjFile(): could not overwrite project file!\n"
            traceback.print_exc()
            sys.exit(1)

    def filesHaveChanged(self):
        """
        Determine whether or not pertinent files have changed. If the
        files have changed, updateVCProjFile() should be called.
        @return True if the files have changed and False otherwise.
        """
        self.progressMsg("start counting changes")
        self.loadSourcesConfigurations()
        current_branch_name = None
        current_module_name = None
        module_names = {}
        module_changes = {}
        for filename in self.sourcesAll():
            # split the filename into three parts:
            # branch_name ('adjunct', 'modules', 'platforms'),
            # module_name and rest of the filename:
            (branch_name, module_name, rel_filename) = filename.split('/', 2)
            if self.isModuleIncluded(branch_name, module_name):
                # Only process files for modules that are included
                if module_name != current_module_name:
                    if current_module_name:
                        self.progressMsg("module %s/%s: %d changes" % (current_branch_name, current_module_name, module_changes[current_module_name]))

                    current_branch_name = branch_name
                    current_module_name = module_name
                    module_node = self.getModuleNode(branch_name, module_name)
                    if branch_name not in module_names:
                        module_names[branch_name] = []
                    module_names[branch_name].append(module_name)
                    if module_name not in module_changes:
                        module_changes[module_name] = 0

                if self.syncFile(group_node=module_node,
                                 filename=rel_filename,
                                 relative_path=filename):
                    module_changes[module_name] += 1

        if current_module_name:
            self.progressMsg("module %s/%s: %d changes" % (current_branch_name, current_module_name, module_changes[current_module_name]))

        for branch_name in module_names.keys():
            # remove all files that have not been marked as seen
            modules = self.getModulesNode(branch_name)
            for module_node in self.getChildNodes(modules, "Filter"):
                module_name = module_node.getAttribute("Name")
                if module_name not in module_changes:
                    module_changes[module_name] = 0
                if module_name not in module_names[branch_name]:
                    # remove a module, if it had no source-file
                    self.progressMsg("remove module %s/%s" % (branch_name, module_name))
                    module_node.parentNode.removeChild(module_node)
                    module_changes[module_name] += 1

                else:
                    # remove all files in that module that have not been
                    # seen:
                    cleaning_changes = 0
                    for file_node in module_node.getElementsByTagName("File"):
                        if self.isUnmarkedFileNode(file_node):
                            filter_node = file_node.parentNode
                            if (file_node.nextSibling and
                                file_node.nextSibling.nodeName == "#text" and
                                file_node.nextSibling.nodeValue.strip() == ""):
                                # remove empty text node
                                filter_node.removeChild(file_node.nextSibling)
                            elif (file_node.previousSibling and
                                  file_node.previousSibling.nodeName == "#text" and
                                  file_node.previousSibling.nodeValue.strip() == ""):
                                filter_node.removeChild(file_node.previousSibling)
                            # remove unmarked file node
                            filter_node.removeChild(file_node)
                            cleaning_changes += 1

                        else: # remove mark, so it is not written to disk
                            self.unmarkFileNode(file_node)

                    if module_changes[module_name] > 0:
                        # Todo: sorting of the node is usually only
                        # required if some files or filters were added
                        # to the module, but not if files or filters
                        # were removed. Here we execute the sorting
                        # also if some of the files changed some
                        # attributes (like pch), this can be optimized.
                        # Note: we don't sort if a file was added
                        # manually with correct attributes, but not in
                        # the correct order...
                        self.progressMsg("sorting module %s/%s" % (branch_name, module_name))
                        if self.sortNodeChildren(module_node):
                            module_changes[module_name] += 1
                    module_changes[module_name] += cleaning_changes
                    self.progressMsg("cleaned module %s/%s: %d changes" % (branch_name, module_name, cleaning_changes))

        changes = reduce(lambda x,y:x+y, module_changes.values(), 0)
        for branch_name in ["adjunct", "modules", "platforms"]:
            xml_node = self.getModulesNode(branch_name)
            if xml_node and self.sortNodeChildren(xml_node):
                changes += 1
        self.progressMsg("found %d changes" % changes)
        return changes

    def projRelativePath(self, relative_path):
        """
        This method converts the relative_path from the entries of any
        of the modules/hardcore/setup/*/sources/sources.* files (which
        is specified as path relative to the source-root) to a path
        relative to the project file. In addition the path-separator
        slash ('/') as specified in relative_path is replaced by a
        backslash ('\\') as used for the "RelativePath" attribute in
        the project file.
        """
        return self.__relpath_projectfile_sourceRoot + '\\' + relative_path.replace('/', '\\')

    def syncFile(self, group_node, filename, relative_path):
        """
        @param group_node is the xml-node of the vcproj tree which is
          expected to have the specified filename as one of its
          children or grandchildren.
        @param filename is the path of the file (where a slash '/' is
          used as a path-separator) relative to the specified
          group_node.
        @param relative_path is the path of the file (where a slash
          '/' is used as a path-separator) relative to the source-root
          (as specified in the constructor). This value is used for
          the "RelativePath" attribute in the "File"'s xml node.
        @return True if any of the xml-nodes have changed; False if
          nothing has changed.
        """
        # Note: the path in module.sources is separated by a slash '/':
        parts = filename.split('/')
        if len(parts) > 1:
            group = parts[0]
            if group != "src": group_node = self.getFilterNode(group_node, group)
            return self.syncFile(group_node=group_node,
                                 filename='/'.join(parts[1:]),
                                 relative_path=relative_path)

        # Note: we have to use a backslash in the filenames in the
        # solution file and not a slash:
        relative_path_proj = self.projRelativePath(relative_path)
        file_node = self.getFirstChildNodeWithAttr(group_node, "File", "RelativePath", relative_path_proj)
        if file_node is not None:
            return self.syncConfigurationsForFileNode(file_node, relative_path)

        # There was no node with the specified name, so create a new node:
        file_node = group_node.ownerDocument.createElement("File")
        file_node.setAttribute("RelativePath", relative_path_proj)
        # Note: each File node is expected to have an empty text node,
        # so it is created as "<File ...>\n</File>" instead of "<File.../>"
        file_node.appendChild(group_node.ownerDocument.createTextNode(""))
        group_node.appendChild(file_node)
        self.syncConfigurationsForFileNode(file_node, relative_path)
        return True

    def markAsSeen(self, node):
        """
        Marks the specified node as "seen". Thus we can remove node
        which no longer are referenced from module.sources.
        @param node is the xml-node to mark
        """
        node.setAttribute("SeenInModuleSources", "Oui")

    def unmarkFileNode(self, node):
        self.removeNodeAttributes(node, ["SeenInModuleSources"])

    def isUnmarkedFileNode(self, node):
        """
        This function checks if a node can/should be removed. If it is
        not marked (and is a c/cpp file) then it should be removed.

        @return True if it was not marked as "seen" (by calling
          markAsSeen()) and False it is marked as "seen".
        """
        if node.nodeName == "File":
            path = node.getAttribute("RelativePath")
            if path.endswith(".cpp") or path.endswith(".c"):
                if node.getAttribute("SeenInModuleSources") != "Oui":
                    return True
        return False

    def syncConfigurationsForFileNode(self, file_node, relative_path):
        """
        Updates all FileConfiguration sections of the specified file_node.
        @param file_node is the xml-node "File" which is associated to
          the specified relative_path.
        @param relative_path is the path of the file relative to the
          source root.

        @return True if any of the FileConfiguration nodes has been
          changed. False if there were no changes.
        """
        self.markAsSeen(file_node)
        changed = False
        for config_name in self.getConfigurations():
            if self.syncConfigurationForFileNode(file_node, config_name, relative_path):
                changed = True
        return changed

    def syncConfigurationForFileNode(self, file_node, config_name, relative_path):
        """
        Updates the FileConfiguration section with the specified
        config_name of the specified file_node.
        @param file_node is the xml-node "File" which is associated to
          the specified relative_path.
        @param config_name is the name of "FileConfiguration" node to
          update.
        @param relative_path is the path of the file relative to the
          source root.

        @return True if the FileConfiguration nodes has been
          changed. False if there were no changes.
        """
        def updateAttribute(node, attribute, value):
            if node.getAttribute(attribute) != value:
                node.setAttribute(attribute, value)
                return True
            else: return False

        if self.jumbo_compile():
            sources_config = self.__sources['jumbo']
        else: sources_config = self.__sources['plain']

        if relative_path in sources_config['pch']:
            pch = "core/pch.h"
            exclude_from_build = False
        elif relative_path in sources_config['nopch']:
            pch = None
            exclude_from_build = False
        elif relative_path in sources_config['pch_jumbo']:
            pch = "core/pch_jumbo.h"
            exclude_from_build = False
        elif relative_path in sources_config['pch_system_includes']:
            pch = "core/pch_system_includes.h"
            exclude_from_build = False
        else:
            # All other sources are excluded from the build:
            pch = "core/pch.h"
            exclude_from_build = True

        config_node = self.getFirstChildNodeWithNameAttr(file_node, "FileConfiguration", config_name)
        if config_node:
            tool_node = self.getFirstChildNodeWithNameAttr(config_node, "Tool", "VCCLCompilerTool")
        else: tool_node = None
        create_pch = False
        if (tool_node and
            tool_node.hasAttribute("UsePrecompiledHeader") and
            tool_node.getAttribute("UsePrecompiledHeader") == "1"):
            # this file is used to create a pch file
            create_pch = True
            if tool_node.hasAttribute("PrecompiledHeaderThrough"):
                pch = tool_node.getAttribute("PrecompiledHeaderThrough")

        if not exclude_from_build and pch == "core/pch.h":
            # The file should be compiled with pre-compiled header
            # file "core/pch.h", which is the default
            # configuration, so if there was a "FileConfiguration"
            # node, it should be removed (unless this is the entry to
            # generate the precompiled header file):

            if config_node and not create_pch:
                file_node.removeChild(config_node)
                return True
            else: # otherwise nothing has changed:
                return False

        # Either exclude_from_build is true, or the pch is not "core/pch.h"
        changed = False
        # create a new "FileConfiguration" node if there is none,
        # because we either have to set "ExcludedFromBuild" or we have
        # to set the pre-compiled header option:
        if config_node is None:
            config_node = file_node.ownerDocument.createElement("FileConfiguration")
            config_node.setAttribute("Name", config_name)
            file_node.appendChild(config_node)
            changed = True

        # set or remove the attribute "ExcludedFromBuild"
        if exclude_from_build:
            changed = updateAttribute(config_node, "ExcludedFromBuild", "true") or changed
        else:
            # instead of specifying "ExcludedFromBuild=false", the
            # attribute is removed:
            changed = self.removeNodeAttributes(config_node, ["ExcludedFromBuild"]) or changed

        if create_pch:
            # if this file_node is used to generate the precompiled
            # header file, we don't update the precompiled header
            # configuration
            return changed

        def createCompilerToolNode(config_node):
            tool_node = config_node.ownerDocument.createElement("Tool")
            tool_node.setAttribute("Name", "VCCLCompilerTool")
            config_node.appendChild(tool_node)
            return tool_node

        # set or remove the attribute "UsePrecompiledHeader"
        if pch is None:
            # don't use a pre-compiled header
            if tool_node is None:
                tool_node = createCompilerToolNode(config_node)
                changed = True
            changed = updateAttribute(tool_node, "UsePrecompiledHeader", "0") or changed
            changed = self.removeNodeAttributes(
                tool_node, ["PrecompiledHeaderThrough",
                            "PrecompiledHeaderFile"]) or changed

        elif pch == "core/pch.h":
            # use default setting for pre-compiled header:
            if tool_node is not None:
                changed = self.removeNodeAttributes(
                    tool_node, ["UsePrecompiledHeader",
                                "PrecompiledHeaderThrough",
                                "PrecompiledHeaderFile"]) or changed
                if (len(tool_node.attributes) <= 1 and
                    not tool_node.hasChildNodes()):
                    # an empty tool_node (it has at least the Name
                    # attribute) can be removed:
                    config_node.removeChild(tool_node)
                    changed = True

        elif pch in ("core/pch_jumbo.h", "core/pch_system_includes.h"):
            if tool_node is None:
                tool_node = createCompilerToolNode(config_node)
                changed = True
            # use the specified pre-compiled header
            pch_file = self.getPchFile(pch)
            if pch_file is None:
                # no PrecompiledHeaderFile was configured, so disable
                # precompiled headers
                changed = updateAttribute(tool_node, "UsePrecompiledHeader", "0") or changed
                changed = self.removeNodeAttributes(
                    tool_node, ["PrecompiledHeaderThrough",
                                "PrecompiledHeaderFile"]) or changed
            else:
                # use the configured precompiled header
                changed = self.removeNodeAttributes(tool_node, ["UsePrecompiledHeader"]) or changed
                changed = updateAttribute(tool_node, "PrecompiledHeaderThrough", pch) or changed
                changed = updateAttribute(tool_node, "PrecompiledHeaderFile", pch_file) or changed

        return changed

if __name__ == "__main__":
    total_time = Timing()
    parsing_time = Timing()
    updating_time = Timing()
    writing_time = Timing()
    total_time.start()

    option_parser = optparse.OptionParser(
        usage="Usage: %prog [options] [project_file]",
        description="".join(
            ["Updates the sources in the Visual Studio 2005 or 2008 project ",
             "file from the modules/hardcore/setup/{jumbo,plain}/sources/",
             "sources.* files which are generated by modules/hardcore/scripts",
             "/operasetup.py.",
             "\n",
             "[project_file] should be a Visual Studio 2005 or 2008 project ",
             "file. If no [project_file] is specified, the file ",
             "'wingogi.vcproj' is used."]))

    option_parser.add_option(
        "--template", metavar="template_file",
        help=" ".join(
            ["generate the project-file from the specifed template filename.",
             "Instead of parsing the project file, updating the xml structure",
             "and writing the output, a simple template file can be used. The",
             "template file needs to contain only the configuration, and the",
             "source-files that are needed to generate the precompiled header",
             "files. Using a template speeds up the parsing and helps to",
             "silence git when the project file has changed."]))

    option_parser.add_option(
        "--exclude_files_pattern",
        help=" ".join(
            ["removes the source files that matches the given pattern.",
             "Usually all sources files of all modules are included in the",
             "project file. By using this option some file types can be",
             "excluded. E.g. wingogi builds call this script with",
             "arguments like: '--exclude_files_pattern *.S'"]))

    option_parser.add_option(
        "--exclude_module", metavar="branch/module", action="append",
        help=" ".join(
            ["removes the source files of the specified module from the",
             "project file. Usually all sources files of all modules are",
             "included in the project file. By using this option some modules",
             "can be excluded. E.g. wingogi builds call this script with",
             "arguments like: '--exclude_module platforms/lingogi'"]))

    option_parser.add_option(
        "--include_module", metavar="branch/module", action="append",
        help=" ".join(
            ["includes only the source files of the specified modules in the",
             "project file. Usually all sources files of all modules are",
             "included in the project file. By using this option all but the",
             "specified modules are excluded."]))

    option_parser.add_option(
        "--force_update", action="store_true",
        help=" ".join(
            ["to skip the dependency check. Usually the project file is only",
             "parsed and updated if any of the source.* files that are created",
             "by operasetup.py are newer than the project file. This saves a",
             "lot of time on parsing the project file. Note: even if",
             "force_update is specified the project file might not be written,",
             "because its content might not have changed."]))

    option_parser.add_option(
        "--jumbo", action="store_true",
        help=" ".join(
            ["to enable compiling with jumbo compile units. All module source",
             "files which are included in a jumbo compile unit are kept in the",
             "project file, but they are excluded from the build process. To",
             "remove the excluded files, use the argument",
             "'--remove_excluded_files'.",
             "\n"
             "Note: the jumbo-compile-units are generated by operasetup.py.",
             "This script only enables or disables the jumbo compile units.",
             "To disable jumbo compilation, don't use the --jumbo argument."]))

    option_parser.add_option(
        "--sources_filter", metavar="NAME", default="Source Files",
        help=" ".join(
            ["add the source files to the filter with the specified name. The",
             "default filter name is '%default'. If an empty string is",
             "specifed, the source files are added directly to project;",
             "eg: --sources_filter="]))

    option_parser.add_option(
        "--remove_excluded_files", action="store_true",
        help=" ".join(
            ["to remove all files from the project file, that are excluded",
             "from the build. By default both the jumbo compile units and the",
             "plain files are added to the project file and the files that",
             "are not used are marked with the attribute 'ExcludedFromBuild'.",
             "This allows to use a jumbo compilation and still keep fast",
             "access to all source files, but the project file will grow very",
             "big and updating it takes much more time. With this option, only",
             "the files that are required for the build are added to the",
             "project file, i.e. for a jumbo-build only the jumbo compile",
             "units are added to the project file and for a plain-build only",
             "the plain sources are added to the project file. This keeps the",
             "project file small and thus it can also be updated fast."]))

    option_parser.add_option(
        "--keep_excluded_files", action="store_false", dest="remove_excluded_files",
        help=" ".join(
            ["is the opposite to option '--remove_excluded_files'. Both the",
             "jumbo compile units and the plain files are added to the project",
             "file and the files that are not used are marked with the",
             "attribute 'ExcludedFromBuild'. This is the default setting."]))

    option_parser.add_option(
        "--pch_jumbo", metavar="FILENAME",
        help=" ".join(
            ["specifies the pch file which is the precompiled header file that",
             "was generated for the '#include \"core/pch_jumbo.h\"' statement.",
             "If this argument is not specified, this script parses the",
             "specifed project file for an entry with configuration",
             "'UsePrecompiledHeader=1' and",
             "'PrecompiledHeaderThrough=core/pch_jumbo.h', i.e. create a",
             "precompiled header file for \"core/pch_jumbo.h\". The filename",
             "is used for the 'PrecompiledHeaderFile' attribute in the project",
             "file."]))

    option_parser.add_option(
        "--no-error_on_update", action="store_false", dest="error_on_update",
        help=" ".join(
            ["Don't exit with an error-code when the project file was",
             "updated. The default is to exit with an error code; thus",
             "Visual Studio stops compilation and has time to reload the",
             "project file before compilation continues."]))

    option_parser.add_option("--quiet", action="store_true", dest="quiet")
    option_parser.add_option(
        "--no-quiet", action="store_false", dest="quiet",
        help=" ".join(["Print some debug messages about the progress."]))

    option_parser.set_defaults(remove_excluded_files=False,
                               jumbo=False,
                               sources_filter="Source Files",
                               force_update=False,
                               template=None,
                               pch_jumbo=None,
                               include_module=[],
                               exclude_module=[],
                               exclude_files_pattern="",
                               error_on_update=True,
                               quiet = True)
    (options, args) = option_parser.parse_args()
    if args is None or len(args) == 0:
        project_file = "wingogi.vcproj"
    elif len(args) == 1:
        project_file = args[0]
    else:
        error("Only one project file expected as argument, not %r" % args)

    if options.template is None:
        project_file_input = project_file
        project_file_output = project_file
    else:
        project_file_input = options.template
        project_file_output = project_file

    if not os.path.exists(project_file_input) and os.path.exists(os.path.join(sourceRoot, project_file_input)):
        project_file_input = os.path.join(sourceRoot, project_file_input)

    if not os.path.exists(project_file_input):
        error("The project file '%s' does not exist" % project_file_input)

    module_set = { 'include' : options.include_module,
                   'exclude' : options.exclude_module }
    module_set_by_branch = { 'include' : {},
                             'exclude' : {} }
    try:
        for type in ('include', 'exclude'):
            for module in module_set[type]:
                (branch, module_name) = module.replace('\\','/').split('/', 1)
                if branch not in module_set_by_branch[type]:
                    module_set_by_branch[type][branch] = []
                module_set_by_branch[type][branch].append(module_name)
    except ValueError:
        error("expected 'branchname/modulename' following --include_module")

    def needToUpdateProjectFile(force_update, project_file_input, project_file_output, sourceRoot, quiet):
        if force_update: return True
        if not os.path.exists(project_file_output): return True
        project_file_mtime = os.stat(project_file)[stat.ST_MTIME]
        projectfile_depends_on = [sys.argv[0]] # this script
        if project_file_input != project_file_output:
            # if input and output are not the same, then the output file depends
            # on the template file:
            projectfile_depends_on.append(project_file_input)
        for type in ('plain', 'jumbo'): # all sources.XXX files
            for name in ('all', 'nopch', 'pch', 'pch_jumbo', 'pch_system_includes'):
                projectfile_depends_on.append(os.path.join(sourceRoot, 'modules', 'hardcore', 'setup', type, 'sources', 'sources.%s' % name))
        for filename in projectfile_depends_on:
            if project_file_mtime < os.stat(filename)[stat.ST_MTIME]:
                # there is a file which is newer than the project file
                if not quiet: print >>sys.stdout, "file '%s' is newer than the project file, need to update the project file" % filename
                return True
        # none of the dependencies are newer than the project file:
        return False

    project_file_changed = False
    if needToUpdateProjectFile(options.force_update, project_file_input, project_file_output, sourceRoot, options.quiet):
        vcproj_updater = VCProjectUpdater(
            sourceRoot=sourceRoot,
            jumbo_compile=options.jumbo,
            remove_excluded_files=options.remove_excluded_files,
            quiet=options.quiet,
            sources_filter=options.sources_filter,
            exclude_files_pattern=options.exclude_files_pattern,
            output=project_file_output)
        for branch_name in module_set_by_branch['include']:
            vcproj_updater.setIncludeModules(branch_name, module_set_by_branch['include'][branch_name])
        for branch_name in module_set_by_branch['exclude']:
            vcproj_updater.setExcludeModules(branch_name, module_set_by_branch['exclude'][branch_name])

        parsing_time.start()
        vcproj_updater.parseVCProjectFile(project_file_input)
        parsing_time.stop()
        updating_time.start()
        if options.pch_jumbo is not None:
            vcproj_updater.setPchFile("core/pch_jumbo.h", options.pch_jumbo)
        if vcproj_updater.filesHaveChanged() or options.template:
            updating_time.stop()
            writing_time.start()
            if vcproj_updater.updateVCProjFile(project_file_output):
                project_file_changed = True
            else:
                print "The file '%s' was already up to date" % project_file_output

        print "Time: %s total (%s parsing project file, %s updating xml, %s writing project file)." % (total_time.timestr(), parsing_time.timestr(), updating_time.timestr(), writing_time.timestr())

        if project_file_changed:
            print "ALERT: a new version of %s has been created and must be reloaded into Visual Studio" % project_file_output
            if options.error_on_update: sys.exit(1)

    else:
        print "The project file '%s' is not updated, because it is" % project_file_output
        print "newer than the dependencies. If you want to force an update (e.g."
        print "because you specified different commandline arguments than last"
        print "time), you should call the script with argument '--force_update'."
