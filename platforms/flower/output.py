
"""
Logging facilities.

The module is automatically made available as the name output in the
global namespaces in the context of which the flow files and
configuration files are interpreted.
"""

import os
import sys
import fcntl
import select
import threading
import errno
import traceback

import errors

class OutputLine(object):
    """
    A line of output captured from a child process.
    """

    def __init__(self, text, stderr):
        """
        Constructor.

        @param text The content of the line, without the terminating
        newline character.

        @param stderr True if the line was written to the standard
        error, false if it was written to the standard output.
        """
        self.text = text
        self.stderr = stderr

class _OutputStream(object):
    """
    A stream capturing output from either stdout or stderr of a child
    process.
    """

    def __init__(self, sink, stderr, process, logger):
        """
        Constructor.

        @param sink The OutputSink object to which to report the
        captured lines.

        @param stderr True if capturing stderr, false if capturing
        stdout.

        @param process The flow.Process object representing the child
        process from which output is being captured.

        @param logger The Logger object for real-time logging or None.
        """
        self._sink = sink
        self._stderr = stderr
        self._process = process
        self._logger = logger
        self._rfd, self._wfd = os.pipe()
        self._buf = ''
        self._eof = False
        fcntl.fcntl(self._rfd, fcntl.F_SETFL, fcntl.fcntl(self._rfd, fcntl.F_GETFL) | os.O_NONBLOCK)
        _manager.addStream(self, self._rfd)

    @property
    def fd(self):
        "File descriptor to which the monitored process should write."
        return self._wfd

    def read(self):
        """
        Read and process as much data as is currently available in the
        pipe buffer.
        """
        try:
            while not self._eof:
                text = os.read(self._rfd, 4096)
                if text:
                    if self._logger:
                        if self._stderr:
                            self._logger.realtimeStderr(self._process, text)
                        else:
                            self._logger.realtimeStdout(self._process, text)
                    lines = (self._buf + text).split('\n')
                    self._sink.addLines([OutputLine(ln, self._stderr) for ln in lines[:-1]])
                    self._buf = lines[-1]
                else:
                    self._eof = True
                    if self._buf:
                        if self._logger:
                            if self._stderr:
                                self._logger.realtimeStderr(self._process, self._buf)
                            else:
                                self._logger.realtimeStdout(self._process, self._buf)
                        self._sink.addLines([OutputLine(self._buf, self._stderr)])
        except EnvironmentError, e:
            if e.errno not in (errno.EAGAIN, errno.EWOULDBLOCK):
                raise

    def close(self):
        """
        Finalize the stream. Will read and process the remaining
        buffered data.
        """
        _manager.removeStream(self, self._rfd)
        os.close(self._wfd)
        fcntl.fcntl(self._rfd, fcntl.F_SETFL, fcntl.fcntl(self._rfd, fcntl.F_GETFL) & ~os.O_NONBLOCK)
        self.read()
        os.close(self._rfd)

class OutputSink(object):
    """
    An output sink for capturing stdout and stderr from a child
    process. Ties together two _OutputStream objects and collects the
    OutputLine objects that they produce.
    """

    def __init__(self, process, logger):
        """
        Constructor.

        @param process The flow.Process object representing the child
        process from which output is being captured.

        @param logger The Logger object for real-time logging or None.
        """
        self._lines = []
        self._stdout = _OutputStream(self, False, process, logger)
        self._stderr = _OutputStream(self, True, process, logger)

    @property
    def stdoutFD(self):
        "File descriptor for the writing end of the stdout pipe."
        return self._stdout.fd

    @property
    def stderrFD(self):
        "File descriptor for the writing end of the stderr pipe."
        return self._stderr.fd

    @property
    def lines(self):
        "A list of OutputLine objects collected so far."
        return self._lines

    def addLines(self, lines):
        """
        Add more output lines. Called by _OutputStream.

        @param lines A list of OutputLine objects.
        """
        self._lines += lines

    def close(self):
        """
        Finalize the streams. Will read and process the remaining
        buffered data.
        """
        self._stdout.close()
        self._stderr.close()

class _OutputManager(threading.Thread):
    """
    A background thread for capturing the output of child processes.
    Singleton.
    """

    def __init__(self):
        """
        Constructor. Starts the thread as a daemon.
        """
        threading.Thread.__init__(self, name='OutputSink')
        self._streams = {}
        self._rfd_control, self._wfd_control = os.pipe()
        self._poll = select.poll()
        self._poll.register(self._rfd_control, select.POLLIN)
        self.daemon = True
        self.start()

    def addStream(self, stream, fd):
        """
        Register a stream object which should receive notifications
        when data are available for reading from a pipe.

        @param stream The _OutputStream object. The thread will call
        read on it when data are available.

        @param fd The file descriptor for the reading end of a pipe.
        """
        assert fd not in self._streams
        self._streams[fd] = stream
        self._poll.register(fd, select.POLLIN)
        os.write(self._wfd_control, '*')

    def removeStream(self, stream, fd):
        """
        Unregister a stream object previously registered with
        addStream.

        @param stream The _OutputStream object.

        @param fd The file descriptor as passed to addStream.
        """
        assert self._streams[fd] is stream
        del self._streams[fd]
        self._poll.unregister(fd)
        os.write(self._wfd_control, '*')

    def run(self):
        "Main loop of the background thread. Does not return."
        while True:
            for fd, event in self._poll.poll():
                if fd == self._rfd_control:
                    # The fd set is updated, restart the poll
                    os.read(self._rfd_control, 1)
                elif fd in self._streams:
                    self._streams[fd].read()

_manager = _OutputManager()

def _formatError(e, indent=''):
    """
    Format the information about an error.

    @param e A Node or Process object that has failed, or an exception.

    @param indent Whitespace to prepend to each line.

    @return Formatted error message.
    """
    if hasattr(e, 'error'):
        if isinstance(e.error[1], errors.CommandFailed):
            res = str(e.error[1])
        else:
            res = ''.join(traceback.format_exception(*e.error))
    else:
        res = str(e)
    return indent + res.replace('\n', '\n' + indent).strip() + '\n'

class Logger(object):
    """
    The base class for loggers.
    """

    def systemMessage(self, text):
        """
        A system-wide event described by text has occurred. An example is a message about the time elapsed while building, which Flower issues before exiting.

        @param text Message text.
        """

    def goalStarting(self, goal):
        """
        The goal is starting.

        @param goal Goal (a Node object).
        """

    def goalCompleted(self, goal):
        """
        The goal has completed successfuly.

        @param goal Goal (a Node object).
        """

    def goalFailed(self, goal, errorList):
        """
        The goal has failed.

        @param goal Goal (a Node object).

        @param errorList A list of failed nodes, commands, and
        exceptions that are not related to any particular node or
        command.
        """

    def nodeStarting(self, node):
        """
        A flow is starting.

        @param node The Node object.
        """

    def nodeCompleted(self, node):
        """
        A flow has completed successfuly.

        @param node The Node object.
        """

    def nodeFailed(self, node):
        """
        A flow has failed.

        @param node The Node object.
        """

    def processStarting(self, process):
        """
        A child process is starting.

        @param process The Process object.
        """

    def processCompleted(self, process):
        """
        A child process has completed successfuly.

        @param process The Process object.
        """

    def processFailed(self, process):
        """
        A child process has failed.

        @param process The Process object.
        """

    def realtimeStdout(self, process, text):
        """
        A child process has written text to its stdandard output.

        @param process The Process object.

        @param text Captured text. Can contain several lines or just a
        part of a line. No newline characters are stripped from it.
        """

    def realtimeStderr(self, process, text):
        """
        A child process has written text to its stdandard error.

        @param process The Process object.

        @param text Captured text. Can contain several lines or just a
        part of a line. No newline characters are stripped from it.
        """

class ConsoleLogger(Logger):
    """
    A logger providing real-time logging to the standard output and
    standard error.
    """

    BLACK		= 0
    RED			= 1
    GREEN		= 2
    YELLOW		= 3
    BLUE		= 4
    MAGENTA		= 5
    CYAN		= 6
    WHITE		= 7

    BRIGHT		= 8

    _stream = {False: sys.stdout, True: sys.stderr}
    _isatty = {False: sys.stdout.isatty(), True: sys.stderr.isatty()}

    def __init__(self, colorize, echoMessage, echoCommands, echoStdout, echoStderr):
        """
        Constructor.

        @param colorize Whether to colorize the output with ANSI
        control sequences (this is only performed if the outupt stream
        is a TTY).

        @param echoMessage Whether to print the progress messages of
        commands that have them.

        @param echoCommands Whether to print the commands that are
        about to be executed.

        @parma echoStdout Whether to display the standard output of
        the invoked commands.

        @parma echoStderr Whether to display the standard error of the
        invoked commands.
        """
        self._colorize = colorize
        self._echoMessage = echoMessage
        self._echoCommands = echoCommands
        self._echoStdout = echoStdout
        self._echoStderr = echoStderr

    def _print(self, text, color, stderr=False):
        """
        Print text in a given color.

        @param text The text to print. No newline will be appended
        automatically.

        @param color Color specified using the constants defined in
        this class.

        @param stderr True if the text should be written to the
        standard error, false if it should go to the standard output.
        """
        if self._colorize and self._isatty[stderr]:
            assert color | 0x0f == 0x0f
            if color & self.BRIGHT:
                csr = "1;" + str((color & 0x07) + 30)
            else:
                csr = str(color + 30)
            self._stream[stderr].write("\x1b[{0}m{1}\x1b[m".format(csr, text))
        else:
            self._stream[stderr].write(text)

    def _println(self, text, color, stderr=False):
        "Same as _print, but a newline is appended to the text."
        self._print(text + '\n', color, stderr)

    def systemMessage(self, text):
        self._println(text, self.CYAN)

    def processStarting(self, process):
        if self._echoMessage:
            msg = process.message
            if msg:
                self._println(msg, self.GREEN)
        if self._echoCommands:
            self._println(process.command, self.BLUE)

    def goalCompleted(self, goal):
        if 'result' in goal:
            self._println(goal['result'], self.GREEN)
        else:
            self._println('The goal is up to date.', self.GREEN)

    def goalFailed(self, goal, errorList):
        msg = "*** Build failed:\n"
        for e in errorList:
            msg += _formatError(e, '    ')
        self._print(msg, self.RED, stderr=True)

    def realtimeStdout(self, process, text):
        if self._echoStdout:
            self._print(text, self.WHITE)

    def realtimeStderr(self, process, text):
        if self._echoStderr:
            self._print(text, self.YELLOW, stderr=True)

class FileLogger(Logger):
    """
    Base class for file-based loggers.
    """

    def __init__(self, filename, keep=0):
        """
        Constructor.

        @param filename The name of the log file to write into.

        @param keep The number of old log files to keep. The existing
        files are rotated so that build.log becomes build.log.1,
        build.log.1 becomes build.log.2 and so on. If 0, no old log
        files are kept, and the log file is truncated when logging is
        started.
        """
        for i in xrange(keep, 1, -1):
            try:
                os.rename("{0}.{1}".format(filename, i - 1), "{0}.{1}".format(filename, i))
            except EnvironmentError:
                pass
        if keep > 0:
            try:
                os.rename(filename, filename + '.1')
            except EnvironmentError:
                pass
        self._stream = open(filename, 'w', 1)

    def _print(self, text):
        """
        Write text to the log.

        @parm text The text to write. No newline will be appended
        automatically.
        """
        self._stream.write(text)

    def _println(self, text):
        "Same as _print, but a newline is appended to the text."
        self._print(text + '\n')

class TextFileLogger(FileLogger):
    """
    A logger writing a text file.

    The standard output and standard error of each command are logged
    at the moment the command completes rather than in real time. This
    prevents intermixing of output when several commands run in
    parallel.
    """

    def systemMessage(self, text):
        self._println(text)

    def goalCompleted(self, goal):
        if 'result' in goal:
            self._println(goal['result'])
        else:
            self._println('The goal is up to date.')

    def goalFailed(self, goal, errorList):
        self._println("*** Build failed:")
        for e in errorList:
            self._print(_formatError(e, '    '))

    def _logProcess(self, process):
        msg = process.message
        if msg:
            self._println(msg)
        self._println(process.command)
        for line in process.output:
            self._println(line.text)
        if process.state == process.FAILED and process.returncode is None:
            self._println("({0})".format(process.error[1]))
        elif process.returncode < 0:
            self._println("(Killed by signal: {0})".format(-process.returncode))
        elif process.returncode > 0:
            self._println("(Exit code: {0})".format(process.returncode))
        self._println('')

    def processCompleted(self, process):
        self._logProcess(process)

    def processFailed(self, process):
        self._logProcess(process)

class LoggerFacade(object):
    """
    A facade that keeps a list of active loggers and relays every
    event to every logger. Conforms to the Logger API, although it
    doesn't subclass Logger.

    Singleton available as log.
    """

    def __init__(self):
        self.loggers = []

    def __getattr__(self, name):
        "Response to an event."
        def do(*args, **kwargs):
            for logger in self.loggers:
                getattr(logger, name)(*args, **kwargs)
        return do

    def __iadd__(self, logger):
        """
        The += operator. Add a logger to the list of active loggers.

        @param logger A Logger object.
        """
        self.loggers.append(logger)
        return self

log = LoggerFacade()
