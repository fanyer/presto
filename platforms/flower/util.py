
"""
An assortment of useful functions and classes.

The module is automatically made available as the name util in the
global namespaces in the context of which the flow files and
configuration files are interpreted.
"""

import os
import sys
import errno
import fnmatch
import subprocess
import inspect
import re
import glob

import flow
import errors
import output

def ensureList(items):
    """
    Check whether items is a sequence other than a string, and if it
    isn't, wrap items in a single-element tuple, ensuring that the
    result is always a sequence. A sequence is any container that has
    a length and is indexable. If items is None, an empty tuple is
    returned.

    @param items A single value, a sequence or None.

    @return A sequence.
    """
    if items == None:
        return ()
    if not hasattr(items, '__len__') or isinstance(items, basestring):
        return (items,)
    else:
        return items

def touch(file):
    """
    Create an empty file with the specified name or update the last
    modification time of the existing file, imitating the UNIX touch
    command.

    @param file The name of the file to touch or a Node whose 'target'
    parameter names the file.
    """
    if isinstance(file, flow.Node):
        file = file['target']
    try:
        os.utime(file, None)
    except EnvironmentError, e:
        if e.errno != errno.ENOENT:
            raise
        open(file, 'w').close()

def hasSuffix(suffix):
    """
    Create a pattern matcher usable with the @flow decorator that
    checks whether the value has the specified suffix.

    @param suffix The suffix to check for.

    @return Pattern matcher (a function).
    """
    return lambda s: s.endswith(suffix)

def hasPrefix(prefix):
    """
    Create a pattern matcher usable with the @flow decorator that
    checks whether the value has the specified prefix.

    @param prefix The prefix to check for.

    @return Pattern matcher (a function).
    """
    return lambda s: s.startswith(prefix)

def removeSuffix(s, suffix):
    """
    Remove the specified suffix from the end of a string. The string
    must end with the suffix and be at least one character longer.
    Also, the character preceding the suffix must not be '/'.

    @param s The string to remove the suffix from.

    @param suffix The suffix to remove.

    @return The string with the suffix removed.
    """
    assert s.endswith(suffix)
    s = s[:-len(suffix)]
    assert s
    assert s[-1] != '/'
    return s

def needsUpdate(target, sources):
    """
    Check whether a target needs updating. This is the implementation
    behind the < operator with a Node on the left-hand side; refer to
    Node.__lt__ for details.

    @param target A node or a filename string.

    @param sources A node, a filename string, or a sequence of such
    items.

    @return True if the target needs updating.
    """
    def mtime(f):
        try:
            return os.stat(f['target'] if isinstance(f, flow.Node) else f).st_mtime
        except EnvironmentError:
            return None
    targetMtime = mtime(target)
    if targetMtime == None:
        return True
    else:
        sources = ensureList(sources)
        for source in sources:
            if isinstance(source, flow.Node) and 'changed' in source:
                if source['changed']:
                    return True
            else:
                sourceMtime = mtime(source)
                if sourceMtime == None or targetMtime < sourceMtime:
                    return True
        return False

def makeDirs(*args):
    """
    Any number of arguments can be specified. If an item is a string,
    treat it as a file path and create all its parent directories that
    do not yet exist, not including the final filename itself. If an
    item is a Node object, it must have a 'target' parameter, and that
    will be used as the file path.

    @param *args Any number of file names or Node objects.
    """
    for arg in args:
        path = os.path.dirname(arg['target'] if isinstance(arg, flow.Node) else arg)
        if path:
            try:
                os.makedirs(path)
            except EnvironmentError, e:
                if e.errno != errno.EEXIST:
                    raise

def listDirRecursive(path, predicate=None, include_dirs=True, include_files=True):
    """
    List all files in 'path' recursively.  The files are returned as a
    list of file names where each file name starts with 'path'.
    'path' itself is not part of the result.

    The result can be further filtered by supplying a predicate.  If
    'predicate' is None, no filtering is performed.

    If 'predicate' is a simple string, it is used as a glob pattern.
    This pattern is matched towards the basename of each file
    (i.e. the file name without any directories prepended).  All file
    names that matches the glob are included, the rest are discarded.

    If 'predicate' is a callable, it is called with the name that
    would be in the returned list.  So it will always start with
    'path'.  If 'predicate' returns a true value, the name is included
    in the result, otherwise it is discarded.

    @param path The directory to start listing files in.

    @param predicate Filters the list of file names.  If 'predicate'
    is None, all names are included.  Otherwise 'predicate' is either
    a string, which is used as a glob pattern, or a callable.  See the
    main function documentation for details.

    @param include_dirs If a true value, directories are included in
    the list of file names.

    @param include_dirs If a true value, ordinary files are included
    in the list of file names.
    """
    names = []
    glob = None
    if isinstance(predicate, basestring):
        glob = predicate
        predicate = None
    for root, curdirs, curfiles in os.walk(path):
        curnames = []
        if include_dirs:
            curnames += curdirs
        if include_files:
            curnames += curfiles
        for name in curnames:
            fullname = "{0}/{1}".format(root, name)
            if glob is not None:
                if fnmatch.fnmatch(name, glob):
                    names.append(fullname)
            elif predicate is not None:
                if predicate(fullname):
                    names.append(fullname)
    return names

def takesArgument(function, argument):
    """
    Check whether the callable takes a keyword argument with the
    specified name. If the callable has a takesArgument method, it is
    called with the argument name, and the result overrides the
    standard check.

    @param function A callable (function, method, class or object with
    __call__).

    @param argument Argument name.

    @return True if the callable takes a keyword argument with the
    specified name or accepts arbitrary keyword arguments through a
    **kwargs specification.
    """
    if hasattr(function, 'takesArgument'):
        return function.takesArgument(argument)
    elif inspect.isfunction(function):
        argspec = inspect.getargspec(function)
        if argspec.keywords:
            return True
        else:
            return argument in argspec.args
    elif inspect.ismethod(function):
        return takesArgument(function.im_func, argument)
    elif inspect.isclass(function):
        return takesArgument(function.__init__, argument)
    else:
        return takesArgument(function.__call__, argument)

class _DepParseError(Exception):
    "Parse error in a makefile snippet. Never escapes readDepFile."
    pass

# Matches makefile substitutions like $(wildcard ...) and $$
_depFileSubstRE = re.compile(r'\$(\([^)]*\)|[^(])')

# Matches makefile comments
_depFileCommentRE = re.compile(r'(?<!\\)#.*$')

def _depFileSubst(match):
    "Callback for re.sub. Processes one makefile substitution."
    text = match.group(1)
    if text == '$':
        return '$'
    if len(text) == 1:
        raise _DepParseError("unsupported substitution ${0}".format(text))
    assert text[0] == '(' and text[-1] == ')'
    if '$(' in text:
        raise _DepParseError("nested substitutions not supported: ${0}".format(text))
    words = text[1:-1].replace('$$', '$').split()
    if not words or words[0] != 'wildcard':
        raise _DepParseError("unsupported substitution $({0})".format(text))
    return ' '.join([' '.join(glob.glob(pattern)) for pattern in words[1:]])

def readDepFile(filename):
    """
    Read the makefile snippet in the GNU make format and return a list
    of all dependencies mentioned there. Only a very limited subset of
    the makefile language is supported. No commands are allowed. Only
    one target can have prerequisites. The only supported
    substitutions are $$ and $(wildcard ...), implemented as in GNU
    make.

    Parse errors are logged and cause an empty list to be returned. If
    the file does not exist, or if None is passed instead of the file
    name, an empty list is returned as well.

    @param filename Name of the makefile to read or None.

    @return A list of dependencies as strings.
    """
    if not filename:
        return []
    try:
        res = []
        longline = ''
        with open(filename) as f:
            for lineno, line in enumerate(f):
                if line.endswith('\\\n'):
                    longline += line[:-2] + ' '
                else:
                    longline += line
                    longline = _depFileCommentRE.sub('', longline) # remove comment
                    longline = longline.replace(r'\#', '#')
                    if longline.startswith('\t') and longline.strip():
                        raise _DepParseError('command encountered')
                    parts = longline.strip().split(':')
                    longline = ''
                    if len(parts) == 1 and parts[0] == '':
                        continue
                    if len(parts) != 2:
                        raise _DepParseError('line syntax must be target: [prerequisites]')
                    if res and parts[1].strip():
                        raise _DepParseError('only one target is allowed to have prerequisites')
                    res += _depFileSubstRE.sub(_depFileSubst, parts[1]).split()
        if longline:
            raise _DepParseError('line continuation at end of file')
        return res
    except _DepParseError, e:
        output.log.systemMessage("{0}:{1}: parse error: {2}".format(filename, lineno + 1, e))
        return []
    except EnvironmentError, e:
        if e.errno == errno.ENOENT:
            return []
        else:
            raise

_runOnceCache = {}

def runOnce(command, input=None):
    """
    Run the command (invoked directly without a shell), optionally
    feeding the input string to its standard input, wait for its
    completion, and return the text the command has written to the
    standard output.

    Only very short-lived and lightweight commands, such as --version
    checks, should be invoked this way. If the command fails to start,
    the OS exception will escape this function. If the command returns
    a non-zero exit code, errors.CommandFailed will be raised.

    The result of successful invocation of a command is cached, and
    the same command won't be invoked twice with the same value of
    input.

    @param command The command to run (a list of non-escaped arguments).

    @param input The text to feed to the command's standard input, or
    None.

    @throws CommandFailed The command has returned a non-zero exit
    code.

    @return The text the command has written to the standard output.
    """

    class UtilityCommand(flow.Process):

        def __init__(self, args, input, logger):
            self._args = args
            self._input = input
            self._logger = logger
            def start():
                return subprocess.Popen(args, close_fds=True,
                                        stdin=subprocess.PIPE if input else None,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
            flow.Process.__init__(self, start)

        @property
        def args(self):
            return self._args

        def run(self):
            assert self.state == flow.Process.PENDING
            self.start(self._logger)
            if self.state == flow.Process.FAILED:
                return None
            assert self.state == flow.Process.RUNNING
            stdout, stderr = self._process.communicate(self._input)
            stdoutLines = stdout.split('\n')
            if stdoutLines[-1] == '':
                del stdoutLines[-1]
            stderrLines = stderr.split('\n')
            if stderrLines[-1] == '':
                del stderrLines[-1]
            self.output = [output.OutputLine(line, False) for line in stdoutLines] + [output.OutputLine(line, True) for line in stderrLines]
            self.exited(self._process.returncode)
            return stdout

        def exited(self, returncode):
            assert self._state == flow.Process.RUNNING
            if returncode:
                self._state = flow.Process.FAILED
                self._error = (errors.CommandFailed, errors.CommandFailed(self), None)
            else:
                self._state = flow.Process.COMPLETED

    key = (tuple(command), input)
    if key not in _runOnceCache:
        process = UtilityCommand(command, input, output.log)
        stdout = process.run()
        if process.state == flow.Process.FAILED:
            raise process.error[0], process.error[1], process.error[2]
        _runOnceCache[key] = stdout
    return _runOnceCache[key]

_readOnceCache = {}

def readOnce(filename):
    """
    Read the specified text file and return the list of lines in it.

    The contents of the file are cached, and the same file won't be
    read twice.

    @param filename The file to read.

    @return The list of lines in the file (with trailing newlines).
    """
    if filename not in _readOnceCache:
        with open(filename) as f:
            _readOnceCache[filename] = f.readlines()
    return _readOnceCache[filename]

def shellEscape(text):
    """
    Escape special characters in the string so that it appears as a
    single word under the POSIX shell syntax.

    @param text Unescaped string.

    @return Escaped string.
    """
    return re.sub(r'([][{}\s\\$\'\"~!#%&*();?<>|])', r'\\\1', text)

def writeIfModified(filename, text):
    """
    Write text to file unless the file exists and contains the very
    same text. An existing file will be overwritten. Directories
    containing the file will be created if they don't exist.

    @param filename The name of the file.

    @param text The text to write.
    """
    try:
        with open(filename) as f:
            if f.read() == text:
                return
    except EnvironmentError, e:
        if e.errno != errno.ENOENT:
            raise
    makeDirs(filename)
    with open(filename, 'w') as f:
        f.write(text)

class Library(object):
    """
    The base class for configuration objects representing libraries.
    This base class provides empty containers as fallback values for
    all properties.
    """

    @property
    def defines(self):
        """
        A dictionary that maps preprocessor macro names to the values
        they should be given when using the library. A name given
        value None will simply be defined, without specifying a value.
        """
        return {}

    @property
    def includePaths(self):
        """
        A list of include paths to specify when using the library.
        """
        return []

    @property
    def libraryPaths(self):
        """
        A list of library paths to specify when using the library.
        """
        return []

    @property
    def libraryNames(self):
        """
        A list of library names (without the lib prefix or any
        standard suffixes) to link with when using the library.
        """
        return []

    @property
    def linkingPrefix(self):
        """
        A list of special flags passed to the linker before all
        library names, include directories etc.
        """
        return []

    @property
    def linkingPostfix(self):
        """
        A list of special flags passed to the linker after all
        library names, include directories etc.
        """
        return []


class StandardLibrary(Library):
    """
    A library with a well-known name that does not need special
    configuration to be linked. An example of such a library is
    pthread.
    """

    def __init__(self, name):
        """
        Constructor.

        @param name The library name. This value ends up the only item
        in the libraryNames list.
        """
        self.name = name

    @property
    def libraryNames(self):
        return [self.name]

class PkgConfig(Library):
    """
    A library about which the pkg-config tool provides information.
    """

    def __init__(self, name, static=False, link=True, pkgConfig='pkg-config'):
        """
        Constructor.

        @param name The name of the library package to be passed to
        pkg-config.

        @param static If true, will prepare a setup for static linking

        @param link If false, the library won't be linked, however,
        the include paths and preprocessor macros will be used.

        @param pkgConfig The name of the pkg-config tool to use.
        """
        self.name = name
        self._defines = {}
        self._includePaths = []
        self._libraryPaths = []
        self._libraryNames = []
        self._static = static
        self._link = link
        command = [pkgConfig, '--cflags', '--libs', name]
        if static:
            command.append('--static')
        for opt in runOnce(command).split():
            if opt.startswith('-D'):
                parts = opt[2:].split('=', 1)
                self._defines[parts[0]] = parts[1] if len(parts) > 1 else None
            elif opt.startswith('-I'):
                self._includePaths.append(opt[2:])
            elif opt.startswith('-L'):
                if link:
                    self._libraryPaths.append(opt[2:])
            elif opt.startswith('-l'):
                if link:
                    self._libraryNames.append(opt[2:])
            else:
                assert opt == '-pthread' # unknown option sometimes emitted by pkg-config

    @property
    def defines(self):
        return self._defines

    @property
    def includePaths(self):
        return self._includePaths

    @property
    def libraryPaths(self):
        return self._libraryPaths

    @property
    def libraryNames(self):
        return self._libraryNames

    @property
    def linkingPrefix(self):
        if self._static and self._link:
            # Tell the linker this one must be linked statically
            return ['-Bstatic']
        return []

    @property
    def linkingPostfix(self):
        if self._static and self._link:
            # Restore dynamic linking for following libs
            return ['-Bdynamic']
        return []
