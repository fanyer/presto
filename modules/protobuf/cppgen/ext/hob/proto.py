import os
import re
import sys
import ConfigParser
import StringIO
import textwrap
from copy import copy
from itertools import chain

from distutils.version import StrictVersion
from hob.utils import dump_docblock, parse_docblock
import util

__all__ = ("ProtoError", "ConfigError",
           "ProtoType", "Proto", "QuantifierType", "Quantifier",
           "Option", "OptionValue", "OptionGroup", "OptionManager", "Options",
           "Field", "Message", "defaultMessage", "Command", "Request", "Event",
           "Service", "assignInternalId", "iterTree",
           "ValidationError", "ErrorType", "Error", "Validator",
           "OperaValidator", "ServiceManager", "PackageManager",
           "iterProtoFiles", "loadService", "Target", "Config", "defaultPath",
           )

class ProtoError(Exception):
    pass

class ConfigError(ProtoError):
    pass

class ProtoType(object):
    def __init__(self, id_, name):
        self.id = id_
        self.name = name

    def __repr__(self):
        return "ProtoType(%r,%r)" % (self.id, self.name)

    def __hash__(self):
        return hash((self.id, self.name))

    def __eq__(self, other):
        if isinstance(other, ProtoType):
            return (self.id, self.name) == (other.id, other.name)
        return False

    def __ne__(self, other):
        return not self.__eq__(other)

class Proto(object):
    Int32    = ProtoType(1,  "int32")
    Uint32   = ProtoType(2,  "uint32")
    Sint32   = ProtoType(3,  "sint32")
    Fixed32  = ProtoType(4,  "fixed32")
    Sfixed32 = ProtoType(5,  "sfixed32")
    Int64    = ProtoType(6,  "int64")
    Uint64   = ProtoType(7,  "uint64")
    Sint64   = ProtoType(8,  "sint64")
    Fixed64  = ProtoType(9,  "fixed64")
    Sfixed64 = ProtoType(10, "sfixed64")
    Bool     = ProtoType(11, "bool")
    String   = ProtoType(14, "string")
    Bytes    = ProtoType(15, "bytes")
    Message  = ProtoType(16, "message")
    Float    = ProtoType(17, "float")
    Double   = ProtoType(18, "double")

    @staticmethod
    def find(name):
        for k in dir(Proto):
            v = getattr(Proto, k)
            if isinstance(v, ProtoType):
                if v.name == name and v.name != "message":
                    return v
        return None

class QuantifierType(object):
    def __init__(self, id_, name):
        self.id = id_
        self.name = name

    def __repr__(self):
        return "QuantifierType(%r,%r)" % (self.id, self.name)

    def __hash__(self):
        return hash((self.id, self.name))

    def __eq__(self, other):
        if isinstance(other, QuantifierType):
            return (self.id, self.name) == (other.id, other.name)
        return False

    def __ne__(self, other):
        return not self.__eq__(other)

class Quantifier(object):
    Required = QuantifierType(1, "required")
    Optional = QuantifierType(2, "optional")
    Repeated = QuantifierType(3, "repeated")

class DocBlock(object):
    def __init__(self, text):
        self.text = text

    def toComment(self, is_block=True, indent=0):
        return dump_docblock(self.text, is_block, indent=indent)

    @staticmethod
    def fromComment(cmt):
        DocBlock(parse_docblock(cmt))

class Element(object):
    group = None
    name = None
    doc = None
    iparent = None
    items = []

    def __init__(self, group, name, doc=None, iparent=None, items=None, options=None):
        self.group = group
        self.name = name
        if doc != None and not isinstance(doc, DocBlock):
            doc = DocBlock(doc)
        self.doc = doc
        self.items = items or []
        self.iparent = iparent
        self.options = Options(options or [])

        if self.iparent and self not in self.iparent.items:
            self.iparent.addItem(self)
        if self.items:
            for child in self.items:
                if child.iparent:
                    raise ProtoError("Element %s already has the parent %s, cannot reassign to %s" % (child.name, child.iparent.name, self.name))
                child.iparent = self

    def path(self, relative=None, filter=None):
        if self.name:
            current = [self.name]
        else:
            current = []
        if self.iparent and self.iparent != relative and isinstance(self.iparent, filter or Element) and not isinstance(self.iparent, Package):
            path = self.iparent.path(relative=relative, filter=filter) + current
        else:
            path  = current
        return path

    def addItem(self, item):
        if item.iparent:
            raise ProtoError("Message %s already has the parent %s, cannot reassign to %s" % (item.name, item.iparent.name, self.name))
        self.items.append(item)
        item.iparent = self

    def getItem(self, name, **kwargs):
        """Returns the first item with the specified name.
        If `default` is set it will return this value when item is not found,
        otherwise KeyError is raised.
        """
        for item in self.items:
            if item.name == name:
                return item
        if "default" in kwargs:
            return kwargs["default"]
        raise KeyError("Could not found item with name '%s'" % name)

    __getitem__ = getItem

    def __contains__(self, name):
        for item in self.items:
            if item.name == name:
                return True
        return False

    # deprecated, should use obj.options[name] = option
    def addOption(self, name, value, doc=None):
        value.doc = doc
        self.options[name] = value

    # TODO: Should find a better way to check for custom options
    def isCustomOption(self, option):
        return _options.isCustomOption(self.group + "Options", option)

# TODO: All option groups should be treated as messages and options as fields,
#       this is how they are defined in Google's Protocol Buffer implementation
class Option(object):
    def __init__(self, name, id_):
        self.name = name
        self.id = id_

    def __hash__(self):
        return hash(self.name)

    def __repr__(self):
        return "Option(%r,%r)" % (self.name, self.id)

def parse_option_value(text):
    """
    >>> parse_option_value("true")
    True
    >>> parse_option_value("false")
    False
    >>> parse_option_value("50")
    50
    >>> parse_option_value("0")
    0
    >>> parse_option_value("5.0")
    5.0
    >>> parse_option_value("an_identifier")
    'an_identifier'
    >>> parse_option_value('"a string"')
    'a string'
    >>> parse_option_value("")
    ''
    """
    try:
        return int(text)
    except ValueError:
        pass
    try:
        return float(text)
    except ValueError:
        pass
    if text == "true":
        return True
    elif text == "false":
        return False
    elif re.match('".*"$', text):
        return text[1:-1].replace('\\"', '"').replace('\\\\', '\\')
    else:
        return text

class OptionValue(object):
    raw = None
    value = None

    def __init__(self, value, raw=None, doc=None):
        if isinstance(value, OptionValue):
            self.value = value.value
            self.raw = value.raw
        else:
            self.value = value
            if raw == None:
                raw = str(value)
            self.raw = raw
        if doc != None and not isinstance(doc, DocBlock):
            doc = DocBlock(doc)
        self.doc = doc

    def __eq__(self, o):
        if isinstance(o, OptionValue):
            o = o.value
        return o == self.value

    def __ne__(self, o):
        return not self == o

    def __repr__(self):
        extra = ""
        if self.doc != None:
            extra = ",%r" % self.doc
        return "OptionValue(%r, raw=%r%s)" % (self.value, self.raw, extra)

    @staticmethod
    def loads(text):
        value = parse_option_value(text)
        return OptionValue(value, raw=text)

    def dumps(self):
        """
        >>> OptionValue(5).dumps()
        '5'
        >>> OptionValue(5.0).dumps()
        '5.0'
        >>> OptionValue(0).dumps()
        '0'
        >>> OptionValue(True).dumps()
        'true'
        >>> OptionValue(False).dumps()
        'false'
        >>> OptionValue('foo').dumps()
        'foo'
        >>> OptionValue('a string').dumps()
        '"a string"'
        >>> OptionValue(None, raw='"a string"').dumps()
        '"a string"'
        >>> OptionValue(None, raw='another string').dumps()
        '"another string"'
        >>> OptionValue(None, raw='id').dumps()
        'id'
        """
        value = self.value
        if value == None:
            value = self.raw
            if not re.match('".*"$', value) and not re.match("[a-zA-Z0-9_-]+$", value):
                value = '"' + value.replace("\n", "\\n").replace('"', '\\"') + '"'
        else:
            if isinstance(value, (str, unicode)):
                if not re.match("[a-zA-Z0-9_-]+$", value):
                    value = '"' + value.replace("\n", "\\n").replace('"', '\\"') + '"'
            elif value is True:
                value = "true"
            elif value is False:
                value = "false"
            else:
                value = str(value)
        return value

    __str__ = dumps

class OptionGroup(dict):
    def __init__(self, name):
        dict.__init__(self)
        self.name = name
        self.nameMap = {}
        self.idMap = {}

    def getOption(self, option):
        if isinstance(option, int):
            return self.idMap[option]
        elif isinstance(option, (str, unicode)):
            return self.nameMap[option]
        else:
            raise TypeError("Invalid type for getOption %r" % type(option))

    def __contains__(self, option):
        if isinstance(option, int):
            return option in self.idMap
        elif isinstance(option, (str, unicode)):
            return option in self.nameMap
        else:
            return dict.__contains__(self, option)

    def __getitem__(self, option):
        if isinstance(option, int):
            option = self.idMap[option]
        elif isinstance(option, (str, unicode)):
            option = self.nameMap[option]
        return dict.__getitem__(self, option)

    def __setitem__(self, option, value):
        if not isinstance(option, Option):
            raise TypeError("must be of type Option")
        dict.__setitem__(self, option, value)
        self.nameMap[option.name] = option
        self.idMap[option.id] = option

    def __delitem__(self, option):
        if isinstance(option, int):
            option = self.idMap[option]
        elif isinstance(option, (str, unicode)):
            option = self.nameMap[option]
        dict.__delitem__(self, option)
        del self.nameMap[option.name]
        del self.idMap[option.id]

class OptionManager(object):
    def __init__(self):
        self.groups = {}
        for group in ("FileOptions", "MessageOptions", "FieldOptions",
                      "EnumOptions", "EnumValueOptions",
                      "ServiceOptions", "MethodOptions"):
            self.groups[group] = OptionGroup(group)

    def getOptionValue(self, group, option):
        return self.groups[group][option]

    def getOption(self, group, option):
        return self.groups[group].getOption(option)

    def setOptionValue(self, group, option, value):
        self.groups[group][option] = value

    def setOptionValues(self, group, items):
        for option, value in items:
            self.groups[group][option] = value

    def delOption(self, group, option):
        del self.groups[group][option]

    def hasOption(self, group, option):
        return group in self.groups and option in self.groups[group]

    def isCustomOption(self, group, option):
        if type(option) == int:
            id_ = option
        elif isinstance(option, Option):
            id_ = option.id
        else:
            if not self.hasOption(group, option):
                return True
            id_ = self.getOption(group, option).id
        return id_ >= 1000

_options = OptionManager()
# Default options from Google
_options.setOptionValues("MessageOptions",
                         [(Option("message_set_wireformat", 1), OptionValue(False, raw="false")),
                          ])
_options.setOptionValues("FileOptions",
                         [(Option("java_package", 1),         OptionValue("", raw="")),
                          (Option("java_outer_classname", 8), OptionValue("", raw="")),
                          (Option("java_multiple_files", 10), OptionValue(False, "false")),
                          (Option("optimize_for", 9),         OptionValue(0, raw="0")),
                          ])
_options.setOptionValues("FieldOptions",
                         [(Option("ctype", 1),                OptionValue(0, raw="0")),
                          (Option("experimental_map_key", 9), OptionValue("", raw="")),
                          ])

# Our options
# 00000-50499 = scope options
# 50500-50799 = cpp options
# 50800-51099 = java options
# 51100-51299 = python options
# 51300-51599 = javascript options
_options.setOptionValues("MessageOptions",
                         [])
#                    [(Option("cpp_send_message", 50500), OptionValue(False, raw="false")),
#                     ])
_options.setOptionValues("ServiceOptions",
                         [(Option("version",      50000), OptionValue("", raw="")),
                          (Option("core_release", 50001), OptionValue("", raw="")),
                          (Option("cpp_class",    50500), OptionValue("", raw="")),
                          (Option("cpp_hfile",    50501), OptionValue("", raw="")),
                          ])
# cpp_response: Controls how the cpp method handles responses,
#               "direct" means filling in response object and responding after the call
#               "deferred" means sending the response at a later time
# TODO: Should cpp_response be an enum?
_options.setOptionValues("MethodOptions",
                         [(Option("cpp_response", 50502), OptionValue("direct", raw="direct")),
                          ])

class Options(dict):
    def __init__(self, *args, **kwargs):
        self.ordered = []
        if args:
            if isinstance(args[0], Options):
                for k, v in args[0].iteritems():
                    self[k] = v
            elif isinstance(args[0], dict):
                for k, v in args[0].iteritems():
                    self[k] = OptionValue(v)
            else:
                for k, v in args[0]:
                    self[k] = OptionValue(v)
        else:
            for k, v in kwargs.iteritems():
                self[k] = OptionValue(v)

    def __setitem__(self, key, value):
        if not isinstance(value, OptionValue):
            value = OptionValue(value)
        dict.__setitem__(self, key, value)
        self.ordered.append(key)

    def __delitem__(self, key):
        dict.__delitem__(self, key)
        self.ordered.remove(key)

    def __iter__(self):
        for key in self.ordered:
            yield self[key]

    def iterkeys(self):
        for key in self.ordered:
            yield key

    def itervalues(self):
        for key in self.ordered:
            yield self[key]

    def iteritems(self):
        for key in self.ordered:
            yield key, self[key]

    def __repr__(self):
        if len(self) > 0:
            return "Options(%s)" % super(Options, self).__repr__()
        else:
            return "Options()"

class Enum(Element):
    def __init__(self, name, values=None, iparent=None, options=None, doc=None):
        super(Enum, self).__init__("Enum", name, iparent=iparent, options=options, doc=doc)
        if values == None:
            values = []
        self.values = values

    def addItem(self, item):
        super(Enum, self).addItem(item)
        if isinstance(item, EnumValue):
            self.values.append(item)
    addValue = addItem

    def __repr__(self):
        extras = [repr(self.values)]
        if self.options:
            extras.append(repr(self.options))
        return "Enum(%r,%s)" % (self.name, ",".join(extras))

class EnumValue(Element):
    def __init__(self, name, value, iparent=None, options=None, doc=None):
        super(EnumValue, self).__init__("EnumValue", name, iparent=iparent, options=options, doc=doc)
        self.value = value

    def __repr__(self):
        extras = []
        if self.options:
            extras = ["", repr(self.options)]
        if self.doc:
            extras.append(repr(self.doc))
        return "EnumValue(%r,%r%s)" % (self.name, self.value, ",".join(extras))

class Package(Element):
    def __init__(self, filename, name=None, messages=None, enums=None, services=None, options=None, imports=None, items=None, doc=None):
        super(Package, self).__init__("File", "", options=options, items=items, doc=doc)
        self.filename = filename
        self.messages = messages or []
        self.enums = enums or []
        self.services = services or []
        self.options = Options(options or [])
        self.imports = imports or []

        self.packages = {"": self}
        self._lookup = {}
        for pkg in self.imports:
            if pkg.name:
                self.packages[pkg.name] = pkg
                l = self._lookup
                names = pkg.name.split(".")
                for name in names[:-1]:
                    l = l[name] = l.get(name, {})
                l[names[-1]] = pkg

    def setName(self, name):
        if self.name:
            del self.packages[self.name]
        self.name = name
        self.packages[self.name] = self

    def addItem(self, item):
        super(Package, self).addItem(item)
        if isinstance(item, Package):
            self.imports.append(item)
        elif isinstance(item, Message):
            self.messages.append(item)
        elif isinstance(item, Enum):
            self.enums.append(item)
        elif isinstance(item, Service):
            self.services.append(item)

    addImport = addItem
    addMessage = addItem
    addEnum = addItem
    addService = addItem

    def getMessage(self, path):
        idx = path.rfind(".")
        if idx != -1:
            package, name = path[0:idx], path[idx + 1:]
        else:
            package, name = "", path
        if package not in self.packages:
            raise KeyError("No such package '%s'" % package)
        for msg in self.packages[package].messages:
            if msg.name == name:
                return msg
        raise KeyError("No such message '%s' in package '%s'" % (name, package))

    def find(self, path, current):
        path = path.split(".")

        def msg_find(msg, path):
            for p in path:
                newmsg = None
                for child in msg.items:
                    if child.name == p:
                        newmsg = child
                        break
                if not newmsg:
                    return None
                msg = newmsg
            return msg

        item = None
        if current:
            while current:
                for child in current.items:
                    num = None
                    if isinstance(child, EnumValue):
                        try:
                            num = int(path[0])
                        except ValueError:
                            pass
                    if child.name == path[0] or (num is not None and child.value == num):
                        item = child
                        if path[1:]:
                            item = msg_find(item, path[1:])
                        if not item:
                            raise NameError("Invalid path '%s'" % ".".join(path))
                        return item
                current = current.iparent

        item = msg_find(self, path)
        if item:
            return item
        for pkg in self.imports:
            if not pkg.name:
                item = msg_find(pkg, path)
            else:
                pkg_path = pkg.name.split(".")
                if len(path) <= len(pkg_path):
                    continue
                if pkg_path != path[0:len(pkg_path)]:
                    continue
                item = msg_find(pkg, path[len(pkg_path):])
            if not item:
                raise NameError("Could not find item in path '%s'" % ".".join(path))
            return item
        raise NameError("Could not find item in path '%s'" % ".".join(path))

    def __repr__(self):
        extra = ""
        if self.doc != None:
            extra = ",%r" % self.doc
        return "Package(%r,%r,%r,%r,%r,%r,%r,%r%s)" % (self.filename, self.name, self.messages, self.enums, self.services, self.options, self.imports, self.items, extra)

class Field(Element):
    def __init__(self, type_, name, number, q=None, default=None, message=None, iparent=None, options=None, doc=None):
        super(Field, self).__init__("Field", name, iparent=iparent, options=options, doc=doc)
        self.type = type_
        self.number = number
        if not q:
            q = Quantifier.Required
        self.q = q
        self.default = default
        self.default_text = str(default)
        self.default_object = default
        self.message = message
        self.comment = None

    def typename(self):
        ptype = self.type
        if self.message:
            return ".".join(self.message.path(relative=self.iparent))
        return ptype.name

    def defaultValue(self):
        if self.message and isinstance(self.message, Enum):
            if isinstance(self.default_object, EnumValue):
                return self.default_object.name
            raise ProtoError("Invalid default value for Enum object %s" % ".".join(self.message.path()))
        elif self.type == Proto.Bool:
            if self.default:
                return "true"
            else:
                return "false"
        elif self.type == Proto.String:
            return repr(self.default)
        elif self.type == Proto.Bytes:
            return repr(self.default)
        elif self.type == Proto.Message:
            raise ProtoError("Cannot use default values for field of type Message")
        else:
            return str(self.default)

    def defaultObject(self):
        return self.default_object

    def defaultText(self):
        return self.default_text

    def setDefault(self, value):
        self.default = value
        self.default_object = value

    def setDefaultValue(self, value):
        self.default = value

    def setDefaultText(self, text):
        self.default_text = text

    def setDefaultObject(self, item):
        self.default_object = item

class Message(Element):
    fields = []
    count = 0
    internal_id = None
    parent = None
    children = []
    references = []

    def __init__(self, name, fields=None, doc=None, update=True, internal_id=None, parent=None, iparent=None, children=None, items=None, options=None, is_extension=False):
        super(Message, self).__init__("Message", name, doc=doc, iparent=iparent, items=(items or []) + (children or []), options=options)
        if fields == None:
            fields = []
        self.fields = fields
        self.fieldMap = {}
        for field in self.fields:
            if not field.name:
                raise ProtoError("Empty `name` for Field object", field)
            self.fieldMap[field.name] = field
        self.internal_id = internal_id
        if children:
            self.children = children[:]
        else:
            self.children = []
        self.parent = parent
        self.enums = []
        self.extensions = []
        self.is_extension = is_extension
        self.comment = None
        if self.parent and self not in self.parent.children:
            self.parent.addMessage(self)
        if self.children:
            for child in self.children:
                if child.parent:
                    raise ProtoError("Message %s already has the parent %s, cannot reassign to %s" % (child.name, child.parent.name, self.name))
                child.parent = self

    # Deprecated, use path. Provided for compatibility with hob=0.1
    def absPath(self):
        if self.parent:
            path = self.parent.absPath() + [self.name]
        else:
            path  = [self.name]
        return path

    def isNonEmpty(self):
        return len(self.fields) != 0

    def addExtension(self, start, end):
        self.extensions.append((start, end))

    def addItem(self, item):
        super(Message, self).addItem(item)
        if isinstance(item, Enum):
            self.enums.append(item)
        elif isinstance(item, Field):
            self.fields.append(item)

    addEnum = addItem
    addField = addItem

    def addMessage(self, msg):
        self.children.append(msg)
        if msg.parent:
            raise ProtoError("Message %s already has the parent %s, cannot reassign to %s" % (msg.name, msg.parent.name, self.name))
        msg.parent = self
        self.addItem(msg)

    def references(self, existing=None, only_top=True):
        if existing == None:
            existing = {}
        existing[self] = True
        for field in self.fields:
            if field.type != Proto.Message or not field.message:
                continue
            message = field.message
            orig = message
            if only_top:
                while isinstance(message.iparent, Message):
                    message = message.iparent
            exported = message in existing
            if orig not in existing:
                for msg in orig.references(existing):
                    yield msg
            if exported:
                continue
            yield message
            for msg in message.references(existing):
                yield msg

defaultMessage = Message("Default",
                          fields=[])

class Command(Element):
    id = None
    message = None

    def __init__(self, id, name, message, options=None, doc=None):
        super(Command, self).__init__("Method", name, doc=doc)
        self.id = id
        if not message:
            message = defaultMessage
        self.message = message

    def messageName(self):
        return self.message.name

    def messageID(self):
        if self.message and self.message.internal_id != None:
            return self.message.internal_id
        return 0

    def responseID(self):
        return 0

class Request(Command):
    response = None
    type = "Command"

    def __init__(self, id, name, message, response, options=None, doc=None):
        super(Request, self).__init__(id, name, message, options=options, doc=doc)
        if not response:
            response = defaultMessage
        self.response = response

    def responseName(self):
        return self.response.name

    def responseID(self):
        if self.response and self.response.internal_id != None:
            return self.response.internal_id
        return 0

    def toEvent(self):
        ev = Event(self.id, self.name, self.response, doc=self.doc)
        ev.options = Options(self.options)
        return ev

    def toRequest(self):
        req = Request(self.id, self.name, self.message, False, doc=self.doc)
        req.options = Options(self.options)
        return req

class Event(Command):
    type = "Event"

    def __init__(self, id, name, message, options=None, doc=None):
        super(Event, self).__init__(id, name, message, options=options, doc=doc)

def assignInternalId(messages):
    unassigned = [msg for msg in messages if msg != defaultMessage and msg.internal_id == None]
    for next_id, msg in enumerate(unassigned):
        msg.internal_id = next_id + 1  # IDs start at 1

def iterTree(messages, existing=None, with_global=True, with_inline=True, filter=None):
    messages = list(messages)
    if existing == None:
        existing = {}
    while messages:
        message = messages.pop(0)
        if message in existing:
            continue
        existing[message] = True
        yield message
        for field in message.fields:
            if field.type != Proto.Message or not field.message or isinstance(field.message, Enum) or field.message in existing:
                continue
            if with_global and isinstance(field.message.iparent, filter or Message):
                continue
            messages.append(field.message)
        if with_inline and message.children:
            messages.extend(message.children)

class Service(Element):
    commands = []

    def __init__(self, name, commands=None, iparent=None, options=None, doc=None):
        super(Service, self).__init__("Service", name, items=commands, iparent=iparent, options=options, doc=doc)
        #self.commands = [item for item in self.items if isinstance(item, Command)]
        self.commands = [cmd for cmd in commands or [] if isinstance(cmd, Command)]

    def addItem(self, item):
        super(Service, self).addItem(item)
        if isinstance(item, Command):
            self.commands.append(item)

    addCommand = addItem

    def getCommandCount(self):
        return len(self.commands)

    def messages(self, only_global=True):
        messages = {}

        def field_messages(message):
            new_fields = message.fields[:]
            while new_fields:
                fields = new_fields
                new_fields = []
                for field in fields:
                    if field.type == Proto.Message and field.message and (not only_global or not isinstance(field.message.iparent, Message)) and field.message not in messages:
                        yield field.message
                        messages[field.message] = True
                        new_fields.extend(field.message.fields)

        for command in self.commands:
            if command.message:
                if command.message not in messages:
                    yield command.message
                    messages[command.message] = True
                    for message in field_messages(command.message):
                        yield message
            if hasattr(command, 'response') and command.response:
                if command.response not in messages:
                    yield command.response
                    messages[command.response] = True
                    for message in field_messages(command.response):
                        yield message

    def itercommands(self):
        for command in sorted(self.commands, key=lambda item: item.id):
            if issubclass(type(command), Request):
                yield command

    def iterevents(self):
        for command in sorted(self.commands, key=lambda item: item.id):
            if issubclass(type(command), Event):
                yield command

class ValidationError(Exception):
    errors = []
    warnings = []

    def __init__(self, errors, warnings):
        self.errors = errors
        self.warnings = warnings

    def __unicode__(self):
        return u"\n".join([unicode(error) for error in self.errors]) + u"\n".join([unicode(warning) for warning in self.warnings])

    def __str__(self):
        return "\n".join([str(error) for error in self.errors]) + "\n".join([str(warning) for warning in self.warnings])

class ErrorType(object):
    # errors
    CommandIDInvalid   = u"command-id-invalid"
    CommandIDConflict  = u"command-id-conflict"
    CommandNameInvalid = u"command-name-invalid"
    EventNameInvalid = u"event-name-invalid"
    MessageNameInvalid = u"message-name-invalid"
    FieldIDInvalid = u"field-id-invalid"
    FieldNameInvalid = u"field-name-invalid"

    # warnings
    FieldIDGap = u"field-id-gap"
    FieldIDSequence = u"field-id-sequence"

    warnings = [FieldIDGap, FieldIDSequence]

class Error(object):
    error_type = None
    description = None
    names = []
    path = None

    def __init__(self, error_type, description, names, path):
        self.error_type = error_type
        self.description = description
        self.names = names
        self.path = path

    def __unicode__(self):
        kind = u"error"
        if self.error_type in ErrorType.warnings:
            kind = u"warning"
        name = u"%s: %s" % (kind, u":".join(map(unicode, self.names)))
        if self.path:
            name = self.path + u": " + unicode(name)
        return name + u": " + self.description

    __str__ = __unicode__

class Validator(object):
    errors = []
    warnings = []
    configuration = {}

    def __init__(self):
        self.errors = self.errors[:]
        self.warnings = self.warnings[:]
        self.configuration = {}
        for v in ErrorType.warnings:
            self.configuration[v] = False  # Warnings are off by default

    def enableWarning(self, warning):
        if warning == "all":
            for warning in self.configuration.iterkeys():
                self.configuration[warning] = True
            return

        if warning in self.configuration:
            self.configuration[warning] = True

    def isValid(self):
        return not self.errors

    def validateMessage(self, message, service=None, path=None):
        pass

    def validateService(self, service, path=None):
        pass

    def reportError(self, error_type, description, names, path=None):
        if not self.configuration.get(error_type, True):
            return
        error = Error(error_type, description, names, path)
        if error_type in ErrorType.warnings:
            self.warnings.append(error)
        else:
            self.errors.append(error)

    def exception(self):
        return ValidationError(errors=self.errors, warnings=self.warnings)

class OperaValidator(Validator):
    def __init__(self):
        super(OperaValidator, self).__init__()
        self.message_re = re.compile("([A-Z][a-z]+)+")
        self.command_re = re.compile("([A-Z][a-z]+)+")
        self.event_re = re.compile("On([A-Z][a-z]+)+")
        self.field_re = re.compile("[a-z]+(([A-Z]+)|([A-Z][a-z]+))*")

    def validateService(self, service, path=None):
        ids = {}
        report_sequence = True
        for idx, command in enumerate(service.commands):
            if report_sequence and idx + 1 != command.id:
                self.reportError(ErrorType.FieldIDSequence, "%s %s should have ID %d but has %d" % (command.type, command.name, idx + 1, command.id), [service.name], path)
                report_sequence = False
            if command.id < 0:
                self.reportError(ErrorType.CommandIDInvalid,
                                 "Invalid ID %d for %s %s, only positive values up can be used"
                                  % (command.id,
                                     command.type,
                                     command.name),
                                   [service.name], path)
            if command.id in ids:
                self.reportError(ErrorType.CommandIDConflict,
                                 "ID conflict for %s %r, ID %d is already used by %s %s"
                                  % (command.type,
                                     command.name,
                                     command.id,
                                     ids[command.id].type,
                                     ids[command.id].name),
                                   [service.name], path)
            else:
                ids[command.id] = command
        for command in service.itercommands():
            self.validateCommand(command, service, path)
        for event in service.iterevents():
            self.validateEvent(event, service, path)

    def validateCommand(self, command, service, path=None):
        if not self.command_re.match(unicode(command.name)):
            self.reportError(ErrorType.CommandNameInvalid,
                             "Invalid command name: %s: Names of commands must be written with capital letters for each word" % command.name, [service.name], path)

    def validateEvent(self, event, service, path=None):
        if not self.event_re.match(unicode(event.name)):
            self.reportError(ErrorType.EventNameInvalid,
                             "Invalid event name: %s: Names of event must be written with capital letters for each word and must start with 'On'" % event.name, [service.name], path)

    def validateMessage(self, message, service=None, path=None):
        if not self.message_re.match(unicode(message.name)):
            self.reportError(ErrorType.MessageNameInvalid,
                             "Invalid message name: %s: Names of messages must be written with capital letters for each word" % message.name, [service.name], path)
        ids = [field.number for field in message.fields]
        for idx, id_ in enumerate(ids):
            if idx + 1 != id_:
                self.reportError(ErrorType.FieldIDSequence, "Field IDs in message %r are not in sequence (1..)" % message.name, [service.name], path)
                break
        if ids:
            ids.sort()
            last = ids[0]
            for id in ids[1:]:
                if id != last + 1:
                    self.reportError(ErrorType.FieldIDGap, "Field IDs in message %r has gaps" % message.name, [service.name], path)
                    break
                last = id
        for field in message.fields:
            self.validateField(message, field, service, path=path)

    def validateField(self, message, field, service=None, path=None):
        if field.number <= 0:
            self.reportError(ErrorType.FieldIDInvalid,
                             "Invalid ID %d for field %s, only positive values from 1 and up can be used"
                              % (field.number,
                                 field.name),
                               [service.name, message.name], path)
        if not self.field_re.match(unicode(field.name)):
            self.reportError(ErrorType.FieldNameInvalid,
                             "Invalid field name: %s: Names of messages must be written with capital letters for each word, except for first word which must be all non-capital" % field.name, [service.name, message.name], path)
        if field.q == Quantifier.Repeated and not unicode(field.name).endswith("List"):
            self.reportError(ErrorType.FieldNameInvalid,
                             "Invalid field name: %s: Names of repeated field must always end with 'List'" % field.name, [service.name, message.name], path)

# TODO: Change this into a PackageManager
class ServiceManager(object):
    services = []
    packages = []
    strict = True

    __validator__ = Validator()
    __validator_type__ = Validator
    validator = __validator__
    validator_type = __validator_type__

    def __init__(self, strict=True):
        self.services = []
        self.packages = []
        self.strict = strict
        self.validator = PackageManager.validator_type()

    def loadFile(self, path, validate=True):
        if path.endswith(".py"):
            g = l = {}
            for n in Message, Proto, Field, Quantifier, Request, Event, Service:
                g[n.__name__] = n
            execfile(path, g, l)
            pkg = Package(path)
            for item in g.itervalues():
                if issubclass(type(item), Service):
                    pkg.addService(item)
                elif issubclass(type(item), Message) and not isinstance(item.iparent, Message):
                    pkg.addMessage(item)
                elif issubclass(type(item), Enum):
                    pkg.addEnum(item)
        else:
            from hob.parser import Lexer, create_lexer_rules, Loader, PackageBuilder

            l = Lexer(create_lexer_rules())
            loader = Loader(l)
            pkg = Package(path)
            builder = PackageBuilder(pkg, loader)
            loader.load(path, builder)
        self.packages.append(pkg)
        if validate:
            self.validatePackage(pkg, path)
        return pkg

    def loadServiceFile(self, path):
        pkg = self.loadFile(path)
        self.services.extend(pkg.services)
        return copy(pkg.services)

    def validatePackage(self, pkg, path):
        for item in pkg.services:
            if self.validator:
                self.validator.validateService(item, path=path)
                for message in iterTree(item.messages()):
                    self.validator.validateMessage(message, service=item, path=path)
        if self.strict and self.validator:
            if not self.validator.isValid():
                raise self.validator.exception()
            if self.validator.warnings:
                for warning in self.validator.warnings:
                    print >> sys.stderr, warning

class PackageManager(ServiceManager):
    pass

def iterProtoFiles(paths):
    def isProtoFile(fname):
        if fname[-3:] == ".py":
            text = open(fname).read(40)
            return bool(re.search("# *hob", text))
        if fname[-6:] == ".proto":
            return True
        return False

    for path in paths:
        if os.path.isdir(path):
            for dirpath, dirnames, files in os.walk(path):
                for file in files:
                    path = os.path.join(dirpath, file)
                    if isProtoFile(path):
                        yield path
        elif os.path.exists(path):
            if isProtoFile(path):
                yield path
            else:
                raise ProtoError("File %s is not a valid proto file" % path)

__managers = {}

def loadService(path, name):
    if path not in __managers:
        manager = __managers[path] = ServiceManager()
        manager.loadServiceFile(path)
    else:
        manager = __managers[path]
    name = name.lower()
    for pkg in manager.packages:
        for s in pkg.services:
            service_name = unicode(s.name)
            if service_name.lower() == name:
                return s
    raise ProtoError("Could not find service named %r" % name)

def loadPackage(path, name):
    manager = ServiceManager()
    return manager.loadFile(path)

class Target(object):
    name = None
    config = None

    def __init__(self, name, config):
        self.name = name
        self.config = config

    def servicePath(self, servicename):
        if ("target." + self.name, servicename) not in self.config:
            raise ConfigError("Could not find service '%s' in target configuration '%s'" % (servicename, self.name))
        filepath = self.config[("target." + self.name, servicename)]
        path = self.config[("services", "path")]
        if not os.path.isabs(path) and self.config.base:
            path = os.path.join(self.config.base, path)
        return os.path.normpath(os.path.join(path, filepath))

    def findService(self, name):
        path = self.name
        if not os.path.isfile(path):
            path = self.servicePath(name)
        return loadService(path, name)

    def findPackage(self, name):
        path = self.name
        if not os.path.isfile(path):
            path = self.servicePath(name)
        return loadPackage(path, name)

    def services(self):
        if "target." + self.name not in self.config:
            return
        for name, value in sorted(self.config["target." + self.name], key=lambda s: s[0]):
            yield name

    def selectServiceFiles(self, names):
        if not names:
            for name in self.services():
                yield self.servicePath(name)
        for name in names:
            if os.path.isfile(name):
                yield name
            else:
                yield self.servicePath(name)

_booleans = {'1': True, 'yes': True, 'true': True, 'on': True,
             '0': False, 'no': False, 'false': False, 'off': False}

class Config(object):
    """
    The config object can either be initialized via a set of filenames
    by using read() or directly from a string using reads(), e.g.
    >>> text = chr(10).join(["[a section]", "key1 = value1"])
    >>> conf = Config()
    >>> conf.reads(text)

    The current config can be written to any file object
    >>> import StringIO
    >>> conf.write(StringIO.StringIO())

    Alternatively the config can more be returned as a string
    >>> conf.tostring().splitlines()
    ['[a section]', 'key1 = value1', '']

    The config entries can be accessed with subscript operators.
    Either supply a string to get back all key/values in a section.
    >>> conf["a section"]
    [('key1', 'value1')]

    Or a tuple(<section>, <key>) to get back the value of specific key.
    >>> conf[("a section", "key1")]
    'value1'

    Assigning new values can also be done for keys by using a tuple.
    >>> conf[("a section", "key3")] = "another one"
    >>> conf[("a section", "key3")]
    'another one'

    For a section the value must be a list of key/value tuples
    >>> conf["new section"] = [("foo", "bar")]
    >>> conf["new section"]
    [('foo', 'bar')]

    To check if a section or key is present use the 'in' operator
    >>> "a section" in conf
    True
    >>> "missing section" in conf
    False

    Removing a section or key is done using the 'del' operator
    >>> del conf[("new section", "foo")]
    >>> del conf["new section"]

    The same can be done for keys by using a tuple.
    >>> ("a section", "key1") in conf
    True
    >>> ("a section", "key2") in conf
    False
    """
    parser = None
    base   = None

    def __init__(self):
        self.parser = ConfigParser.SafeConfigParser()
        self.files = []

    def reset(self):
        self.parser = ConfigParser.SafeConfigParser()

    def read(self, filenames):
        files = []
        for fname in filenames:
            if util.fileTracker.addInput(fname):
                files.append(fname)
        self.files += files
        return self.parser.read(files)

    def reads(self, text):
        return self.parser.readfp(StringIO.StringIO(text))

    def write(self, filename):
        return self.parser.write(filename)

    def tostring(self):
        import StringIO
        s = StringIO.StringIO()
        self.parser.write(s)
        return s.getvalue()

    def __getitem__(self, name):
        if type(name) in (tuple, list):
            return self.parser.get(name[0], name[1])
        else:
            return self.parser.items(name)

    def __setitem__(self, name, value):
        if type(name) in (tuple, list):
            if not self.parser.has_section(name[0]):
                self.parser.add_section(name[0])
            return self.parser.set(name[0], name[1], value)
        else:
            if not self.parser.has_section(name):
                self.parser.add_section(name)
            for key, val in value:
                self.parser.set(name, key, val)

    def __delitem__(self, name):
        if type(name) in (tuple, list):
            return self.parser.remove_option(name[0], name[1])
        else:
            return self.parser.remove_section(name)

    def __contains__(self, name):
        if type(name) in (tuple, list):
            return self.parser.has_option(name[0], name[1])
        else:
            return self.parser.has_section(name)

    def bool(self, name, key, default=False):
        if (name, key) not in self:
            return default
        value = self[(name, key)].lower()
        if value not in _booleans:
            raise ConfigError("%s.%s is not a boolean ('%s')" % (name, key, self[(name, key)]))
        return _booleans[value]

    def currentTarget(self):
        return Target(self[("hob", "target")], self)

defaultPath = [os.path.expanduser('~/.hob.conf'), os.path.expanduser('~/hob.conf'), "hob.conf"]
