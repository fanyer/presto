
"""
This module exposes properties that describe the modules comprising
the Opera source tree. The information is obtained by reading the
readme.txt files.

The module is automatically made available as the name otree in the
global namespaces in the context of which the flow files and
configuration files are interpreted.
"""

import sys
import os.path
import errno
import glob
import re

class ModuleSet(object):
    """
    A set of modules grouped by a top-level directory, of which there
    are currently four: modules, data, adjunct and platform.

    The modules property is a read-only dictionary mapping the short
    name of a module to a Module object.
    """

    modules = {}

    def __init__(self, name):
        """
        Constructor.

        Reads readme.txt and understands special comment syntax. If
        the comment on the same line as a module name contains a
        semicolon-separated list of platforms in square brackets, the
        module should only be used on the platform(s) listed. Valid
        platforms are mac, unix, and windows.

        @param name The name of the module set.
        """
        self.name = name
        if sys.platform.startswith('linux') or sys.platform.startswith('freebsd'):
            platform = 'unix'
        elif sys.platform == 'darwin':
            platform = 'mac'
        elif sys.platform == 'win32':
            platform = 'windows'
        else:
            assert False, 'Unknown platform'
        osListRE = re.compile(r'#.*\[([\w;]+)\]')
        with open(name + '/readme.txt') as f:
            for line in f:
                parts = line.split('#', 1)[0].split()
                if not parts:
                    continue
                modname = parts[0]
                match = osListRE.search(line)
                if match:
                    platforms = match.group(1).split(';')
                    for p in platforms:
                        assert p in ('unix', 'mac', 'windows'), "Module {m} targets unknown platform {p}".format(m=modname, p=platform)
                    if platform not in platforms:
                        continue
                self.modules[modname] = Module(self, modname)

class Module(object):
    """
    A module.

    The name property is the short name of the module (does not
    contain a slash). The fullname property is the module's full name
    (the module set name, a slash, and the module name).
    """

    def __init__(self, modset, name):
        """
        Constructor.

        @param modset The relevant ModuleSet object.

        @param name The short name of the module.
        """
        self.name = name
        self.fullname = modset.name + '/' + name

    def file(self, name):
        """
        Open a file in the module, if the file exists.

        @param name The name of the file relative to the module's
        top-level directory.

        @return Python file object or None if the file does not exist.
        """
        path = self.fullname + '/' + name
        try:
            return open(path)
        except EnvironmentError, e:
            if e.errno == errno.ENOENT:
                return None
            else:
                raise

    def glob(self, pattern):
        """
        Return a list of files in the module matching a glob pattern.

        @param pattern The glob pattern relative to the module's
        top-level directory.

        @return A list of matching file names relative to the module's
        top-level directory.
        """
        prefixLen = len(self.fullname) + 1
        return [f[prefixLen:] for f in glob.glob(self.fullname + '/' + pattern)]

modsets = {}
modules = {}
for i in ('modules', 'data', 'adjunct', 'platforms'):
    modsets[i] = ModuleSet(i)
    for j in modsets[i].modules:
        m = modsets[i].modules[j]
        modules[m.fullname] = m
