
"""
Flow subsystem.

The system automatically makes the names flow (the decorator), command
and goal available in the global namespace in the context of which the
flow files are interpreted. Flow files should not import this module
explicitly.
"""

import os
import sys
import subprocess
import signal
import errno
import string

import util
import config
import output
import errors

class _Flow(object):
    """
    A wrapper around a flow function created by the @flow decorator.
    """

    def __init__(self, function, patterns):
        """
        Constructor.

        @param function The flow function.
        
        @param patterns A dictionary mapping node parameter names to
        pattern matchers.
        """
        self._function = function
        self._patterns = patterns

    @property
    def function(self):
        "The flow function."
        return self._function

    @property
    def patterns(self):
        "A dictionary mapping node parameter names to pattern matchers."
        return self._patterns

    def match(self, kwargs):
        """
        Apply pattern matchers to the specified set of node parameters.

        @param kwargs A dictionary of node parameters.

        @return True iff all patterns match.
        """
        for key, pattern in self._patterns.items():
            if callable(pattern):
                if not pattern(kwargs.get(key)):
                    return False
            elif hasattr(pattern, 'search'):
                if not pattern.search(kwargs.get(key)):
                    return False
            elif pattern != kwargs.get(key):
                return False
        return True

class _FlowGroup(object):
    """
    A group of flow variants.
    """

    def __init__(self, name):
        """
        Constructor.

        @param name The name of the flow.
        """
        self._alternatives = {} # priority -> [_Flow]
        self._nodes = {}        # node key -> Node
        self._name = name

    def add(self, flow, priority):
        """
        Add a flow variant.

        @param flow A _Flow object.

        @param priority Priority (number).
        """
        if priority not in self._alternatives:
            self._alternatives[priority] = []
        self._alternatives[priority].append(flow)

    def __call__(self, **kwargs):
        """
        Create a Node or retrieve an existing instance. The same set
        of node parameters retrieves the same Node.

        @param **kwargs Node parameters.

        @return A Node object.
        """
        nodeKey = tuple(sorted(kwargs.items()))
        try:
            n = self._nodes[nodeKey]
        except KeyError:
            self._nodes[nodeKey] = n = Node(self._chain(kwargs), kwargs)
        return n

    def _chain(self, attrs):
        """
        An iterator over _Flow objects matching the node parameters, in
        the order of decreasing priority.

        @param attrs Node parameters.

        @throws errors.AmbiguousMatch More than one flow variant with
        the same priority matches.

        @throws errors.NoMatch No flow variants match. On subsequent
        iterations: no flow variants with lower priority match.
        """
        matched = False
        for priority in reversed(sorted(self._alternatives.keys())):
            candidate = None
            for flow in self._alternatives[priority]:
                if flow.match(attrs):
                    if candidate:
                        raise errors.AmbiguousMatch(self._name, attrs)
                    candidate = flow
            if candidate:
                yield candidate
                matched = True
        if not matched:
            raise errors.NoMatch(self._name, attrs)

def flow(_priority=0, **kwargs):
    """
    A decorator wrapping a flow function in a _Flow object. It adds the
    object into the _FlowGroup, creating one if it doesn't exist. The
    entire argument list is optional, not just the arguments.

    @param _priority _Flow variant priority.

    @param **kwargs Pattern matchers for named node parameters.
    """
    def decorating(f, priority, patterns):
        if f.func_name in f.func_globals:
            group = f.func_globals[f.func_name]
            assert isinstance(group, _FlowGroup)
        else:
            group = _FlowGroup(f.func_name)
        group.add(_Flow(f, patterns), priority)
        return group
    if callable(_priority):
        return decorating(_priority, 0, {})
    else:
        return lambda f: decorating(f, _priority, kwargs)

class Node(object):
    """
    A node.

    Conforms to the dictionary protocol and supports dictionary-like
    indexing with string keys, membership testing, and iteration. When
    used as a dictionary, represents the set of node parameters.

    Conforms to the context manager protocol and, when used in a with
    statement, becomes the current context for the duration of the
    statement's body.
    """

    PENDING     = 0 # Not currently running, but scheduled (not blocked).
    RUNNING     = 1 # Running. There is at most one such node.
    BLOCKED     = 2 # Suspended until some nodes and/or commands complete.
    COMPLETED   = 3 # Completed successfuly.
    FAILED      = 4 # Failed.

    def __init__(self, flows, attrs):
        """
        Constructor.

        This constructor is internal. User code should invoke a
        _FlowGroup as a function instead.

        @param flows A sequence of _Flow objects to run. The second and
        subsequent items of the sequence will only be used in case of
        chaining.

        @param attrs Initial node parameters. The constructor does not
        make a copy of this dictionary.
        """
        self._flowiter = iter(flows)
        self._attrs = attrs
        # The state must be RUNNING while flow code runs (which can
        # happen in _startgen if the flow function doesn't contain a
        # yield statement).
        self._state = Node.RUNNING
        self._generator = self._startgen(False)
        self._state = Node.PENDING if self._generator else Node.COMPLETED
        self._blocked = 0
        self._error = (None, None, None)

    @property
    def state(self):
        """
        The state of the node's flow. One of PENDING, RUNNING,
        BLOCKED, COMPLETED, FAILED.
        """
        return self._state

    @property
    def error(self):
        "A tuple (type, value, traceback) with information about the exception that caused the flow to fail."
        return self._error

    # Dictionary protocol implementation:
    def __contains__(self, key): return key in self._attrs
    def __getitem__(self, key): return self._attrs[key]
    def __setitem__(self, key, value): self._attrs[key] = value
    def __delitem__(self, key): del self._attrs[key]
    def __iter__(self): return iter(self._attrs)

    # Context manager protocol implementation:
    def __enter__(self):  config.pushContext(self)
    def __exit__(self, exc_type, exc_value, traceback): config.popContext(self)

    def __lt__(self, other):
        """
        The < operator. Check whether the node's target needs
        updating. Returns true iff any of the following is true:

        * The file named by self['target'] does not exist.

        * Any of the strings listed in other names a file that does
          not exist or is newer than the file named by self['target'].

        * The 'target' parameter of any of the nodes listed in other
          names a file that does not exist or is newer than the file
          named by self['target'].

        For Node objects appearing on the right-hand side, the
        optional 'changed' parameter serves as an override. If it's
        true, the dependency is considered to be newer than the
        target; if false, it's considered to be older. In either case,
        the existence and timestamp of the 'target' are not checked.

        @param other A node, a filename string, or a sequence of such
        items.

        @return True if self's target needs updating.
        """
        return util.needsUpdate(self, other)

    def _startgen(self, chained):
        """
        Obtain a flow variant from the chain iterator and call the
        flow function. Note that a flow function is a generator if it
        contains a yield statement, and an ordinary function
        otherwise.

        @param chained True if this is a chained (second or
        subsequent) invocation.

        @return The result of the call to the flow function (an
        iterator or None).

        @throws TypeError When chained is false, node parameters that
        are neither matched by patterns nor accepted as arguments by
        the flow function cause a TypeError.
        """
        try:
            flow = next(self._flowiter)
        except StopIteration:
            return None
        function = flow.function
        kwargs = {}
        for key in self._attrs:
            if util.takesArgument(function, key):
                kwargs[key] = self._attrs[key]
            elif not chained and key not in flow.patterns:
                raise TypeError("{f}() got an unexpected keyword argument '{arg}'".format(f=function.func_name, arg=key))
        with self:
            return function(self, **kwargs)

    def unblock(self):
        """
        Called by the scheduler when a node or command on which this
        node is blocked has completed. The state must be BLOCKED. If
        the node is not blocked on any more nodes or commands, the
        state changes to PENDING.
        """
        assert self._state == Node.BLOCKED and self._blocked > 0
        self._blocked -= 1
        if self._blocked == 0:
            self._state = Node.PENDING

    def step(self):
        """
        Run a slice of the flow until it yields or returns. The state
        must be PENDING, and becomes RUNNING for the duration of the
        slice, then changes to PENDING, BLOCKED, COMPLETED or FAILED.

        @return If the resulting state is BLOCKED, a sequence of nodes
        or commands on which the node is blocked. Otherwise a false
        value.
        """
        assert self._state == Node.PENDING and self._blocked == 0 and self._generator
        try:
            self._state = Node.RUNNING
            with self:
                yielded = self._generator.next()
            if yielded is self:
                self._generator.close()
                self._generator = self._startgen(True)
                self._state = Node.PENDING if self._generator else Node.COMPLETED
                return None
            blocked = util.ensureList(yielded)
            self._blocked = len(blocked)
            self._state = Node.BLOCKED if self._blocked else Node.PENDING
            return blocked
        except StopIteration:
            self._generator = None
            self._state = Node.COMPLETED
            if hasattr(self._flowiter, 'close'):
                self._flowiter.close()
            self._flowiter = None
            return None
        except:
            self._generator = None
            self._state = Node.FAILED
            self._error = sys.exc_info()
            if hasattr(self._flowiter, 'close'):
                self._flowiter.close()
            self._flowiter = None
            return None

    def close(self):
        """
        Finalize the node whose flow will not run anymore.

        Closes the flow if that's still in progress, ensuring that any
        outstanding finally blocks run.

        If the node has not completed successfully, this deletes the
        file named by self['target'], given such a parameter exists.
        """
        if self._flowiter is not None and hasattr(self._flowiter, 'close'):
            self._flowiter.close()
            self._flowiter = None
        if self._state != Node.COMPLETED:
            if 'target' in self:
                try:
                    os.unlink(self['target'])
                except EnvironmentError, e:
                    if e.errno != errno.ENOENT:
                        raise

class Process(object):
    """
    Abstract base class for child processes.
    """

    PENDING     = 0 # Not currently running, but scheduled.
    RUNNING     = 1 # Running.
    COMPLETED   = 2 # Completed succesfully.
    FAILED      = 3 # Failed to start or reported runtime failure.

    def __init__(self, startfunc):
        """
        Constructor.

        @param startfunc A callable to invoke for spawning the
        process.
        """
        self._startfunc = startfunc
        self._state = Process.PENDING
        self._process = None
        self._returncode = None
        self._error = (None, None, None)

    @property
    def state(self):
        "The state of the process."
        return self._state

    @property
    def pid(self):
        "The process ID or None if not started."
        return self._process.pid if self._process else None

    @property
    def returncode(self):
        "Exit code or None if not completed."
        return self._process.returncode if self._process else None

    @property
    def error(self):
        "The exception tuple, if state is FAILED, or (None, None, None)."
        return self._error

    message = None # Progress message.
    shell = False  # Whether to use the shell.
    cwd = None     # Working directory override.
    env = None     # Environment override.
    output = ()    # Lines of output produced by the process.

    @property
    def command(self):
        "The command string. Includes environment and cwd."
        cmd = ''
        if self.cwd:
            cmd += "cd {0} && ".format(util.shellEscape(self.cwd))
        if self.env:
            for k, v in self.env.iteritems():
                if v != os.env.get(k):
                    cmd += "{0}={1} ".format(k, util.shellEscape(v))
            for k, v in os.env.iteritems():
                if v and k not in self.env:
                    cmd += "{0}='' ".format(k)
        if self.shell:
            cmd += self.args[0]
        else:
            cmd += ' '.join([util.shellEscape(arg) for arg in self.args])
        return cmd

    def start(self, logger):
        """
        Start the process. The state must be PENDING and changes to
        RUNNING or FAILED.

        @param logger The logger object to use for real-time output
        logging.
        """
        assert self._state == Process.PENDING
        self._logger = logger
        try:
            self._process = self._startfunc()
            self._state = Process.RUNNING
        except:
            self.failedToStart(*sys.exc_info())

    def exited(self, returncode):
        """
        Handle process completion. Called by the scheduler after a
        child process has terminated, and its return code has been
        obtained. The state must be RUNNING and changes to COMPLETED.

        @param returncode Return code.
        """
        # This code calls an undocumented internal method
        # Popen._handle_exitstatus. This is unfortunately necessary
        # becase the API of the module does not provide a way to tell
        # it that the process has already been waited for, and a
        # return code has been obtained.
        assert self._state == Process.RUNNING
        self._process._handle_exitstatus(returncode)
        self._state = Process.COMPLETED

    def failedToStart(self, eType, eValue, eTraceback):
        """
        Handle failure to start. The state must be PENDING and changes
        to FAILED.

        @param eType Exception type.
        @param eValue Exception value.
        @param eTraceback Traceback.
        """
        assert self._state == Process.PENDING
        self._state = Process.FAILED
        self._error = (errors.CommandFailed, errors.CommandFailed(self, eType, eValue, eTraceback), None)

class Command(Process):
    """
    An asynchronous command invoked by a flow.
    """

    def __init__(self, args, message=None, fail=True, shell=False, cwd=None, env=None):
        """
        Constructor.

        @param args A sequence of command words (the command and its
        arguments) if shell is false, or a single command string if
        shell is true.

        @param message The progress message to display when starting
        the command. The string is interpreted as a new-style Python
        format string (as used with str.format) into which the
        parameters of the current node are interpolated as named
        items. None means no progress message.

        @param fail Specifies whether the command should be considered
        failed if it fails to start or completes with a non-zero exit
        code. If a boolean, true means fail on a non-zero exit code,
        and false means ignore it. If a callable, will be called under
        the described circumstances with the command object as the
        only argument to obtain a boolean-like value.

        @param shell Whether the command should be passed to the
        shell. If false, the command is invoked directly, and the
        arguments don't have to be escaped.

        @param cwd If not None, the directory which should be made
        current before executing the command. None leaves the current
        directory unchanged.

        @param env A dictionary of environment variables to pass to
        the child process. If None, the environment is inherited.
        """
        self._args = args
        self._message = string.Formatter().vformat(message, (), config.currentContext()) if message else None
        self._fail = fail
        self._shell = shell
        self._cwd = cwd
        self._env = env
        def start():
            self._sink = output.OutputSink(self, self._logger)
            return subprocess.Popen(args, close_fds=True, shell=shell, cwd=cwd, env=env, stdout=self._sink.stdoutFD, stderr=self._sink.stderrFD)
        Process.__init__(self, start)

    @property
    def args(self):
        "The value of args used to create the command."
        return self._args

    @property
    def message(self):
        "The value of message after interpolation."
        return self._message

    @property
    def shell(self):
        "The value of shell used to create the command."
        return self._shell

    @property
    def cwd(self):
        "The value of cwd used to create the command."
        return self._cwd

    @property
    def env(self):
        "The value of env used to create the command."
        return self._env

    @property
    def output(self):
        "The list of output.OutputLine objects for the output captured from the process."
        if self._sink:
            return self._sink.lines
        else:
            return []

    def exited(self, returncode):
        super(Command, self).exited(returncode)
        self._sink.close()
        if returncode:
            if callable(self._fail):
                if self._fail(self):
                    self._state = Process.FAILED
            elif self._fail:
                self._state = Process.FAILED
        if self._state == Process.FAILED:
            self._error = (errors.CommandFailed, errors.CommandFailed(self), None)

    def failedToStart(self, eType, eValue, eTraceback):
        super(Command, self).failedToStart(eType, eValue, eTraceback)
        if callable(self._fail):
            if not self._fail(self):
                self._state = Process.COMPLETED
        elif not self._fail:
            self._state = Process.COMPLETED

def make(goal, logger, quota=1, failEarly=True):
    """
    Execute the flow of the goal. This function is the entry point of
    the scheduler.

    @param goal The goal node.

    @param logger A Logger object, typically one that forwards to all
    others.

    @param quota Maximum number of parallel child processes to run.

    @param failEarly If true, exit as soon as possible (without
    killing any running child processes) after a node or a command
    fails. If false, continue running unaffected flows as long as
    possible (similar to make -k).

    @return A list of failed nodes, commands, and exceptions that are
    not related to any particular node or command. An empty list means
    that the goal has completed successfully.
    """
    pending = []         # [PENDING Node]
    blockedBy = {}       # blocking Node|Process -> [BLOCKED Node]
    procPending = []     # [PENDING Process]
    procRunning = {}     # pid -> RUNNING Process
    errorsCollected = [] # [Node|Process|Exception] to be returned
    interrupted = []     # [interrupting signal caught]
    failedNodes = set()  # set(FAILED Node)

    def unblock(on):
        """
        Unblock nodes blocked on a Node or Process that has completed.

        @param on Node or Process that has completed.
        """
        if on in blockedBy:
            for b in blockedBy[on]:
                b.unblock()
                if b.state == Node.PENDING:
                    pending.append(b)
            del blockedBy[on]

    def fail(obj):
        """
        Take note of a Node or Process that has failed.

        @param obj Node or Process that has failed.
        """
        if not interrupted: # don't record failures after an interrupting signal
            errorsCollected.append(obj)
        if failEarly:
            failedNodes.update(set(pending))
            pending[:] = []
            procPending[:] = []

    def signalHandler(signum, frame):
        interrupted.append(signum)

    try:
        signals = (signal.SIGHUP, signal.SIGINT, signal.SIGQUIT, signal.SIGTERM)
        oldSignalHandlers = {}
        for sig in signals:
            oldSignalHandlers[sig] = signal.getsignal(sig)
            signal.signal(sig, signalHandler)

        logger.goalStarting(goal)
        logger.nodeStarting(goal)
        if goal.state == Node.PENDING:
            pending.append(goal)
        elif goal.state == Node.FAILED:
            failedNodes.add(goal)
            logger.nodeFailed(goal)
            fail(goal)
        elif goal.state == Node.COMPLETED:
            logger.nodeCompleted(goal)

        while pending or procPending or procRunning:
            if procPending and quota > 0: # we have a process to start
                p = procPending.pop(0)
                logger.processStarting(p)
                p.start(logger)
                if p.state == Process.RUNNING:
                    quota -= 1
                    procRunning[p.pid] = p
                elif p.state == Process.COMPLETED:
                    logger.processCompleted(p)
                    unblock(p)
                elif p.state == Process.FAILED:
                    logger.processFailed(p)
                    fail(p)
            elif pending: # we have a flow to run
                n = pending[0]
                blockedOn = n.step()
                if n.state == Node.COMPLETED:
                    del pending[0]
                    logger.nodeCompleted(n)
                    unblock(n)
                elif n.state == Node.BLOCKED:
                    for b in blockedOn:
                        if isinstance(b, Node):
                            if b.state == Node.COMPLETED:
                                n.unblock()
                                continue
                            elif b.state == Node.PENDING and b not in pending:
                                logger.nodeStarting(b)
                                pending.append(b)
                        else:
                            assert isinstance(b, Process) and b.state == Process.PENDING
                            procPending.append(b)
                        if b in blockedBy:
                            blockedBy[b].append(n)
                        else:
                            blockedBy[b] = [n]
                    if n.state == Node.BLOCKED:
                        del pending[0]
                elif n.state == Node.FAILED:
                    del pending[0]
                    failedNodes.add(n)
                    logger.nodeFailed(n)
                    fail(n)
            else: # we have to wait
                try:
                    pid, returncode = os.wait()
                except EnvironmentError, e:
                    if e.errno == errno.EINTR:
                        if interrupted:
                            pending = []
                            procPending = []
                        continue
                    else:
                        raise
                p = procRunning.pop(pid)
                quota += 1
                p.exited(returncode)
                if p.state == Process.COMPLETED:
                    logger.processCompleted(p)
                    unblock(p)
                elif p.state == Process.FAILED:
                    logger.processFailed(p)
                    fail(p)

        for sig in interrupted:
            errorsCollected.append(errors.BuildInterrupted(sig))

        if blockedBy:
            if errorsCollected:
                for nodes in blockedBy.values():
                    failedNodes |= set(nodes)
            else:
                # All remaining nodes are blocked, which means we have
                # a dependency loop
                errorsCollected.append(errors.CircularDependency())

        for n in failedNodes:
            n.close()

        if errorsCollected:
            logger.goalFailed(goal, errorsCollected)
        else:
            logger.goalCompleted(goal)

    finally:
        for sig in signals:
            signal.signal(sig, oldSignalHandlers[sig])

    return errorsCollected

class _GoalArgument(object):
    """
    A positional argument of a command-line goal.
    """

    def __init__(self, arg, help=None, default=None, type='string', metavar=None, value=NotImplemented):
        """
        Constructor.

        @param arg The name of the node parameter to which this
        argument maps.

        @param help A one-line description of the parameter that will
        appear in the help text.

        @param default The default value to be used in case the
        argument is omitted on the command line. If None (the
        default), the argument is mandatory.

        @param type The type of the argument, either 'string' (the
        default) or 'int'.

        @param metavar The meta-variable that will be used to refer to
        the parameter in the help text, such as FILE.

        @param value If specified, even as None, the argument merely
        specifies a hardcoded value for the node parameter to be used
        when instantiating the goal. Such an argument does not consume
        anything when parsing the command line, and is not mentioned
        in the help text.
        """
        self.name = arg
        self.help = help
        self.default = default
        self.type = type
        self.metavar = metavar or arg.upper()
        self.value = value
        assert help or value != NotImplemented

    def parse(self, argument, dictionary):
        """
        Parse the argument as specified on the command line.

        @param argument The command-line argument or None if the end
        of the argument list has been reached.

        @param dictionary The dictionary in which to set the node
        parameter.

        @throws errors.CommandLineError The argument is mandatory but
        missing.

        @return Number of command-line arguments consumed (0 or 1).
        """
        if self.value != NotImplemented:
            dictionary[self.name] = self.value
            return 0
        elif argument == None:
            if self.default == None:
                raise errors.CommandLineError('Missing required argument: ' + self.metavar)
            dictionary[self.name] = self.default
            return 0
        elif self.type == 'string':
            dictionary[self.name] = argument
            return 1
        elif self.type == 'int':
            dictionary[self.name] = int(argument)
            return 1
        else:
            assert False, 'Unsupported argument type'

    def formatHelp(self, formatter):
        """
        Format the help text for the argument.

        @param formatter A formatter object.

        @return Formatted help text.
        """
        if self.value == NotImplemented:
            desc = self.help
            if self.default != None:
                desc += ' Default: ' + str(self.default)
            return formatter.format_goal_argument(self.metavar, desc)
        else:
            return ''

class _Goal(object):
    """
    A command-line goal.
    """

    def __init__(self, flowGroup, name, desc, *args):
        """
        Constructor.

        @param flowGroup The _FlowGroup object for the goal.

        @param name The keyword that identifies this goal on the
        command line.

        @param desc A one-line description of the goal that will
        appear in the help text.

        @param *args Dictionaries describing positional arguments.
        """
        self.flowGroup = flowGroup
        self.name = name
        self.desc = desc
        self.args = [_GoalArgument(**arg) for arg in args]

    def parse(self, arguments):
        """
        Parse the goal arguments as specified on the command line.

        @param arguments A list of non-option command-line arguments
        following the goal name.

        @throws errors.CommandLineError Unexpected arguments
        encountered.

        @return A dictionary of node parameters.
        """
        pos = 0
        dictionary = {}
        for arg in self.args:
            pos += arg.parse(arguments[pos] if pos < len(arguments) else None, dictionary)
        if pos < len(arguments):
            raise errors.CommandLineError("Too many arguments to goal {name}".format(name=self.name))
        return dictionary

    def formatHelp(self, formatter):
        """
        Format the help text for the goal.

        @param formatter A formatter object.

        @return Formatted help text.
        """
        goal = self.name
        brackets = ''
        for arg in self.args:
            if arg.value == NotImplemented:
                goal += ' '
                if arg.default != None:
                    goal += '['
                    brackets += ']'
                goal += arg.metavar
        goal += brackets
        return formatter.format_goal(goal, self.desc, self.args)

class _GoalParser(object):
    """
    _Goal parser. Singleton available as goalParser. Keeps a list of
    all defined goals.
    """

    def __init__(self):
        self.goals = []

    def decorator(self, *args, **kwargs):
        """
        Implementation of @goal.

        This method of the singleton goalParser is available in flow
        files as @goal(). It returns a decorator which declares a
        flow-group to be a goal. It doesn't affect what value is
        actually bound to the name of the flow-function - that's still
        the flow-group. All arguments are passed to the _Goal
        constructor, after the flow-group object being decorated.
        """
        def decorating(f):
            self.goals.append(_Goal(f, *args, **kwargs))
            return f
        return decorating

    def parse(self, arguments):
        """
        Parse the goal as specfified on the command line.

        @param arguments A list of non-option command-line arguments.

        @throws errors.CommandLineError Unknown goal name encountered.

        @return The goal Node.
        """
        for goal in self.goals:
            if goal.name == arguments[0]:
                dictionary = goal.parse(arguments[1:])
                return goal.flowGroup(**dictionary)
        raise errors.CommandLineError("Unknown goal: {name}".format(name=arguments[0]))

    def formatHelp(self, formatter):
        """
        Format the help text for all declared goals.

        @param formatter A formatter object.

        @return Formatted help text.
        """
        return ''.join([g.formatHelp(formatter) for g in self.goals])

goalParser = _GoalParser()
