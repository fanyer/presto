#!/usr/bin/env python

import sys
import os
import re
import hashlib
import tempfile
import re

if len(sys.argv) < 3:
	print "Usage:", sys.argv[0], "rootdir skeletonfile"
	sys.exit(0)

rootdir = sys.argv[1]
skeleton = sys.argv[2]
use_jumbo = int(sys.argv[3])
print "USE_JUMBO_COMPILE: ", use_jumbo

def checkexists(path):
	if not os.path.exists(path):
		print "Error:", path, "does not exist"
		sys.exit(1)

checkexists(rootdir)
checkexists(skeleton)

toplevelgroups = ["adjunct", "modules", "platforms"]
avoidgroups = ["CVS", "build", "fontix", "paxage", "plugix", "quix", "ubscommon", "ubs", "unix", "unix_common", "utilix", "viewix", "windows", "windows_common", "component_opera_autoupdatechecker"]
filetypes = [".c", ".cpp", ".h", ".ot", ".m", ".mm", ".xcconfig"]

appfiles = ["QuickOperaAppExtraSymbols.cpp",
	    "QuickOperaAppExtraSymbols.mm",
	    "OperaWidgetAppEvents.cpp",
	    "QuickCommandConverter.cpp",
	    "QuickWidgetApp.cpp",
	    "QuickWidgetLibHandler.cpp",
	    "QuickWidgetManager.cpp",
	    "QuickWidgetMenu.cpp",
	    "systemcapabilities.cpp",
	    "UMenuIDManager.cpp",
	    "string_convert2.cpp",
	    "CocoaApplication.mm",
	    "OperaExternalNSMenu.mm",
	    "CocoaQuickWidgetMenu.mm",
	    "crashlog.cpp",
	    "OperaExternalNSMenuItem.mm"]

widgetapp_files = ["emBrowserTestSuite.cpp",
		"OperaControl.cpp",
		"WidgetApp.cpp",
		"WidgetAppEvents.cpp"]

appfiles = ["QuickOperaAppExtraSymbols.cpp",
	    "QuickOperaAppExtraSymbols.mm",
	    "OperaWidgetAppEvents.cpp",
	    "QuickCommandConverter.cpp",
	    "QuickWidgetApp.cpp",
	    "QuickWidgetLibHandler.cpp",
	    "QuickWidgetManager.cpp",
	    "QuickWidgetMenu.cpp",
	    "UMenuIDManager.cpp",
	    "string_convert2.cpp",
	    "CocoaApplication.mm",
	    "OperaExternalNSMenu.mm",
	    "CocoaQuickWidgetMenu.mm",
	    "crashlog.cpp",
	    "OperaExternalNSMenuItem.mm"]

widgetapp_files = ["emBrowserTestSuite.cpp",
		"OperaControl.cpp",
		"WidgetApp.cpp",
		"WidgetAppEvents.cpp"]

currenttoplevel = ""
projectfiles = []
applicationfiles = []
applicationfilenames = []
buildfiles = []
buildfilenames = []
pluginwrapperfiles = []
pluginwrapperfilenames = []
configfiles = []
groups = {}

modulesources = tempfile.TemporaryFile()

class ProjectItem:
	def createref(self, tohash):
		self.ref = hashlib.sha256(tohash).hexdigest().upper()

	def fileref(self):
		return self.ref[0:24]

	def buildref(self, offset):
		offset = offset + 1
		return self.ref[-24-offset:-offset]


class ProjectFile(ProjectItem):

	def __init__(self, dirname, filename):
		self.build = False
		self.filename = filename
		self.dirname = dirname[len(rootdir):]
		self.createref(dirname + filename)

class ProjectGroup(ProjectItem):

	def __init__(self, dirname):
		self.children = {}
		self.included = False
		self.parent = None
		self.filename = dirname
		self.createref(dirname)

	def include(self):
		group = self
		while (group):
			group.included = True
			group = group.parent

def avoidgroup(dirname):

	if currenttoplevel.startswith("platforms"):
		for group in avoidgroups:
			if os.path.isdir(dirname) and dirname.endswith(group):
				return True
			if dirname.find("/" + group + "/") != -1:
				return True

	return False

def writePBXBuildFile(projectfile):
	for buildfile in buildfiles:
		projectfile.write("\t\t" + buildfile.buildref(0) + " /* " + buildfile.filename + " in Sources */ = {isa = PBXBuildFile; fileRef = " + buildfile.fileref() + " /* " + buildfile.filename + " */; };\n")
	for buildfile in applicationfiles:
		projectfile.write("\t\t" + buildfile.buildref(1) + " /* " + buildfile.filename + " in Sources */ = {isa = PBXBuildFile; fileRef = " + buildfile.fileref() + " /* " + buildfile.filename + " */; };\n")
	for buildfile in pluginwrapperfiles:	# 32-bit file references
		projectfile.write("\t\t" + buildfile.buildref(2) + " /* " + buildfile.filename + " in Sources */ = {isa = PBXBuildFile; fileRef = " + buildfile.fileref() + " /* " + buildfile.filename + " */; };\n")
	for buildfile in pluginwrapperfiles:	# 64-bit file references
		projectfile.write("\t\t" + buildfile.buildref(3) + " /* " + buildfile.filename + " in Sources */ = {isa = PBXBuildFile; fileRef = " + buildfile.fileref() + " /* " + buildfile.filename + " */; };\n")

def writePBXFileReference(projectfile):
	for file in projectfiles:
		if file.filename.endswith(".h"):
			filetype = "sourcecode.c.h"
		elif file.filename.endswith(".c"):
			filetype = "sourcecode.c.c"
		elif file.filename.endswith(".cpp"):
			filetype = "sourcecode.cpp.cpp"
		elif file.filename.endswith(".mm"):
			filetype = "sourcecode.cpp.objcpp"
		elif file.filename.endswith(".m"):
			filetype = "sourcecode.c.objc"
		elif file.filename.endswith(".xcconfig"):
			filetype = "text.xcconfig"
		else:
			filetype = "text"

		projectfile.write("\t\t" + file.fileref() + " /* " + file.filename + " */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = " + filetype + "; name = " + file.filename + "; path = " + os.path.join(file.dirname, file.filename) + "; sourceTree = \"<group>\"; };\n")

		if file.filename.endswith(".xcconfig"):
			configfiles.append(file)

def writeSourceFiles(projectfile, bfiles, offset):
	for buildfile in bfiles:
		projectfile.write("\t\t\t\t" + buildfile.buildref(offset) + " /* " + buildfile.filename + " in Sources */,\n")

def writeHeaderFiles(projectfile):
	projectfile.write("")

def compareItems(a,b):
	if a.filename == b.filename:
		return 0
	elif a.filename > b.filename:
		return 1;
	else:
		return -1;

def writeTopLevelGroups(projectfile):
	for toplevelgroup in toplevelgroups:
		group = groups[os.path.join(rootdir, toplevelgroup)]
		projectfile.write("\t\t" + group.fileref() + " /* " + toplevelgroup + " */ = {\n")
		projectfile.write("\t\t\tisa = PBXGroup;\n")
		projectfile.write("\t\t\tchildren = (\n")
		children = group.children.values()
		children.sort(cmp=compareItems)
		for child in children:
			projectfile.write("\t\t\t\t" + child.fileref() +" /* " + os.path.basename(child.filename) + " */,\n")
		projectfile.write("\t\t\t);\n")
		projectfile.write("\t\t\tname = " + toplevelgroup + ";\n")
		projectfile.write("\t\t\tpath = ../..;\n")
		projectfile.write("\t\t\tsourceTree = \"<group>\";\n")
		projectfile.write("\t\t};\n")

def writeTopLevelReferences(projectfile):
	for toplevelgroup in toplevelgroups:
		# adjunct, modules, platforms
		group = groups[os.path.join(rootdir, toplevelgroup)]
		projectfile.write("\t\t\t\t"+ group.fileref() + " /* " + os.path.basename(group.filename) + " */,\n")

def writeGroups(projectfile):
	for group in groups.values():
		name = os.path.basename(group.filename)
		if name in toplevelgroups:
			continue

		parentname = os.path.basename(group.parent.filename)
		path = os.path.join(group.parent.filename, name)
		path = path[len(rootdir):]

		projectfile.write("\t\t" + group.fileref() + " /* " + name + " */ = {\n")
		projectfile.write("\t\t\tisa = PBXGroup;\n")
		projectfile.write("\t\t\tchildren = (\n")
		grs = group.children.values()
		grs.sort(cmp=compareItems)
		for child in grs:
			projectfile.write("\t\t\t\t" + child.fileref() +" /* " + os.path.basename(child.filename) + " */,\n")
		projectfile.write("\t\t\t);\n")
		projectfile.write("\t\t\tname = " + name + ";\n")
		projectfile.write("\t\t\tsourceTree = \"<group>\";\n")
		projectfile.write("\t\t};\n")

def haschanged(projectfile):
	modulesources.seek(0)
	newhash = hashlib.sha1()
	newhash.update(modulesources.read())
	if os.path.isfile(rootdir + "/platforms/mac/Opera.xcodeproj/module.sources"):
		oldprojectfile = open(rootdir + "/platforms/mac/Opera.xcodeproj/module.sources")
		oldhash = hashlib.sha1()
		oldhash.update(oldprojectfile.read())
		oldprojectfile.close()

		return newhash.hexdigest() != oldhash.hexdigest()
	else:
		return True;

def copyprojectfile(projectfile):
	newprojectfile = open(rootdir + "/platforms/mac/Opera.xcodeproj/project.pbxproj", 'w')
	newmodulesources = open(rootdir + "/platforms/mac/Opera.xcodeproj/module.sources", 'w')
	projectfile.seek(0)
	for line in projectfile:
		newprojectfile.write(line)
	newprojectfile.close()

	modulesources.seek(0)
	for line in modulesources:
		newmodulesources.write(line)
	newmodulesources.close()

def writeConfigFiles(projectfile):
	for config in configfiles:
		debug   = ProjectItem()
		debug.createref("Debug" + config.filename)
		release = ProjectItem()
		release.createref("Release" + config.filename)

		if not config.filename.endswith("Release.xcconfig"):
			projectfile.write("\t\t" + debug.fileref() + " /* Debug */ = {\n")
			projectfile.write("\t\t\tisa = XCBuildConfiguration;\n")
			projectfile.write("\t\t\tbaseConfigurationReference = " + config.fileref() + " /* " + config.filename + " */;\n")
			projectfile.write("\t\t\tbuildSettings = {\n")

			if config.filename.startswith("OperaFramework.xcconfig"):
				projectfile.write("\t\t\t\tFRAMEWORK_SEARCH_PATHS = (\n\t\t\t\t\t\"$(inherited)\",\n\t\t\t\t\t\"$(FRAMEWORK_SEARCH_PATHS_QUOTED_FOR_TARGET_1)\",\n\t\t\t\t\t\"$(FRAMEWORK_SEARCH_PATHS_QUOTED_FOR_TARGET_2)\",\n")
				projectfile.write("\t\t\t\t);\n\t\t\t\tFRAMEWORK_SEARCH_PATHS_QUOTED_FOR_TARGET_1 = \"\\\"$(SRCROOT)/notifications\\\"\";\n\t\t\t\tFRAMEWORK_SEARCH_PATHS_QUOTED_FOR_TARGET_2 = \"\\\"$(SRCROOT)/notifications\\\"\";\n")

			projectfile.write("\t\t\t};\n\t\t\tname = Debug;\n\t\t};\n\n")

		if not config.filename.endswith("Debug.xcconfig"):
			projectfile.write("\t\t" + release.fileref() + " /* Release */ = {\n")
			projectfile.write("\t\t\tisa = XCBuildConfiguration;\n")
			projectfile.write("\t\t\tbaseConfigurationReference = " + config.fileref() + " /* " + config.filename + " */;\n")
			projectfile.write("\t\t\tbuildSettings = {\n")

			if config.filename.startswith("OperaFramework.xcconfig"):
				projectfile.write("\t\t\t\tFRAMEWORK_SEARCH_PATHS = (\n\t\t\t\t\t\"$(inherited)\",\n\t\t\t\t\t\"$(FRAMEWORK_SEARCH_PATHS_QUOTED_FOR_TARGET_1)\",\n\t\t\t\t\t\"$(FRAMEWORK_SEARCH_PATHS_QUOTED_FOR_TARGET_2)\",\n")
				projectfile.write("\t\t\t\t);\n\t\t\t\tFRAMEWORK_SEARCH_PATHS_QUOTED_FOR_TARGET_1 = \"\\\"$(SRCROOT)/notifications\\\"\";\n\t\t\t\tFRAMEWORK_SEARCH_PATHS_QUOTED_FOR_TARGET_2 = \"\\\"$(SRCROOT)/notifications\\\"\";\n")

			projectfile.write("\t\t\t};\n\t\t\tname = Release;\n\t\t};\n\n")


def loadsourcesfile(root_dir, type, name):
	"""
	Loads the contents of the sourceslist with the specified name.
	@param type can be 'plain' or 'jumbo'
	@param name can be one of 'all', 'nopch', 'pch', 'pch_jumbo' or
	'pch_system_includes'.
	@return a list of source-files.
	"""
	filename = os.path.join(root_dir, 'modules', 'hardcore', 'setup', type, 'sources', 'sources.%s' % name)
	sources = []
	f = None
	try:
		f = open(filename)
		for source in f:
			source = source.strip()
			sources.append(source)
	finally:
		if f: f.close()

	return sources


def addFile(root_dir, file_path, pch_name, compile_file):
	"""Create ProjectGroup for each unique directory"""
	last_group = None
	dirs = file_path.split('/')
	file_name = dirs.pop()
	for i in range(len(dirs)):
		dir = '/'.join(dirs[0:i+1])
		root = os.path.join(root_dir, dir)

		if not root in groups:
			group = ProjectGroup(root)
			if last_group:
				group.parent = last_group;
				last_group.children[os.path.basename(root)] = group

			groups[root] = group
			last_group = group
		else:
			last_group = groups[root]

	full_path = os.path.dirname(os.path.join(rootdir, file_path))
	projectfile = ProjectFile(full_path, file_name)
	projectfiles.append(projectfile)

	'''Add the files to the correct target'''
	last_group.children[file_name] = projectfile
	modulesources.write(os.path.join(rootdir, file_path) + "\n")

	'''Add the header files for the cpp/mm files added to make the indexer in Xcode 4.1 better'''
	filename = os.path.join(full_path, file_name)
	if filename.endswith(".cpp") or filename.endswith(".c") or filename.endswith(".mm"):
		filename = re.sub("[^.]*$", "h", filename)
		if not os.path.exists(filename):
			filename = re.sub("[^\/]*\/([^\/.]*)\.[^.]*$", r"\g<1>.h", filename);
		if os.path.exists(filename):
			projectheaderfile = ProjectFile(os.path.dirname(filename), os.path.basename(filename))
			projectfiles.append(projectheaderfile)
			last_group.children[os.path.basename(filename)] = projectheaderfile
			modulesources.write(filename + "\n")

	if not compile_file:
		return

	if pch_name == "component_macquickoperaapp":
		applicationfiles.append(projectfile)
	elif pch_name == "component_framework":
		pluginwrapperfiles.append(projectfile)
		buildfiles.append(projectfile)
	elif pch_name == "component_plugin":
		pluginwrapperfiles.append(projectfile)
	elif not file_path.endswith(".xcconfig"):
		buildfiles.append(projectfile)

def addallfiles(rootDir):
	# the set of all sources is created by operasetup script
	sources = {
		'plain' : { 'all' : None,
					'component_framework' : None,
					'component_plugin' : None,
					'component_macquickoperaapp' : None,
					'nopch' : None,
					'pch' : None,
					'pch_jumbo' : None,
					'pch_system_includes' : None,
					'component_opera_autoupdatechecker' : None },
		'jumbo' : { 'all' : None,
					'component_framework' : None,
					'component_plugin' : None,
					'component_macquickoperaapp' : None,
					'nopch' : None,
					'pch' : None,
					'pch_jumbo' : None,
					'pch_system_includes' : None,
					'component_opera_autoupdatechecker' : None }
		}

	for type in ('plain', 'jumbo'):
		for configuration in ('all', 'component_framework', 'component_plugin', 'component_macquickoperaapp', 'nopch', 'pch', 'pch_jumbo', 'pch_system_includes', 'component_opera_autoupdatechecker'):
			sources[type][configuration] = loadsourcesfile(rootDir, type, configuration)

	# sources_all = sorted(set(sources['plain']['all']) | set(sources['jumbo']['all']))
	if use_jumbo == 1:
		sources_all = sorted(set(sources['jumbo']['all']))
		sources_config = sources['jumbo']
	else:
		sources_all = sorted(set(sources['plain']['all']))
		sources_config = sources['plain']
	for source in sources_all:
		if not source.split('/')[1] in avoidgroups:
			component_handled = False
			if source in sources_config['component_framework']:
				addFile(rootDir, source, "component_framework", True)
				component_handled = True
			if source in sources_config['component_plugin']:
				addFile(rootDir, source, "component_plugin", True)
				component_handled = True
			if source in sources_config['component_macquickoperaapp']:
				addFile(rootDir, source, "component_macquickoperaapp", True)
				component_handled = True
			if source in sources_config['component_opera_autoupdatechecker']:
				addFile(rootDir, source, "component_opera_autoupdatechecker", False)
				component_handled = True

			if component_handled:
				continue

			if source in sources_config['pch']:
				addFile(rootDir, source, "pch", True)
			elif source in sources_config['nopch']:
				addFile(rootDir, source, "", True)
			elif source in sources_config['pch_jumbo']:
				addFile(rootDir, source, "pch_jumbo", True)
			elif source in sources_config['pch_system_includes']:
				addFile(rootDir, source, "pch_system_includes", True)
			else:
				addFile(rootDir, source, "pch", False)

	if use_jumbo == 1:
		sources_all = sorted(set(sources['plain']['all']))
		sources_config = sources['plain']
		for source in sources_all:
			if not source.split('/')[1] in avoidgroups:
				addFile(rootDir, source, "pch", False)


def createprojectfile():
	skeletonfile = open(skeleton, 'r')
	projectfile = tempfile.TemporaryFile()

	for line in skeletonfile:
		if line.startswith("$PBXBuildFile"):
			writePBXBuildFile(projectfile)
		elif line.startswith("$PBXFileReference"):
			writePBXFileReference(projectfile)
		elif line.startswith("$SourceFiles"):
			writeSourceFiles(projectfile, buildfiles, 0)
		elif line.startswith("$AppSourceFiles"):
			writeSourceFiles(projectfile, applicationfiles, 1)
		elif line.startswith("$PluginWrapper32SourceFiles"):
			writeSourceFiles(projectfile, pluginwrapperfiles, 2)
		elif line.startswith("$PluginWrapper64SourceFiles"):
			writeSourceFiles(projectfile, pluginwrapperfiles, 3)
		elif line.startswith("$HeaderFiles"):
			writeHeaderFiles(projectfile)
		elif line.startswith("$TopLevelGroups"):
			writeTopLevelGroups(projectfile)
		elif line.startswith("$TopLevelReferences"):
			writeTopLevelReferences(projectfile)
		elif line.startswith("$ConfigFiles"):
			writeConfigFiles(projectfile)
		elif line.startswith("$Groups"):
			writeGroups(projectfile)
		else:
			projectfile.write(line)

	if haschanged(projectfile):
		copyprojectfile(projectfile)
		projectfile.close()
		print("Project changed")
		sys.exit(1)
	projectfile.close()
	modulesources.close()

addallfiles(rootdir)
createprojectfile()
sys.exit(0)
