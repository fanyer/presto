from configuration import configuration

class Position:
    lastLinePrinted = None

    def __init__(self, script, linenr):
        self.__script = script
        self.__linenr = linenr

    def getScript(self):
        return self.__script

    def getLineNr(self):
        return self.__linenr

    def getLine(self, includeLineNr=True):
        return self.__script.getLine(self.__linenr, includeLineNr)

    def getShortDescription(self):
        return "%s line %d" % (self.__script.getShortDescription(), self.__linenr)

    def getLineForOutput(self):
        Position.lastLinePrinted = self

        if configuration['gud']:
            filename = self.getScript().getFileName()
            linenr = self.getScript().adjustLineNr(self.getLineNr())
            return "\x1a\x1a%s:%d:%d:" % (filename, linenr, 0)
        else: return self.getScript().getLine(self.getLineNr())
