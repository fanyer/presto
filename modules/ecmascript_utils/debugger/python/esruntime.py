from esthread import ESThread
from threading import Condition
from message import Message, ContinueMessage, EvalMessage, EvalVariableAuxiliary
from configuration import configuration

class ContinueError:
    def __init__(self, message): self.__message = message

    def __getitem__(self, index):
        if index == 0: return self.__message
        else: raise IndexError

    def __str__(self): return "%s: %s" % (self.__class__.__name__, self.__message)

class ESRuntime:
    def __init__(self, session, id):
        self.__session = session
        self.__id = id
        self.__threads = {}
        self.__currentThread = []
        self.__finishedThreads = []
        self.__windowId = None
        self.__framePath = None
        self.__documentURI = None
        self.__evalTag = None
        self.__evalResult = None
        self.__waiting = False
        self.__condition = Condition()
        self.__currentFrameIndex = 0

    def getSession(self):
        return self.__session

    def getId(self):
        return self.__id

    def getThread(self, id):
        if id != 0:
            if not self.__threads.has_key(id):
                self.__threads[id] = ESThread(self, id)
            return self.__threads[id]
        else:
            return None

    def getCurrentThread(self):
        if self.__currentThread: return self.__currentThread[-1]
        else: return None

    def getStack(self):
        stack = []
        for thread in self.__currentThread: stack = thread.getStack() + stack
        return stack

    def threadStarted(self, thread):
        self.__condition.acquire()
        if not self.getCurrentThread() or self.getCurrentThread() == thread.getParentThread():
            self.__currentThread.append(thread)
        self.__condition.release()
        self.__session.getDebugger().getUI().updatePrompt()

    def threadFinished(self, thread):
        self.__condition.acquire()
        try:
            if self.__threads.has_key(thread.getId()):
                self.__finishedThreads.append(thread)
                del self.__threads[thread.getId()]
                if thread in self.__currentThread: self.__currentThread.remove(thread)

            self.__condition.notify()
        finally:
            self.__condition.release()
        self.__session.getDebugger().getUI().updatePrompt()

    def getThreads(self):
        return self.__currentThread[:]

    def getShortDescription(self):
        if self.__windowId is not None:
            return "%s: %s" % (self.__framePath, self.__documentURI)
        else:
            return "no information available"

    def setInformation(self, windowId, framePath, documentURI):
        self.__windowId = windowId
        self.__framePath = framePath
        self.__documentURI = documentURI

    def getFramePath(self):
        return self.__framePath

    def getDocumentURI(self):
        return self.__documentURI

    def continueThread(self, mode):
        self.__condition.acquire()
        try:
            self.__currentFrameIndex = 0

            thread = self.getCurrentThread()
            if thread and thread.getStatus() == ESThread.STATUS_STOPPED:
                thread.continueThread(mode)

                self.__waiting = True
                try:
                    broken = False
                    while self.getCurrentThread() and self.getCurrentThread().getStatus() == ESThread.STATUS_RUNNING and self.__waiting:
                        try: self.__condition.wait(0.1)
                        except KeyboardInterrupt:
                            if broken: raise ContinueError, "thread still running"
                            if not self.getSession().isSupportedMessage(Message.TYPE_BREAK): "breaking not supported by Opera"

                            broken = True
                            thread.breakThread()
                finally: self.__waiting = False

                return True
            else:
                return False
        finally:
            self.__condition.release()

    def threadStopped(self, thread, position):
        self.__condition.acquire()
        try:
            thread.setCurrentPosition(position)

            if thread == self.getCurrentThread():
                thread.setStatus(ESThread.STATUS_STOPPED)

            self.__condition.notify()
        finally:
            self.__condition.release()

    def eval(self, script, expression=True, preprocess=True):
        self.__condition.acquire()
        try:
            if expression: script = "(%s)" % script.strip()

            frameIndex = self.__currentFrameIndex
            thread = self.getCurrentThread()

            if frameIndex != 0:
                threads = self.getThreads()[1:]
                threads.reverse()
                for thread in threads:
                    stack = thread.getStack()
                    if stack and len(stack) > frameIndex: break
                    else: frameIndex -= len(stack)

            message = EvalMessage(self.getSession(), self, thread, script, frameIndex)

            if preprocess:
                variables = self.getSession().preprocess(script)
                if len(variables) == 1 and script == "(%s)" % variables[0][0]:
                    return variables[0][1]

                for name, value in variables:
                    message.addAuxiliary(EvalVariableAuxiliary(message, name, value))

            self.__evalTag = message.getTag()
            self.__evalResult = None

            self.getSession().addQuery(self.__evalTag, message)
            self.getSession().getConnection().sendMessage(message)

            self.__waiting = True
            try:
                while self.__evalTag and self.__waiting: self.__condition.wait(0.1)
            finally: self.__waiting = False

            return self.__evalResult
        finally:
            self.__condition.release()

    def evalFinished(self, tag, value=None):
        self.__condition.acquire()
        try:
            if self.__evalTag == tag:
                self.__evalTag = None
                self.__evalResult = value
                if self.__waiting: self.__condition.notify()
        finally:
            self.__condition.release()

    def getStackFrame(self, frameIndex):
        threads = self.getThreads()
        threads.reverse()
        for thread in threads:
            stack = thread.getStack()
            if stack and len(stack) > frameIndex: return stack[frameIndex]
            else: frameIndex -= len(stack)
        return None

    def getPosition(self, frameIndex=None):
        if frameIndex is None:
            thread = self.getCurrentThread()
            if thread: return thread.getCurrentPosition()
        else:
            threads = self.getThreads()
            threads.reverse()
            for thread in threads:
                stack = thread.getStack()
                if stack and len(stack) > frameIndex: return stack[frameIndex][3]
                else: frameIndex -= len(stack)
        return None

    def setCurrentFrame(self, frameIndex): self.__currentFrameIndex = frameIndex
    def getCurrentFrame(self): return self.__currentFrameIndex
