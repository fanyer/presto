from commands import CommandError
from position import Position
from configuration import configuration
from logger import logger

lastPrintedLines = None

def centered(linenr, listsize):
    return (linenr - (listsize - 1) / 2, linenr + (listsize - (listsize - 1) / 2 - 1))

def cmdList(invoker):
    global lastPrintedLines

    invoker.checkUnsupportedOptions()

    arguments = invoker.getArguments().strip()
    runtime = invoker.getRuntime()
    listsize = configuration['listsize']
    getCurrentScript = False
    getCurrentPosition = False

    if Position.lastLinePrinted:
        script = Position.lastLinePrinted.getScript()
        firstLine, lastLine = centered(Position.lastLinePrinted.getLineNr(), listsize)
        Position.lastLinePrinted = None
        lastWasList = False
    elif lastPrintedLines:
        script, firstLine, lastLine = lastPrintedLines
        lastWasList = True
    else:
        getCurrentScript = True
        getCurrentPosition = True
        lastWasList = False

    if arguments:
        if arguments == '-':
            if lastWasList:
                lastLine = firstLine - 1
                firstLine = lastLine - (listsize - 1)
        else:
            words = arguments.split()

            if words[0][0] == '#':
                try:
                    scriptindex = int(words[0][1:])
                    script = invoker.getSession().getScriptByIndex(scriptindex)
                    if not script: raise ValueError
                except ValueError:
                    raise CommandError, "%s: invalid script index: %s" % (invoker.getName(), words[0][1:])
                words = words[1:]
                firstLine, lastLine = centered(1 + (listsize - 1) / 2, listsize)
                getCurrentScript = False
                getCurrentPosition = False
            if words:
                try:
                    firstLine, lastLine = centered(int(words[0]), listsize)
                    getCurrentPosition = False
                except ValueError:
                    raise CommandError, "%s: listing function not implemented yet (use 'print/s <expression>' instead)" % invoker.getName()
    elif lastWasList:
        firstLine = lastLine + 1
        lastLine = firstLine + (listsize - 1)

    if getCurrentScript:
        # Triggers appropriate error message if there is no current thread.
        invoker.getThread()

        position = runtime.getPosition(runtime.getCurrentFrame())
        script = position.getScript()

        if getCurrentPosition:
            firstLine, lastLine = centered(position.getLineNr(), listsize)

    length = script.getLength()
    firstLine = max(1, firstLine)
    lastLine = min(length, lastLine)

    if firstLine < lastLine: result = [script.getLine(linenr) for linenr in range(firstLine, lastLine + 1)]
    else: result = []

    lastPrintedLines = (script, firstLine, lastLine)
    return result

__commands__ = { "list": cmdList }
