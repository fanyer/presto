from threading import Condition
from message import ContinueMessage, EvalMessage, EvalVariableAuxiliary, BacktraceMessage, BreakMessage

class ESThread:
    MODE_RUN      = 1
    MODE_STEPINTO = 2
    MODE_STEPOVER = 3
    MODE_FINISH   = 4

    TYPE_INLINE   = 1
    TYPE_EVENT    = 2
    TYPE_URL      = 3
    TYPE_TIMEOUT  = 4
    TYPE_JAVA     = 5
    TYPE_OTHER    = 6

    STATUS_STOPPED   = -2
    STATUS_RUNNING   = -1
    STATUS_FINISHED  = 1
    STATUS_EXCEPTION = 2
    STATUS_ABORTED   = 3
    STATUS_CANCELLED = 4

    def __init__(self, runtime, id):
        self.__runtime = runtime
        self.__id = id
        self.__position = None
        self.__running = False
        self.__evalTag = None
        self.__evalResult = None
        self.__waiting = False
        self.__condition = Condition()
        self.__status = ESThread.STATUS_RUNNING
        self.__type = ESThread.TYPE_OTHER
        self.__parentThread = None
        self.__eventNamespaceURI = None
        self.__eventType = None
        self.__tag = None
        self.__stack = None

    def getRuntime(self): return self.__runtime
    def getId(self): return self.__id

    def setStatus(self, status):
        self.__status = status
        self.__runtime.getSession().getUI().updatePrompt()
    def getStatus(self): return self.__status

    def setType(self, type): self.__type = type
    def getType(self): return self.__type

    def setNamespaceURI(self, namespaceURI):
        self.__eventNamespaceURI = namespaceURI

    def setEventType(self, eventType):
        self.__eventType = eventType

    def setParentThread(self, parentThread):
        self.__parentThread = parentThread
        if self.__parentThread: self.__parentThread.interrupt()

    def getParentThread(self):
        return self.__parentThread

    def setCurrentPosition(self, position):
        self.__position = position
        self.setStatus(ESThread.STATUS_STOPPED)

    def getCurrentPosition(self):
        return self.__position

    def interrupt(self):
        pass
        #self.__condition.acquire()
        #if self.__waiting:
        #    self.__waiting = False
        #    self.__condition.notify()
        #self.__condition.release()

    def getShortDescription(self):
        if self.__status == ESThread.STATUS_STOPPED:
            status = "stopped"
        elif self.__status == ESThread.STATUS_RUNNING:
            status = "running"
        else:
            status = "finished"
        
        if self.__type == ESThread.TYPE_INLINE:
            type = "inline thread"
        elif self.__type == ESThread.TYPE_EVENT:
            type = "event thread"
        elif self.__type == ESThread.TYPE_URL:
            type = "'javascript:' URL thread"
        elif self.__type == ESThread.TYPE_TIMEOUT:
            type = "timeout thread"
        elif self.__type == ESThread.TYPE_JAVA:
            type = "java thread"
        else:
            type = "unknown thread"

        return "%s %s" % (status, type)

    def cancel(self):
        self.__condition.acquire()
        if self.__status < 0:
            self.__running = False
            self.__evalTag = None
            self.__evalResult = None
            self.setStatus(ESThread.STATUS_CANCELLED)
            if self.__waiting:
                self.__waiting = False
                self.__condition.notify()
        self.__condition.release()

    def continueThread(self, mode):
        self.__stack = None
        message = ContinueMessage(self.getRuntime().getSession(), self.getRuntime(), self, mode)
        self.getRuntime().getSession().getConnection().sendMessage(message)
        self.setStatus(ESThread.STATUS_RUNNING)

    def breakThread(self):
        message = BreakMessage(self.getRuntime().getSession(), self.getRuntime(), self)
        self.getRuntime().getSession().getConnection().sendMessage(message)

    def getStack(self):
        if self.__stack is None: self.updateStack()
        return self.__stack[:]

    def updateStack(self):
        self.__condition.acquire()
        try:
            self.__stack = None

            session = self.getRuntime().getSession()
            message = BacktraceMessage(session, self)
            self.__tag = message.getTag()
            session.addQuery(self.__tag, message)
            session.getConnection().sendMessage(message)

            try:
                while self.__tag and not self.__stack: self.__condition.wait(0.1)
            finally: self.__tag = None
        finally: self.__condition.release()

    def setStack(self, tag, stack):
        self.__condition.acquire()
        try:
            if self.__tag == tag:
                self.__tag = None
                self.__stack = stack
                self.__condition.notify()
        finally: self.__condition.release()
