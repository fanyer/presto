from commands import CommandError
from message import Message

def frameDescription(index, frame):
    if frame[0]: spec = "%s ()" % frame[0].getFunctionName()
    else: spec = "<program>"
    return "#%-2d %s at %s" % (index, spec, frame[3].getShortDescription())

def cmdBacktrace(invoker):
    invoker.checkUnsupportedOptions()

    if invoker.getArguments(): raise CommandError, "%s: unexpected arguments: %s" % (invoker.getName(), invoker.getArguments())
    if not invoker.getSession().isSupportedMessage(Message.TYPE_BACKTRACE): raise CommandError, "%s: command not supported in this session" % invoker.getName()

    runtime = invoker.getRuntime()
    thread = invoker.getThread()

    try: stack = runtime.getStack()
    except KeyboardInterrupt: raise CommandError, "%s: interrupted" % invoker.getName()

    result = []
    for index, frame in enumerate(stack): result.append(frameDescription(index, frame))
    return result

def cmdFrame(invoker):
    invoker.checkUnsupportedOptions()

    if invoker.getName() == "frame" and not invoker.getArguments(): raise CommandError, "%s: missing argument" % invoker.getName()

    if invoker.getArguments():
        try: frameIndex = int(invoker.getArguments().strip())
        except ValueError: raise CommandError, "%s: invalid argument: %s" % (invoker.getName(), invoker.getArguments())
    else: frameIndex = 1

    runtime = invoker.getRuntime()

    if invoker.getName() == "up": frameIndex = runtime.getCurrentFrame() + frameIndex
    elif invoker.getName() == "down": frameIndex = runtime.getCurrentFrame() - frameIndex

    if frameIndex < 0: raise CommandError, "%s: no such frame: %d" % (invoker.getName(), frameIndex)

    frame = runtime.getStackFrame(frameIndex)
    position = runtime.getPosition(frameIndex)

    if frame is None or position is None: raise CommandError, "%s: no such frame: %d" % (invoker.getName(), frameIndex)

    runtime.setCurrentFrame(frameIndex)

    return [frameDescription(frameIndex, frame), position.getLineForOutput()]

__commands__ = { "backtrace": cmdBacktrace,
                 "bt": ("backtrace", cmdBacktrace),
                 "where": cmdBacktrace,
                 "frame": cmdFrame,
                 "f": ("frame", cmdFrame),
                 "up": cmdFrame,
                 "down": cmdFrame }
