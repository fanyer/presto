from message import ExamineMessage
from threading import Condition
from re import compile

re_number = compile("[0-9]+")
re_identifier = compile("[$_A-Za-z][$_0-9A-Za-z]*")

def propertyName(name):
    if re_number.match(name) and long(name) < 0xffffffffl or re_identifier.match(name): return name
    else: return repr(name)

class Examiner:
    def __init__(self, runtime, object, expandPrototypes, expandObjects, filterIn, filterOut, names, inScope):
        self.__runtime = runtime
        self.__object = object
        self.__cond = Condition()
        self.__waiting = False
        self.__processed = None
        self.__queue = None
        self.__properties = {}
        self.__expandPrototypes = expandPrototypes
        self.__expandObjects = expandObjects
        self.__filterIn = filterIn
        self.__filterOut = filterOut
        self.__names = {}
        for name in names: self.__names[name] = True
        self.__inScope = inScope
        self.__scope = []

    def getObject(self):
        return self.__object

    def getProperties(self, object):
        return self.__properties.get(object.getId(), [])

    def getScope(self): return self.__scope[:]

    def update(self):
        self.__cond.acquire()
        try:
            while True:
                self.__processed = {}
                self.__queue = []

                if self.__inScope:
                    if self.__scope:
                        for object in self.__scope: self.__pushObject(object)
                        if not self.__queue: break
                else:
                    self.__pushObject(self.__object)
                    if not self.__queue: break

                message = ExamineMessage(self.__runtime.getSession(), self.__runtime, self, self.__inScope and not self.__queue and self.__runtime.getCurrentThread() or None, self.__runtime.getCurrentFrame(), self.__queue)

                self.__tag = message.getTag()
                self.__runtime.getSession().addQuery(self.__tag, message)
                self.__runtime.getSession().getConnection().sendMessage(message)

                self.__waiting = True
                try:
                    while self.__waiting: self.__cond.wait(0.1)
                finally: self.__waiting = False

                if self.__inScope and not self.__scope: break
        finally:
            self.__cond.release()

    def examineFinished(self, objects):
        self.__cond.acquire()
        try:
            if self.__inScope and not self.__scope: self.__scope = [object for object, properties in objects]
            for object, properties in objects: self.__properties[object.getId()] = properties

            self.__waiting = False
            self.__cond.notify()
        finally:
            self.__cond.release()

    def __pushObject(self, object, depth=0):
        if object and not self.__processed.has_key(object.getId()):
            self.__processed[object.getId()] = True
            if not self.__properties.has_key(object.getId()): self.__queue.append(object)
            if depth < self.__expandPrototypes: self.__pushObject(object.getPrototype(), depth + 1)
            if depth < self.__expandObjects:
                for name, value in self.__properties.get(object.getId()):
                    if value.isObject() and self.__include(name): self.__pushObject(value.getValue(), depth + 1)

    def __include(self, name):
        if self.__filterIn: return self.__names.has_key(name)
        elif self.__filterOut: return not self.__names.has_key(name)
        else: return True

    def getObjectPropertiesAsStrings(self, object, depth=0):
        indent = "  " * (depth + 1)
        strings = []

        self.__processed[object.getId()] = True

        if depth == 0:
            strings.append("%s = {" % object)
            self.__processed = {}

        properties = self.__properties.get(object.getId(), [])
        properties = [(name, value) for name, value in properties if self.__include(name)]

        if depth < self.__expandPrototypes and object.getPrototype():
            comma = properties and "," or ""
            if not self.__processed.has_key(object.getPrototype().getId()):
                strings.append("%s[[Prototype]]: %s = {" % (indent, object.getPrototype()))
                strings.extend(self.getObjectPropertiesAsStrings(object.getPrototype(), depth + 1))
                strings.append("%s}%s" % (indent, comma))
            else:
                strings.append("%s[[Prototype]]: %s%s <already expanded>" % (indent, object.getPrototype(), comma))

        for index, (name, value) in enumerate(properties):
            comma = index + 1 < len(properties) and "," or ""
            if value.isObject() and depth < self.__expandObjects:
                if not self.__processed.has_key(value.getValue().getId()):
                    strings.append("%s%s: %s = {" % (indent, propertyName(name), value.toStringLimited()))
                    strings.extend(self.getObjectPropertiesAsStrings(value.getValue(), depth + 1))
                    strings.append("%s}%s" % (indent, comma))
                else: strings.append("%s%s: %s%s <already expanded>" % (indent, propertyName(name), value.toStringLimited(), comma))
            else: strings.append("%s%s: %s%s" % (indent, propertyName(name), value.toStringLimited(), comma))

        if depth == 0: strings.append("}")
        return strings

    def getObjectProperties(self, object):
        properties = self.__properties.get(object.getId(), [])
        return [(name, value) for name, value in properties if self.__include(name)]
