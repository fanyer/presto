from commands import CommandError

def cmdShow(invoker):
    invoker.checkUnsupportedOptions()

    argument = invoker.getArguments().strip()

    if not argument: raise CommandError, "%s: missing argument" % invoker.getName()

    if argument == "sessions":
        sessions = invoker.getDebugger().getSessions()

        if not sessions: return ["No sessions."]
        else: return ["#%-2d %s" % (index, session.getShortDescription()) for index, session in enumerate(sessions)]
    elif argument == "threads":
        session = invoker.getSession()
        runtimes = filter(lambda runtime: runtime.getCurrentThread() is not None, session.getRuntimes())

        if not runtimes: return ["No threads."]
        else:
            result = []
            for index, runtime in enumerate(runtimes):
                result.append("#%-2d %s" % (index, runtime.getShortDescription()))
                for thread in runtime.getThreads(): result.append("  %s" % thread.getShortDescription())
            return result
    elif argument == "scripts":
        session = invoker.getSession()
        scripts = session.getScripts()

        if not scripts: return ["No scripts."]
        else:
            result = []
            for index, script in enumerate(scripts):
                result.append("#%-2d %s (%d lines, %d characters)" % (index, script.getShortDescription(), script.getLength(), script.getLengthInCharacters()))
            return result
    elif argument == "breakpoints":
        session = invoker.getSession()
        breakpoints = session.getBreakpoints()

        if not breakpoints: return ["No breakpoints."]
        else:
            result = []
            for index, breakpoint in enumerate(breakpoints):
                if breakpoint.getEnabled():
                    result.append("Breakpoint %d at %s" % (breakpoint.getId(), str(breakpoint.getFunction())))
                else:
                    result.append("Breakpoint %d (disabled) at %s" % (breakpoint.getId(), str(breakpoint.getFunction())))
            return result
    else: raise CommandError, "%s: invalid argument: %s" % (invoker.getName(), argument)

def cmdSelect(invoker):
    invoker.checkUnsupportedOptions()

    arguments = invoker.getArguments().strip().split(' ')

    if not arguments: raise CommandError, "%s: missing argument" % invoker.getName()
    if len(arguments) != 2: raise CommandError, "%s: invalid arguments: %s" % (invoker.getName(), invoker.getArguments())

    what = arguments[0]
    if not what in ("session", "thread"): raise CommandError, "%s: invalid argument: %s (expected 'session' or 'thread')" % (invoker.getName(), what)
    
    try: which = int(arguments[1])
    except ValueError: raise CommandError, "%s: invalid argument: %s (expected integer)" % (invoker.getName(), arguments[1])

    if what == "session":
        sessions = invoker.getDebugger().getSessions()

        if which < 0 or len(sessions) <= which: raise CommandError, "%s: no such session" % invoker.getName()
        else: invoker.getDebugger().setCurrentSession(sessions[which])
    elif what == "thread":
        session = invoker.getSession()
        runtimes = filter(lambda runtime: runtime.getCurrentThread() is not None, session.getRuntimes())

        if which < 0 or len(runtimes) <= which: raise CommandError, "%s: no such thread" % invoker.getName()
        else: session.setCurrentRuntime(runtimes[which])

    thread = invoker.getSession().getCurrentThread()
    if thread:
        position = thread.getCurrentPosition()
        if position: return [position.getLineForOutput()]

    return []

__commands__ = { "show": cmdShow,
                 "select": cmdSelect }
