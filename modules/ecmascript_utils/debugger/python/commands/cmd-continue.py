from commands import CommandError
from esthread import ESThread
from esruntime import ContinueError

def cmdContinue(invoker):
    invoker.checkUnsupportedOptions()

    if invoker.getArguments(): raise CommandError, "%s: invalid arguments: %s" % (invoker.getName(), invoker.getArguments())

    runtime = invoker.getRuntime()
    thread = invoker.getThread()

    if thread.getStatus() != ESThread.STATUS_STOPPED: raise CommandError, "%s: thread not stopped" % invoker.getName()

    mode = { "step": ESThread.MODE_STEPINTO,
             "next": ESThread.MODE_STEPOVER,
             "finish": ESThread.MODE_FINISH,
             "continue": ESThread.MODE_RUN }[invoker.getName()]

    try: runtime.continueThread(mode)
    except ContinueError, e: raise CommandError, e[0]

__commands__ = { "step": cmdContinue,
                 "s": ("step", cmdContinue),
                 "next": cmdContinue,
                 "n": ("next", cmdContinue),
                 "finish": cmdContinue,
                 "continue": cmdContinue,
                 "c": ("continue", cmdContinue) }
