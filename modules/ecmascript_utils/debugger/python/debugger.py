from commands import CommandInvoker, CommandError
from session import Session
from socketmanager import SocketManager, SocketManagerListener
from connection import Connection
from sys import exit
from ui import UI
from logger import logger
from esthread import ESThread
from tabcompleter import TabCompleter
from threading import RLock

class DebuggerError:
    def __init__(self, message):
        self.__message = message

    def __getitem__(self, index):
        if index == 0: return self.__message
        else: raise IndexError

    def __str__(self):
        return "DebuggerError: %s" % self.__message

class Debugger(SocketManagerListener):
    def __init__(self):
        self.__ui = UI()
        self.__ui.setDebugger(self)

        self.__socketmanager = SocketManager()
        self.__socketmanager.setListener(self)

        self.__sessions = []
        self.__currentSession = None
        self.__commands = []
        self.__quit = False
        self.__newSessionCommands = []
        self.__currentLines = None
        self.__commandQueue = []
        self.__commandQueueLock = RLock()

    def getUI(self): return self.__ui
    def getSocketManager(self): return self.__socketmanager

    def getCurrentSession(self): return self.__currentSession
    def setCurrentSession(self, session): self.__currentSession = session

    def getNewSessionCommands(self): return self.__newSessionCommands[:]
    def setNewSessionCommands(self, commands): self.__newSessionCommands = commands
    
    def getSessions(self): return self.__sessions[:]

    def addSession(self, session):
        self.__sessions.append(session)
        if self.__currentSession is None: self.__currentSession = session

    def removeSession(self, session):
        self.__sessions.remove(session)
        if self.__currentSession == session: self.__currentSession = None

    def getCurrentRuntime(self):
        if self.getCurrentSession(): return self.getCurrentSession().getCurrentRuntime()
        else: return None
        
    def getCurrentThread(self):
        runtime = self.getCurrentRuntime()
        if runtime: return runtime.getCurrentThread()
        else: return None

    def addCommand(self, name, data):
        existing = [True for name0, realname0, command0 in self.__commands if name == name0]
        if existing: raise DebuggerError, "Command name already registered: %s" % name
        if type(data) == type(()): self.__commands.append((name, data[0], data[1]))
        else: self.__commands.append((name, name, data))

    def getCommandNames(self): return [realname for name, realname, command in self.__commands]

    def getCommandInvoker(self, line):
        try: name, arguments = line.split(' ', 1)
        except ValueError: name, arguments = line.strip(), ""
        try: name, options = name.split('/', 1)
        except ValueError: name, options = name, ""

        perfect = [(realname0, command0) for name0, realname0, command0 in self.__commands if name == name0]
        if perfect: return CommandInvoker(self, perfect[0][1], perfect[0][0], options, arguments)

        candidates = [(name0, realname0, command0) for name0, realname0, command0 in self.__commands if len(name) <= len(name0) and name0.startswith(name)]
        if len(candidates) == 1: return CommandInvoker(self, candidates[0][2], candidates[0][1], options, arguments)

        if len(candidates) == 0: message = "Unknown command: %s" % name
        else: message = "Ambiguous command.  Candidates are: %s" % ", ".join([name0 for name0, realname0, command0 in candidates])

        raise DebuggerError, message

    def execute(self, lines, withCurrentSession=None, withCurrentRuntime=None):
        currentSessionBefore = self.__currentSession
        currentRuntimeBefore = currentSessionBefore and currentSessionBefore.getCurrentRuntime()

        if withCurrentRuntime: withCurrentSession = withCurrentRuntime.getSession()
        if withCurrentSession:
            self.__currentSession = withCurrentSession
            if withCurrentRuntime:
                withCurrentSession.setCurrentRuntime(withCurrentRuntime)

        try:
            previousLines = self.__currentLines
            self.__currentLines = lines[:]
            while self.__currentLines:
                line = self.__currentLines.pop(0)
                if line:
                    output = self.getCommandInvoker(line)()
                    if output: self.getUI().writeln(*output)
        finally:
            self.__currentLines = previousLines
            if withCurrentSession:
                self.__currentSession = currentSessionBefore
                if withCurrentRuntime and currentSessionBefore:
                    currentSessionBefore.setCurrentRuntime(currentRuntimeBefore)

    def executeLater(self, lines, withCurrentSession=None, withCurrentRuntime=None):
        self.__commandQueueLock.acquire()
        try: self.__commandQueue.append((lines, withCurrentSession, withCurrentRuntime))
        finally: self.__commandQueueLock.release()

    def readLine(self, prompt):
        if type(prompt) == type(""): prompt = lambda value=prompt: value
        if self.__currentLines: return self.__currentLines.pop(0)
        else: return self.__ui.readLine(prompt)

    def isStopped(self):
        return self.getCurrentThread() and self.getCurrentThread().getStatus() == ESThread.STATUS_STOPPED

    def getPrompt(self):
        try:
            if not self.getCurrentSession(): return "(sessionless) "
            elif not self.getCurrentThread(): return "(idle) "
            elif self.getCurrentThread().getStatus() == ESThread.STATUS_STOPPED: return "(stopped) "
            elif self.getCurrentThread().getStatus() == ESThread.STATUS_RUNNING: return "(running) "
            else: return "(confused) "
        except AttributeError:
            return "(weird exception)"
        
    def run(self, lines):
        self.__socketmanager.start()

        try: self.__ui.start("Opera ECMAScript debugger\n")
        except:
            logger.logException()
            logger.logFatal("UI failure, exiting.")
        else:
            lastLine = None

            withCurrentSession, withCurrentRuntime = None, None

            while not self.__quit:
                if not lines:
                    try:
                        line = self.__ui.readLine(self.getPrompt, TabCompleter(self))

                        if line is None:
                            self.__quit = True
                            break

                        if not line.strip():
                            if lastLine: line = lastLine
                            else: continue
                        else: lastLine = line.split(" ")[0]

                        lines = [line]
                        withCurrentSession, withCurrentRuntime = None, None
                    except:
                        logger.logException()
                        logger.logFatal("UI failure, exiting.")
                        break

                try: self.execute(lines, withCurrentSession, withCurrentRuntime)
                except CommandError, error: self.__ui.writeln(error[0])
                except DebuggerError, error: self.__ui.writeln(error[0])
                except:
                    logger.logException()
                    self.__ui.writeln("Internal debugger error.")

                self.__commandQueueLock.acquire()
                try:
                    if self.__commandQueue: lines, withCurrentSession, withCurrentRuntime = self.__commandQueue.pop(0)
                    else: lines, withCurrentSession, withCurrentRuntime = [], None, None
                finally: self.__commandQueueLock.release()

        try: self.__ui.exit(self.__quit and "Good bye!" or None)
        except: logger.logException()
        try: self.__socketmanager.exit()
        except: logger.logException()
        if self.__quit: exit(0)
        else: exit(1)

    # Called by the "quit" command.
    def quit(self):
        self.__quit = True

    # From SocketManagerListener: called by SocketManager when a new connection is established.
    def getSocketHandler(self, socket):
        session = Session()
        connection = Connection(socket)

        session.setDebugger(self)
        session.setConnection(connection)
        connection.setSession(session)
        connection.setSocketManager(self.__socketmanager)

        self.__sessions.append(session)
        return connection

    def reportNewSession(self, newSession):
        data = ["New session: %s" % newSession.getShortDescription()]
        if not self.__currentSession:
            self.__currentSession = newSession
            data.append("Setting new session as current session.")
        self.__ui.writeln(*data)
        if self.__newSessionCommands:
            try: self.execute(self.__newSessionCommands, newSession)
            except CommandError, error: self.__ui.writeln(error[0])
            except DebuggerError, error: self.__ui.writeln(error[0])

    def sessionConnectionClosed(self, session):
        self.__sessions.remove(session)
        if self.getCurrentSession() == session:
            if self.__sessions: self.__currentSession = self.__sessions[0]
            else: self.__currentSession = None
            self.getUI().writeln("Current session connection closed.")
        else: self.getUI().writeln("Session connection closed.")
