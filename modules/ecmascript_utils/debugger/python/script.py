import re
from codecs import getencoder
from os import mkdir
from os.path import exists, join
from configuration import configuration

class Script:
    TYPE_INLINE    = 1
    TYPE_LINKED    = 2
    TYPE_GENERATED = 3
    TYPE_OTHER     = 4

    def __init__(self, runtime, id, type, uri, source):
        self.__runtime = runtime
        self.__id = id
        self.__type = type
        self.__uri = uri
        self.__source = source
        self.__lines = [self.__encode(line) for line in re.split("\n\r|\r\n|\n|\r", source)]
        self.__filename = None
        self.__offset = 0

    def getRuntime(self):
        return self.__runtime

    def getId(self):
        return self.__id

    def getType(self):
        return self.__type

    def getURI(self):
        return self.__uri

    def getSource(self):
        return self.__source

    def getLine(self, linenr, includeLineNr=True):
        if linenr < 1: return "<unknown line>"
        elif linenr <= len(self.__lines):
            if includeLineNr:
                format = "%%-%dd %%s" % len(str(len(self.__lines)))
            return format % (linenr, self.__lines[linenr - 1])
        else: return "<invalid line>"

    def getLength(self):
        return len(self.__lines)

    def getLengthInCharacters(self):
        return len(self.__source)

    def adjustLineNr(self, linenr):
        return self.__offset + linenr

    def getShortDescription(self):
        if self.__type == Script.TYPE_INLINE:
            return "<inline script>"
        elif self.__type == Script.TYPE_LINKED:
            return self.__uri
        elif self.__type == Script.TYPE_GENERATED:
            return "<generated script>"
        else:
            return "<unknown script>"

    def getFileName(self):
        if self.__filename is None:
            directory = configuration['gud-temporary-files']

            if not exists(directory):
                mkdir(directory)

            self.__filename = join(directory, 'script-%d' % self.__id)
            self.__offset = 5

            file = open(self.__filename, "w")

            header = "/* -*- coding: utf-8 -*-\n * Frame path: %s\n * Document URI: %s\n"
            header = header % (self.__runtime.getFramePath(), self.__runtime.getDocumentURI())

            if self.__type == Script.TYPE_LINKED:
                header += " * Script URI: %s\n" % self.__uri
                self.__offset += 1

            text = header + " */\n\n" + '\n'.join(self.__lines) + '\n'

            file.write(getencoder("utf-8")(text)[0])

        return self.__filename
        
    def __encode(self, string):
        codec = getencoder(configuration['encoding'])

        try: return codec(string)[0]
        except UnicodeEncodeError:
            result = ""

            for ch in string:
                try: result += codec(ch)[0]
                except UnicodeEncodeError: result += repr(ch)[2:-1]

            return result
