class ESObject:
    printedIdCounter = 1
    printedIdMap = {}

    def __init__(self, session, id):
        self.__session = session
        self.__id = id
        self.__printedId = None
        self.__prototype = None
        self.__isCallable = False
        self.__isFunction = False
        self.__name = "Object"

    def update(self, prototype, isCallable, isFunction, name):
        self.__prototype = prototype
        self.__isCallable = isCallable
        self.__isFunction = isFunction
        self.__name = name

    def getId(self):
        return self.__id

    def getPrototype(self):
        return self.__prototype

    def isCallable(self):
        return self.__isCallable

    def isFunction(self):
        return self.__isFunction

    def getClassName(self):
        if self.__isFunction: return "Function"
        else: return self.__name

    def getFunctionName(self):
        return self.__name

    def __str__(self):
        if self.__isFunction: format = "$$%d = function %s() { ... }" 
        else: format = "$$%d = [object %s]"
        return format % (self.__getPrintedId(), self.__name)

    def __getPrintedId(self):
        if self.__printedId is None:
            self.__printedId = ESObject.printedIdCounter
            ESObject.printedIdMap[self.__printedId] = self
            ESObject.printedIdCounter += 1
        return self.__printedId

    def getByPrintedId(printedId):
        return ESObject.printedIdMap.get(printedId)
    getByPrintedId = staticmethod(getByPrintedId)
