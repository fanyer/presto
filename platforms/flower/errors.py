
"""
Custom exception classes.

The module is automatically made available as the name errors in the
global namespaces in the context of which the flow files and
configuration files are interpreted.
"""

import signal

class AmbiguousMatch(Exception):
    """
    Raised when instantiating a node or chaining in case of a tie
    between two or more matching flow variants with the highest
    priority (next highest, in case of chaining).
    """

    def __init__(self, name, attrs):
        """
        Constructor.

        @param name The name of the flow being instantiated.

        @param attrs A dictionary of initial node parameters passed by
        the caller.
        """
        self.name = name
        self.attrs = attrs

    def __str__(self):
        return "Ambiguous match: {name}({args})".format(name=self.name, args=', '.join(["{0}={1!r}".format(*item) for item in self.attrs.iteritems()]))

class NoMatch(Exception):
    """
    Raised when instantiating a node or chaining in case when no
    matching flow variants are found.
    """

    def __init__(self, name, attrs):
        """
        Constructor.

        @param name The name of the flow being instantiated.

        @param attrs A dictionary of initial node parameters passed by
        the caller.
        """
        self.name = name
        self.attrs = attrs

    def __str__(self):
        return "No match: {name}({args})".format(name=self.name, args=', '.join(["{0}={1!r}".format(*item) for item in self.attrs.iteritems()]))

class CommandFailed(Exception):
    """
    Raised when a command fails to start or returns a non-zero exit
    code.
    """

    def __init__(self, command, eType=None, eValue=None, eTraceback=None):
        """
        Constructor.

        If the command didn't fail to start, but rather returned a
        non-zero exit code, the last three arguments should be None.

        @param command The Command that failed.

        @param eType Exception type.

        @param eValue Exception value.

        @param eTraceback Exception traceback.
        """
        self.command = command
        self.error = (eType, eValue, eTraceback)

    def __str__(self):
        if self.error[1]:
            return "{err} ({c})".format(err=self.error[1], c=self.command.message or self.command.command)
        else:
            return "Command exited with return code {rc} ({c})".format(rc=self.command.returncode, c=self.command.message or self.command.command)

class CircularDependency(Exception):
    """
    Raised when a node indirectly waits for its own completion.
    """

    def __str__(self):
        return 'The nodes have circular dependencies'

class BuildInterrupted(Exception):
    """
    Raised when the build is interrupted with a signal.
    """

    def __init__(self, signal):
        """
        Constructor.

        @param signal The signal with which the build was interrupted.
        """
        self.signal = signal

    def __str__(self):
        if self.signal == signal.SIGHUP:
            return 'Hangup detected'
        elif self.signal == signal.SIGINT:
            return 'Keyboard interrupt'
        elif self.signal == signal.SIGQUIT:
            return 'Keyboard quit'
        elif self.signal == signal.SIGTERM:
            return 'Build terminated'
        else:
            return "Build killed with signal {0}".format(self.signal)

class CommandLineError(Exception):
    """
    Raised when parsing of the command-line arguments fails.
    """
