from message import AddBreakpointMessage, RemoveBreakpointMessage

class Breakpoint:
    TYPE_INVALID  = -1
    TYPE_POSITION = 1
    TYPE_FUNCTION = 2
    TYPE_EVENT    = 3
    TYPE_USER     = 4

    idCounter = 1

    def getNextId():
        try: return Breakpoint.idCounter
        finally: Breakpoint.idCounter += 1
    getNextId = staticmethod(getNextId)

    def __init__(self, session):
        self.__session = session
        self.__id = Breakpoint.getNextId()
        self.__type = Breakpoint.TYPE_INVALID
        self.__position = None
        self.__function = None
        self.__eventType = None
        self.__enabled = False
        self.__commands = None

    def getId(self): return self.__id
    def getType(self): return self.__type
    def getCommands(self): return self.__commands and self.__commands[:] or []
    def getPosition(self): return self.__position
    def getFunction(self): return self.__function
    def getEventType(self): return self.__eventType
    def getEnabled(self): return self.__enabled

    def setCommands(self, commands):
        self.__commands = commands

    def setPosition(self, position):
        self.__type = Breakpoint.TYPE_POSITION
        self.__position = position

    def setFunction(self, function):
        self.__type = Breakpoint.TYPE_FUNCTION
        self.__function = function

    def setEventType(self, namespaceURI, eventType):
        self.__type = Breakpoint.TYPE_EVENT
        self.__eventType = (namespaceURI, eventType)

    def setUserType(self):
	self.__type = Breakpoint.TYPE_USER

    def enable(self):
        if not self.__enabled:
            message = AddBreakpointMessage(self.__session, self)
            self.__session.getConnection().sendMessage(message)
            self.__enabled = True

    def disable(self):
        if self.__enabled:
            message = RemoveBreakpointMessage(self.__session, self)
            self.__session.getConnection().sendMessage(message)
            self.__enabled = False
