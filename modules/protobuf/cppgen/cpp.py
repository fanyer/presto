from copy import copy
import os
import sys
import string
import re
import time
import cPickle as pickle

from hob.proto import Element, Request, Event, Proto, Quantifier, defaultMessage, iterTree, Target, Package, Service, Command, Message, Field, Enum, Option, DocBlock
from hob import utils, proto
import util

# TODO: If the response message is Default the code should call the SendDefaultResponse instead of creating a Default object and sending that.

class GeneratorError(Exception):
    pass

class CppError(Exception):
    pass


class MtimeCache(object):
    def __init__(self):
        from cppgen.utils import cachePath
        self.times = {} # Maps from filename to mtime
        self.root = None
        self.path = os.path.join(cachePath, "mtimes.dat")
        self._makeRelative = None

    def load(self):
        try:
            self.times = pickle.load(open(self.path, "rb"))
        except Exception, e:
            # If it fails to load the cache continue as normal, it just means
            # mtimes will be read from the file instead
            return False
        return True

    def save(self):
        dirname = os.path.dirname(self.path)
        util.makedirs(dirname)
        pickle.dump(self.times, open(self.path, "wb"))

    def setRoot(self, root):
        from cppgen.utils import makeRelativeFunc, cachePath
        self.root = root
        if root:
            self._makeRelative = makeRelativeFunc(root)
        else:
            self._makeRelative = None
        self.path = os.path.join(root or "", cachePath, "mtimes.dat")

    def _normalize(self, path):
        if self._makeRelative:
            path = self._makeRelative(path)
        return path.replace("\\", "/")

    def getTime(self, file_path):
        """Gets the cached or real mtime for the given file"""
        file_path = self._normalize(file_path)
        abs_file_path = os.path.join(self.root or "", file_path)
        mtime = None
        if os.path.exists(abs_file_path):
            mtime = int(os.stat(abs_file_path).st_mtime)
            if file_path in self.times:
                mtime = max(mtime, self.times[file_path])
        return mtime

    def updateTime(self, file_path, mtime=None):
        file_path = self._normalize(file_path)
        if not mtime:
            mtime = int(time.time())
        self.times[file_path] = mtime

    def removeTime(self, file_path):
        file_path = self._normalize(file_path)
        if file_path in self.times:
            del self.times[file_path]

mtimeCache = MtimeCache()

def commaList(items):
    items = list(items)
    for item in items[0:-1]:
        yield item, ","
    if items:
        yield items[-1], ""

def iterOpMessages(pkg):
    for message in iterTree(pkg.messages):
        if message.cpp.needsOpMessageClass():
            yield message

def memoize(fctn):
        memory = {}
        def memo(*args,**kwargs):
                haxh = args + (pickle.dumps(sorted(kwargs.iteritems())), )

                if haxh not in memory:
                        memory[haxh] = fctn(*args,**kwargs)

                return memory[haxh]
        if memo.__doc__:
            memo.__doc__ = "\n".join([memo.__doc__,"This function is memoized."])
        return memo

def strQuote(string):
    string = unicode(string)
    return u'"' + string.replace(u"\\", u"\\\\").replace(u'"', u'\\"').replace(u'\0', u'\\0') + u'"'

def commandName(name):
    """
    >>> commandName("bar")
    u'"bar"'
    >>> commandName("Foo")
    u'"foo"'
    >>> commandName("FooBar")
    u'"foo-bar"'
    >>> commandName("HTTPVersion")
    u'"http-version"'
    >>> commandName("serviceID")
    u'"service-id"'
    >>> commandName("ID")
    u'"id"'
    >>> commandName("CamelCaseIsHere")
    u'"camel-case-is-here"'
    """
    return strQuote(utils.dashed_name(name))

def cpp_path_norm(path):
    return path.replace("\\", "/")

def message_className(message):
    return "_" .join(message.path(filter=Message))

def root(item, types=None):
    if not types:
        types = Element
    while item.iparent and isinstance(item.iparent, types):
        item = item.iparent
    return item

def opMessage_className(message):
    package = root(message)
    prefix = package.cpp.opMessagePrefix
    suffix = package.cpp.opMessageSuffix
    if message.options["cpp_opmessage_prefix"].value:
        prefix = message.options["cpp_opmessage_prefix"].value.strip()
    if message.options["cpp_opmessage_suffix"].value:
        suffix = message.options["cpp_opmessage_suffix"].value.strip()
    return prefix + ("_" .join(message.path(filter=Message))) + suffix

def message_inlineClassName(message):
    return message.name

def upfirst(name):
    name = unicode(name)
    return name[0].upper() + name[1:]

def item_name(item, name):
    if "cpp_name" in item.options:
        return item.options["cpp_name"].value
    else:
        return name

def enum_name(enum):
    return element_path_string(enum, filter=(proto.Message, proto.Enum))

def element_path(element, filter=None):
    items = []
    while element:
        if not filter or isinstance(element, filter):
            items.insert(0, element)
        element = element.iparent
    return items

def element_path_name(item, filter=None):
    for i in element_path(item, filter=filter):
        if "cpp_name" in i.options:
            yield i.options["cpp_name"].value
        else:
            yield i.name

def element_path_string(item, filter=None, separator="_"):
    return separator.join(element_path_name(item, filter=filter))

def enum_valueName(enum_value):
    return item_name(enum_value, enum_value.iparent.name.upper() + "_" + enum_value.name)

def messageClassPath(message, class_path=None):
    cpp_class_name = message_inlineClassName(message)
    names = []
    names.append(message_className(message))
    names.reverse()
    if class_path:
        names.insert(0, class_path)
    cpp_class_path = "::".join(map(unicode, names))
    return cpp_class_name, cpp_class_path

def _member_name(name):
    return utils.join_underscore(utils.split_camelcase(name))

def _var_name(name):
    return utils.join_underscore(utils.split_camelcase(name)) + "_"

def _var_path(path):
    return _var_name("__".join(path))

class GeneratorManager(object):
    def __init__(self):
        self.messages = {}
        self.services = {}

    def getService(self, service):
        if service in self.services:
            return self.services[service]
        return None

    def removeService(self, service):
        del self.services[service]

    def setService(self, service, generator):
        self.services[service] = generator
        return generator

    def getMessage(self, message):
        if message in self.messages:
            return self.messages[message]
        return None

    def removeMessage(self, message):
        del self.messages[messsage]

    def setMessage(self, message, generator):
        self.messages[message] = generator
        return generator

generatorManager = GeneratorManager()

def fileHeader():
    return render("""
        /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
        **
        ** Copyright (C) 1995-%(year)d Opera Software AS.  All rights reserved.
        **
        ** This file is part of the Opera web browser.  It may not be distributed
        ** under any circumstances.
        **
        ** Note: This file is generated, any modifications to this file will
        **       be overwritten when the file is regenerated. See modules/protobuf/cpp.conf for
        **       details on how this file is generated.
        */
        """, {
            "year": time.localtime().tm_year,
        }) + "\n"


_indent_lines_re = re.compile(r"(^[ \t]*)(?:[^ \t\n])", re.MULTILINE)
# similar to textwrap.dedent, but preserves tabs on all Python versions
def dedent(text):
    margin = None
    for line in _indent_lines_re.findall(text):
        if margin is None:
            margin = line
        elif line.startswith(margin):
            pass
        elif margin.startswith(line):
            margin = line
        else:
            return text
    if margin:
        text = re.sub(r"(?m)^" + margin, '', text)
    return text

def cleanup(text):
    text = dedent(unicode(text))
    text = text.replace(u"\r\n", u"\n").strip()
    return text

def render(tpl, args=None):
    text = cleanup(tpl)
    if args:
        text = text % args
    return text

def template(is_modifier=False):
    def wrap(func):
        def render(obj, methodArgs, *args, **kwargs):
            text = func(obj, methodArgs, *args, **kwargs)
            text = cleanup(text)
            if is_modifier:
                text = methodArgs["fieldSet"] + text
            return text
        return render
    return wrap

class TemplatedGenerator(object):
    def __init__(self, indentation=4, usetabs=True, tabsize=4):
        self.indentation = indentation
        self.usetabs = usetabs
        self.tabsize = tabsize

    def messageIdentifier(self, message):
        if not message or message == defaultMessage:
            raise GeneratorError("Cannot generate message identifier for default message")
        return "Message_" + _var_path(message.absPath())

    def indent(self, text, levels=1):
        out = []
        indent = self.indentation * levels
        if self.usetabs:
            prefix = "\t"*(indent / self.tabsize) + " "*(indent % self.tabsize)
        else:
            prefix = " "*indent
        for line in text.splitlines(True):
            if line.strip() == "":
                out.append(line.strip(" \t"))
            elif line[0:1] == "#": # C++ macros
                out.append(line[0:1] + prefix + line[1:])
            else:
                out.append(prefix + line)
        return "".join(out)

def path(item, filter=None, extra=None, relative=None):
    items = []
    if extra:
        items.extend(extra)
    if relative and relative == item:
        return [item] + items
    while item:
        if not filter or isinstance(item, filter):
            items.insert(0, item)
        item = item.parent
    if relative:
        relative_path = path(relative, filter=filter)
        for relative_item in relative_path:
            if not items or relative_item != items[0]:
                break
            items.pop(0)
    return items

def path_name(item, filter=None, extra=None, relative=None):
    for i in path(item, filter=filter, extra=extra, relative=relative):
        if i.proto and "cpp_name" in i.proto.options:
            yield i.proto.options["cpp_name"].value
        else:
            yield i.name

def path_string(item, filter=None, relative=None, extra=None, separator="::"):
    return separator.join(path_name(item, filter=filter, extra=extra, relative=relative))

filters = {}

class CppElement(object):
    def __init__(self, parent=None, proto=None, doc=None, comment=None, impl_comment=None, conds=None, impl_conds=None):
        self.parent = parent
        self.proto = proto
        if doc and not isinstance(doc, DocBlock):
            doc = DocBlock(doc)
        self.doc = doc
        self.comment = comment
        self.impl_comment = impl_comment
        self.children = []
        self.spacing = 0
        self.impl_spacing = 1
        self.conds = conds or []
        self.impl_conds = impl_conds or []

    def setDoc(self, doc):
        self.doc = doc

    def setComment(self, comment):
        self.comment = comment

    def setImplementationComment(self, comment):
        self.impl_comment = comment

    def setConditions(self, conds):
        self.conds = conds

    def setImplementationConditions(self, conds):
        self.impl_conds = conds

    def add(self, child):
        if child.parent:
            child.parent.children.remove(child)
        self.children.append(child)
        child.parent = self

    def iterChildDeclarations(self):
        spacing = 0
        for child in self.children:
            if child.hasDeclaration():
                if spacing:
                    yield Spacer(spacing)
                if child.comment:
                    yield child.comment
                if child.conds:
                    yield MacroIf([(child.conds, child)])
                else:
                    yield child
                spacing = child.spacing

    def iterChildImplementations(self):
        spacing = 0
        for child in self.children:
            if child.hasImplementation():
                if spacing:
                    yield Spacer(spacing)
                if child.impl_comment:
                    yield child.impl_comment
                if child.impl_conds:
                    yield MacroIf([(child.impl_conds, child)])
                else:
                    yield child
                spacing = child.impl_spacing

    def path(self, relative=None):
        return path(self, relative=relative)

    def cppPath(self, relative=None):
        return path_string(self, relative=relative)

    def declaration(self, relative=None):
        pass

    def implementation(self, relative=None):
        pass

    def hasDeclaration(self):
        """Checks if the element has a declaration, if so declaration() may be called."""
        return True

    def hasImplementation(self):
        """Checks if the element has an implementation, if so implementation() may be called."""
        return False

class Comment(CppElement):
    def __init__(self, text, eol=True):
        super(Comment, self).__init__()
        self.text = text
        self.eol = eol

    def path(self, relative=None):
        return []

    def cppPath(self, relative=None):
        return []

    def declaration(self, relative=None):
        if self.eol:
            return "\n".join([("//" + ["", " "][len(line) > 0] + line) for line in self.text.splitlines()])
        else:
            lines = self.text.splitlines()
            if len(lines) == 1:
                return "/* " + lines[0] + " */"
            else:
                return "/*\n" + ("\n".join([(["", "  "][len(line) > 0] + line) for line in lines])) + "*/"

    implementation = declaration

    def hasImplementation(self):
        return True

class MacroIf(CppElement):
    def __init__(self, bodies):
        super(MacroIf, self).__init__()
        self.bodies = bodies

    def path(self, relative=None):
        return []

    def cppPath(self, relative=None):
        return []

    def _render(self, rendertype, relative=None):
        def part(iftype, cond, body):
            text  = "#" + iftype
            if cond:
                text += " " + cond
            text += "\n" + getattr(body, rendertype)(relative=relative) + "\n"
            return text
        text = part("if", self.bodies[0][0], self.bodies[0][1])
        for cond, body in self.bodies[1:]:
            if cond:
                text += part("else if", cond, body)
            else:
                text += part("else", cond, body)
        text += "#endif // %s\n" % self.bodies[0][0]
        return text

    def declaration(self, relative=None):
        return self._render("declaration", relative=relative)

    def implementation(self, relative=None):
        return self._render("implementation", relative=relative)

    def hasImplementation(self):
        return True

class Spacer(CppElement):
    def __init__(self, lines=1):
        super(Spacer, self).__init__()
        self.lines = lines

    def path(self, relative=None):
        return []

    def cppPath(self, relative=None):
        return []

    def declaration(self, relative=None):
        return "\n"*(self.lines - 1)

    implementation = declaration

    def hasImplementation(self):
        return True

class Class(CppElement):
    def __init__(self, name, proto=None, doc=None):
        super(Class, self).__init__(proto=proto, doc=doc)
        self.name = name
        self.inheritance = []
        self.members = []

    def declarationName(self):
        return self.name

    def symbol(self, relative=None):
        return path_string(self, relative=relative)

    # TODO: Converted message generator to this method
    # def declaration(self, relative=None):
        # if not relative and self.parent:
            # relative = self.parent

# enum Foo {...};
class Enum(CppElement):
    def __init__(self, name, proto=None):
        super(Enum, self).__init__(proto=proto)
        self.name = name
        self.values = []

    def declarationName(self):
        return self.name

    def symbol(self, relative=None):
        return path_string(self, relative=relative)

    def declaration(self, relative=None):
        text = "enum %s\n{\n" % self.name
        for comma, value in zip(([","]*(len(self.values)-1)) + [""], self.values):
            text += "    %s%s\n" % (value.declaration(), comma)
        text += "};\n"
        return text

# Foo = 1,
# Bar,
class EnumValue(CppElement):
    def __init__(self, enum, name, value=None, proto=None):
        super(EnumValue, self).__init__(proto=proto)
        self.enum = enum
        if enum:
            self.parent = enum
        self.name = name
        self.value = value

    def declarationName(self):
        return self.name

    def symbol(self, relative=None):
        return path_string(self.enum.parent, relative=relative, extra=[self])

    def declaration(self, relative=None):
        if self.value:
            return "%s = %s" % (self.name, self.value)
        else:
            return self.name

    def path(self):
        return path(self, filter=filters['enum-value'])

    def cppPath(self):
        return path_string(self, filter=filters['enum-value'])

    def ref(self):
        return EnumValueReference(self)

# Foo
# Bar
class EnumValueReference(CppElement):
    def __init__(self, enum_value, proto=None):
        super(EnumValueReference, self).__init__(proto=proto)
        self.enum_value = enum_value
        self.name = enum_value.declarationName()

    def declarationName(self):
        return self.enum_value.declarationName()

    def symbol(self, relative=None):
        return path_string(self.enum_value.parent, relative=relative, extra=[self])

    def declaration(self, relative=None):
        return self.symbol(relative=relative)

    def path(self):
        return self.enum_value.path()

    def cppPath(self):
        return self.enum_value.cppPath()

class TypeDef(CppElement):
    def __init__(self, name, ref, proto=None):
        super(TypeDef, self).__init__(proto=proto)
        self.name = name;
        self.ref = ref

    def declarationName(self):
        return self.name

    def symbol(self, relative=None):
        return path_string(self, relative=relative)

    def declaration(self, relative=None):
        if not relative and self.parent:
            relative = self.parent
        return "typedef %s %s;" % (self.ref.symbol(relative=relative), self.name)

class Method(CppElement):
    def __init__(self, name, rtype, rtype_outer=None, parameters=None, keywords=None, implementation=None, proto=None, doc=None, is_inline=False):
        super(Method, self).__init__(proto=proto, doc=doc)
        self.name = name
        self.rtype = rtype
        self.rtype_outer = rtype_outer or rtype
        self.parameters = []
        if parameters:
            for param in parameters:
                if not isinstance(param, Parameter):
                    param = Parameter(param)
                self.parameters.append(param)
        self.keywords = set(keywords or [])
        if is_inline:
            self.keywords.add("inline")
        self.implementation = implementation
        self.is_inline = is_inline

    def declarationName(self):
        return self.name

    def symbol(self, relative=None):
        return path_string(self, relative=relative)

class Parameter(CppElement):
    def __init__(self, type_, default=None, proto=None):
        super(Parameter, self).__init__(proto=proto)
        self.type = type_
        self.default = default

class Argument(CppElement):
    def __init__(self, name, type, proto=None, default=None):
        super(Argument, self).__init__(proto=proto)
        self.name = name
        if not isinstance(type, CppElement):
            type = toIntegralType(type)
        self.type = type
        if default is not None and not isinstance(default, CppElement):
            default = toIntegral(default)
        self.default = default

    def declarationName(self):
        return self.name

    def declaration(self, relative=None):
        deftext = ""
        if self.default is not None:
            deftext = " = " + self.default.symbol(relative=relative)
        return "%s %s%s" % (self.type.symbol(relative=relative), self.name, deftext)

    def symbol(self, relative=None):
        return self.name

    def value(self):
        return self.name

    def ref(self):
        return ArgumentReference(self)

class ArgumentReference(CppElement):
    def __init__(self, arg, proto=None):
        super(ArgumentReference, self).__init__(proto=proto)
        self.arg = arg

    def declarationName(self):
        return self.arg.declarationName()

    def declaration(self, relative=None):
        return self.arg.declarationName()

    def symbol(self, relative=None):
        return self.arg.declarationName()

    def value(self):
        return self.arg.declarationName()

class Initializer(CppElement):
    def __init__(self, name, value, proto=None):
        super(Initializer, self).__init__(proto=proto)
        self.name = name
        self.value = value

    def declarationName(self):
        return self.name

    def declaration(self, relative=None):
        return "%s(%s)" % (self.name, self.value.declaration())

    def symbol(self, relative=None):
        return self.name

class CustomType(CppElement):
    def __init__(self, text):
        super(CustomType, self).__init__(proto=None)
        self.text = text

    def declarationName(self):
        return self.text

    def declaration(self, relative=None):
        return self.text

    def symbol(self, relative=None):
        return self.text

class Code(CppElement):
    def __init__(self, text):
        super(Code, self).__init__(proto=None)
        self.text = text

    def statement(self):
        return self.text

    def declaration(self, relative=None):
        return self.text

    def __mod__(self, v):
        return Code(self.text % v)

class CustomValue(CppElement):
    def __init__(self, text):
        super(CustomValue, self).__init__(proto=None)
        self.text = text

    def declarationName(self):
        return self.text

    def declaration(self, relative=None):
        return self.text

    def symbol(self, relative=None):
        return self.text

class IntegralType(CppElement):
    def __init__(self, name):
        super(IntegralType, self).__init__(proto=None)
        self.name = name

    def declarationName(self):
        return self.name

    def declaration(self, relative=None):
        return self.name

    def symbol(self, relative=None):
        return self.name

    def __call__(self, value):
        return IntegralValue(self, str(value))

class NullValue(CppElement):
    def __init__(self):
        super(NullValue, self).__init__(proto=None)

    def declarationName(self):
        return "NULL"

    def declaration(self, relative=None):
        return "NULL"

    def symbol(self, relative=None):
        return "NULL"

class integrals(object):
    int = IntegralType("int")
    unsigned = IntegralType("unsigned")
    bool = IntegralType("BOOL")
    float = IntegralType("float")
    double = IntegralType("double")

def toIntegral(text):
    if isinstance(text, (str, unicode)) and text.lower() in ('true', 'false'):
        return integrals.bool(text)
    else:
        return integrals.int(text)

def toIntegralType(text):
    if text in ("bool", "BOOL"):
        return integrals.bool
    elif text in ("int", "signed int"):
        return integrals.int
    elif text in ("int", "unsigned int"):
        return integrals.unsigned
    elif text == "float":
        return integrals.float
    elif text == "double":
        return integrals.double
    else:
        return CustomType(text)

class IntegralValue(CppElement):
    def __init__(self, type, value):
        super(IntegralValue, self).__init__(proto=None)
        self.type = type
        self.value_ = value

    def __str__(self):
        return str(self.value_)

    def declarationName(self):
        return str(self.value_)

    def declaration(self, relative=None):
        return str(self.value_)

    def symbol(self, relative=None):
        return str(self.value_)

    def value(self):
        return str(self.value_)

class Variable(CppElement):
    def __init__(self, name, ref, value=None, static=False, mutable=False, volatile=False, is_integral=False, proto=None):
        """Define a variable consisting of a name and reference to a type/class and optionally with a value.
        The following options can also be set:
        static - Make the variable static.
        mutable - Make the variable mutable.
        volatile - Make the variable volatile.
        is_integral - Whether the type/ref is considered an integral type or enum. If so the value will be placed in the declaration and not the implementation.

        Example of a variables with an integral type:
        static const int foo = 42;
        static const AnEnum bar = Enum_Value;
        """
        super(Variable, self).__init__(proto=proto)
        self.name = name
        self.ref = ref
        self.value = value
        self.static = static
        self.mutable = mutable
        self.volatile = volatile
        self.is_integral = is_integral
        self._declaration = None
        self._impl = None

    def _setDirty(self):
        self._declaration = None
        self._impl = Non

    def symbol(self, relative=None):
        return path_string(self, relative=relative)

    def declaration(self, relative=None):
        def renderDeclaration(relative):
            if not relative and self.parent:
                relative = self.parent
            items = []
            for name, flag in [('static', self.static), ('mutable', self.mutable), ('volatile', self.volatile)]:
                if flag:
                    items.append(name)
            items.extend([self.ref.symbol(relative=relative), self.name])
            if self.is_integral and self.value:
                items.extend(["=", self.value.symbol(relative=relative)])
            return (" ".join(items)) + ";"

        if relative:
            return renderDeclaration(relative)
        if not self._declaration:
            self._declaration = renderDeclaration(relative)
        return self._declaration

    def implementation(self, relative=None):
        def renderImplementation(relative):
            items = []
            for name, flag in [('/*static*/', self.static), ('/*mutable*/', self.mutable), ('/*volatile*/', self.volatile)]:
                if flag:
                    items.append(name)
            items.extend([self.ref.symbol(relative=relative), self.symbol(relative=relative)])
            if not self.is_integral and self.value:
                items.extend(["=", self.value.symbol(relative=relative)])
            return (" ".join(items)) + ";"

        if relative:
            return renderImplementation(relative)
        if not self._impl:
            self._impl = renderImplementation(relative)
        return self._impl

    def hasImplementation(self):
        return True

class Keyword(CppElement):
    def __init__(self, ref, pre=True, proto=None):
        super(Keyword, self).__init__(proto=proto)
        self.ref = ref
        self.pre = pre
        self.active = True

    def symbol(self, relative=None):
        return self.declaration(relative=relative)

    def declaration(self, relative=None):
        if not relative and self.parent:
            relative = self.parent
        if self.active:
            if self.pre:
                return self._keyword() + " " + self.ref.symbol(relative=relative)
            else:
                return self.ref.symbol(relative=relative) + " " + self._keyword()
        else:
            return self.ref.symbol(relative=relative)

    def _keyword(self):
        pass

class Symbol(Keyword):
    def __init__(self, name, ref, proto=None):
        super(Symbol, self).__init__(ref, proto=proto)
        self.name = name

    def _keyword(self):
        return self.name

class Const(Keyword):
    def _keyword(self):
        return "const"

filters['element'] = set([Class, Enum, EnumValue, TypeDef, Method, Parameter, Variable, Const, Symbol])
filters['enum-value'] = set(filters['element']) - set([Enum])

def dict_override(orig, **kwargs):
    d = copy(orig)
    d.update(kwargs)
    return d

def make_dependency(obj):
    if isinstance(obj, Dependency):
        return obj
    else:
        # Assuming a path string
        return PathDependency(obj)

class DependencyList(set):
    def __repr__(self):
        extra = ""
        deps = list(self)
        if deps:
            extra = "%r" % deps
        return "DependencyList(%s)" % extra

    def iterDependencies(self):
        for dep in self:
            if dep:
                yield make_dependency(dep)

class Dependency(object):
    pass

    def mtime(self):
        """Return the modification time of the dependency as an int.

        Should return None when there is no modification time.
        """
        return None

    def isModified(self, mtime):
        """Return True if the dependency has been updated compared to the target mtime, False otherwise."""
        return True

ForcedDependency = Dependency

class PathDependency(Dependency):
    def __init__(self, path):
        Dependency.__init__(self)
        self.path = path

    def __repr__(self):
        return "PathDependency(%r)" % self.path

    def isModified(self, mtime):
        # TODO: Should store md5/mtime and check if it has changed when target.mtime differs
        return self.mtime() > mtime

    @memoize
    def mtime(self):
        if os.path.exists(self.path):
            return mtimeCache.getTime(self.path)
        else:
            return None

    def __eq__(self, other):
        if isinstance(other, PathDependency):
            return self.path == other.path
        elif isinstance(other, Dependency):
            return other == self
        else:
            return self.path == other

class Rule(object):
    """Represents a dependency rule in a build system.
    If the dependencies changes it means that target must be recreated.

    @param target The target which will be generated.
    @param dependencies Optional list of files the target depends on.
                        Any iterable type can be used, must contain strings.

    """
    def __init__(self, target=None, dependencies=None, mtime=None):
        self.target = target
        self.dependencies = DependencyList(dependencies or [])
        self.custom_mtime = mtime
        self._mtime = None
        self._check = None

    def addDependency(self, path):
        self._check = None
        self.dependencies.add(path)

    def addDependencies(self, dependencies):
        self._check = None
        self.dependencies.update(dependencies)

    def removeDependencies(self, dependencies):
        self.dependencies.difference_update(dependencies)

    def mtime(self):
        return max(self.targetMtime(), self.dependenciesMtime())

    def customMtime(self):
        return self.custom_mtime

    def targetMtime(self):
        if self._mtime is None:
            self._mtime = self.custom_mtime = mtimeCache.getTime(self.target)
        else:
            self._mtime = None
        return self._mtime

    def updateTargetMtime(self, mtime):
        self._mtime = self.custom_mtime = max(mtime, self.custom_mtime)

    def needsUpdate(self):
        if self._check is None:
            if not os.path.exists(self.target):
                self._check = True
                return True
            for dep in self.dependencies.iterDependencies():
                if dep.isModified(self.targetMtime()):
                    self._check = True
                    return True
            self._check = False
            return False
        return self._check

    def setNeedsUpdate(self, value):
        self._check = value

    def dependenciesMtime(self):
        mtime = None
        for dep in self.dependencies.iterDependencies():
            _mtime = dep.mtime()
            if _mtime:
                mtime = max(mtime, _mtime)
        if not mtime:
            mtime = time.time()
        return mtime

class RuleDependency(Dependency):
    def __init__(self, rules):
        self.rules = rules

    def __repr__(self):
        return "RuleDependency(%r)" % self.rules

    def isModified(self, mtime):
        return self.mtime() > mtime

    @memoize
    def mtime(self):
        mtime = None
        for rule in self.rules:
            mtime = max(mtime, rule.mtime())
        return mtime

def get_python_src(path):
    if not path.endswith(".py"):
        return os.path.splitext(path)[0] + ".py"
    return path

class GeneratedFile(object):
    def __init__(self, path, build_module, package=None, jumbo_component=None, dependencies=None, rule=None, text=None, callback=None):
        self.path = path
        self.build_module = build_module
        self.package = package
        self.modified = False
        self.jumbo_component = jumbo_component
        if rule is None:
            rule = Rule(os.path.abspath(path), dependencies or DependencyList())
        self.rule = rule
        self.text = text
        self.callback = callback

class CppGenerator(object):
    def __init__(self, opera_root=None):
        self.operaRoot = opera_root
        from cppgen.utils import makeRelativeFunc
        if self.operaRoot:
            self._makeRelative = makeRelativeFunc(self.operaRoot)

    def makeRelativePath(self, fname):
        """Returns the path of fname relative to the opera root"""
        return self._makeRelative(fname)

    def renderDefines(self, defines):
        if not defines:
            return ""
        if len(defines) == 1:
            return "#ifdef %s" % defines[0]
        else:
            return "#if %s" % (" && ".join(["defined(%s)" % define for define in defines]))

    def renderEndifs(self, defines):
        if not defines:
            return ""
        if len(defines) == 1:
            return "#endif // %s" % defines[0]
        else:
            return "#endif // %s" % (" && ".join(["defined(%s)" % define for define in defines]))

    def renderIncludes(self, includes):
        text = ""
        for include in includes:
            text += "#include %s\n" % strQuote(include)
        return text

    def renderFile(self, path, content, includes=None, defines=None, proto_file=None, is_header=True, use_pch=True):
        includes = includes or []
        defines = defines or []

        proto_source = ""
        if proto_file:
            proto_source = "// Generated from %s" % proto_file

        guard_pre = ""
        guard_post = ""
        pch_code = ""
        if is_header:
            guard = re.sub("_+", "_", re.sub("[^a-zA-Z0-9]+", "_", os.path.basename(path))).strip("_").upper()
            guard_pre = render("""
                #ifndef %(guard)s
                #define %(guard)s
                """, {
                    "guard": guard,
                })
            guard_post = render("""
                #endif // %(guard)s
                """, {
                    "guard": guard,
                })
        elif use_pch:
            pch_code = '#include "core/pch.h"\n'

        text = fileHeader()
        text += "\n" + render("""
            %(proto_source)s

            %(pch_code)s

            %(guard_pre)s

            %(defines)s

            %(includes)s

            %(content)s

            %(endifs)s

            %(guard_post)s
            """, {
                "proto_source": proto_source,
                "pch_code": pch_code,
                "guard_pre": guard_pre,
                "guard_post": guard_post,
                "defines": self.renderDefines(defines),
                "endifs": self.renderEndifs(defines),
                "includes": self.renderIncludes(includes),
                "content": content,
            }) + "\n"
        return text

class CppScopeBase(CppGenerator):
    filePath = None

    def __init__(self, file_path, dependencies=None, opera_root=None):
        self.filePath = file_path
        self.dependencies = DependencyList(dependencies or [])
        self.rule = Rule(os.path.abspath(file_path), self.dependencies)
        super(CppScopeBase, self).__init__(opera_root)

    def getRule(self):
        return self.rule

    def addDependency(self, path):
        self.rule.addDependency(path)

    def createFile(self):
        return self._createFile()

    def _createFile(self, build_module=None, package=None, jumbo_component=None):
        generated_file = GeneratedFile(self.filePath, build_module=build_module, package=package, jumbo_component=jumbo_component)
        if not generated_file.jumbo_component and generated_file.package:
            # Individual proto files may specify cpp_component=<value>, indicating whether they should
            # exist in all binaries ('framework'), some particular selection, or core components only (None).
            if "cpp_component" in package.options:
                generated_file.jumbo_component = package.options["cpp_component"].value
        generated_file.rule = self.rule
        return generated_file

settings = {
    "config": None,
    "cpp_config": None,
    }

class CppCustomType(object):
    def __init__(self, name, type=None, convToExt=None, convFromExt=None, returnToExt=None, assignFromExt=None, default=None, includes=None):
        self.name = name
        self.type = type or CustomType(name)
        self.convToExt = convToExt or Code("static_cast<%(fieldTypeOuter)s>(%(memberName)s)")
        self.convFromExt = convFromExt or Code("static_cast<%(dataType)s>(%(var)s)")
        self.returnToExt = returnToExt or Code("return %(expr)s")
        self.assignFromExt = assignFromExt or Code("%(memberName)s = %(expr)s")
        self.default = default
        self.includes = includes or []

class CppBuildOptions(object):
    def __init__(self, moduleType, moduleName, config=None):
        self.moduleType = moduleType
        self.moduleName = moduleName
        self.config = config

class CppBuilder(object):
    def __init__(self, options):
        self.options = options

class CppPackageBuilder(CppBuilder):
    def __init__(self, package, options, prefix=None):
        super(CppPackageBuilder, self).__init__(options=options)
        self.package = package
        if not prefix:
            if "cpp_prefix" in package.options:
                prefix = package.options["cpp_prefix"].value
            if package.services and "cpp_prefix" in package.services[0].options:
                prefix = package.services[0].options["cpp_prefix"].value
        base = None
        self.basename = basename = os.path.splitext(os.path.basename(package.filename))[0]
        base = upfirst(options.moduleName) + utils.join_camelcase(basename.split("_"))
        self.descriptorID = "desc_" + options.moduleName.lower() + "_" + re.sub(r"[^a-zA-Z]+", "_", basename).lower()

        # Figure out filenames
        basePath = os.path.join(options.moduleType, options.moduleName, "src", "generated")
        messageBasename = basename
        messagePrefix = "g_proto_" + options.moduleName + "_"
        descriptorPrefix = "g_proto_descriptor_" + options.moduleName + "_"
        opMessagePrefix = "g_message_" + options.moduleName + "_"
        if "cpp_file" in package.options:
            messageBasename = package.options["cpp_file"].value
        self.filenames = {
            "descriptorImplementation": os.path.join(basePath, descriptorPrefix + messageBasename + ".cpp"),
            "descriptorDeclaration": os.path.join(basePath, descriptorPrefix + messageBasename + ".h"),
            "messageImplementation": os.path.join(basePath, messagePrefix + messageBasename + ".cpp"),
            "messageDeclaration": os.path.join(basePath, messagePrefix + messageBasename + ".h"),
            "opMessageImplementation": os.path.join(basePath, opMessagePrefix + messageBasename + ".cpp"),
            "opMessageDeclaration": os.path.join(basePath, opMessagePrefix + messageBasename + ".h"),
        }

        if "cpp_class" in package.options:
            prefix = ""
            base = package.options["cpp_class"].value
        if package.services and "cpp_class" in package.services[0].options:
            prefix = ""
            base = package.services[0].options["cpp_class"].value
        if not base:
            for service in package.services:
                base = service.name
        if prefix == None:
            prefix = "Op"
        self.useOpMessageSet = bool(package.options["cpp_opmessage_set"].value)
        self.opMessagePrefix = package.options["cpp_opmessage_prefix"].value
        self.opMessageSuffix = package.options["cpp_opmessage_suffix"].value
        self.prefix = prefix
        self.base = base
        self.is_built = False
        self._messageSet = None
        self._opMessageSet = None
        self._descriptor = None
        self._services = None
        self._needsOpMessage = None

    def isBuilt(self):
        return self.is_built

    def element(self):
        return self.cppMessageSet()

    def cppMessageSet(self):
        if not self._messageSet:
            if "cpp_class" in self.package.options:
                cppClass = self.package.options["cpp_class"].value
            else:
                cppClass = self.prefix + self.base + "_MessageSet"
            self._messageSet = Class(cppClass)
        return self._messageSet

    def cppOpMessageSet(self):
        if not self._opMessageSet:
            self._opMessageSet = Class(self.prefix + self.base + "_OpMessageSet")
        return self._opMessageSet

    def descriptorClass(self):
        if not self._descriptor:
            self._descriptor = Class(self.prefix + self.base + "_Descriptors")
        return self._descriptor

    def descriptorIdentifier(self):
        return self.descriptorID

    def defines(self):
        defines = []
        if self.package.services:
            for service in self.package.services:
                if "cpp_feature" in service.options:
                    defines.append(service.options["cpp_feature"].value)
                if "cpp_defines" in service.options:
                    defines += map(str.strip, service.options["cpp_defines"].value.split(","))
        if "cpp_defines" in self.package.options:
            defines += map(str.strip, self.package.options["cpp_defines"].value.split(","))
        return defines

    def includes(self):
        return [self.filenames["messageDeclaration"].replace("\\", "/")]

    def descriptorIncludes(self):
        return [self.filenames["descriptorDeclaration"].replace("\\", "/")]

    def opMessageIncludes(self):
        return [self.filenames["opMessageDeclaration"].replace("\\", "/")]

    def needsServiceClass(self):
        "Return True if an OpService class should be made for this package"
        return bool(self.package.services)

    def needsOpMessageClass(self):
        "Return True if the OpMessage style classes should be made for this package"
        if self._needsOpMessage is None:
            needClass = self.package.options["cpp_opmessage"].value
            if needClass:
                needClass = False
                for msg in iterTree(self.package.messages):
                    if msg.cpp.needsOpMessageClass():
                        needClass = True
                        break
            self._needsOpMessage = needClass
        return self._needsOpMessage

class CppServiceBuilder(CppBuilder):

    def __init__(self, service, options):
        super(CppServiceBuilder, self).__init__(options=options)
        self.service = service

        basePath = os.path.join(options.moduleType, options.moduleName)
        if "cpp_base" in service.options:
            basePath = os.path.normpath(service.options["cpp_base"].value)

        generatedPath = os.path.join(basePath, "src", "generated")
        if "cpp_gen_base" in service.options:
            generatedPath = os.path.normpath(service.options["cpp_gen_base"].value)

        srcPath = os.path.join(basePath, "src")
        if "cpp_src_base" in service.options:
            srcPath = os.path.normpath(service.options["cpp_src_base"].value)

        if "cpp_file" in service.options:
            cpp_file = service.options["cpp_file"].value
            prefix = "g_"
        else:
            cpp_file = utils.join_underscore(utils.split_camelcase(service.name))
            prefix = "g_scope_"

        serviceDecl = os.path.join(srcPath, cpp_file + ".h")
        if "cpp_hfile" in service.options:
            serviceDecl = os.path.normpath(service.options["cpp_hfile"].value)

        self.filenames = {
            "generatedServiceImplementation": os.path.join(generatedPath, prefix + cpp_file + "_interface.cpp"),
            "generatedServiceDeclaration": os.path.join(generatedPath, prefix + cpp_file + "_interface.h"),
            "serviceImplementation": os.path.join(srcPath, cpp_file + ".cpp"),
            "serviceDeclaration": serviceDecl,
        }

    def files(self):
        inc = {}
        for name in self.filenames:
            inc[name] = self.filenames[name].replace("\\", "/")
        return inc

class AllocationStrategy(object):
    # Regular alloc using OP_NEW(...)
    ALLOCATE_NEW = 1
    # Use OP_USE_MEMORY_POOL_DECL/IMPL with OP_NEW
    ALLOCATE_OPMEMORYPOOL = 2

class CppMessageBuilder(CppBuilder):

    def __init__(self, message, options):
        super(CppMessageBuilder, self).__init__(options=options)
        self.message = message
        allocStrategies = {"new": AllocationStrategy.ALLOCATE_NEW,
                           "OpMemoryPool": AllocationStrategy.ALLOCATE_OPMEMORYPOOL,
                           }
        opMessageAlloc = message.options["cpp_opmessage_allocate"].value
        self.opMessageAlloc = allocStrategies.get(opMessageAlloc, AllocationStrategy.ALLOCATE_NEW)
        self._klass = None
        self._typedef = None
        self._opMessage = None
        self._needsOpMessage = None
        self._opMessageType = None
        self.opMessageID = None

    def klass(self):
        if not self._klass:
            self._klass = Class(message_className(self.message), doc=self.message.doc)
            self._klass.proto = self.message
        return self._klass

    def typedef(self, name=None):
        if not self._typedef:
            self._typedef = TypeDef(name or message_inlineClassName(self.message), self.klass())
            self._typedef.proto = self.message
        return self._typedef

    def element(self):
        return self.klass()

    def defines(self):
        defines = []
        if "cpp_defines" in self.message.options:
            defines += [define.strip() for define in  self.message.options["cpp_defines"].value.split(",")]
        return defines

    def opMessage(self):
        if not self._opMessage:
            self._opMessage = Class(opMessage_className(self.message), doc=self.message.doc)
            self._opMessage.proto = self.message
        return self._opMessage

    def opMessageEnum(self):
        if not self._opMessageType:
            message_type = Class("OpTypedMessageType")
            enum = Enum("Enum")
            message_type.add(enum)
            self._opMessageType = EnumValue(enum, ("MESSAGE_%s_%s" % (self.options.moduleName, "_".join(self.message.path()))).upper(), proto=self.message)
        return self._opMessageType

    def needsOpMessageClass(self):
        "Return True if a OpMessage style classe should be made for this message"
        if self._needsOpMessage is None:
            option = self.message.options["cpp_opmessage"].value
            if option == "": # No value, needs automatic setup
                # Only top-level messages are turned into OpMessage classes
                needClass = not isinstance(self.message.iparent, Message) and root(self.message).options["cpp_opmessage"].value
            else:
                needClass = bool(option)
            self._needsOpMessage = needClass
        return self._needsOpMessage

class CppFieldBuilder(CppBuilder):
    def __init__(self, field, options):
        super(CppFieldBuilder, self).__init__(options=options)
        self.field = field
        self._ext = None

    def extType(self):
        if not self._ext:
            if self.field.options["cpp_exttype"].value:
                name = self.field.options["cpp_exttype"].value
                section = name + ".custom"
                if self.options.config.has_section(section):
                    config = self.options.config
                    type = None
                    if config.has_option(section, "type"):
                        type = CustomType(config.get(section, "type", True).strip())
                    kwargs = {}
                    for key, override in ("to", "convToExt"), ("from", "convFromExt"), ("return", "returnToExt"), ("assign", "assignFromExt"):
                        if config.has_option(section, key):
                            kwargs[override] = Code(config.get(section, key, True).strip())
                    # Parse comma-separated lists
                    for key, override in ("includes", "includes"), :
                        if config.has_option(section, key):
                            kwargs[override] = [inc.strip() for inc in config.get(section, key, True).strip().split(",") if inc.strip()]
                    self._ext = CppCustomType(name, type=type, **kwargs)
                else:
                    self._ext = CppCustomType(name)
                self._ext.proto = self.field
        return self._ext

class CppEnumBuilder(CppBuilder):
    def __init__(self, enum, options):
        super(CppEnumBuilder, self).__init__(options=options)
        self.enum = enum
        self._enum = None
        self._typedef = None
        self._enumValues = None

    def cppEnum(self):
        if not self._enum:
            self._enum = Enum(enum_name(self.enum))
            for evalue in self.enum.items:
                self._enum.values.append(evalue.cpp.cppEnumValue())
            self._enum.proto =  self.enum
        return self._enum

    def typedef(self, name=None):
        if not self._typedef:
            self._typedef = TypeDef(name or item_name(self.enum, self.enum.name), self.cppEnum())
            self._typedef.proto = self.enum
        return self._typedef

    def enumValues(self):
        if not self._enumValues:
            self._enumValues = []
            for value in self.enum.values:
                enum_value = Variable(enum_valueName(value), Const(value.cpp.cppEnumValue().enum), value=value.cpp.cppEnumValue(), static=True, proto=value, is_integral=True)
                enum_value.impl_conds = "defined(OP_PROTO_STATIC_CONST_DEFINITION)"
                self._enumValues.append(enum_value)
        return self._enumValues

    def element(self):
        return self.cppEnum()

class CppEnumValueBuilder(CppBuilder):
    def __init__(self, enum_value, options):
        super(CppEnumValueBuilder, self).__init__(options=options)
        self.enum_value = enum_value
        self._enum_value = None

    def cppEnumValue(self):
        if not self._enum_value:
            if isinstance(self.enum_value.iparent.iparent, proto.Package): # Is this a top-level enum?
                name = enum_valueName(self.enum_value)
            else:
                name = path_string(self.enum_value.iparent.cpp.cppEnum(), separator="_") + "_" + enum_valueName(self.enum_value)
            self._enum_value = EnumValue(None, name, self.enum_value.value)
            self._enum_value.enum = self.enum_value.iparent.cpp.cppEnum()
        return self._enum_value

    def element(self):
        return self.cppEnumValue()

def buildCppElements(package):
    if package.cpp.isBuilt():
        return

    message_set = package.cpp.cppMessageSet()
    op_message_set = package.cpp.cppOpMessageSet()
    descriptor = package.cpp.descriptorClass()

    proto_set = set()

    def processMessage(parent, message):
        cpp_el = message.cpp.klass()
        # Temporarily disabled, enable when classes are built entirely from CppElement
        #message_set.add(cpp_el)
        if op_message_set and package.cpp.useOpMessageSet:
            op_message_set.add(message.cpp.opMessage())
        if parent:
            typedef = message.cpp.typedef()
            typedef.comment = Comment("Message %s" % ".".join(message.path()))
            parent.add(typedef) # sub-messages are placed as typedefs to the global cpp class
            parent.add(Spacer())
        for item in message.enums:
            process(cpp_el, item)
        for item in message.items:
            process(cpp_el, item)

    def processEnum(parent, enum):
        cpp_el = enum.cpp.cppEnum()
        message_set.add(cpp_el)
        if parent:
            typedef = enum.cpp.typedef()
            typedef.comment = Comment("Enum %s" % ".".join(enum.path()))
            parent.add(typedef) # sub-enums are placed as typedefs to the global cpp enum
            for enum_value in enum.cpp.enumValues():
                enum_value.setImplementationComment(Comment("Enum value %s" % ".".join(enum_value.proto.path())))
                parent.add(enum_value) # enum values are placed as static const variables
            parent.add(Spacer())
        for item in enum.items:
            process(cpp_el, item)

    def process(parent, proto_el):
        if proto_el in proto_set:
            return
        proto_set.add(proto_el)
        if isinstance(proto_el, proto.Message):
            processMessage(parent, proto_el)
        elif isinstance(proto_el, proto.Enum):
            processEnum(parent, proto_el)

    for item in package.items:
        process(None, item)

    package.cpp.is_built = True

optionMap = {}

def getDefaultOptions(item):
    "Returns the default options for a given item as a dictionary."
    options = {}
    if isinstance(item, Field):
        cpp_method = set(["GetRef"])
        # String fields should replaces the GetRef with a SetUniStringPtr method
        if item.type == Proto.String and item.q != Quantifier.Repeated:
            cpp_method.remove("GetRef")
            cpp_method.add("SetUniStringPtr")
            # UniString is good at assigning from a const ref so add that by default
            if item.options["cpp_datatype"] == "UniString":
                cpp_method.add("SetString")
        if item.type == Proto.Bytes and item.q != Quantifier.Repeated:
            # OpData is good at assigning from a const ref so add that by default
            if item.options["cpp_datatype"] == "OpData":
                cpp_method.remove("GetRef")
                cpp_method.add("SetByteBuffer")
                cpp_method.add("SetBytePtr")
        if item.type == Proto.Message and item.q == Quantifier.Optional:
            cpp_method.remove("GetRef")
            cpp_method.add("SetMessage")
        options["cpp_method"] = ",".join(cpp_method)
    return options

class OptionError(Exception):
    pass

def checkOptions(item):
    if isinstance(item, Service):
        service = item
        if "cpp_class" not in service.options:
            raise OptionError("Service '%s': Required option 'cpp_file' is missing" % service.name)

def applyOptions(pkg, pkg_name, base, cpp_config, build_options):
    """
    Applies default options for all elements in a protobuf package.
    The options are either determined by the element type or from the cpp.conf file.
    """
    if pkg in optionMap:
        return
    optionMap[pkg] = True
    if not cpp_config:
        cpp_config = currentCppConfig()
    build_options.config = cpp_config
    service_name = ""
    if pkg.services:
        service = pkg.services[0]
        service_name = service.name

    base = base.replace("\\", "/")
    global_options = {"Field": {
                        "cpp_datatype": "",
                        "cpp_exttype": "",
                        },
                      "Service": {
                        "cpp_generate": "true",
                        "cpp_base": base,
                        "cpp_src_base": base + "/src",
                        "cpp_gen_base": base + "/src/generated",
                        },
                      "Package": {
                        "cpp_opmessage": False,
                        "cpp_opmessage_set": False,
                        "cpp_opmessage_prefix": "Op",
                        "cpp_opmessage_suffix": "Message",
                        },
                      "Message": {
                        "cpp_opmessage": "", # Default is to use the package option
                        "cpp_opmessage_prefix": "",
                        "cpp_opmessage_suffix": "",
                        # Allocation strategy for OpMessage, defaults to regular new
                        "cpp_opmessage_allocate": "new",
                        },
                      }
    for element in (Package, Service, Command, Request, Event, Message, Field, Enum, Option):
        if element.__name__ not in global_options:
            global_options[element.__name__] = {}

        def updateOptions(optionstext):
            options = {}
            for option in optionstext.split(";"):
                if not option.strip(): # Skip empty entries
                    continue
                name, value = option.split(":", 1)
                options[name.strip()] = value.strip()
            global_options[element.__name__].update(options)

        if cpp_config.has_option("options", "_" + element.__name__):
            updateOptions(cpp_config.get("options", "_" + element.__name__))
        if cpp_config.has_option(pkg_name + ".options", "_" + element.__name__):
            updateOptions(cpp_config.get(pkg_name + ".options", "_" + element.__name__))
        if cpp_config.has_option(service_name + ".options", "_" + element.__name__):
            updateOptions(cpp_config.get(service_name + ".options", "_" + element.__name__))

    definitions = {
        "Package": CppPackageBuilder,
        "Message": CppMessageBuilder,
        "Service": CppServiceBuilder,
        "Field": CppFieldBuilder,
        "Enum": CppEnumBuilder,
        "EnumValue": CppEnumValueBuilder,
        }

    def applyItemOptions(item):
        path = ".".join(item.path(relative=pkg))
        if type(item).__name__ in global_options:
            for name, value in global_options[type(item).__name__].iteritems():
                if name not in item.options:
                    item.options[name] = value
        options = None
        if cpp_config.has_option(pkg_name + ".options", path):
            options = cpp_config.get(pkg_name + ".options", path)
        elif service_name and cpp_config.has_option(service_name + ".options", path):
            options = cpp_config.get(service_name + ".options", path)
        if options:
            for option in options.split(";"):
                if not option.strip(): # Skip empty entries
                    continue
                name, value = option.split(":", 1)
                item.options[name.strip()] = value.strip()

        for subitem in item.items:
            applyItemOptions(subitem)
        checkOptions(item)

    def applyBuilder(item):
        if not hasattr(item, "cpp") and type(item).__name__ in definitions:
            item.cpp = definitions[type(item).__name__](item, build_options)
        for subitem in item.items:
            applyBuilder(subitem)

    applyItemOptions(pkg)
    applyBuilder(pkg)

    # Default to OpData and UniString when not generating a service class
    if not pkg.cpp.needsServiceClass():
        def scan(item):
            if isinstance(item, Field):
                field = item
                if field.options["cpp_datatype"].value == "":
                    if field.type == Proto.String:
                        field.options["cpp_datatype"] = "UniString"
                    elif field.type == Proto.Bytes:
                        field.options["cpp_datatype"] = "OpData"
            for subitem in item.items:
                scan(subitem)
        scan(pkg)

def currentConfig():
    return settings["config"]

def currentCppConfig():
    return settings["cpp_config"]

def setCurrentConfig(config, cpp_config):
    settings["config"] = config
    settings["cpp_config"] = cpp_config

def loadPackage(path, base, cpp_config=None):
    from hob.proto import loadPackage
    pkg = loadPackage(path, "")
    build_options = CppBuildOptions(os.path.dirname(base), os.path.basename(base))
    applyOptions(pkg, os.path.splitext(os.path.basename(path))[0], base=base, cpp_config=cpp_config, build_options=build_options)
    return pkg
