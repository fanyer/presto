from logger import logger
from os import listdir, access, R_OK, X_OK
from os.path import isfile, isdir, join

class CommandError:
    def __init__(self, message): self.__message = message

    def __getitem__(self, index):
        if index == 0: return self.__message
        else: raise IndexError

    def __str__(self): return "%s: %s" % (self.__class__.__name__, self.__message)

class MissingArgumentError(CommandError):
    def __init__(self): CommandError.__init__(self, "missing argument")

class InvalidArgumentError(CommandError):
    def __init__(self, argument, expected): CommandError.__init__(self, "invalid argument: '%s' (expected %s)" % (argument, expected))

class CommandInvoker:
    def __init__(self, debugger, command, name, options, arguments):
        self.__debugger = debugger
        self.__command = command
        self.__name = name
        self.__options = options
        self.__unsupportedOptions = options
        self.__arguments = arguments

    def getDebugger(self): return self.__debugger
    def getName(self): return self.__name
    def getArguments(self): return self.__arguments

    def hasOption(self, option):
        self.__unsupportedOptions = self.__unsupportedOptions.replace(option, "")
        return option in self.__options

    def checkUnsupportedOptions(self):
        if self.__unsupportedOptions: raise CommandError, "%s: unsupported options: %s" % (self.__name, self.__unsupportedOptions)

    def getSession(self, needSession=True):
        session = self.__debugger.getCurrentSession()
        if session or not needSession: return session
        else: raise CommandError, "%s: need current session." % self.__name

    def getRuntime(self, needRuntime=True, needSession=True):
        session = self.getSession(needSession)
        if session:
            runtime = session.getCurrentRuntime()
            if runtime or not needRuntime: return runtime
            else: raise CommandError, "%s: need current runtime." % self.__name

    def getThread(self, needThread=True, needRuntime=True, needSession=True):
        runtime = self.getRuntime(needRuntime, needSession)
        if runtime:
            thread = runtime.getCurrentThread()
            if thread or not needThread: return thread
            else: raise CommandError, "%s: need current thread." % self.__name

    def __call__(self):
        try: return self.__command(self)
        except CommandError, e:
            if not e[0].startswith("%s: " % self.getName()): raise CommandError, "%s: %s" % (self.getName(), e[0])
            else: raise
        except:
            logger.logException()
            raise CommandError, "%s: command failed." % self.__name

def loadCommandsFromFile(path):
    if not isfile(path): raise CommandError, "Not a regular file: %s" % path
    if not access(path, R_OK): raise CommandError, "Can not read file: %s" % path

    names = {}

    try: execfile(path, names)
    except SyntaxError:
        logger.logException()
        raise CommandError, "Syntax error while loading file: %s" % path
    except:
        logger.logException()
        raise CommandError, "Failed to load file: %s" % path

    commands = names.get("__commands__", {})
    if not commands: raise CommandError, "File contained no commands: %s" % path

    for name, data in commands.items():
        valid = True
        if type(name) != type(""): valid = False
        if type(data) == type(()) and (len(data) != 2 or type(data[0]) != type("") or not callable(data[1])): valid = False
        if type(data) != type(()) and not callable(data): valid = False
        if not valid: raise CommandError, "File contained invalid commands: %s" % path

    return commands.items()

def loadCommandsFromDirectory(path):
    if not isdir(path): raise CommandError, "Not a directory: %s" % path
    if not access(path, R_OK | X_OK): raise CommandError, "Can not list directory: %s" % path

    commands = []

    entries = listdir(path)
    for entry in entries:
        if entry.startswith("cmd-") and entry.endswith(".py"):
            commands.extend(loadCommandsFromFile(join(path, entry)))

    if not commands: raise CommandError, "Directory contained no commands: %s" % path
    
    return commands

