from commands import CommandError
from examiner import Examiner

def cmdExamine(invoker):
    preprocess = not invoker.hasOption("*")
    expandPrototypes = invoker.hasOption("p")
    expandObjects = invoker.hasOption("x")
    filterIn = invoker.hasOption("+")
    filterOut = invoker.hasOption("-")
    invoker.checkUnsupportedOptions()

    if not invoker.getArguments(): raise CommandError, "%s: missing argument" % invoker.getName()
    if filterIn and filterOut: raise CommandError, "%s: options '+' and '-' are mutually exclusive" % invoker.getName()

    arguments = invoker.getArguments()
    limit = 0xffff

    if expandObjects or expandPrototypes:
        words = arguments.split(' ', 1)
        if len(words) > 1:
            try:
                limit = int(words[0])
                arguments = words[1]
            except ValueError: pass

    if filterIn or filterOut:
        words = arguments.split(' ', 1)
        if len(words) != 2: raise CommandError, "%s: missing filter argument" % invoker.getName()
        filterArgument = words[0]
        arguments = words[1]
        if filterArgument[0] != '{' or filterArgument[-1] != '}': raise CommandError, "%s: invalid filter argument: %s" % (invoker.getName(), filterArgument)
        names = filterArgument[1:-1].split(',')
        if len(names) == 0: raise CommandError, "%s: invalid filter argument: %s" % (invoker.getName(), filterArgument)
    else:
        names = []

    try: result = invoker.getRuntime().eval(arguments, True, preprocess)
    except KeyboardInterrupt: result = None

    if result:
        if not result.isObject(): raise CommandError, "%s: not an object: %s" % (invoker.getName(), arguments)

        examiner = Examiner(invoker.getRuntime(), result.getValue(), expandPrototypes and limit or 0, expandObjects and limit or 0, filterIn, filterOut, names, False)
        examiner.update()

        return examiner.getObjectPropertiesAsStrings(result.getValue())

__commands__ = { "examine": cmdExamine,
                 "x": ("examine", cmdExamine) }
