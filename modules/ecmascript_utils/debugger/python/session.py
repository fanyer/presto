from esruntime import ESRuntime
from esobject import ESObject
from esvalue import ESValue
from message import Message, Auxiliary, EvalReplyMessage, SetConfigurationMessage
from logger import logger
from breakpoint import Breakpoint
from position import Position
from esthread import ESThread
from configuration import configuration
from re import compile
from script import Script

re_valueref = compile("[^$]\\$([0-9]+)")
re_objectref = compile("[^$]\\$\\$([0-9]+)")
re_specialref = compile("[^$]\\$([nutfze])")

class Session:
    defaultStopAt = {}

    def __init__(self):
        self.__debugger = None
        self.__connection = None
        self.__protocolVersion = None
        self.__os = None
        self.__platform = None
        self.__userAgent = None
        self.__supportedMessages = {}
        self.__runtimes = {}
        self.__currentRuntime = None
        self.__objects = {}
        self.__queries = {}
        self.__breakpoints = []
        self.__breakpointTable = {}
        self.__scriptsBySource = {}
        self.__protocolVersion = 0
        self.__values = {}
        self.__valueIdCounter = 1
        self.__stopAtScript = Session.defaultStopAt.get("script", True)
        self.__stopAtException = Session.defaultStopAt.get("exception", False)
        self.__stopAtError = Session.defaultStopAt.get("error", False)
        self.__stopAtAbort = Session.defaultStopAt.get("abort", False)
        self.__stopAtGC = Session.defaultStopAt.get("gc", False)
        self.__scriptsById = {}
        self.__scripts = []

    def setDebugger(self, debugger): self.__debugger = debugger
    def getDebugger(self): return self.__debugger

    def setConnection(self, connection): self.__connection = connection
    def getConnection(self): return self.__connection

    def getUI(self): return self.__debugger.getUI()

    def getRuntime(self, id):
        if not self.__runtimes.has_key(id):
            self.__runtimes[id] = ESRuntime(self, id)
        return self.__runtimes[id]

    def getRuntimes(self): return self.__runtimes.values()

    def getObject(self, id):
        if id == 0: return None
        if not self.__objects.has_key(id): self.__objects[id] = ESObject(self, id)
        return self.__objects[id]

    def setCurrentRuntime(self, runtime):
        self.__currentRuntime = runtime
        self.__debugger.getUI().updatePrompt()

    def getCurrentRuntime(self): return self.__currentRuntime

    def getCurrentThread(self):
        if self.__currentRuntime: return self.__currentRuntime.getCurrentThread()
        else: return None

    def addQuery(self, tag, message):
        self.__queries[tag] = message

    def getQuery(self, tag):
        return self.__queries[tag]

    def __handleHelloMessage(self, message):
        self.__protocolVersion = message.getProtocolVersion()
        self.__os = message.getOS()
        self.__platform = message.getPlatform()
        self.__userAgent = message.getUserAgent()
        self.__supportedMessages = message.getSuppertedMessages()
        self.__debugger.reportNewSession(self)

        message = SetConfigurationMessage(self, self.__stopAtScript, self.__stopAtException, self.__stopAtError, self.__stopAtAbort, self.__stopAtGC)
        self.__connection.sendMessage(message)

    def __handleNewScriptMessage(self, message):
        script = message.getScript()

        self.__scripts.append(script)
        self.__scriptsById[script.getId()] = script

        identical = self.__scriptsBySource.setdefault(script.getSource(), {})
        if identical:
            lines = {}
            for bp in self.__breakpoints[:]:
                if bp.getType() == Breakpoint.TYPE_POSITION and identical.has_key(bp.getPosition().getScript().getId()):
                    linenr = bp.getPosition().getLineNr()
                    if not lines.has_key(linenr):
                        lines[linenr] = True
                        self.addBreakpointAtPosition(Position(script, linenr))
        identical[script.getId()] = True

    def __handleStoppedAtMessage(self, message):
        if self.__currentRuntime == message.getRuntime():
            data = []

            returnValues = message.getAuxiliary(Auxiliary.TYPE_RETURN_VALUE)
            returnValues.reverse()
            for returnValue in returnValues:
                functionName = returnValue.getFunction().getFunctionName()
                value = returnValue.getValue()
                data.append("  %s returned $%s = %s" % (functionName, self.registerValue(value), value))

            exceptionsThrown = message.getAuxiliary(Auxiliary.TYPE_EXCEPTION_THROWN)
            for exceptionThrown in exceptionsThrown:
                exception = exceptionThrown.getException()
                data.append("Exception thrown: $%s = %s" % (self.registerValue(exception), exception))

            breakpointsTriggered = message.getAuxiliary(Auxiliary.TYPE_BREAKPOINT_TRIGGERED)
            for breakpointTriggered in breakpointsTriggered:
                bp = breakpointTriggered.getBreakpoint()
                if bp:
                    string = "Breakpoint %d at " % bp.getId()
                    if bp.getType() == Breakpoint.TYPE_POSITION:
                        string += bp.getPosition().getShortDescription()
                    elif bp.getType() == Breakpoint.TYPE_FUNCTION:
                        string += str(bp.getFunction())
                    else:
                        string += "event %s" % bp.getEventType()[1]
                    data.append(string)

            heapStatisticsItems = message.getAuxiliary(Auxiliary.TYPE_HEAP_STATISTICS)
            for heapStatistics in heapStatisticsItems:
                data = ["Number of GCs                       %s" % str(heapStatistics.getNumCollections()),
                        "Cumulative time spent in GC (ms)    %s" % str(heapStatistics.getMsCollecting()),
                        "Bytes live following GC             %s" % str(heapStatistics.getBytesLiveAfterGC()),
                        "Bytes found free during GC          %s" % str(heapStatistics.getBytesFreeAfterGC()),
                        "Objects live following GC           %s" % str(heapStatistics.getObjectsLiveAfterGC()),
                        "Objects free following GC           %s" % str(heapStatistics.getObjectsFreeAfterGC()),
                        "Cumulative bytes allocated          %s" % str(heapStatistics.getBytesAllocated()),
                        "Cumulative bytes reclaimed (approx) %s" % str(heapStatistics.getBytesReclaimed()),
                        "Heap limit                          %s" % str(heapStatistics.getBytesHeapLimit()),
                        "GC load factor                      %s" % str(heapStatistics.getLoadFactor()),
                        "Bytes allocated to the heap         %s" % str(heapStatistics.getBytesInHeap()),
                        "Peak bytes allocated to the heap    %s" % str(heapStatistics.getBytesInHeapPeak())]

            self.getCurrentRuntime().threadStopped(message.getThread(), message.getPosition())

            data.append(self.__getCurrentLine())
            self.getUI().writeln(*data)

            breakpointsTriggered = message.getAuxiliary(Auxiliary.TYPE_BREAKPOINT_TRIGGERED)
            for breakpointTriggered in breakpointsTriggered:
                bp = breakpointTriggered.getBreakpoint()
                if bp:
                    commands = bp.getCommands()
                    if commands: self.__debugger.executeLater(commands, withCurrentRuntime=message.getRuntime())

    def __handleEvalReplyMessage(self, message):
        query = self.getQuery(message.getTag())

        if message.getStatus() == EvalReplyMessage.FINISHED:
            returnValues = message.getAuxiliary(Auxiliary.TYPE_RETURN_VALUE)
            if returnValues: query.getRuntime().evalFinished(message.getTag(), returnValues[0].getValue())
        else:
            if message.getStatus() == EvalReplyMessage.EXCEPTION: error = "  an exception was thrown"
            elif message.getStatus() == EvalReplyMessage.ABORTED: error = "  evaluation was aborted"
            else: error = "  evaluation was cancelled"
            self.getUI().writeln(error)
            query.getRuntime().evalFinished(message.getTag())

    def __handleExamineReplyMessage(self, message):
        query = self.getQuery(message.getTag())
        properties = query.getProperties()
        objects = []
        for index in range(message.getObjectsCount()):
            objects.append((message.getObject(index), message.getObjectProperties(index)))
        properties.examineFinished(objects)

    def __handleThreadStartedMessage(self, message):
        thread = message.getThread()
        thread.getRuntime().threadStarted(thread)
        if not self.getCurrentRuntime() or not self.getCurrentRuntime().getCurrentThread(): self.setCurrentRuntime(thread.getRuntime())

    def __handleThreadFinishedMessage(self, message):
        thread = message.getThread()
        result = []
        if thread.getRuntime().getCurrentThread() == thread:
            returnValues = message.getAuxiliary(Auxiliary.TYPE_RETURN_VALUE)
            returnValues.reverse()
            for returnValue in returnValues:
                function = returnValue.getFunction()
                if function: functionName = "  " + returnValue.getFunction().getFunctionName()
                else:
                    functionName = "Thread"
                    if returnValue.getValue().isUndefined(): continue
                result.append("%s returned %s\n" % (functionName, returnValue.getValue()))
        thread.getRuntime().threadFinished(thread)

        runtime = self.getCurrentRuntime()
        if thread.getRuntime() == runtime and not runtime.getCurrentThread():
            for runtime in self.__runtimes.values():
                if runtime.getCurrentThread():
                    self.setCurrentRuntime(runtime)
                    if runtime.getCurrentThread().getStatus() == ESThread.STATUS_STOPPED:
                        result.append(self.__getCurrentLine())
                    else:
                        result = []

        if result: self.getUI().writeln(*result)

    def __handleBacktraceReplyMessage(self, message):
        query = self.getQuery(message.getTag())
        query.getThread().setStack(message.getTag(), message.getStack())

    def handleMessage(self, message):
        if message.getType() == Message.TYPE_HELLO:
            self.__handleHelloMessage(message)
        elif message.getType() == Message.TYPE_NEW_SCRIPT:
            self.__handleNewScriptMessage(message)
        elif message.getType() == Message.TYPE_STOPPED_AT:
            self.__handleStoppedAtMessage(message)
        elif message.getType() == Message.TYPE_EVAL_REPLY:
            self.__handleEvalReplyMessage(message)
        elif message.getType() == Message.TYPE_EXAMINE_REPLY:
            self.__handleExamineReplyMessage(message)
        elif message.getType() == Message.TYPE_THREAD_STARTED:
            self.__handleThreadStartedMessage(message)
        elif message.getType() == Message.TYPE_THREAD_FINISHED:
            self.__handleThreadFinishedMessage(message)
        elif message.getType() == Message.TYPE_BACKTRACE_REPLY:
            self.__handleBacktraceReplyMessage(message)

    def connectionClosed(self):
        self.__debugger.sessionConnectionClosed(self)

        for runtime in self.__runtimes.values():
            for thread in runtime.getThreads():
                try: thread.cancel()
                except: logger.logException()

    def getShortDescription(self):
        return "%s/%s [%s]" % (self.__os, self.__platform, self.__connection.getShortDescription())

    def showThreads(self):
        self.__ui.open()
        try:
            if len(self.__runtimes) == 0:
                self.__ui.write("No runtimes.\n")
            else:
                threadIndexOffset = 1
                for index in range(len(self.__runtimes)):
                    runtime = self.__runtimes.values()[index]
                    if runtime == self.__currentRuntime: active = " (active)"
                    else: active = ""
                    self.__ui.write("%d: Runtime: %s\n" % (index + 1, runtime.getShortDescription()))
                    threads = runtime.getThreads()
                    if len(threads) == 0:
                        self.__ui.write("  No threads.\n")
                    else:
                        for index in range(len(threads)):
                            thread = threads[index]
                            if thread == runtime.getCurrentThread(): active = " (active)"
                            else: active = ""
                            self.__ui.write("  %d: %s\n" % (index + threadIndexOffset, thread.getShortDescription()))
                        threadIndexOffset += len(threads)
        finally:
            self.__ui.close()

    def selectThread(self, index, silent=False):
        self.__ui.open()
        try:
            index -= 1
            for runtime in self.__runtimes.values():
                threads = runtime.getThreads()
                if index < len(threads):
                    thread = threads[index]
                    if runtime != self.__currentRuntime: self.__currentRuntime = runtime
                    runtime.setCurrentThread(thread)
                    self.__ui.write("Selected thread: %s\n" % thread.getShortDescription())
                    self.__outputCurrentLine()
                    return
                else:
                    index -= len(threads)
            else:
                if not silent:
                    self.__ui.write("No such thread (use 'show threads').\n")
        finally:
            self.__ui.close()

    def getNextThread(self):
        for runtime in self.__runtimes.values():
            threads = runtime.getThreads()
            for thread in threads:
                if thread.getStatus() == ESThread.STATUS_STOPPED:
                    return thread
        return None

    def addBreakpointAtPosition(self, position):
        bp = Breakpoint(self)
        bp.setPosition(position)
        bp.enable()
        self.__breakpoints.append(bp)
        self.__breakpointTable[bp.getId()] = bp
        return bp.getId()

    def addBreakpointAtFunction(self, function):
        bp = Breakpoint(self)
        bp.setFunction(function)
        bp.enable()
        self.__breakpoints.append(bp)
        self.__breakpointTable[bp.getId()] = bp
        return bp.getId()

    def getBreakpoint(self, id): 
        if id == ~0:
            bp = Breakpoint(self)
            bp.setUserType()
            return bp
	else:
            return self.__breakpointTable.get(id)

    def getBreakpoints(self): return self.__breakpoints[:]

    def removeBreakpoint(self, id):
        if self.__breakpointTable.has_key(id): del self.__breakpointTable[id]

    def __getCurrentLine(self, frameIndex=None):
        if self.__currentRuntime: return self.__currentRuntime.getPosition(frameIndex).getLineForOutput()
        else: return None

    def isSupportedVersion(self, protocolVersion):
        if protocolVersion in (1, 2, 3):
            self.__protocolVersion = protocolVersion
            return True

        self.getDebugger().getUI().writeln("Unsupported protocol version: %d.  Closing connection." % protocolVersion)
        return False

    def getProtocolVersion(self): return self.__protocolVersion
    def isSupportedMessage(self, type): return self.__supportedMessages[type]

    def registerValue(self, value):
        if value.isNull(): return "n"
        elif value.isUndefined(): return "u"
        elif value.isBoolean() and value.getValue(): return "t"
        elif value.isBoolean() and not value.getValue(): return "f"
        elif value.isNumber() and value.getValue() == 0: return "z"
        elif value.isString() and len(value.getValue()) == 0: return "e"
        else:
            id = self.__valueIdCounter
            self.__valueIdCounter += 1
            self.__values[id] = value
            return id

    def preprocess(self, script):
        logger.logDebug("preprocess", "Preprocessing: %s" % script)

        index = 0
        values = {}
        objects = {}
        specials = {}

        while True:
            valueref = re_valueref.search(script, index)
            objectref = re_objectref.search(script, index)
            specialref = re_specialref.search(script, index)
            matches = []
            if valueref: matches.append((valueref.start(), valueref))
            if objectref: matches.append((objectref.start(), objectref))
            if specialref: matches.append((specialref.start(), specialref))
            if matches:
                start, match = min(matches)
                if match:
                    index = match.end()
                    id = match.group(1)
                    if match == valueref: values[id] = True
                    elif match == objectref: objects[id] = True
                    elif match == specialref: specials[id] = True
                    continue
            break

        variables = []

        for valueid in values.keys():
            variables.append(("$" + valueid, self.__values.get(int(valueid))))

        for objectid in objects.keys():
            object = ESObject.getByPrintedId(int(objectid))
            if object: value = ESValue(ESValue.TYPE_OBJECT, object)
            else: value = None
            variables.append(("$$" + objectid, value))

        if specials:
            if specials.has_key("n"): variables.append(("$n", ESValue(ESValue.TYPE_NULL)))
            if specials.has_key("u"): variables.append(("$u", ESValue(ESValue.TYPE_UNDEFINED)))
            if specials.has_key("t"): variables.append(("$t", ESValue(ESValue.TYPE_BOOLEAN, True)))
            if specials.has_key("f"): variables.append(("$f", ESValue(ESValue.TYPE_BOOLEAN, False)))
            if specials.has_key("z"): variables.append(("$z", ESValue(ESValue.TYPE_NUMBER, 0)))
            if specials.has_key("e"): variables.append(("$e", ESValue(ESValue.TYPE_STRING, "")))

        variables.sort()

        for name, value in variables:
            logger.logDebug("preprocess", "Variable: %s => %s" % (name, value))

        return variables

    def setStopAt(self, when, value):
        changed = False
        if when == "script":
            changed = value != self.__stopAtScript
            self.__stopAtScript = value
        elif when == "exception":
            changed = value != self.__stopAtException
            self.__stopAtException = value
        elif when == "error":
            changed = value != self.__stopAtError
            self.__stopAtError = value
        elif when == "abort":
            changed = value != self.__stopAtAbort
            self.__stopAtAbort = value
        elif when == "gc":
            changed = value != self.__stopAtGC
            self.__stopAtGC = value

        if changed:
            message = SetConfigurationMessage(self, self.__stopAtScript, self.__stopAtException, self.__stopAtError, self.__stopAtAbort, self.__stopAtGC)
            self.getConnection().sendMessage(message)

    def getStopAt(self):
        return self.__stopAtScript, self.__stopAtException, self.__stopAtError, self.__stopAtAbort, self.__stopAtGC

    def setDefaultStopAt(when, value):
        Session.defaultStopAt[when] = value
    setDefaultStopAt = staticmethod(setDefaultStopAt)

    def getDefaultStopAt():
        return Session.defaultStopAt.get("script", True), Session.defaultStopAt.get("exception", False), Session.defaultStopAt.get("error", False), Session.defaultStopAt.get("abort", False), Session.defaultStopAt.get("gc", False)
    getDefaultStopAt = staticmethod(getDefaultStopAt)

    def getScript(self, id):
        if not self.__scriptsById.has_key(id):
            script = Script(self, id, Script.TYPE_OTHER, None, "")
            self.__scripts.append(script)
            self.__scriptsById[id] = script
        return self.__scriptsById[id]

    def getScripts(self):
        return self.__scripts

    def getScriptByIndex(self, index):
        if index >= len(self.__scripts): return None
        else: return self.__scripts[index]
