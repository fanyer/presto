class ESValue:
    TYPE_NULL      = 1
    TYPE_UNDEFINED = 2
    TYPE_BOOLEAN   = 3
    TYPE_NUMBER    = 4
    TYPE_STRING    = 5
    TYPE_OBJECT    = 6

    def __init__(self, type, value=False):
        self.__type = type
        self.__value = value

    def __str__(self):
        if self.__type == ESValue.TYPE_NULL:
            return "null"
        elif self.__type == ESValue.TYPE_UNDEFINED:
            return "undefined"
        elif self.__type == ESValue.TYPE_BOOLEAN:
            if self.__value: return "true"
            else: return "false"
        elif self.__type == ESValue.TYPE_NUMBER:
            string = str(self.__value)
            if string.endswith(".0"): return string[:-2]
            else: return string
        elif self.__type == ESValue.TYPE_STRING:
            return repr(self.__value)[1:]
        elif self.__type == ESValue.TYPE_OBJECT:
            return str(self.__value)

    def getType(self):
        return self.__type

    def isNull(self):
        return self.__type == ESValue.TYPE_NULL

    def isUndefined(self):
        return self.__type == ESValue.TYPE_UNDEFINED

    def isBoolean(self):
        return self.__type == ESValue.TYPE_BOOLEAN

    def isNumber(self):
        return self.__type == ESValue.TYPE_NUMBER

    def isString(self):
        return self.__type == ESValue.TYPE_STRING

    def isObject(self):
        return self.__type == ESValue.TYPE_OBJECT

    def getValue(self):
        return self.__value

    def toStringLimited(self):
        if self.__type == ESValue.TYPE_STRING and len(self.__value) > 40: return repr(self.__value[:40])[1:] + "..."
        else: return str(self)
