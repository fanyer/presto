from commands import CommandError
from configuration import configuration
from session import Session

def getBooleanValue(name, value):
    if value in ("true", "on", "yes", "1"): return True
    elif value in ("false", "off", "no", "0"): return False
    else: raise CommandError, "%s: invalid boolean value: %s" % (name, value)

def getNumberValue(name, value):
    try: return int(value)
    except ValueError: raise CommandError, "%s: invalid number value: %s" % (name, value)

def setStopAt(session, when, value):
    if session: session.setStopAt(when, value)
    else: Session.setDefaultStopAt(when, value)

def cmdSet(invoker):
    invoker.checkUnsupportedOptions()

    session = invoker.getDebugger().getCurrentSession()
    variables = []

    if session: stopAtScript, stopAtException, stopAtError, stopAtAbort, stopAtGC = session.getStopAt()
    else: stopAtScript, stopAtException, stopAtError, stopAtAbort, stopAtGC = Session.getDefaultStopAt()

    variables.append(("stop-at-script", stopAtScript and "true" or "false"))
    variables.append(("stop-at-exception", stopAtException and "true" or "false"))
    variables.append(("stop-at-error", stopAtError and "true" or "false"))
    variables.append(("stop-at-abort", stopAtAbort and "true" or "false"))
    variables.append(("stop-at-gc", stopAtGC and "true" or "false"))
    variables.append(("listsize", str(configuration['listsize'])))

    if not invoker.getArguments(): return ["%s: %s" % (name, value) for name, value in variables]
    else:
        words = invoker.getArguments().split(' ', 1)
        if len(words) == 1:
            for name, value in variables:
                if name == words[0]: return ["%s: %s" % (name, value)]
            else: raise CommandError, "%s: unknown variable: %s" % (invoker.getName(), words[0])
        else:
            for name, value in variables:
                if name == words[0]:
                    if name == "stop-at-script": setStopAt(session, "script", getBooleanValue(invoker.getName(), words[1]))
                    elif name == "stop-at-exception": setStopAt(session, "exception", getBooleanValue(invoker.getName(), words[1]))
                    elif name == "stop-at-error": setStopAt(session, "error", getBooleanValue(invoker.getName(), words[1]))
                    elif name == "stop-at-abort": setStopAt(session, "abort", getBooleanValue(invoker.getName(), words[1]))
                    elif name == "stop-at-gc": setStopAt(session, "gc", getBooleanValue(invoker.getName(), words[1]))
                    elif name == "listsize": configuration['listsize'] = getNumberValue(invoker.getName(), words[1])
                    return
            else: raise CommandError, "%s: unknown variable: %s" % (invoker.getName(), words[0])

__commands__ = { "set": cmdSet }
