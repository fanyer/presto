from traceback import print_exc
from sys import stderr
from threading import Lock

try:
    from fcntl import fcntl, F_GETFL, F_SETFL
    from os import O_NONBLOCK
    hasFcntl = True
except:
    hasFcntl = False

class Logger:
    def __init__(self, logExceptions, logWarnings, logDebugTypes):
        self.__logExceptions = logExceptions
        self.__logWarnings = logWarnings
        self.__logDebugTypes = logDebugTypes
        self.__lock = Lock()
        self.__file = stderr
        self.__flags = None

    def __start(self):
        self.__lock.acquire()
        if hasFcntl:
            self.__flags = fcntl(self.__file.fileno(), F_GETFL)
            flags = self.__flags
            flags = flags & ~O_NONBLOCK
            fcntl(self.__file.fileno(), F_SETFL, flags)

    def __end(self):
        if hasFcntl:
            fcntl(self.__file.fileno(), F_SETFL, self.__flags)
        self.__lock.release()

    def logException(self):
        self.__start()
        if self.__logExceptions:
            print_exc(None, self.__file)
        self.__end()

    def logFatal(self, fatal):
        self.__start()
        print >>self.__file, fatal
        self.__end()

    def logWarning(self, warning):
        self.__start()
        if self.__logWarnings:
            print >>self.__file, warning
        self.__end()

    def logDebug(self, type, debug):
        self.__start()
        if type in self.__logDebugTypes or "*" in self.__logDebugTypes:
            print >>self.__file, debug
        self.__end()

logger = Logger(True, True, [])
