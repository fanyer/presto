
"""
Configuration subsystem.

The system automatically makes the name config available in the global
namespace in the context in which flow files are interpreted, and the
names config, default (if available) and option in the global
namespace in the context in which configuration files are interpreted.
User-defined code does not need to import this module explicitly.
"""

import copy
import threading

import otree
import util
import errors
import output

_tls = threading.local()
_tls.contextStack = [{}] # The stack of contexts. The bottom one is an empty fallback.

def currentContext():
    """
    @return The current context.
    """
    return _tls.contextStack[-1]

def pushContext(context):
    """
    Push a context on top of the stack, making it current.

    @param context The context to push.

    @return The former top of the stack.
    """
    prev = currentContext()
    _tls.contextStack.append(context)
    return prev

def popContext(context):
    """
    Pop a context from the stack.

    @param context The context that the caller expects to be the
    current top of the stack. Used to verify that calls to pushContext
    and popContext are properly balanced.
    """
    assert currentContext() is context
    _tls.contextStack.pop()

class _ConfigLayer(object):
    """
    A configuration layer, the result of loading one configuration
    file. Answers configuration queries or forwards them to the
    previous layer.
    """

    def __init__(self, source, parent):
        """
        Constructor.

        @param source A Python file object to load or a callable. If a
        callable, it will be called with the dictionary representing
        the configuration file namespace as the only positional
        argument; it is expected to insert overrides (variables,
        functions and classes) into the dictionary just as importing a
        configuration file would insert them into the module
        namespace.

        @param parent The parent configuration layer or None.
        """
        self._parent = parent
        self._dict = {'config': facade,
                      'option': optParser.decorator,
                      'otree': otree,
                      'util': util,
                      'errors': errors,
                      'output': output}
        if parent:
            self._dict['default'] = parent
        if callable(source):
            source(self._dict)
        else:
            exec source in self._dict

    def __getattr__(self, name):
        """
        The object exposes a read-only property for every top-level
        configuration query defined in any of the configuration files
        read. Reading the property evaluates the query in the context
        of the currently running flow.
        """
        if name in self._dict:
            if name[0] == '_':
                return self._dict[name]
            else:
                res = self._augment(self._dict[name], currentContext())
                if res != NotImplemented:
                    return res
        if self._parent:
            return getattr(self._parent, name)
        else:
            raise AttributeError, name

    def __call__(self, **kwargs):
        """
        When invoked as a function with one or more keyword arguments,
        the config object returns a temporary object with a similar
        public interface, which answers configuration queries in the
        context modified by the specified arguments. Node parameters
        not modified in this way are left unchanged from the current
        context. This operation can be applied repeatedly. If one of
        the parameters specified in this way is named softFallback,
        and it is set to true, the temporary object will apply soft
        fallback logic when evaluating queries: if an exception is
        raised during evaluation, instead of failing, the previous
        overridden configuration function is used, and so on. The soft
        fallback logic does not apply to configuration class methods.
        """
        return _ContextAdapter(self, kwargs)

    def _invoke(self, function, context):
        """
        Call a configuration function or method.

        @param function The configuration function or method.

        @param context The context which should provide values for the
        callable's keyword arguments.
        """
        if context is not currentContext():
            with context:
                return self._invoke(function, context)
        params = {}
        for key in context:
            if util.takesArgument(function, key):
                params[key] = context[key]
        return function(**params)

    def _augment(self, obj, context):
        """
        Apply the magic to a top-level variable in a configuration
        file or to a property of a configuration object.

        If obj is a class, it will be subclassed, and all its members
        with names not beginning with an underscore will be overridden
        by properties that apply the same magic to their values. If
        it's a callable, it will be called, and the current context
        will provide values for the callable's keyword arguments.
        Otherwise, a deep copy of obj is made.

        @param obj The value to which to apply the above.

        @param context The context in which the query is being
        evaluated.

        @return See the description above.
        """
        if isinstance(obj, type):
            try:
                if issubclass(obj.__wrapped, obj):
                    return obj.__wrapped
            except AttributeError:
                pass
            def __init__(_self, *args, **kwargs):
                _self.__context = currentContext()
                obj.__init__(_self, *args, **kwargs)
            d = {'__init__': __init__}
            for name, item in obj.__dict__.items():
                if name[0] != '_':
                    # The outer lambda function is necessary in order to establish a fixed scope in which _name
                    # does not change. Without it, the inner lambda would refer to the outer local variable name,
                    # which keeps changing as the loop iterates. The lambda cannot be an ordinary inner def,
                    # either, because we need a new distinct instance on every iteration of the loop.
                    d[name] = (lambda _name: property(lambda _self: self._augment(getattr(super(newclass, _self), _name), _self.__context)))(name)
            obj.__wrapped = newclass = type(obj.__name__, (obj,), d)
            return newclass
        elif callable(obj):
            try:
                return self._invoke(obj, context)
            except:
                if 'softFallback' in context and context['softFallback']:
                    return NotImplemented
                raise
        else:
            return copy.deepcopy(obj)

class _ContextAdapter(object):
    """
    Temporary object returned by _ConfigLayer.__call__. The API is
    similar to that of _ConfigLayer.
    """

    def __init__(self, layer, params):
        """
        Constructor.

        @param layer The configuration layer on which the adapter
        should be based.

        @param params The dictionary of overridden parameters.
        """
        self._layer = layer
        self._params = params

    def __getattr__(self, name):
        with _ModifiedContext(self._params):
            return self._layer.__getattr__(name)

    def __call__(self, **kwargs):
        return _ContextAdapter(self, kwargs)

class _ModifiedContext(object):
    """
    A modified context produced by _ConfigLayer.__call__. For
    non-overridden parameters, refers to the parent context.

    Conforms to the dictionary protocol just like Node does, but
    _ModifiedContext is read-only.

    Conforms to the context manager protocol just like Node does.
    """

    def __init__(self, params):
        """
        Constructor.

        @param params The dictionary of overridden parameters.
        """
        self._params = params
        self._parent = None

    # Dictionary protocol implementation:
    def __contains__(self, key):
        return key in self._params or key in self._parent
    def __getitem__(self, key):
        return self._params[key] if key in self._params else self._parent[key]
    def __iter__(self):
        return iter(set(self._params) | set(self._parent))

    # Context manager protocol implementation:
    def __enter__(self):
        assert self._parent == None
        self._parent = pushContext(self)
    def __exit__(self, exc_type, exc_value, traceback):
        popContext(self)
        self._parent = None

class _ConfigFacade(object):
    """
    A facade for a stack of _ConfigLayer objects. The API is similar
    to that of _ConfigLayer.

    It is illegal to do anything else with this object before at least
    one layer is added. The caller should begin by adding the
    lowest-priority configuration file.

    Singleton available as facade.
    """

    _top = None # Top of the stack

    def __getattr__(self, name):
        return getattr(self._top, name)

    def __call__(self, **kwargs):
        return self._top(**kwargs)

    def __iadd__(self, source):
        """
        The += operator. Add another configuration file, whose
        settings take precedence over those from any files added so
        far.

        @param source A Python file object to load or a callable
        (passed as source to the _ConfigLayer constructor, see its
        documentation).
        """
        self._top = _ConfigLayer(source, self._top)
        return self

class _Option(object):
    """
    A command-line option.
    """

    def __init__(self, name, opt, desc, value=None, default=None, metavar=None, replace=False):
        """
        Constructor.

        @param name The name of the configuration query to which the
        option overrides the answer.

        @param opt A string or a sequence of strings defining the
        command-line options. The options can be short (a dash
        followed by one letter) or long (two dashes followed by a
        word). Case sensitive.

        @param desc A one-line description of the option that will
        appear in the help text.

        @param value For a boolean option, the value that this option
        sets when specified. Up to two @option decorators can be put
        on the same function with different value arguments. The
        default behavior for a boolean option when value is not
        specified is that the option sets a true value, and the
        opposite option name is generated automatically. Short options
        are not inverted. The inversion is not performed when value is
        specified.

        @param default The default value to display in the help text.
        This is only for documentation purposes and does not affect
        the actual default used when the option is not specified. The
        default behavior when default is not specified is to use the
        actual value obtained from reading the configuration files up
        to the point of parsing the command line.

        @param metavar The meta-variable that will be used to refer to
        the option's value in the help text, such as FILE. The default
        is N for integer options, STRING for strings and lists, and
        KEY=VALUE for dictionaries.

        @param replace Only for list or dictionary options. If true,
        the first use of the option on the command line clears the
        list or dictionary; subsequent uses add items to it. If false,
        every use of the option adds an item.
        """
        self.name = name
        self.opt = util.ensureList(opt)
        self.desc = desc
        self.value = value
        self.default = default
        self.metavar = metavar
        self.replace = replace

    def addOptparseOption(self, parser):
        """
        Add the option(s) to an option parser.

        @param parser An optparse.OptionParser object.
        """
        default = getattr(facade(softFallback=True), self.name)
        if isinstance(default, bool):
            if isinstance(self.value, bool):
                parser.add_option(*self.opt, action='store_true' if self.value else 'store_false', dest=self.name,
                                  help=self._helpBool(self.default or default, self.value))
            else:
                assert self.value == None
                parser.add_option(*self.opt, action='store_true', dest=self.name,
                                  help=self._helpBool(self.default or default, True))
                inverted = self._invertedOpt()
                if inverted:
                    parser.add_option(*inverted, action='store_false', dest=self.name,
                                      help=self._helpBoolOpposite(self.default or default, False))
        elif isinstance(default, int):
            parser.add_option(*self.opt, action='store', type='int', dest=self.name,
                              help=self._help(self.default or default), metavar=self.metavar or 'N')
        elif isinstance(default, basestring):
            parser.add_option(*self.opt, action='store', type='string', dest=self.name,
                              help=self._help(self.default or default), metavar=self.metavar or 'STRING')
        elif isinstance(default, dict):
            parser.add_option(*self.opt, action='callback', type='string', callback=self._dictCallback,
                              help=self._helpDict(self.default or default), metavar=self.metavar or 'KEY=VALUE')
        elif isinstance(default, list):
            parser.add_option(*self.opt, action='append', type='string', dest=self.name,
                              help=self._helpList(self.default or default), metavar=self.metavar or 'STRING')
        else:
            assert False, 'Unsupported data type for a command-line option'

    def _dictCallback(self, option, opt, value, parser):
        "An optparse callback to set dictionary items."
        parts = value.split('=', 1)
        d = getattr(parser.values, self.name, None)
        if not d:
            d = {}
            setattr(parser.values, self.name, d)
        d[parts[0]] = parts[1] if len(parts) == 2 else None

    def _help(self, default):
        """
        Format help for an option with a scalar argument (int or
        string).

        @param default Default value.

        @return Help text.
        """
        text = self.desc
        if default != None and default != '':
            text += " Default: " + str(default)
        return text

    def _helpBool(self, default, value):
        """
        Format help for a boolean option.

        @param default Default value.

        @param value The value that the option sets.

        @return Help text.
        """
        text = self.desc
        if isinstance(default, str):
            text += " Default: " + default
        if value == default:
            text += ' This is the default.'
        return text

    def _helpBoolOpposite(self, default, value):
        """
        Format help for an automatically generated opposite boolean option.

        @param default Default value.

        @param value The value that the option sets.

        @return Help text.
        """
        text = 'The opposite of the above.'
        if value == default:
            text += ' This is the default.'
        return text

    def _helpDict(self, default):
        """
        Format help for a dictionary option.

        @param default Default value.

        @return Help text.
        """
        text = self.desc
        noneSpecified = ' when none specified' if self.replace else ''
        if isinstance(default, str):
            d = " Default{0}: {1}".format(noneSpecified, default)
        elif default:
            d = " Default{0}: {1}".format(noneSpecified, ' '.join(["{0}={1}".format(key, value) if value != None else key for key, value in default.iteritems()]))
        else:
            d = ''
        text += " Can be repeated." + d
        return text

    def _helpList(self, default):
        """
        Format help for a list option.

        @param default Default value.

        @return Help text.
        """
        text = self.desc
        noneSpecified = ' when none specified' if self.replace else ''
        if isinstance(default, str):
            d = " Default{0}: {1}".format(noneSpecified, default)
        elif default:
            d = " Default{0}: {1}".format(noneSpecified, ' '.join(default))
        else:
            d = ''
        text += " Can be repeated." + d
        return text

    def _invertedOpt(self):
        """
        Generate the name for an opposite boolean option.

        @return Generated name.
        """
        res = []
        for o in self.opt:
            if o.startswith('--with-'):
                res.append('--without-' + o[7:])
            elif o.startswith('--without-'):
                res.append('--with-' + o[10:])
            elif o.startswith('--no-'):
                res.append('--' + o[5:])
            elif o.startswith('--enable-'):
                res.append('--disable-' + o[9:])
            elif o.startswith('--disable-'):
                res.append('--enable-' + o[10:])
            elif o.startswith('--'):
                res.append('--no-' + o[2:])
        return res

    def handleValue(self, values, d):
        """
        Insert an override based on the specified option value into
        the dictionary. Called by _OptionParser.handleValues.

        @param values The values object produced by
        optparse.OptionParser.

        @param d The dictionary into which to insert the override.
        """
        name = self.name
        val = getattr(values, name, None)
        default = d['default']
        if val is None:
            pass
        elif self.replace:
            d[name] = val
        elif isinstance(val, dict):
            def function():
                data = getattr(default, name)
                data.update(val)
                return data
            function.__name__ = name
            d[name] = function
        elif isinstance(val, list):
            def function():
                data = getattr(default, name)
                data += val
                return data
            function.__name__ = name
            d[name] = function
        else:
            d[name] = val

class _OptionParser(object):
    """
    Option parser. Singleton available as optParser.

    The actual parsing is done by the Python library class
    optparse.OptionParser. This class is responsible for adding the
    correct options to it and for handling the collected values after
    the parsing.

    Keeps a list of all defined options.
    """

    def __init__(self):
        self.options = []

    def decorator(self, *args, **kwargs):
        """
        This decorator declares a configuration function as an option
        and is available in configuration files as @option. All
        arguments are passed to the _Option constructor after the
        function's name.
        """
        def decorating(f):
            self.options.append(_Option(f.__name__, *args, **kwargs))
            return f
        return decorating

    def addOptparseOptions(self, parser):
        """
        Add the options to an option parser.

        @param parser An optparse.OptionParser object.
        """
        for opt in self.options:
            opt.addOptparseOption(parser)

    def handleValues(self, values):
        """
        Add a layer of overrides based on the specified option values
        to the configuration stack.

        @param values The values object produced by
        optparse.OptionParser.
        """
        global facade
        def handler(d):
            for opt in self.options:
                opt.handleValue(values, d)
        facade += handler

facade = _ConfigFacade()
optParser = _OptionParser()
