# -*- indent-tabs-mode: nil; tab-width: 4 -*-

from commands import CommandError

commands = {}

def readCommands():
    global commands

    if commands == {}:
        commands = readCommandsFromFile("manual.txt")

def readCommandsFromFile(filename):
    try: input = open(filename)
    except IOError: return {}

    lines = input.readlines()
    i=0
    commands = {}
    while i < len(lines):
        if lines[i].find("@cmd") == 0:
            acc=[""]
            names=lines[i][4:].strip().split(",")
            i = i + 1
            while i < len(lines) and lines[i].find("@cmd") == -1 and lines[i].find("@end") == -1:
                acc.append(lines[i].rstrip())
                i = i + 1
            for name in names:
                commands[name] = acc
        else:
            i = i + 1

    return commands

def cmdHelp(invoker):
    global commands

    invoker.checkUnsupportedOptions();

    readCommands()

    argument = invoker.getArguments().strip()

    if not argument: 
        result = ["", "Help is available on the following topics"]
        for name in commands:
            result.append("  %s" % name)
        result.append("")
        return result
    else:
        if argument in commands:
            return commands[argument]
        else:
            return ["Unknown command '%s'" % argument]

__commands__ = { "help": cmdHelp, "?": cmdHelp }
