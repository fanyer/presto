from commands import CommandError
from os import access, R_OK
from os.path import isfile

def cmdPrint(invoker):
    preprocess = not invoker.hasOption("*")
    convertToString = invoker.hasOption("s")
    invoker.checkUnsupportedOptions()

    if invoker.getName() == "print":
        if not invoker.getArguments(): raise CommandError, "%s: missing argument" % invoker.getName()

        script = invoker.getArguments().strip()
        expression = True
    elif invoker.getName() == "evaluate":
        if invoker.getArguments():
            argument = invoker.getArguments().strip()
            if not argument.startswith("<<"): raise CommandError, "%s: unexpected argument: %s" % (invoker.getName(), argument)
            delimiter = argument[2:]
        else: delimiter = None

        lines = []
        while True:
            line = invoker.getDebugger().readLine("> ")
            if delimiter:
                if line == delimiter: break
            elif line is None: break
            if line is not None: lines.append(line)

        script = "\n".join(lines) + "\n"
        expression = False
    else:
        if not invoker.getArguments(): raise CommandError, "%s: missing argument" % invoker.getName()

        path = invoker.getArguments().strip()

        if not isfile(path): raise CommandError, "%s: no such file: %s" % (invoker.getName(), path)
        elif not access(path, R_OK): raise CommandError, "%s: file not readable: %s" % (invoker.getName(), path)

        script = open(path, "r").read()
        expression = False

    try: result = invoker.getRuntime().eval(script, expression, preprocess)
    except KeyboardInterrupt: result = None

    if result:
        id = invoker.getDebugger().getCurrentSession().registerValue(result)
        if convertToString:
            try: result = invoker.getRuntime().eval("''+$%d" % id, True, True)
            except KeyboardInterrupt: result = None
            if result and result.isString(): return [result.getValue()]
            else: return ["Not a string."]
        else: return ["  $%s = %s" % (id, result)]

__commands__ = { "print": cmdPrint,
                 "p": ("print", cmdPrint),
                 "execute": cmdPrint,
                 "evaluate": cmdPrint }
