from sys import stdin, stdout, argv, exit
from traceback import print_exc
from threading import RLock, Condition

from socketmanager import SocketManager, SocketManagerListener
from connection import Connection
from session import Session
from esthread import ESThread
from logger import logger
from position import Position
from configuration import configuration
from pollfallback import poll, error, POLLIN, EINTR
from tabcompleter import TabCompletionCandidates

if __name__ == "__main__":
    print "Use main.py instead of ui.py."
    exit(0)

class UIBase:
    def __init__(self):
        self.__debugger = None
        self.__lock = RLock()

    def setDebugger(self, debugger): self.__debugger = debugger
    def getDebugger(self): return self.__debugger

    def start(self, welcome):
        if welcome: self.writeln(welcome)

    def exit(self, goodbye):
        if goodbye: self.writeln(goodbye)

    def lock(self): self.__lock.acquire()
    def unlock(self): self.__lock.release()

try:
    if configuration["gud"]: raise Exception

    from termios import tcgetattr, tcsetattr, ICANON, TCSADRAIN, ECHO, TCSANOW
    from fcntl import fcntl, F_GETFL, F_SETFL
    from os import O_NONBLOCK

    escapeChain1a = { "~": "home" }
    escapeChain1c = { "~": "delete" }
    escapeChain1d = { "~": "end" }
    escapeChain2 = { "A": "up",
                     "B": "down",
                     "C": "right",
                     "D": "left",
                     "1": escapeChain1a,
                     "3": escapeChain1c,
                     "4": escapeChain1d }
    escapeChain3 = { "[": escapeChain2 }
    startChain = { chr(27): escapeChain3,
                   chr(127): "backspace",
                   chr(4): "eof",
                   chr(10): "eol",
                   chr(1): "home",
                   chr(5): "end",
                   chr(11): "kill",
                   chr(9): "tab" }

    class UI(UIBase):
        def __init__(self):
            UIBase.__init__(self)

            # Prompt handling
            self.__promptActive = False
            self.__promptDisplayed = False
            self.__promptDisabled = False
            self.__prompt = None

            # Command line history
            self.__history = []
            self.__historyPosition = 0

            self.__lastChar = '\n'
            self.__input = ""
            self.__buffer = ""
            self.__cursor = 0

            self.__pollobject = poll()
            self.__stopped = False
            self.__lastCommand = ""

        def __showPrompt(self):
            if self.__prompt and not self.__promptActive:
                self.__promptActive = True
                self.__displayPrompt()

        def __hidePrompt(self):
            if self.__promptActive:
                self.__promptActive = False
                self.__undisplayPrompt()

        def __disablePrompt(self):
            self.__promptDisabled = True
            self.__undisplayPrompt()

        def __enablePrompt(self):
            self.__promptDisabled = False
            self.__displayPrompt()

        def __displayPrompt(self):
            if self.__promptActive and not self.__promptDisplayed and not self.__promptDisplayed and self.__lastChar == "\n":
                self.__lineLength = len(self.__prompt()) + len(self.__buffer)
                stdout.write("\r%s\x1b[s%s\x1b[u" % (self.__prompt(), self.__buffer))
                if self.__cursor > 0: stdout.write("\x1b[%dC" % self.__cursor)
                stdout.flush()
                self.__promptDisplayed = True

        def __undisplayPrompt(self):
            if self.__promptDisplayed:
                stdout.write("\r" + " " * self.__lineLength + "\r")
                stdout.flush()
                self.__promptDisplayed = False

        def __write(self, string):
            if not self.__stopped and string:
                self.__undisplayPrompt()
                self.__lastChar = string[-1]
                stdout.write(string)
                stdout.flush()
                self.__displayPrompt()

        def __historySetPosition(self):
            self.__undisplayPrompt()
            if self.__historyPosition < len(self.__history):
                self.__buffer, self.__cursor = self.__history[self.__historyPosition]
            else:
                self.__buffer = ""
                self.__cursor = 0
            self.__displayPrompt()

        def __historyStorePosition(self):
            data = (self.__buffer, self.__cursor)
            if self.__historyPosition < len(self.__history):
                self.__history[self.__historyPosition] = data
            else:
                self.__history.append(data)

        def __historyPrevious(self):
            if self.__historyPosition > 0:
                self.__historyStorePosition()
                self.__historyPosition -= 1
                self.__historySetPosition()

        def __historyNext(self):
            if self.__historyPosition < len(self.__history) - 1:
                self.__historyStorePosition()
                self.__historyPosition += 1
                self.__historySetPosition()

        def __setBuffer(self, string):
            if self.__promptActive:
                self.__undisplayPrompt()
                self.__buffer = string
                self.__displayPrompt()

        def __insertText(self, text, moveCursor=True):
            self.__setBuffer(self.__buffer[:self.__cursor] + text + self.__buffer[self.__cursor:])
            if moveCursor: self.__cursor += len(text)
            self.__updateCursor()

        def __handleChar(self, ch):
            self.__insertText(ch)
            self.__lastCommand = ""

        def __unhandledChars(self, chars):
            str = ""
            for ch in chars:
                if ord(ch) < 32 or ord(ch) >= 127: str += "\\x%02x" % ord(ch)
                else: str += ch
            self.__insertText("<%s>" % str)
            self.__lastCommand = ""

        def __updateCursor(self):
            stdout.write("\x1b[u\x1b[%dC" % self.__cursor)
            stdout.flush()

        def __endInput(self):
            self.__buffer = ""
            stdout.write("\n")
            stdout.flush()
            self.__cursor = 0

        def __handleCommand(self, cmd):
            lastCommandWas = self.__lastCommand
            self.__lastCommand = cmd

            if cmd == "backspace" and self.__cursor != 0:
                self.__cursor -= 1
                self.__setBuffer(self.__buffer[:self.__cursor] + self.__buffer[self.__cursor + 1:])
            elif cmd == "delete" and self.__cursor < len(self.__buffer):
                self.__setBuffer(self.__buffer[:self.__cursor] + self.__buffer[self.__cursor + 1:])
            elif cmd == "left" and self.__cursor != 0:
                self.__cursor -= 1
                stdout.write("\x1b[1D")
                stdout.flush()
            elif cmd == "right" and self.__cursor < len(self.__buffer):
                self.__cursor += 1
                stdout.write("\x1b[1C")
                stdout.flush()
            elif cmd == "home":
                self.__cursor = 0
                stdout.write("\x1b[u")
                stdout.flush()
            elif cmd == "end":
                self.__cursor = len(self.__buffer)
                self.__updateCursor()
            elif cmd == "kill": self.__setBuffer(self.__buffer[:self.__cursor])
            elif cmd == "up": self.__historyPrevious()
            elif cmd == "down": self.__historyNext()
            elif cmd == "eof" and len(self.__buffer) == 0: self.__endInput()
            elif cmd == "eol":
                if len(self.__buffer) != 0:
                    if self.__history and len(self.__history[-1][0]) == 0:
                        self.__historyPosition = len(self.__history) - 1
                    else:
                        self.__historyPosition = len(self.__history)
                    self.__historyStorePosition()
                    self.__historyPosition += 1

                self.__line = self.__buffer

                self.__promptActive = False
                self.__promptDisplayed = False
                self.__endInput()
            elif cmd == "tab" and self.__tabCompleter:
                try:
                    success, buffer, cursor = self.__tabCompleter(self.__buffer, self.__cursor, lastCommandWas == "tab")
                    if success:
                        self.__cursor = cursor
                        self.__setBuffer(buffer)
                except TabCompletionCandidates, e:
                    self.__write("Candidates: %s\n" % ", ".join(e.candidates))
            elif cmd == "break":
                self.__promptActive = False
                self.__promptDisplayed = False
                self.__endInput()

        def __setupTerminal(self):
            self.__terminalBackup = tcgetattr(stdin.fileno())

            attr = tcgetattr(stdin.fileno())
            attr[3] = attr[3] & ~ICANON
            attr[3] = attr[3] & ~ECHO
            tcsetattr(stdin.fileno(), TCSADRAIN, attr)

        def __restoreTerminal(self):
            tcsetattr(stdin.fileno(), TCSANOW, self.__terminalBackup)

        def start(self, welcome):
            self.__setupTerminal()
            UIBase.start(self, welcome)

        def exit(self, goodbye):
            try: UIBase.exit(self, goodbye)
            except: pass
            self.__stopped = True
            self.__restoreTerminal()

        def readLine(self, prompt, tabCompleter=None):
            self.lock()

            try:
                self.__prompt = prompt
                self.__tabCompleter = tabCompleter

                while True:
                    self.__showPrompt()

                    input = self.__input
                    pollobject = self.__pollobject

                    self.unlock()
                    try:
                        fileno = stdin.fileno()

                        # Wait until input is available.
                        pollobject.register(fileno, POLLIN)
                        try:
                            while True:
                                try:
                                    ready = pollobject.poll()
                                    break
                                except error, (code, msg):
                                    if code != EINTR: raise
                                except KeyboardInterrupt:
                                    self.lock()
                                    try:
                                        self.__handleCommand("break")
                                        self.__showPrompt()
                                    finally: self.unlock()
                        finally: pollobject.unregister(fileno)

                        # Set stdin non-blocking so that we never get stuck here waiting for input.
                        flags = fcntl(fileno, F_GETFL)
                        fcntl(fileno, F_SETFL, flags | O_NONBLOCK)
                        try: input += stdin.read()
                        finally: fcntl(fileno, F_SETFL, flags)
                    finally:
                        self.lock()

                    self.__input = input

                    while self.__input:
                        chain = startChain
                        for index, ch in enumerate(self.__input):
                            next = chain.get(ch, None)
                            if type(next) == type(""):
                                self.__input = self.__input[index + 1:]
                                if len(next) == 1: self.__handleChar(next)
                                else:
                                    self.__handleCommand(next)
                                    if next == "eol": return self.__line
                                    elif next == "eof" and not self.__buffer: return None
                                break
                            elif type(next) == type({}):
                                chain = next
                            elif index == 0:
                                self.__handleChar(ch)
                                self.__input = self.__input[1:]
                                break
                            else:
                                self.__unhandledChars(self.__input[:index + 1])
                                self.__input = self.__input[index + 1:]
                                break
                        else:
                            break
            finally:
                self.__hidePrompt()
                self.unlock()

        def write(self, *data):
            self.lock()
            try:
                for datum in data: self.__write(datum)
            finally: self.unlock()

        def writeln(self, *data):
            self.lock()
            try:
                for datum in data: self.__write(datum + "\n")
            finally: self.unlock()

        def updatePrompt(self):
            self.__undisplayPrompt()
            self.__displayPrompt()

except:
    class UI(UIBase):
        def __init__(self):
            UIBase.__init__(self)
            self.__supportsWrite = True
            self.__condition = Condition()
            self.__stopped = False

        def exit(self, goodbye):
            UIBase.exit(self, goodbye)
            self.__condition.acquire()
            self.__stopped = True
            self.__condition.notify()
            self.__condition.release()

        def readLine(self, prompt, tabCompleter):
            while True:
                self.__wait()
                self.__supportsWrite = False
                try:
                    stdout.write(prompt())
                    stdout.flush()
                
                    try: line = stdin.readline()
                    except KeyboardInterrupt:
                        stdout.write("\n")
                        continue

                    if not line: return None
                    else: return line.strip()
                finally:
                    self.__supportsWrite = True

        def write(self, *data):
            if self.__supportsWrite:
                for datum in data: stdout.write(datum)
            self.__condition.acquire()
            try: self.__condition.notify()
            finally: self.__condition.release()

        def writeln(self, *data):
            if self.__supportsWrite:
                for datum in data: stdout.write(datum + "\n")
            self.__condition.acquire()
            try: self.__condition.notify()
            finally: self.__condition.release()

        def updatePrompt(self):
            pass

        def __wait(self):
            self.__condition.acquire()
            try:
                while not self.__stopped and not self.getDebugger().isStopped(): self.__condition.wait()
            finally: self.__condition.release()
