from codecs import getencoder

from script import Script
from position import Position
from esvalue import ESValue
from logger import logger
from socketmanager import Close

class MessageData:
    def __init__(self, size=0, length=0, data=""):
        self.__size = size
        self.__length = length
        self.__data = data
        self.__messageText = data

    def __iadd__(self, other):
        self.__size += len(other)
        self.__length += 1
        self.__data += other

    def __isub__(self, other):
        self.__size -= other
        self.__length -= 1
        self.__data = self.__data[other:]

    def __nonzero__(self):
        return self.__length > 0

    def __getitem__(self, index):
        return self.__data[index]

    def __str__(self):
        return self.__data

    def getMessageText(self):
        return self.__messageText

    def parseBoolean(self):
        value = self.__data[0] == "t"
        self -= 1
        return value

    def parseInteger(self):
        if self.__data[1] == "-": sign = -1
        else: sign = 1
        if self.__data[0] == "i":
            value = sign * int(self.__data[2:6], 16)
            self -= 6
        else:
            value = sign * int(self.__data[2:10], 16)
            self -= 10
        return value

    def parseUnsigned(self):
        if self.__data[0] == "u":
            value = int(self.__data[1:5], 16)
            self -= 5
        else:
            value = int(self.__data[1:9], 16)
            self -= 9
        return value

    # Python does not have syntax for infinities or NaN, and apparently
    # cannot represent them either.

    def parseDouble(self):
        length = int(self.__data[1:3], 16)
	valuestr = self.__data[3:3 + length]
	if valuestr == "Infinity":
	    value = "Infinity"
	elif valuestr == "-Infinity":
	    value = "-Infinity"
	elif valuestr == "NaN":
	    value = "NaN"
	else:
            value = float(valuestr)
        self -= 3 + length
        return value

    def parseString(self):
        if self.__data[0] == "s":
            length = int(self.__data[1:5], 16)
            logger.logDebug("parsing", "parseString: " + repr(self.__data[5:5 + length]))
            value = unicode(self.__data[5:5 + length], "utf-8")
            self -= 5 + length
        else:
            length = int(self.__data[1:9], 16)
            logger.logDebug("parsing", "parseString: " + repr(self.__data[9:9 + length]))
            value = unicode(self.__data[9:9 + length], "utf-8")
            self -= 9 + length
        return value

    def parseValue(self, session):
        type = self.parseUnsigned()
        if type <= ESValue.TYPE_BOOLEAN:
            value = self.parseBoolean()
        elif type == ESValue.TYPE_NUMBER:
            value = self.parseDouble()
        elif type == ESValue.TYPE_STRING:
            value = self.parseString()
        else:
            value = session.getObject(self.parseUnsigned())
        return ESValue(type, value)

    def parseAny(self):
        if self.__data[0] == "t" or self.__data[0] == "f":
            return self.parseBoolean()
        elif self.__data[0] == "i" or self.__data[0] == "I":
            return self.parseInteger()
        elif self.__data[0] == "u" or self.__data[0] == "U":
            return self.parseUnsigned()
        elif self.__data[0] == "d":
            return self.parseDouble()
        elif self.__data[0] == "s" or self.__data[0] == "S":
            return self.parseString()
        else:
            raise Exception()

    def addBoolean(self, value):
        if value: self += "t"
        else: self += "f"

    def addInteger(self, value):
        if value < 0: sign = "-"
        else: sign = "+"
        if abs(value) <= 65535: format = "i%c%04x"
        else: format = "I%c%08x"
        self += format % (sign, abs(value))

    def addUnsigned(self, value):
        if value <= 65535: format = "u%04x"
        else: format = "U%08x"
        self += format % value

    def addDouble(self, value):
        value = str(value)
        self += "d%02x%s" % (len(value), value)

    def addString(self, value):
        value = getencoder("utf-8")(value)[0]
        if len(value) <= 65535: format = "s%04x%s"
        else: format = "S%08x%s"
        self += format % (len(value), value)

    def addValue(self, value):
        self.addUnsigned(value.getType())
        if value.isUndefined() or value.isNull():
            self.addBoolean(False)
        elif value.isBoolean():
            self.addBoolean(value.getValue())
        elif value.isNumber():
            self.addDouble(value.getValue())
        elif value.isString():
            self.addString(value.getValue())
        else:
            self.addUnsigned(value.getValue().getId())

    def getData(self):
        return "%08x%04x%s" % (self.__size, self.__length, self.__data)

class Message:
    TYPE_HELLO             = 1
    TYPE_NEW_SCRIPT        = 2
    TYPE_THREAD_STARTED    = 3
    TYPE_THREAD_FINISHED   = 4
    TYPE_STOPPED_AT        = 5
    TYPE_CONTINUE          = 6
    TYPE_EVAL              = 7
    TYPE_EVAL_REPLY        = 8
    TYPE_EXAMINE           = 9
    TYPE_EXAMINE_REPLY     = 10
    TYPE_ADD_BREAKPOINT    = 11
    TYPE_REMOVE_BREAKPOINT = 12
    TYPE_SET_CONFIGURATION = 13
    TYPE_BACKTRACE         = 14
    TYPE_BACKTRACE_REPLY   = 15
    TYPE_BREAK             = 16

    TYPE_GENERIC           = -1
    TYPE_FIRST             = TYPE_NEW_SCRIPT
    TYPE_LAST              = TYPE_BREAK
    
    tagCounter = 1

    def __init__(self, session, type):
        self.__session = session
        self.__type = type
        self.__auxiliary = []

    def getSession(self):
        return self.__session

    def getType(self):
        return self.__type

    def getAuxiliary(self, type=None):
        if type is None: return self.__auxiliary[:]
        else: return [auxiliary for auxiliary in self.__auxiliary if auxiliary.getType() == type]

    def addAuxiliary(self, auxiliary):
        self.__auxiliary.append(auxiliary)

    def parseMessage(session, data):
        type = data.parseUnsigned()
        messageType = messageTypes.get(type, GenericMessage)
        logger.logDebug("message", "Parsing message: %r" % messageType)
        message = messageType(session)
        try: message.parse(data)
        except Close: raise
        except:
            logger.logException()
            logger.logDebug("parsing-failed", data.getMessageText())
        return message
    parseMessage = staticmethod(parseMessage)

    def getTag():
        try: return Message.tagCounter
        finally: Message.tagCounter += 1
    getTag = staticmethod(getTag)

    def parseAuxiliary(self, data):
        logger.logDebug("parsing", str(data))

        while data:
            try: type = data.parseUnsigned()
            except:
                logger.logDebug("auxbug", "While parsing: %d" % self.getType())
                logger.logException()
            auxiliaryType = auxiliaryTypes.get(type)
            if not auxiliaryType:
                logger.logWarning("Unknown auxiliary type: %d (skipping rest of message)" % type)
                while data: data.parseAny()
            else:
                auxiliary = auxiliaryType(self)
                auxiliary.parse(data)
                self.__auxiliary.append(auxiliary)

    def appendAuxiliary(self, data):
        for auxiliary in self.__auxiliary: auxiliary.append(data)

class GenericMessage(Message):
    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_GENERIC)
        self.__items = []

    def parse(self, data):
        logger.logDebug("generic", "Parsing generic message")
        while data: self.__items.append(data.parseAny())
        logger.logDebug("generic", "Parsed %d items" % len(self.__items))

class HelloMessage(Message):
    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_HELLO)
        self.__protocolVersion = None
        self.__os = None
        self.__platform = None
        self.__userAgent = None
        self.__supportedMessages = {}

    def getProtocolVersion(self):
        return self.__protocolVersion

    def getOS(self):
        return self.__os

    def getPlatform(self):
        return self.__platform

    def getUserAgent(self):
        return self.__userAgent

    def getSuppertedMessages(self):
        return self.__supportedMessages

    def parse(self, data):
        self.__protocolVersion = data.parseUnsigned()
        if not self.getSession().isSupportedVersion(self.__protocolVersion):
            raise Close
        self.__os = data.parseString()
        self.__platform = data.parseString()
        self.__userAgent = data.parseString()
        supportedMessages1 = data.parseUnsigned()
        for message in range(Message.TYPE_FIRST, Message.TYPE_LAST + 1):
            if supportedMessages1 & (1 << (message - 1)): self.__supportedMessages[message] = True
            else: self.__supportedMessages[message] = False

class NewScriptMessage(Message):
    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_NEW_SCRIPT)
        self.__runtime = None
        self.__script = None

    def __str__(self):
        raise Exception, "NewScriptMessage.__str__ not implemented"

    def getRuntime(self):
        return self.__runtime

    def getScript(self):
        return self.__script

    def parse(self, data):
        self.__runtime = self.getSession().getRuntime(data.parseUnsigned())

        id = data.parseUnsigned()
        type = data.parseUnsigned()
        source = data.parseString()

        if type == Script.TYPE_LINKED: uri = data.parseString()
        else: uri = None

        self.__script = Script(self.__runtime, id, type, uri, source)
        self.parseAuxiliary(data)

class ThreadStartedMessage(Message):
    TYPE_INLINE   = 1
    TYPE_EVENT    = 2
    TYPE_URL      = 3
    TYPE_TIMEOUT  = 4
    TYPE_JAVA     = 5
    TYPE_OTHER    = 6

    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_THREAD_STARTED)
        self.__runtime = None
        self.__thread = None
        self.__parentThread = None

    def __str__(self):
        raise Exception, "ThreadStartedMessage.__str__ not implemented"

    def getRuntime(self):
        return self.__runtime

    def getThread(self):
        return self.__thread

    def getParentThread(self):
        return self.__parentThread

    def parse(self, data):
        self.__runtime = self.getSession().getRuntime(data.parseUnsigned())
        self.__thread = self.__runtime.getThread(data.parseUnsigned())
        self.__thread.setParentThread(self.__runtime.getThread(data.parseUnsigned()))
        self.__thread.setType(data.parseUnsigned())

        logger.logDebug("threads", "New thread: %d" % self.__thread.getId())
        if self.__thread.getParentThread():
            logger.logDebug("threads", "Parent thread: %d" % self.__thread.getParentThread().getId())
        else:
            logger.logDebug("threads", "No parent thread")

        if self.__thread.getType() == ThreadStartedMessage.TYPE_EVENT:
            self.__thread.setNamespaceURI(data.parseString())
            self.__thread.setEventType(data.parseString())

        self.parseAuxiliary(data)

class ThreadFinishedMessage(Message):
    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_THREAD_FINISHED)
        self.__runtime = None
        self.__thread = None

    def __str__(self):
        raise Exception, "ThreadFinishedMessage.__str__ not implemented"

    def getRuntime(self):
        return self.__runtime

    def getThread(self):
        return self.__thread

    def parse(self, data):
        self.__runtime = self.getSession().getRuntime(data.parseUnsigned())
        self.__thread = self.__runtime.getThread(data.parseUnsigned())
        self.__thread.setStatus(data.parseUnsigned())
        self.parseAuxiliary(data)

class StoppedAtMessage(Message):
    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_STOPPED_AT)
        self.__runtime = None
        self.__thread = None
        self.__position = None

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.__runtime.getId())
        data.addUnsigned(self.__thread.getId())
        data.addUnsigned(self.__position.getScript().getId())
        data.addUnsigned(self.__position.getLineNr())
        return data.getData()

    def getRuntime(self):
        return self.__runtime

    def getThread(self):
        return self.__thread

    def getPosition(self):
        return self.__position

    def parse(self, data):
        logger.logDebug("parsing", "StoppedAt: " + str(data))
        
        self.__runtime = self.getSession().getRuntime(data.parseUnsigned())
        self.__thread = self.__runtime.getThread(data.parseUnsigned())

        script = self.getSession().getScript(data.parseUnsigned())
        linenr = data.parseUnsigned()

        self.parseAuxiliary(data)

        logger.logDebug("parsing", "Parsed StoppedAt: %d\n" % linenr)

        self.__position = Position(script, linenr)
        self.__thread.setCurrentPosition(self.__position)

class ContinueMessage(Message):
    def __init__(self, session, runtime=None, thread=None, mode=None):
        Message.__init__(self, session, Message.TYPE_CONTINUE)
        self.__runtime = runtime
        self.__thread = thread
        self.__mode = mode

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.getType())
        data.addUnsigned(self.__runtime.getId())
        data.addUnsigned(self.__thread.getId())
        data.addUnsigned(self.__mode)
        return data.getData()

    def getRuntime(self):
        return self.__runtime

    def getThread(self):
        return self.__thread

    def parse(self, data):
        self.__runtime = self.getSession().getRuntime(data.parseUnsigned())
        self.__thread = self.__runtime.getThread(data.parseUnsigned())
        self.__mode = data.parseUnsigned()

class EvalMessage(Message):
    def __init__(self, session, runtime=None, thread=None, script=None, frameIndex=0):
        Message.__init__(self, session, Message.TYPE_EVAL)
        self.__tag = Message.getTag()
        self.__runtime = runtime
        self.__thread = thread
        self.__frameIndex = frameIndex
        self.__script = script

    def getTag(self):
        return self.__tag

    def getRuntime(self):
        return self.__runtime

    def getThread(self):
        return self.__thread

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.getType())
        data.addUnsigned(self.__tag)
        data.addUnsigned(self.__runtime.getId())
        data.addUnsigned(self.__thread and self.__thread.getId() or 0)
        data.addUnsigned(self.__frameIndex)
        data.addString(self.__script)
        self.appendAuxiliary(data)
        return data.getData()

class EvalReplyMessage(Message):
    FINISHED  = 1
    EXCEPTION = 2
    ABORTED   = 3
    CANCELLED = 4

    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_EVAL_REPLY)
        self.__tag = None
        self.__status = None

    def getTag(self):
        return self.__tag

    def getStatus(self):
        return self.__status

    def parse(self, data):
        self.__tag = data.parseUnsigned()
        self.__status = data.parseUnsigned()
        self.parseAuxiliary(data)

class ExamineMessage(Message):
    def __init__(self, session, runtime=None, properties=None, thread=None, frameIndex=0, objects=None):
        Message.__init__(self, session, Message.TYPE_EXAMINE)
        self.__tag = Message.getTag()
        self.__runtime = runtime
        self.__thread = thread
        self.__frameIndex = frameIndex
        self.__properties = properties
        if objects is not None: self.__objects = objects
        else: self.__objects = []

    def getTag(self): return self.__tag
    def getProperties(self): return self.__properties

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.getType())
        data.addUnsigned(self.__tag)
        if self.getSession().getProtocolVersion() >= 2:
            data.addUnsigned(self.__runtime.getId())
        if self.__thread is not None:
            if self.getSession().getProtocolVersion() >= 3:
                data.addBoolean(True)
                data.addUnsigned(self.__thread.getId())
                data.addUnsigned(self.__frameIndex)
            else:
                raise "Not supported by protocol."
        else:
            if self.getSession().getProtocolVersion() >= 3:
                data.addBoolean(False)
            data.addUnsigned(len(self.__objects))
            for object in self.__objects: data.addUnsigned(object.getId())
        return data.getData()

class ExamineReplyMessage(Message):
    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_EXAMINE_REPLY)
        self.__tag = None
        self.__objects = []

    def getTag(self):
        return self.__tag

    def getObjectsCount(self):
        return len(self.__objects)

    def getObject(self, index):
        return self.__objects[index][0]

    def getObjectProperties(self, index):
        return self.__objects[index][1][:]

    def parse(self, data):
        self.__tag = data.parseUnsigned()
        objectsCount = data.parseUnsigned()
        for oindex in range(objectsCount):
            object = self.getSession().getObject(data.parseUnsigned())
            properties = []
            propertiesCount = data.parseUnsigned()
            for pindex in range(propertiesCount):
                name = data.parseString()
                value = data.parseValue(self.getSession())
                properties.append((name, value))
            self.__objects.append((object, properties))
        self.parseAuxiliary(data)

class AddBreakpointMessage(Message):
    TYPE_POSITION = 1
    TYPE_FUNCTION = 2
    TYPE_EVENT    = 3

    def __init__(self, session, breakpoint):
        Message.__init__(self, session, Message.TYPE_ADD_BREAKPOINT)
        self.__breakpoint = breakpoint

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.getType())
        data.addUnsigned(self.__breakpoint.getId())
        data.addUnsigned(self.__breakpoint.getType())

        if self.__breakpoint.getType() == AddBreakpointMessage.TYPE_POSITION:
            data.addUnsigned(self.__breakpoint.getPosition().getScript().getId())
            data.addUnsigned(self.__breakpoint.getPosition().getLineNr())
        elif self.__breakpoint.getType() == AddBreakpointMessage.TYPE_FUNCTION:
            data.addUnsigned(self.__breakpoint.getFunction().getId())
        else:
            data.addString(self.__breakpoint.getEventType()[0])
            data.addString(self.__breakpoint.getEventType()[1])

        return data.getData()

class RemoveBreakpointMessage(Message):
    def __init__(self, session, breakpoint):
        Message.__init__(self, session, Message.TYPE_REMOVE_BREAKPOINT)
        self.__breakpoint = breakpoint

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.getType())
        data.addUnsigned(self.__breakpoint.getId())
        return data.getData()

class SetConfigurationMessage(Message):
    def __init__(self, session, stopAtScript, stopAtException, stopAtError, stopAtAbort, stopAtGC):
        Message.__init__(self, session, Message.TYPE_SET_CONFIGURATION)
        self.__stopAtScript = stopAtScript
        self.__stopAtException = stopAtException
        self.__stopAtError = stopAtError
        self.__stopAtAbort = stopAtAbort
        self.__stopAtGC = stopAtGC

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.getType())
        data.addBoolean(self.__stopAtScript)
        data.addBoolean(self.__stopAtException)
        data.addBoolean(self.__stopAtError)
        data.addBoolean(self.__stopAtAbort)
        data.addBoolean(self.__stopAtGC)
        return data.getData()

class BacktraceMessage(Message):
    def __init__(self, session, thread, maxFrames=0):
        Message.__init__(self, session, Message.TYPE_BACKTRACE)
        self.__tag = Message.getTag()
        self.__thread = thread
        self.__maxFrames = maxFrames

    def getTag(self): return self.__tag
    def getThread(self): return self.__thread

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.getType())
        data.addUnsigned(self.__tag)
        data.addUnsigned(self.__thread.getRuntime().getId())
        data.addUnsigned(self.__thread.getId())
        data.addUnsigned(self.__maxFrames)
        return data.getData()

class BacktraceReplyMessage(Message):
    def __init__(self, session):
        Message.__init__(self, session, Message.TYPE_BACKTRACE_REPLY)
        self.__tag = None
        self.__stack = []

    def getTag(self): return self.__tag
    def getStack(self): return self.__stack[:]

    def parse(self, data):
        self.__tag = data.parseUnsigned()
        for index in range(data.parseUnsigned()):
            function = self.getSession().getObject(data.parseUnsigned())
            arguments = self.getSession().getObject(data.parseUnsigned())
            variables = self.getSession().getObject(data.parseUnsigned())
            scriptid = self.getSession().getScript(data.parseUnsigned())
            linenr = data.parseUnsigned()
            self.__stack.append((function, arguments, variables, Position(scriptid, linenr)))
        self.parseAuxiliary(data)

class BreakMessage(Message):
    def __init__(self, session, runtime=None, thread=None):
        Message.__init__(self, session, Message.TYPE_BREAK)
        self.__runtime = runtime
        self.__thread = thread

    def __str__(self):
        data = MessageData()
        data.addUnsigned(self.getType())
        data.addUnsigned(self.__runtime.getId())
        data.addUnsigned(self.__thread.getId())
        return data.getData()

    def getRuntime(self):
        return self.__runtime

    def getThread(self):
        return self.__thread

    def parse(self, data):
        self.__runtime = self.getSession().getRuntime(data.parseUnsigned())
        self.__thread = self.__runtime.getThread(data.parseUnsigned())

class Auxiliary:
    TYPE_OBJECT_INFO          = 1
    TYPE_WATCH_UPDATE         = 2
    TYPE_STACK                = 3
    TYPE_BREAKPOINT_TRIGGERED = 4
    TYPE_RETURN_VALUE         = 5
    TYPE_EXCEPTION_THROWN     = 6
    TYPE_EVAL_VARIABLE        = 7
    TYPE_RUNTIME_INFORMATION  = 8
    TYPE_HEAP_STATISTICS      = 9

    def __init__(self, message, type):
        self.__message = message
        self.__type = type

    def getMessage(self):
        return self.__message

    def getSession(self):
        return self.getMessage().getSession()

    def getType(self):
        return self.__type

class ObjectInfoAuxiliary(Auxiliary):
    def __init__(self, message):
        Auxiliary.__init__(self, message, Auxiliary.TYPE_OBJECT_INFO)
        self.__object = None
        self.__prototype = None
        self.__isCallable = False
        self.__isFunction = False
        self.__name = None

    def parse(self, data):
        self.__object = self.getSession().getObject(data.parseUnsigned())
        prototype_id = data.parseUnsigned()
        if prototype_id == 0: self.__prototype = None
        else: self.__prototype = self.getSession().getObject(prototype_id)
        self.__isCallable = data.parseBoolean()
        self.__isFunction = data.parseBoolean()
        self.__name = data.parseString()
        self.__object.update(self.__prototype, self.__isCallable, self.__isFunction, self.__name)

class ReturnValueAuxiliary(Auxiliary):
    def __init__(self, message):
        Auxiliary.__init__(self, message, Auxiliary.TYPE_RETURN_VALUE)
        self.__function = None
        self.__value = None

    def getFunction(self):
        return self.__function

    def getValue(self):
        return self.__value

    def parse(self, data):
        functionid = data.parseUnsigned()
        if functionid == 0: self.__function = None
        else: self.__function = self.getSession().getObject(functionid)
        self.__value = data.parseValue(self.getSession())

class ExceptionThrownAuxiliary(Auxiliary):
    def __init__(self, message):
        Auxiliary.__init__(self, message, Auxiliary.TYPE_EXCEPTION_THROWN)
        self.__exception = None

    def getException(self):
        return self.__exception

    def parse(self, data):
        self.__exception = data.parseValue(self.getSession())

class BreakpointTriggeredAuxiliary(Auxiliary):
    def __init__(self, message):
        Auxiliary.__init__(self, message, Auxiliary.TYPE_BREAKPOINT_TRIGGERED)
        self.__breakpoint = None

    def getBreakpoint(self):
        return self.__breakpoint

    def parse(self, data):
        self.__breakpoint = self.getSession().getBreakpoint(data.parseUnsigned())

class EvalVariableAuxiliary(Auxiliary):
    def __init__(self, message, name=None, value=None):
        Auxiliary.__init__(self, message, Auxiliary.TYPE_EVAL_VARIABLE)
        self.__name = name
        self.__value = value

    def parse(self, data):
        self.__name = data.parseString()
        self.__value = data.parseValue(self.getSession())

    def append(self, data):
        data.addUnsigned(self.getType())
        data.addString(self.__name)
        data.addValue(self.__value)

class RuntimeInformationAuxiliary(Auxiliary):
    def __init__(self, message):
        Auxiliary.__init__(self, message, Auxiliary.TYPE_RUNTIME_INFORMATION)

    def parse(self, data):
        runtime = self.getSession().getRuntime(data.parseUnsigned())

        windowid = data.parseUnsigned()
        framepath = data.parseString()
        documenturi = data.parseString()

        runtime.setInformation(windowid, framepath, documenturi)

class HeapStatisticsAuxiliary(Auxiliary):
    def __init__(self, message):
        Auxiliary.__init__(self, message, Auxiliary.TYPE_HEAP_STATISTICS)

    def getNumCollections(self): return self.__numCollections
    def getMsCollecting(self): return self.__msCollecting
    def getLoadFactor(self): return self.__loadFactor
    def getBytesHeapLimit(self): return self.__bytesHeapLimit
    def getBytesAllocated(self): return self.__bytesAllocated
    def getBytesReclaimed(self): return self.__bytesReclaimed
    def getBytesInHeap(self): return self.__bytesInHeap
    def getBytesInHeapPeak(self): return self.__bytesInHeapPeak
    def getBytesLiveAfterGC(self): return self.__bytesLiveAfterGC
    def getBytesFreeAfterGC(self): return self.__bytesFreeAfterGC
    def getObjectsLiveAfterGC(self): return self.__objectsLiveAfterGC
    def getObjectsFreeAfterGC(self): return self.__objectsFreeAfterGC

    def parse(self, data):
        self.__loadFactor = data.parseDouble()
        self.__numCollections = data.parseUnsigned()
        self.__msCollecting = data.parseUnsigned()
        self.__bytesHeapLimit = data.parseUnsigned()
        self.__bytesAllocated = data.parseUnsigned()
        self.__bytesReclaimed = data.parseUnsigned()
        self.__bytesInHeap = data.parseUnsigned()
        self.__bytesInHeapPeak = data.parseUnsigned()
        self.__bytesLiveAfterGC = data.parseUnsigned()
        self.__bytesFreeAfterGC = data.parseUnsigned()
        self.__objectsLiveAfterGC = data.parseUnsigned()
        self.__objectsFreeAfterGC = data.parseUnsigned()

messageTypes = { Message.TYPE_HELLO: HelloMessage,
                 Message.TYPE_NEW_SCRIPT: NewScriptMessage,
                 Message.TYPE_THREAD_STARTED: ThreadStartedMessage,
                 Message.TYPE_THREAD_FINISHED: ThreadFinishedMessage,
                 Message.TYPE_STOPPED_AT: StoppedAtMessage,
                 Message.TYPE_EVAL_REPLY: EvalReplyMessage,
                 Message.TYPE_EXAMINE_REPLY: ExamineReplyMessage,
                 Message.TYPE_BACKTRACE_REPLY: BacktraceReplyMessage }

auxiliaryTypes = { Auxiliary.TYPE_OBJECT_INFO: ObjectInfoAuxiliary,
                   Auxiliary.TYPE_RETURN_VALUE: ReturnValueAuxiliary,
                   Auxiliary.TYPE_EXCEPTION_THROWN: ExceptionThrownAuxiliary,
                   Auxiliary.TYPE_BREAKPOINT_TRIGGERED: BreakpointTriggeredAuxiliary,
                   Auxiliary.TYPE_RUNTIME_INFORMATION: RuntimeInformationAuxiliary,
                   Auxiliary.TYPE_HEAP_STATISTICS: HeapStatisticsAuxiliary }
