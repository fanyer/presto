# -*- indent-tabs-mode: nil; tab-width: 4 -*-

from commands import CommandError
from position import Position

# Valid forms:
#
# 1  break
# 2  break <function>
# 3  break <line>
# 4  break #<script> <line>
#
# where form 1 and 3 imply the use of the script to which the current
# execution frame belongs, and form 1 implies the current line number
# of that script.

def cmdBreak(invoker):
    preprocess = not invoker.hasOption("*")
    invoker.checkUnsupportedOptions()

    arguments = invoker.getArguments().strip()
    runtime = invoker.getRuntime()
    position = runtime.getPosition(runtime.getCurrentFrame())

    if not arguments:
        if not position:
            raise CommandError, "%s: no current position" % invoker.getName()
        id = invoker.getSession().addBreakpointAtPosition(position)
        at = position.getShortDescription()
    else:
        words = arguments.split()
        linenr_required = False

        if words[0][0] == '#':
            linenr_required = True
            linenr_string = words[1]
            try:
                scriptindex = int(words[0][1:])
                script = invoker.getSession().getScriptByIndex(scriptindex)
                if not script: 
                    raise ValueError
            except ValueError:
                raise CommandError, "%s: invalid script index: %s" % (invoker.getName(), words[0][1:])
        else:
            linenr_string = words[0]
            script = False

        try:
            linenr = int(linenr_string)
            if not script:
                if not position: 
                    raise CommandError, "%s: no current script" % invoker.getName()
                script = position.getScript()
            position = Position(script, linenr)
            id = invoker.getSession().addBreakpointAtPosition(position)
            at = position.getShortDescription()
        except ValueError:
            if linenr_required:
                raise CommandError, "%s: line number required" % invoker.getName()
            try:
                result = runtime.eval(words[0], True, preprocess)
            except KeyboardInterrupt: 
                return []
            if not result.isObject() or not result.getValue().isFunction(): 
                raise CommandError, "%s: not a function: %s" % (invoker.getName(), result)
            id = invoker.getSession().addBreakpointAtFunction(result.getValue())
            at = str(result)

    return ["Breakpoint %d at %s." % (id, at)]

def cmdDisableOrEnableOrDelete(invoker):
    invoker.checkUnsupportedOptions()

    argument = invoker.getArguments().strip()
    session = invoker.getSession()

    if not argument: raise CommandError, "%s: missing argument" % invoker.getName()

    try: id = int(argument)
    except ValueError: raise CommandError, "%s: invalid argument: %s" % (invoker.getName(), argument)

    breakpoint = session.getBreakpoint(id)

    if not breakpoint: return ["No such breakpoint."]

    if invoker.getName() == "disable":
        breakpoint.disable()
    elif invoker.getName() == "delete": 
        breakpoint.disable()
        session.removeBreakpoint(id)
    else:
        breakpoint.enable()

    return []

__commands__ = { "break": cmdBreak,
                 "b": ("break", cmdBreak),
                 "disable": cmdDisableOrEnableOrDelete,
                 "delete": cmdDisableOrEnableOrDelete,
                 "enable": cmdDisableOrEnableOrDelete }
