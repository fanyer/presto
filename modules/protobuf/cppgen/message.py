import os
from copy import copy
import re

from hob import utils, proto
from hob.proto import Request, Event, Proto, Quantifier, defaultMessage, iterTree, Target, Package, Service, Command, Message, Field, Enum, Option, DocBlock
from cppgen.cpp import TemplatedGenerator, CppScopeBase, CppGenerator, memoize, DependencyList, render, strQuote, _var_name, _var_path, message_className, messageClassPath, template, currentCppConfig, getDefaultOptions, generatorManager, upfirst, enum_name, enum_valueName, message_inlineClassName
from cppgen.cpp import CppElement, Class, Method, EnumValue, Parameter, Argument, ArgumentReference, Initializer, NullValue, CustomValue, path_string, path_name, toIntegral
from cppgen.cpp import fileHeader, cpp_path_norm, commaList, root as messageRoot, GeneratedFile
from cppgen.utils import stable_topological_sort

BY_VALUE = 1
BY_REF = 2

def passBy(field):
    """
    Return the type of parameter passing which is needed for the proto field.
    """
    ptype = field.type
    if field.q == Quantifier.Repeated:
        return BY_REF
    elif field.q == Quantifier.Optional and ptype == Proto.Message and isinstance(field.message, Message):
        return BY_VALUE
    else:
        if ptype in (Proto.String, Proto.Bytes, Proto.Message):
             return BY_REF
        return BY_VALUE

def initializerType(v):
    if type(v) in [str, unicode]:
        return strQuote(v)
    elif type(v) == bool:
        return {True: "TRUE", False: "FALSE"}[v]
    else:
        return str(v)

class TypeMap(object):
    def __init__(self, klass, map):
        self.klass = klass
        self.map = {}
        if isinstance(map, dict):
            for ptype, value in map.iteritems():
                self.map[ptype.id] = (ptype, value)
        else:
            for ptype, value in map:
                self.map[ptype.id] = (ptype, value)

    def __contains__(self, ptype):
        if isinstance(ptype, self.klass):
            key = ptype.id
        else:
            key = ptype
        return key in self.map

    def __getitem__(self, ptype):
        if not isinstance(ptype, self.klass):
            raise TypeError("Only works %s objects" % self.klass.__name__)
        key = ptype.id
        return self.map[key][1]

    def __setitem__(self, ptype, value):
        if not isinstance(ptype, self.klass):
            raise TypeError("Only works %s objects" % self.klass.__name__)
        key = ptype.id
        self.map[key] = (ptype, value)

    def iteritems(self):
        for item in self.map.itervalues():
            yield item

class ProtoTypeMap(TypeMap):
    def __init__(self, map):
        TypeMap.__init__(self, proto.ProtoType, map)

class QuantifierMap(TypeMap):
    def __init__(self, map):
        TypeMap.__init__(self, proto.QuantifierType, map)

ProtoTypeMap = dict
QuantifierMap = dict

def fieldType(field, allow_enum=True, relative=None, filter=None, prefix=None, exttype=None):
    cpp_relative = None
    if relative and hasattr(relative, "cpp"):
        cpp_relative = relative.cpp.element()
    if exttype:
        return exttype.type.symbol(relative=cpp_relative)
    ptype = field.type
    if field.q == Quantifier.Repeated:
        if ptype == Proto.Message and field.message and isinstance(field.message, Message):
            name = field.message.name
            if hasattr(field.message, "cpp"):
                name = field.message.cpp.element().symbol(relative=cpp_relative)
            return "OpProtobufMessageVector<%s>" % name

        if ptype == Proto.Bytes:
            byte_types = {"ByteBuffer": "OpAutoVector<ByteBuffer>",
                          "OpData": "OpProtobufValueVector<OpData>",
                          }
            datatype = field.options["cpp_datatype"].value
            if datatype not in byte_types:
                datatype = "ByteBuffer"
            return byte_types[datatype]

        if ptype == Proto.String:
            string_types = {"OpString": "OpAutoVector<OpString>",
                            "TempBuffer": "OpAutoVector<TempBuffer>",
                            "UniString": "OpProtobufValueVector<UniString>",
                            }
            datatype = field.options["cpp_datatype"].value
            if datatype not in string_types:
                datatype = "OpString"
            return string_types[datatype]

        ptypes = ProtoTypeMap(
                 {Proto.Int32: "OpValueVector<INT32>"
                 ,Proto.Uint32: "OpValueVector<UINT32>"
                 ,Proto.Sint32: "OpValueVector<INT32>"
                 ,Proto.Fixed32: "OpValueVector<UINT32>"
                 ,Proto.Sfixed32: "OpValueVector<INT32>"
                 ,Proto.Int64: "OpValueVector<INT64>"
                 ,Proto.Uint64: "OpValueVector<UINT64>"
                 ,Proto.Sint64: "OpValueVector<INT64>"
                 ,Proto.Fixed64: "OpValueVector<UINT64>"
                 ,Proto.Sfixed64: "OpValueVector<INT64>"
                 ,Proto.Bool: "OpINT32Vector"
                 ,Proto.Float: "OpValueVector<float>"
                 ,Proto.Double: "OpValueVector<double>"
                 })
        return ptypes[ptype]
    else:
        if ptype == Proto.Message or (field.message and allow_enum):
            if isinstance(field.message, Enum) and allow_enum:
                return field.message.cpp.cppEnum().symbol()
            path = path_name(field.message.cpp.klass())
            path = "::".join(path)
            if field.q == Quantifier.Required or isinstance(field.message, Enum):
                return path
            else:
                return path + " *"

        if ptype == Proto.Bytes:
            byte_types = {"ByteBuffer": "ByteBuffer",
                          "OpData": "OpData",
                          }
            datatype = field.options["cpp_datatype"].value
            if datatype not in byte_types:
                datatype = "ByteBuffer"
            return byte_types[datatype]

        if ptype == Proto.String:
            string_types = {"OpString": "OpString",
                            "TempBuffer": "TempBuffer",
                            "UniString": "UniString",
                            }
            datatype = field.options["cpp_datatype"].value
            if datatype not in string_types:
                datatype = "OpString"
            return string_types[datatype]

        ptypes = ProtoTypeMap(
                 {Proto.Int32: "INT32"
                 ,Proto.Uint32: "UINT32"
                 ,Proto.Sint32: "INT32"
                 ,Proto.Fixed32: "UINT32"
                 ,Proto.Sfixed32: "INT32"
                 ,Proto.Int64: "INT64"
                 ,Proto.Uint64: "UINT64"
                 ,Proto.Sint64: "INT64"
                 ,Proto.Fixed64: "UINT64"
                 ,Proto.Sfixed64: "INT64"
                 ,Proto.Bool: "BOOL"
                 ,Proto.Float: "float"
                 ,Proto.Double: "double"
                 })
        return ptypes[ptype]

def quantifierType(q):
    qs = QuantifierMap(
         {Quantifier.Required: "Required"
         ,Quantifier.Optional: "Optional"
         ,Quantifier.Repeated: "Repeated"})
    return "OpProtobufField::" + qs[q]

def protoType(field):
    ptype = field.type
    ptypes = ProtoTypeMap(
             {Proto.Int32: "Int32"
             ,Proto.Uint32: "Uint32"
             ,Proto.Sint32: "Sint32"
             ,Proto.Fixed32: "Fixed32"
             ,Proto.Sfixed32: "Sfixed32"
             ,Proto.Int64: "Int64"
             ,Proto.Uint64: "Uint64"
             ,Proto.Sint64: "Sint64"
             ,Proto.Fixed64: "Fixed64"
             ,Proto.Sfixed64: "Sfixed64"
             ,Proto.Bool: "Bool"
             ,Proto.String: "String"
             ,Proto.Bytes: "Bytes"
             ,Proto.Message: "Message"
             ,Proto.Float: "Float"
             ,Proto.Double: "Double"
             })
    return ptypes[ptype]

def field_memberExistsName(name):
    name = unicode(name)
    return u"Has" + upfirst(name)

def field_memberGetName(name):
    name = unicode(name)
    return u"Get" + upfirst(name)

def field_memberSetName(name):
    name = unicode(name)
    return u"Set" + upfirst(name)

def field_memberAppendName(field):
    name = unicode(field.name)
    if field.type == Proto.Message:
        return u"AppendNew" + upfirst(name)
    else:
        return u"AppendTo" + upfirst(name)

class MethodGenerator(object):
    """
    Generates the C++ implementation for the various methods used by message fields.
    """

    def renderMethod(self, methodName, kwargs):
        func = getattr(self, "render" + methodName)
        text = func(kwargs)
        return text

    ## == Field ==

    # index
    @template()
    def renderSetField(self, kwargs):
        return """has_bits_.Reference().SetBit(%(index)s);""" % kwargs

    # index
    @template()
    def renderIsSet(self, kwargs):
        field = kwargs["field"]
        if field.q == Quantifier.Required:
            return """return TRUE;"""
        else:
            return """return has_bits_.Reference().IsSet(%(index)s);""" % kwargs

    ## == Getters ==

    ## By-Value

    # fieldTypeOuter, memberName
    @template()
    def renderGetEnum(self, kwargs):
        return """return static_cast<%(fieldTypeOuter)s>(%(memberName)s);""" % kwargs

    # memberName
    @template()
    def renderGet(self, kwargs):
        exttype = kwargs["field"].cpp.extType()
        if exttype:
            kwargs = kwargs.copy()
            kwargs["expr"] = exttype.convToExt.statement() % kwargs
            kwargs["return"] = exttype.returnToExt.statement() % kwargs
            return """%(return)s;""" % kwargs
        else:
            return """return %(memberName)s;""" % kwargs

    ## By-Reference

    # memberName
    @template()
    def renderGetMessage(self, kwargs):
        return """return %(memberName)s;""" % kwargs

    # memberName
    @template()
    def renderGetConstRef(self, kwargs):
        return """return %(memberName)s;""" % kwargs

    ## == Setters ==

    ## Message

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderSetMessage(self, kwargs):
        return """
            OP_DELETE(%(memberName)s);
            %(memberName)s = v;
            """ % kwargs
        ## TODO: fix ret value

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderSetMessageL(self, kwargs):
        return """
            OP_DELETE(%(memberName)s);
            %(memberName)s = OP_NEW_L(%(fieldType)s, ());
            return %(memberName)s;
            """ % kwargs

    ## By-Value

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderSetEnum(self, kwargs):
        return """
            %(memberName)s = static_cast<%(fieldType)s>(v);
            """ % kwargs
        ## TODO: fix ret value

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderSetValue(self, kwargs):
        exttype = kwargs["field"].cpp.extType()
        if exttype:
            kwargs = kwargs.copy()
            kwargs["var"] = "v"
            kwargs["expr"] = exttype.convFromExt.statement() % kwargs
            kwargs["assignment"] = exttype.assignFromExt.statement() % kwargs
            return """%(assignment)s;""" % kwargs
        else:
            return """
                %(memberName)s = v;
                """ % kwargs
        ## TODO: fix ret value

    ## Reference

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderGetRef(self, kwargs):
        return """
            return %(memberName)s;
            """ % kwargs

    ## String

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderSetString(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "TempBuffer":
            return """
                %(memberName)s.Clear();
                return %(memberName)s.Append(v.GetStorage(), v.Length());
                """ % kwargs
        elif datatype == "UniString":
            return """
                %(memberName)s = v;
                return OpStatus::OK;
                """ % kwargs
        else:
            return """
                return %(memberName)s.Set(v);
                """ % kwargs

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderSetUniStringPtr(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "TempBuffer":
            return """
                %(memberName)s.Clear();
                return %(memberName)s.Append(v, l);
                """ % kwargs
        elif datatype == "UniString":
            return """
                return %(memberName)s.SetCopyData(v, l);
                """ % kwargs
        else:
            return """
                return %(memberName)s.Set(v, l);
                """ % kwargs

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderSetStringPtr(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "TempBuffer":
            return """
                %(memberName)s.Clear();
                return %(memberName)s.Append(v, l);
                """ % kwargs
        elif datatype == "UniString":
            return """
                {
                \tTempBuffer buf;
                \tRETURN_IF_ERROR(buf.Append(v, l));
                \treturn %(memberName)s.SetCopyData(buf.GetStorage(), buf.Length());
                }
                """ % kwargs
        else:
            return """
                return %(memberName)s.Set(v, l);
                """ % kwargs

    ## Bytebuffer

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderSetByteBuffer(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "OpData":
            return """
                %(memberName)s = v;
                return OpStatus::OK;
                """ % kwargs
        else:
            return """
                return OpProtobufAppend(%(memberName)s, v);
                """ % kwargs

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderSetBytePtr(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "OpData":
            return """
                return %(memberName)s.SetCopyData(v, l);
                """ % kwargs
        else:
            return """
                %(memberName)s.Clear();
                return %(memberName)s.AppendBytes(v, l);
                """ % kwargs

    ## == Repeated fields ==

    ## String entries

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendString(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "TempBuffer":
            return """
                return OpProtobufUtils::AppendTempBuffer(%(memberName)s, v);
                """ % kwargs
        elif datatype == "UniString":
            return """
                return OpProtobufUtils::AppendUniString(%(memberName)s, v);
                """ % kwargs
        else:
            return """
                return OpProtobufUtils::AppendString(%(memberName)s, v);
                """ % kwargs

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendUniStringPtr(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "TempBuffer":
            return """
                return OpProtobufUtils::AppendTempBuffer(%(memberName)s, v, l);
                """ % kwargs
        elif datatype == "UniString":
            return """
                return OpProtobufUtils::AppendUniString(%(memberName)s, v, l);
                """ % kwargs
        else:
            return """
                return OpProtobufUtils::AppendString(%(memberName)s, v, l);
                """ % kwargs

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendStringPtr(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "TempBuffer":
            return """
                return OpProtobufUtils::AppendTempBuffer(%(memberName)s, v, l);
                """ % kwargs
        elif datatype == "UniString":
            return """
                return OpProtobufUtils::AppendUniString(%(memberName)s, v, l);
                """ % kwargs
        else:
            return """
                return OpProtobufUtils::AppendString(%(memberName)s, v, l);
                """ % kwargs

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendNewString(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "UniString":
            raise ProgramError("UniString not supported for AppendNew")
        else:
            return """
                OpAutoPtr<%(fieldType)s> tmp(OP_NEW(%(fieldType)s, ()));
                if (!tmp.get())
                    return NULL;
                RETURN_VALUE_IF_ERROR(%(memberName)s.Add(tmp.get()), NULL);
                return tmp.release();
                """ % kwargs

    ## ByteBuffer entries

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendByteBuffer(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "OpData":
            return """
                return %(memberName)s.Add(v);
                """ % kwargs
        else:
            return """
                return OpProtobufUtils::AppendBytes(%(memberName)s, v);
                """ % kwargs

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendBytePtr(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "OpData":
            return """
                OpData tmp;
                RETURN_IF_ERROR(tmp.SetCopyData(v, l));
                return %(memberName)s.Add(tmp);
                """ % kwargs
        else:
            return """
                return OpProtobufUtils::AppendBytes(%(memberName)s, v, l);
                """ % kwargs

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendNewByteBuffer(self, kwargs):
        datatype = kwargs["field"].options["cpp_datatype"].value
        if datatype == "OpData":
            raise ProgramError("OpData not supported for AppendNew")
        else:
            return """
                OpAutoPtr<%(fieldType)s> tmp(OP_NEW(%(fieldType)s, ()));
                if (!tmp.get())
                    return NULL;
                RETURN_VALUE_IF_ERROR(%(memberName)s.Add(tmp.get()), NULL);
                return tmp.release();
                """ % kwargs

    ## Message entries

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendNewMessage(self, kwargs):
        return """
            OpAutoPtr<%(fieldType)s> tmp(OP_NEW(%(fieldType)s, ()));
            if (!tmp.get())
                return NULL;
            RETURN_VALUE_IF_ERROR(%(memberName)s.Add(tmp.get()), NULL);
            return tmp.release();
            """ % kwargs

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendNewMessageL(self, kwargs):
        return """
            OpAutoPtr<%(fieldType)s> tmp(OP_NEW_L(%(fieldType)s, ()));
            %(memberName)s.AddL(tmp.get());
            return tmp.release();
            """ % kwargs
        ## TODO: Should raise error

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderAppendToMessage(self, kwargs):
        return """
            if (!v) return OpStatus::ERR_NULL_POINTER;
            return %(memberName)s.Add(v);
            """ % kwargs

    ## Enum entries

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderAppendEnum(self, kwargs):
        return """
            return %(memberName)s.Add(static_cast<%(fieldType)s>(v));
            """ % kwargs

    # fieldSet, fieldType, memberName
    @template(is_modifier=True)
    def renderSetRepeatedEnumItem(self, kwargs):
        return """
            OP_ASSERT(i < %(memberName)s.GetCount());
            return %(memberName)s.Replace(i, static_cast<%(fieldType)s>(v));
            """ % kwargs

    # fieldTypeOuter, memberName
    @template()
    def renderGetRepeatedEnumCount(self, kwargs):
        return """
            return %(memberName)s.GetCount();
            """ % kwargs

    # fieldTypeOuter, memberName
    @template()
    def renderGetRepeatedEnumItem(self, kwargs):
        return """
            OP_ASSERT(i < %(memberName)s.GetCount());
            return static_cast<%(fieldType)s>(%(memberName)s.Get(i));
            """ % kwargs

    ## General append-by-ref

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderAppendFieldRef(self, kwargs):
        return """
            return %(memberName)s.Add(v);
            """ % kwargs

    ## General append-by-value

    # fieldSet, memberName
    @template(is_modifier=True)
    def renderAppendFieldValue(self, kwargs):
        return """
            %(memberName)s = v;
            """ % kwargs
        ## TODO: fix ret value

class CppMessageGenerator(TemplatedGenerator):
    message = None

    # Generated text
#    decl = None
#    impl = None

    def __init__(self, message, class_path, existing=None):
        super(CppMessageGenerator, self).__init__()

        self.message = message
        self.package = messageRoot(message)
        self.class_path = class_path
        self.isMembersInitialized = False
        if existing == None:
            existing = {}
        self.existing = existing
        self.existing[message] = self
        self.use_separate_inlines = False
        self.methodGenerator = MethodGenerator()
        self.klass = message.cpp.klass()
        self.generate(self.message)
        self.type_decl_includes = set()

    def renderMethod(self, methodName, args):
        return self.methodGenerator.renderMethod(methodName, args)

    def varName(self, name):
        return _var_name(name)

    def varPath(self, path):
        return _var_path(path)

    def messageClassPath(self, message):
        return messageClassPath(message, self.class_path)

    def generate(self, message):
        root = message
        while isinstance(root.iparent, Message):
            root = root.iparent
        self.cpp_class_name, self.cpp_class_path = messageClassPath(message, self.class_path)
        self.decl_includes = ["modules/protobuf/src/protobuf_utils.h",
                              "modules/protobuf/src/protobuf_message.h",
                              ]
        self.decl_forward_decls = ["OpScopeTPMessage",
                                   "OpProtobufField",
                                   "OpProtobufMessage",
                                   ]

    def initMembers(self):
        if self.isMembersInitialized:
            return

        message = self.message
        members = []
        offsets = []
        protofields = []
        exists  = []
        getters = []
        setters = []
        redirect_members = []
        self.type_decl_includes = type_decl_includes = set()

        constructor = self.constructor = [] # Primary constructor, contains Argument objects
        constructor_def_init = self.constructor_def_init = [] # Primary constructor, contains Initializer objects but only for initializing the default value
        constructor_init = self.constructor_init = [] # Primary constructor, contains Initializer objects

        construct = self.construct = [] # Secondary constructor ie. OP_STATUS Construct(...), contains Argument objects
        construct_init = self.construct_init = [] # Secondary constructor ie. OP_STATUS Construct(...), contains code elements

        messagefields = []
        classargs = self.classargs = []
        typeinit = ProtoTypeMap(
                   {Proto.Int32: 0, Proto.Uint32: 0, Proto.Sint32: 0, Proto.Fixed32: 0, Proto.Sfixed32: 0
                   ,Proto.Int64: 0, Proto.Uint64: 0, Proto.Sint64: 0, Proto.Fixed64: 0, Proto.Sfixed64: 0
                   ,Proto.Bool:  'FALSE'})
        for key, value in typeinit.iteritems():
            typeinit[key] = toIntegral(value)
        cpp_config = currentCppConfig()

        for idx, field in enumerate(message.fields):
            if "cpp_method" in field.options:
                if field.options["cpp_method"].value == "*":
                    cpp_method = set(["GetRef",
                                      "SetMessage", "SetMessageL", # Message helper methods
                                      "SetString", "SetUniStringPtr", "SetStringPtr", # String helper methods
                                      "SetByteBuffer", "SetBytePtr", # Bytes helper methods

                                      # Helper methods for repeated fields
                                      "AppendTo", "AppendNew", "AppendNewL", # Message helper methods
                                      "AppendString", "AppendUniStringPtr", "AppendStringPtr", # String helper methods
                                      "AppendByteBuffer", "AppendBytePtr", # Bytes helper methods
                                     ])
                else:
                    cpp_method = set([n.strip() for n in field.options["cpp_method"].value.split(",")])
            else:
                cpp_method = set([n.strip() for n in getDefaultOptions(field)["cpp_method"].split(",")])

            sub_message_class_path = ""
            if field.message:
                if isinstance(field.message.iparent, Message):
                    sub_message_class_path = messageClassPath(field.message.iparent, self.class_path)[1]
                else:
                    sub_message_class_path = self.class_path
                sub_message_class_path += "::"
            memberName = "_" + field.name
            members += [(fieldType(field, allow_enum=False), memberName)]
            offsets += [(path_string(self.klass), memberName)]

            methodArgs = {"fieldType":  fieldType(field),
                          "memberName": memberName,
                          "index":      idx,
                          "field":      field,
                         }
            # Most of the time the outer type is the same as the inner
            methodArgs["fieldTypeOuter"] = methodArgs["fieldType"]
            if isinstance(field.message, Enum):
                methodArgs["fieldTypeOuter"] = fieldType(field, relative=message, filter=Message)
            methodArgs["dataType"] = methodArgs["fieldType"]
            exttype = field.cpp.extType()
            if exttype:
                methodArgs["fieldType"] = exttype.type.symbol(relative=self.klass)
                methodArgs["fieldTypeOuter"] = exttype.type.symbol()
            fieldset = ""
            if field.q != Quantifier.Required:
                fieldset = self.renderMethod("SetField", methodArgs) + "\n"
            methodArgs["fieldSet"] = fieldset
            if field.q == Quantifier.Repeated:
                field_single = copy(field)
                field_single.q = Quantifier.Required
                methodArgs.update({"fieldType": fieldType(field_single),
                                  })
                # Most of the time the outer type is the same as the inner
                methodArgs["fieldTypeOuter"] = methodArgs["fieldType"]
            elif field.type == Proto.Message and field.q == Quantifier.Optional:
                # turns field into a required field to get rid of pointer in type
                field_single = copy(field)
                field_single.q = Quantifier.Required
                methodArgs.update({"fieldType": fieldType(field_single),
                                  })
                methodArgs["fieldTypeOuter"] = methodArgs["fieldType"]

            # Figure out necessary includes
            cpp_types = {"UniString": {"includes": ["modules/opdata/UniString.h"],
                                       },
                         "OpString": {"includes": ["modules/util/opstring.h"],
                                      },
                         "TempBuffer": {"includes": ["modules/util/tempbuf.h"],
                                      },
                         "OpData": {"includes": ["modules/opdata/OpData.h"],
                                    },
                         "ByteBuffer": {"includes": ["modules/util/adt/bytebuffer.h"],
                                        },
                         "OpProtobufValueVector": {"includes": ["modules/protobuf/src/protobuf_vector.h"],
                                                   },
                         "OpAutoVector": {"includes": ["modules/util/adt/opvector.h"],
                                          },
                         "OpValueVector": {"includes": ["modules/protobuf/src/opvaluevector.h"],
                                           },
                         "OpINT32Vector": {"includes": ["modules/util/adt/opvector.h"],
                                           },
                         "OpProtobufMessageVector": {"includes": ["modules/protobuf/src/protobuf_message.h"],
                                                     },
                         }
            cpp_type = field.options["cpp_datatype"].value
            cpp_used_types = set()
            if field.q == Quantifier.Required:
                if field.type == Proto.String:
                    if not cpp_type:
                        cpp_type = "OpString"
                elif field.type == Proto.Bytes:
                    if not cpp_type:
                        cpp_type = "ByteBuffer"

                cpp_used_types.add(cpp_type)
            elif field.q == Quantifier.Optional:
                if field.type == Proto.String:
                    if not cpp_type:
                        cpp_type = "OpString"
                elif field.type == Proto.Bytes:
                    if not cpp_type:
                        cpp_type = "ByteBuffer"

                cpp_used_types.add(cpp_type)
            elif field.q == Quantifier.Repeated:
                if field.type == Proto.String:
                    if not cpp_type:
                        cpp_type = "OpString"
                    if cpp_type == "UniString":
                        cpp_used_types.add("OpProtobufValueVector")
                    else:
                        cpp_used_types.add("OpAutoVector")
                elif field.type == Proto.Bytes:
                    if not cpp_type:
                        cpp_type = "ByteBuffer"
                    if cpp_type == "OpData":
                        cpp_used_types.add("OpProtobufValueVector")
                    else:
                        cpp_used_types.add("OpAutoVector")
                elif field.type == Proto.Message and isinstance(field.message, Message):
                    cpp_type = "OpProtobufMessageVector"
                elif field.type == Proto.Bool:
                    cpp_type = "OpINT32Vector"
                else:
                    cpp_type = "OpValueVector"

                cpp_used_types.add(cpp_type)

            # Add includes needed by exttypes
            if field.cpp and field.cpp.extType():
                exttype = field.cpp.extType()
                if exttype.includes:
                    type_decl_includes |= set(exttype.includes)

            for cpp_type in cpp_used_types:
                if cpp_type in cpp_types:
                    type_decl_includes |= set(cpp_types[cpp_type]["includes"])

            doc = field.doc
            exist_doc = DocBlock(render("""
                Check if the field @c %(name)s is set.
                @return TRUE if the field has been set, @c FALSE otherwise.
                """, {
                    "name": field.name,
                }))
            get_doc = DocBlock(render("""
                Get the field @c %(name)s.
                %(doc)s
                """, {
                    "name": field.name,
                    "doc": doc and ("\n" + doc.text) or "",
                }))
            set_doc = DocBlock(render("""
                Set the field @c %(name)s.
                %(doc)s
                """, {
                    "name": field.name,
                    "doc": doc and ("\n" + doc.text) or "",
                }))
            class RedirectMethod(Method):
                def __init__(self, method, parameter_names=None):
                    super(RedirectMethod, self).__init__(method.name, method.rtype, rtype_outer=method.rtype_outer, parameters=method.parameters, keywords=method.keywords, proto=method.proto, doc=method.doc, is_inline=method.is_inline)
                    args = ""
                    if parameter_names:
                        args = ", ".join(parameter_names)
                    if self.rtype and self.rtype != "void":
                        self.implementation = "return protobuf_data.%(method)s(%(args)s);" % {
                            "method": self.name,
                            "args": args,
                            }
                    else:
                        self.implementation = "protobuf_data.%(method)s(%(args)s);" % {
                            "method": self.name,
                            "args": args,
                            }

            exists  += [Method(field_memberExistsName(field.name), "BOOL", keywords=["const", "inline"],
                               doc=exist_doc,
                               is_inline=True,
                               implementation=self.renderMethod("IsSet", methodArgs))]
            redirect_members += [RedirectMethod(exists[-1])]
            pass_by = passBy(field)
            if pass_by == BY_VALUE:
                if isinstance(field.message, Enum):
                    getters += [Method(field_memberGetName(field.name), fieldType(field, relative=message, filter=Message), rtype_outer=fieldType(field, filter=Message, prefix=sub_message_class_path), keywords=["const", "inline"],
                                       doc=get_doc,
                                       is_inline=True,
                                       implementation=self.renderMethod("GetEnum", methodArgs))]
                else:
                    getters += [Method(field_memberGetName(field.name), fieldType(field, exttype=exttype), rtype_outer=fieldType(field, filter=True, prefix=sub_message_class_path, exttype=exttype), keywords=["const", "inline"],
                                       doc=get_doc,
                                       is_inline=True,
                                       implementation=self.renderMethod("Get", methodArgs))]
                redirect_members += [RedirectMethod(getters[-1])]
            elif pass_by == BY_REF:
                if field.type == Proto.Message and field.q == Quantifier.Optional:
                    getters += [Method(field_memberGetName(field.name), "const %s" % fieldType(field), rtype_outer="const %s" % fieldType(field, filter=True, prefix=sub_message_class_path), keywords=["const", "inline"],
                                       doc=get_doc,
                                       is_inline=True,
                                       implementation=self.renderMethod("GetMessage", methodArgs))]
                else:
                    getters += [Method(field_memberGetName(field.name), "const %s &" % fieldType(field), rtype_outer="const %s &" % fieldType(field, filter=True, prefix=sub_message_class_path), keywords=["const", "inline"],
                                       doc=get_doc,
                                       is_inline=True,
                                       implementation=self.renderMethod("GetConstRef", methodArgs))]
                redirect_members += [RedirectMethod(getters[-1])]


            datatype = field.options["cpp_datatype"].value
            if field.q == Quantifier.Repeated:
                field_single = copy(field)
                field_single.q = Quantifier.Required
                if isinstance(field.message, Enum):
                    # unsigned Get%sCount() e.g. unsigned GetTypeListCount()
                    getters += [Method(field_memberGetName(field.name) + "Count", "UINT32", keywords=["const", "inline"],
                                       doc=get_doc,
                                       is_inline=True,
                                       implementation=self.renderMethod("GetRepeatedEnumCount", methodArgs))]
                    redirect_members += [RedirectMethod(getters[-1])]

                    getters += [Method(field_memberGetName(field.name) + "Item", fieldType(field_single), keywords=["const", "inline"],
                                       parameters=["UINT32 i"],
                                       doc=get_doc,
                                       is_inline=True,
                                       implementation=self.renderMethod("GetRepeatedEnumItem", methodArgs))]
                    redirect_members += [RedirectMethod(getters[-1], ["i"])]

            if pass_by == BY_VALUE:
                if field.type == Proto.Message and field.q == Quantifier.Optional:
                    if "SetMessage" in cpp_method:
                        setters += [Method(field_memberSetName(field.name), "void", keywords=[],
                                           parameters=[fieldType(field) + " v"],
                                           doc=set_doc,
                                           implementation=self.renderMethod("SetMessage", methodArgs))]
                        redirect_members += [RedirectMethod(setters[-1], ["v"])]
                    if "SetMessageL" in cpp_method:
                        setters += [Method("New" + upfirst(field.name) + "_L", fieldType(field), rtype_outer=fieldType(field, prefix=sub_message_class_path), keywords=[],
                                           doc=set_doc,
                                           implementation=self.renderMethod("SetMessageL", methodArgs))]
                        redirect_members += [RedirectMethod(setters[-1])]
                else:
                    if isinstance(field.message, Enum):
                        methodArgs.update({"fieldType": fieldType(field, relative=message, filter=Message),})
                        methodArgs["fieldTypeOuter"] = methodArgs["fieldType"]
                        setters += [Method(field_memberSetName(field.name), "void", keywords=[],
                                           parameters=[fieldType(field, relative=message, filter=Message) + " v"],
                                           doc=set_doc,
                                           implementation=self.renderMethod("SetEnum", methodArgs))]
                        redirect_members += [RedirectMethod(setters[-1], ["v"])]
                    else:
                        setters += [Method(field_memberSetName(field.name), "void", keywords=[],
                                           parameters=[fieldType(field, exttype=exttype) + " v"],
                                           doc=set_doc,
                                           is_inline=True,
                                           implementation=self.renderMethod("SetValue", methodArgs))]
                        redirect_members += [RedirectMethod(setters[-1], ["v"])]
            elif pass_by == BY_REF:
                if "GetRef" in cpp_method:
                    setters += [Method(field_memberGetName(field.name) + "Ref", fieldType(field) + " &", rtype_outer=fieldType(field, prefix=sub_message_class_path) + " &", keywords=[],
                                       doc=set_doc,
                                       implementation=self.renderMethod("GetRef", methodArgs))]
                    redirect_members += [RedirectMethod(setters[-1])]

                # Add some extra setters for strings/bytes
                if field.q != Quantifier.Repeated:
                    if field.type == Proto.String:
                        if "SetString" in cpp_method:
                            setters += [Method(field_memberSetName(field.name), "OP_STATUS", keywords=[],
                                               parameters=["const %s & v" % fieldType(field)],
                                               doc=set_doc,
                                               implementation=self.renderMethod("SetString", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v"])]
                        if "SetUniStringPtr" in cpp_method:
                            setters += [Method(field_memberSetName(field.name), "OP_STATUS", keywords=[],
                                               parameters=["const uni_char * v", Parameter("int l", "KAll")],
                                               doc=set_doc,
                                               implementation=self.renderMethod("SetUniStringPtr", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v", "l"])]
                        if "SetStringPtr" in cpp_method:
                            setters += [Method(field_memberSetName(field.name), "OP_STATUS", keywords=[],
                                               parameters=["const char * v", Parameter("int l", "KAll")],
                                               doc=set_doc,
                                               implementation=self.renderMethod("SetStringPtr", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v", "l"])]
                    elif field.type == Proto.Bytes:
                        if "SetByteBuffer" in cpp_method:
                            setters += [Method(field_memberSetName(field.name), "OP_STATUS", keywords=[],
                                        parameters=["const %s & v" % fieldType(field)],
                                        doc=set_doc,
                                        implementation=self.renderMethod("SetByteBuffer", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v"])]
                        if "SetBytePtr" in cpp_method:
                            setters += [Method(field_memberSetName(field.name), "OP_STATUS", keywords=[],
                                        parameters=["const char * v", "int l"],
                                        doc=set_doc,
                                        implementation=self.renderMethod("SetBytePtr", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v", "l"])]

            if field.q == Quantifier.Repeated:
                if pass_by == BY_VALUE:
                    setters += [Method(field_memberAppendName(field), "OP_STATUS", keywords=[],
                                       parameters=[fieldType(field_single) + " v"],
                                       doc=set_doc,
                                       implementation=self.renderMethod("AppendFieldValue", methodArgs))]
                    redirect_members += [RedirectMethod(setters[-1], ["v"])]
                elif pass_by == BY_REF:
                    if field.type == Proto.String:
                        if "AppendString" in cpp_method:
                            setters += [Method(field_memberAppendName(field), "OP_STATUS", keywords=[],
                                               parameters=["const %s &v" % fieldType(field_single)],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendString", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v"])]
                        if "AppendUniStringPtr" in cpp_method:
                            setters += [Method(field_memberAppendName(field), "OP_STATUS", keywords=[],
                                               parameters=["const uni_char * v", Parameter("int l", "KAll")],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendUniStringPtr", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v", "l"])]
                        if "AppendStringPtr" in cpp_method:
                            setters += [Method(field_memberAppendName(field), "OP_STATUS", keywords=[],
                                               parameters=["const char * v", Parameter("int l", "KAll")],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendStringPtr", methodArgs))]
                        if "AppendNew" in cpp_method:
                            if datatype in ("OpString", "TempBuffer", ""):
                                setters += [Method("AppendNew" + upfirst(field.name), fieldType(field_single) + " *", rtype_outer=fieldType(field_single) + " *", keywords=[],
                                                   doc=set_doc,
                                                   implementation=self.renderMethod("AppendNewString", methodArgs))]
                                redirect_members += [RedirectMethod(setters[-1])]
                    elif field.type == Proto.Bytes:
                        if "AppendByteBuffer" in cpp_method:
                            setters += [Method(field_memberAppendName(field), "OP_STATUS", keywords=[],
                                               parameters=["const %s &v" % fieldType(field_single)],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendByteBuffer", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v"])]
                        if  "AppendBytePtr" in cpp_method:
                            setters += [Method(field_memberAppendName(field), "OP_STATUS", keywords=[],
                                               parameters=["const char * v", "int l"],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendBytePtr", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v", "l"])]
                        if "AppendNew" in cpp_method:
                            if datatype in ("ByteBuffer", ""):
                                setters += [Method("AppendNew" + upfirst(field.name), fieldType(field_single) + " *", rtype_outer=fieldType(field_single) + " *", keywords=[],
                                                   doc=set_doc,
                                                   implementation=self.renderMethod("AppendNewByteBuffer", methodArgs))]
                                redirect_members += [RedirectMethod(setters[-1])]
                    elif field.type == Proto.Message and isinstance(field.message, Message):
                        if "AppendNew" in cpp_method:
                            setters += [Method("AppendNew" + upfirst(field.name), fieldType(field_single) + " *", rtype_outer=fieldType(field_single, prefix=sub_message_class_path) + " *", keywords=[],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendNewMessage", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1])]
                        if "AppendNewL" in cpp_method:
                            setters += [Method("AppendNew" + upfirst(field.name) + "_L", fieldType(field_single) + " *", rtype_outer=fieldType(field_single, prefix=sub_message_class_path) + " *", keywords=[],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendNewMessageL", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1])]
                        if "AppendTo" in cpp_method:
                            setters += [Method("AppendTo" + upfirst(field.name), "OP_STATUS", keywords=[], parameters=["%s * v" % fieldType(field_single)],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendToMessage", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v"])]
                    else:
                        if isinstance(field.message, Enum):
                            # Enums are stored as INT32 internally
                            methodArgs["fieldType"] = "INT32"
                            setters += [Method(field_memberAppendName(field), "OP_STATUS", keywords=[],
                                               parameters=["%s v" % fieldType(field_single)],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendEnum", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v"])]

                            # Enums are stored as INT32 internally
                            setters += [Method(field_memberSetName(field.name) + "Item", "OP_STATUS", keywords=[],
                                               parameters=["UINT32 i", "%s v" % fieldType(field_single)],
                                               doc=set_doc,
                                               implementation=self.renderMethod("SetRepeatedEnumItem", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["i", "v"])]

                        else:
                            setters += [Method(field_memberAppendName(field), "OP_STATUS", keywords=[],
                                               parameters=["%s v" % fieldType(field_single)],
                                               doc=set_doc,
                                               implementation=self.renderMethod("AppendFieldRef", methodArgs))]
                            redirect_members += [RedirectMethod(setters[-1], ["v"])]
            if field.type == Proto.Message and isinstance(field.message, Message):
                messagefields += [(str(idx), field.message.cpp.klass().symbol(relative=message.cpp.klass()))]

            protofields += [(protoType(field), str(field.number), strQuote(field.name), quantifierType(field.q), field.message, field)]
            # TODO: What should happen to enums without default value?
            argname = utils.join_underscore(utils.split_camelcase(field.name)) + "_arg"

            # Figure arguments to constructor and Construct()
            if isinstance(field.message, Enum):
                argdef = None
                if field.default is not None:
                    for enumval in field.message.values:
                        if enumval.value == field.default:
                            argdef = enumval.cpp.cppEnumValue()
                            break
                if argdef is None:
                    if field.message.values:
                        argdef = field.message.values[0].cpp.cppEnumValue()
                arg = Argument(argname, field.message.cpp.cppEnum(), default=argdef)
                if field.q == Quantifier.Optional:
                    constructor_init.append(Initializer(memberName, argdef and argdef.ref()))
                    if argdef:
                        if not isinstance(argdef, CppElement):
                            definit = toIntegral(argdef)
                        else:
                            definit = argdef.ref()
                        constructor_def_init.append(Initializer(memberName, definit))
                elif field.q == Quantifier.Required:
                    constructor.append((arg, idx))
                    constructor_init.append(Initializer(memberName, arg.ref()))
                    if argdef:
                        if not isinstance(argdef, CppElement):
                            definit = toIntegral(argdef)
                        else:
                            definit = argdef.ref()
                        constructor_def_init.append(Initializer(memberName, definit))
            elif field.type in typeinit:
                if field.q == Quantifier.Required:
                    if field.default is None:
                        argdef = typeinit[field.type]
                    else:
                        argdef = initializerType(field.default)
                    arg = Argument(argname, fieldType(field, exttype=exttype), default=argdef)
                    value = arg.ref()
                    if exttype:
                        args = methodArgs.copy()
                        args["var"] = value.declaration()
                        value = exttype.convFromExt % args
                        arg.default = None
                        if argdef:
                            constructor_def_init.append(Initializer(memberName, toIntegral(argdef)))
                        constructor_init.append(Initializer(memberName, value))
                    else:
                        constructor_def_init.append(Initializer(memberName, arg.ref()))
                        constructor_init.append(Initializer(memberName, value))
                    constructor.append((arg, idx))
                elif field.q == Quantifier.Optional:
                    if field.default is None:
                        argdef = typeinit[field.type]
                    else:
                        argdef = initializerType(field.default)
                    value = toIntegral(argdef)
                    constructor_init.append(Initializer(memberName, value))
                    if argdef:
                        constructor_def_init.append(Initializer(memberName, toIntegral(argdef)))
            elif field.type in (Proto.Float, Proto.Double):
                if field.default is None:
                    argdef = "0.0"
                else:
                    argdef = initializerType(field.default)
                suffix = {Proto.Float: "f", Proto.Double: ""}[field.type]
                argdef += suffix
                if field.q == Quantifier.Required:
                    arg = Argument(argname, fieldType(field, exttype=exttype), default=argdef)
                    constructor.append((arg, idx))
                    constructor_init.append(Initializer(memberName, arg.ref()))
                    if argdef:
                        constructor_def_init.append(Initializer(memberName, toIntegral(argdef)))
                elif field.q == Quantifier.Optional:
                    constructor_init.append(Initializer(memberName, toIntegral(argdef)))
                    if argdef:
                        constructor_def_init.append(Initializer(memberName, toIntegral(argdef)))
            elif field.type == Proto.String:
                if field.q == Quantifier.Required:
                    if field.options["cpp_datatype"].value == "UniString":
                        if pass_by == BY_VALUE:
                            arg = Argument(argname, fieldType(field), default="UniString()")
                        elif pass_by == BY_REF:
                            arg = Argument(argname, "const %s &" % fieldType(field), default=CustomValue("UniString()"))
                        constructor.append((arg, idx))
                        constructor_init.append(Initializer(memberName, arg.ref()))
                    elif field.options["cpp_datatype"].value in ("TempBuffer", "OpString", ""):
                        if pass_by == BY_VALUE:
                            arg = Argument(argname, fieldType(field))
                        elif pass_by == BY_REF:
                            arg = Argument(argname, "const %s &" % fieldType(field))
                        construct.append((arg, idx))
                        if field.options["cpp_datatype"].value == "TempBuffer":
                            construct_init.append("%(memberName)s.Clear();\nRETURN_IF_ERROR(%(memberName)s.Append(%(arg)s.GetStorage(), %(arg)s.Length()));" % {"memberName": memberName, "arg": arg.name,})
                        else:
                            construct_init.append("RETURN_IF_ERROR(%s.Set(%s));" % (memberName, arg.name))
                elif field.q == Quantifier.Optional:
                    if field.options["cpp_datatype"].value in ("TempBuffer", "OpString", "UniString", ""):
                        arg = Argument(argname, "const %s *" % fieldType(field), default=NullValue())
                        if field.options["cpp_datatype"].value == "UniString":
                            constructor.append((arg, idx))
                        else:
                            construct.append((arg, idx))
                        if field.options["cpp_datatype"].value == "TempBuffer":
                            construct_init.append(render("""
                                if (%(arg)s)
                                {
                                \t%(memberName)s.Clear();
                                \tRETURN_IF_ERROR(%(memberName)s.Append(%(arg)s->GetStorage(), %(arg)s->Length()));
                                \t%(fieldset)s
                                }
                                """, {
                                    "fieldset": fieldset,
                                    "memberName": memberName,
                                    "arg": arg.name,
                                }))
                        elif field.options["cpp_datatype"].value == "UniString":
                            constructor_init.append(render("""
                                if (%(arg)s)
                                {
                                \t%(memberName)s = *%(arg)s;
                                \t%(fieldset)s
                                }
                                """, {
                                    "fieldset": fieldset,
                                    "memberName": memberName,
                                    "arg": arg.name,
                                }))
                        else:
                            construct_init.append(render("""
                                if (%(arg)s)
                                {
                                \tRETURN_IF_ERROR(%(memberName)s.Set(*%(arg)s));
                                \t%(fieldset)s
                                }
                                """, {
                                    "fieldset": fieldset,
                                    "memberName": memberName,
                                    "arg": arg.name,
                                }))
            elif field.type == Proto.Bytes:
                if field.q == Quantifier.Required:
                    if field.options["cpp_datatype"].value in ("ByteBuffer", ""):
                        if pass_by == BY_VALUE:
                            arg = Argument(argname, fieldType(field))
                        elif pass_by == BY_REF:
                            arg = Argument(argname, "const %s &" % fieldType(field))
                        construct.append((arg, idx))
                        construct_init.append("RETURN_IF_ERROR(OpProtobufAppend(%s, %s));" % (memberName, arg.name))
                    elif field.options["cpp_datatype"].value == "OpData":
                        if pass_by == BY_VALUE:
                            arg = Argument(argname, fieldType(field), default="OpData()")
                        elif pass_by == BY_REF:
                            arg = Argument(argname, "const %s &" % fieldType(field), default=CustomValue("OpData()"))
                        constructor.append((arg, idx))
                        constructor_init.append(Initializer(memberName, arg.ref()))
                elif field.q == Quantifier.Optional:
                    arg = Argument(argname, "const %s *" % fieldType(field), default=NullValue())
                    if field.options["cpp_datatype"].value in ("ByteBuffer", ""):
                        construct.append((arg, idx))
                        construct_init.append(render("""
                            if (%(arg)s)
                            {
                            \tRETURN_IF_ERROR(OpProtobufAppend(%(memberName)s, *%(arg)s));
                            \t%(fieldset)s
                            }
                            """, {
                                "fieldset": fieldset,
                                "memberName": memberName,
                                "arg": arg.name,
                            }))
                    elif field.options["cpp_datatype"].value == "OpData":
                        constructor.append((arg, idx))
                        constructor_init.append(render("""
                            if (%(arg)s)
                            {
                            \t%(memberName)s = *%(arg)s;
                            \t%(fieldset)s
                            }
                            """, {
                                "fieldset": fieldset,
                                "memberName": memberName,
                                "arg": arg.name,
                            }))
            elif field.type == Proto.Message and isinstance(field.message, Message):
                if field.q == Quantifier.Required:
                    if pass_by == BY_VALUE:
                        arg = Argument(argname, fieldType(field))
                        constructor.append((arg, idx))
                        constructor_init.append(Initializer(memberName, arg.ref()))
                    elif pass_by == BY_REF:
                        arg = Argument(argname, "const %s &" % fieldType(field))
                elif field.q == Quantifier.Optional:
                    if pass_by == BY_VALUE:
                        constructor_init.append(Initializer(memberName, NullValue()))
                        constructor_def_init.append(Initializer(memberName, NullValue()))

        constructor_init.append(Initializer("encoded_size_", toIntegral(-1)))
        constructor_def_init.append(Initializer("encoded_size_", toIntegral(-1)))

        # Ensure args without default values are placed first
        constructor.sort(key=lambda i: i[0].default != None)
        construct.sort(key=lambda i: i[0].default != None)

        import hob.proto
        self.exists = exists
        self.getters = getters
        self.setters = setters
        self.members = members
        self.redirect_members = redirect_members
        self.protofields = protofields
        self.offsets = offsets
        self.messagefields = messagefields
        self.isMembersInitialized = True

    @memoize
    def subGenerators(self):
        generators = []
        for child in self.message.children:
            if child not in self.existing:
                subGenerator = generatorManager.getMessage(child)
                if not subGenerator:
                    subGenerator = generatorManager.setMessage(child, CppMessageGenerator(child, self.class_path, existing=self.existing))
                generators.append(subGenerator)
        return generators

    def hasSubGenerators(self):
        for child in self.message.children:
            if child not in self.existing:
                return True
        return False

    @memoize
    def getDeclaration(self):
        self.initMembers()
        return self.renderDeclaration(self.message, self.exists, self.getters, self.setters, self.members)

    @memoize
    def getIncludes(self):
        return self.renderDeclarations(self.decl_includes, self.decl_forward_decls)

    def renderDeclarations(self, includes, forwardDeclarations):
        text = "\n"
        for include in includes:
            text += "#include %s\n" % strQuote(include)
        text += "\n"
        for decl in forwardDeclarations:
            text += "class %s;\n" % decl
        return text

    @memoize
    def getInlines(self):
        self.initMembers()
        return self.renderInlines(self.exists, self.getters, self.setters)

    def renderParameters(self, params, with_default=False):
        if with_default:
            text = ", ".join([param.type + (param.default and " = " + param.default or "") for param in params])
        else:
            text = ", ".join([param.type for param in params])
        return text

    def renderFunctionImplementation(self, func):
        text = ""
        if func.implementation:
            if func.keywords - set(["inline", "const"]):
                text += "/* " + " ".join(func.keywords - set(["inline", "const"])) + " */"
            text += render("""
            %(keywords_outer)s
            %(rtype_outer)s
            %(class)s::%(method)s(%(params)s) %(keywords)s
            {
            %(implementation)s
            }
            """, {
                "keywords_outer": " ".join(func.keywords & set(["inline"])),
                "rtype_outer": func.rtype_outer,
                "class": self.cpp_class_path,
                "method": func.name,
                "params": self.renderParameters(func.parameters),
                "keywords": " ".join(func.keywords & set(["const"])),
                "implementation": self.indent(func.implementation),
            })
        return text

    def renderFunctionDefinition(self, func, use_rtype_outer=False):
        rtype = func.rtype
        if use_rtype_outer:
            rtype = func.rtype_outer
        doc = ""
        if func.doc:
            doc = func.doc.toComment() + "\n"
        text = render("%(doc)s%(keywords_outer)s%(rtype_outer)s %(method)s(%(params)s)%(keywords)s",
                      {
                        "doc": doc,
                        "keywords_outer": "".join([k + " " for k in func.keywords - set(["const", "inline"])]),
                        "rtype_outer": rtype,
                        "method": func.name,
                        "params": self.renderParameters(func.parameters, with_default=True),
                        "keywords": "".join([" " + k for k in func.keywords & set(["const"])]),
                      })
        if func.implementation and func.is_inline and not self.use_separate_inlines:
            text += "\n" + render("""
                {
                %(implementation)s
                }
                """, {
                    "implementation": self.indent(func.implementation),
                })
        else:
            text += ";"
        return text

    def renderInlines(self, exists, getters, setters):
        text = ""
        if self.use_separate_inlines:
            text += "// Inlines for %s\n" % self.cpp_class_path
            for func in exists:
                text += self.renderFunctionImplementation(func) + "\n"

            for func in getters:
                text += self.renderFunctionImplementation(func) + "\n"

            for func in setters:
                text += self.renderFunctionImplementation(func) + "\n"
        return text

    def getTypeDef(self):
        typedef  = self.message.cpp.typedef()
        return "typedef %s %s;" % (typedef.ref.symbol(), typedef.declarationName())

    def renderDeclaration(self, message, exists, getters, setters, members):
        text = ""
        cleanup = [f for f in message.fields if f.type == Proto.Message and f.q == Quantifier.Optional]

        doc = ""
        if self.klass.doc:
            doc = self.klass.doc.toComment() + "\n"
        text += render("""
            %(doc)sclass %(class)s
            {
            public:
            """, {
                "class": self.klass.declarationName(),
                "doc": doc,
            }) + "\n"

        text += "	// BEGIN: Nested items\n\n"
        for child in self.klass.iterChildDeclarations():
             text += self.indent(child.declaration() + "\n")
        text += "	// END: Nested items\n\n"

        text += self.indent(render("""
            	// BEGIN: Internal enums
            	enum _MetaInfo {
            		FieldCount = %(fieldCount)s
            	};
            	// END: Internal enums

            """, {
                "fieldCount": len(message.fields),
            })) + "\n\n"

        text += "\n"

        def renderConstructor(args, init):
            text = self.klass.declarationName() + "(\n"
            for arg, comma in commaList(args):
                text += "	" + arg[0].declaration(relative=self.klass) + comma + "\n"
            text += "	)\n"
            prefix = "	: "
            for item in init:
                if isinstance(item, Initializer):
                    text += "%s%s\n" % (prefix, item.declaration())
                    prefix = "	, "
            text += "{\n"
            for code in init:
                if not isinstance(code, Initializer):
                    text += self.indent(code) + "\n"
            text += "}"
            return text

        def renderConstruct(args, init, args2, init2):
            all_args = args2
            text = "OP_STATUS Construct(\n"
            for arg, comma in commaList(args2):
                text += "	" + arg[0].declaration() + comma + "\n"
            text += "	)\n"
            text += "{\n"
            for code in init2:
                text += self.indent(code) + "\n"
            text += "	return OpStatus::OK;\n}"
            return text

        # Check if we need the default constructor with no args, if it contains only
        # args with default values it will conflict with the default constructor so we don't make it
        for arg in self.constructor:
            if not arg[0].default:
                initializers = []
                for init in self.constructor_def_init:
                    if isinstance(init.value, ArgumentReference):
                        if init.value.arg.default:
                            value = init.value.arg.default
                            if isinstance(value, EnumValue):
                                value = value.ref()
                            initializers.append(Initializer(init.name, value))
                    else:
                        initializers.append(init)
                text += self.indent(renderConstructor([], initializers)) + "\n"
                break
        text += self.indent(renderConstructor(self.constructor, self.constructor_init)) + "\n"
        text += self.indent(renderConstruct(self.constructor, self.constructor_init, self.construct, self.construct_init)) + "\n"

        if cleanup:
            func = "~%s()\n{\n" % self.klass.declarationName()
            for field in cleanup:
                func += "	OP_DELETE(_%s);\n" % field.name
            func += "}\n"
            text += self.indent(func)

        text += "\n"

        spacing = False
        text += "	// Checkers\n"
        for func in exists:
            if spacing:
                text += "\n"
            else:
                spacing = True
            text += self.indent(self.renderFunctionDefinition(func)) + "\n"

        text += "\n	// Getters\n"
        for func in getters:
            if spacing:
                text += "\n"
            else:
                spacing = True
            text += self.indent(self.renderFunctionDefinition(func)) + "\n"

        text += "\n	// Setters\n"
        for func in setters:
            if spacing:
                text += "\n"
            else:
                spacing = True
            text += self.indent(self.renderFunctionDefinition(func)) + "\n"

        text += ("\n" +
                 "	static const OpProtobufMessage *GetMessageDescriptor(%(descriptorClass)s *descriptors);\n" +
                 "	void ResetEncodedSize() { encoded_size_ = -1; }\n" +
                 "\n" +
                 "private:\n") % {
                    "descriptorClass": self.package.cpp.descriptorClass().symbol(),
                 }
        for type, name in members:
            text += "	%(type)s %(name)s;\n" % {"type": type, "name": name,}

        text += ("\n" +
                 "	OpProtobufBitField<%d> has_bits_;\n" +
                 "	mutable int                 encoded_size_;\n" +
                 "};\n") % max(1, len(message.fields))
        return text

    @memoize
    def getImplementation(self):
        self.initMembers()
        return self.renderImplementation(self.message, self.protofields, self.offsets, self.messagefields)

    def renderImplementation(self, message, protofields, offsets, messagefields):
        parent_id = 0
        if isinstance(message.parent, proto.Message):
            parent_id = message.parent.internal_id
        if len(protofields) > 0:
            fields_var = "fields_"
        else:
            fields_var = "NULL"
        if len(offsets) > 0:
            offsets_var = "offsets_"
        else:
            offsets_var = "NULL"
        protofields_idx = [([str(idx)] + list(fields)) for idx, fields in enumerate(protofields)]
        class_name = message.cpp.klass().name
        class_path = message.cpp.klass().symbol()
        parent_class_path = None
        if isinstance(message.parent, Message):
            parent_class_path = message.iparent.cpp.klass().symbol()

        class_name_base = messageRoot(message).cpp.cppMessageSet().symbol()
        if class_name_base.find("_MessageSet"):
            cpp_descriptor_class_name = class_name_base.replace("_MessageSet", "_Descriptors")
        else:
            cpp_descriptor_class_name = class_name_base + "_Descriptors"

        text = render("""
            /*static*/
            const OpProtobufMessage *
            %(path)s::GetMessageDescriptor(%(descriptorClass)s *descriptors)
            {
            	OP_ASSERT(descriptors);
            	if (!descriptors)
            		return NULL;
            	OpProtobufMessage *&message = descriptors->message_list[%(descriptorClass)s::_gen_MessageOffs_%(messageVarPath)s];
            	if (message)
            		return message;
            """, {
                "path": class_path,
                "messageVarPath": self.varPath(message.absPath()),
                "descriptorClass": self.package.cpp.descriptorClass().symbol(),
            }) + "\n\n"

        if len(protofields) > 0:
            text += self.indent(render("""
                OpProtobufField *fields = OP_NEWA(OpProtobufField, FieldCount);
                if (fields == NULL)
                	return NULL;
                """)) + "\n"

            for idx, wireformat, number, name, quantifier, reference, field in protofields_idx:
                text += self.indent(render("""
                    fields[%(idx)s] = OpProtobufField(
                    					OpProtobufFormat::%(wireformat)s,
                    					%(number)s,
                    					UNI_L(%(name)s),
                    					%(quantifier)s
                """, {
                    "idx": idx,
                    "wireformat": wireformat,
                    "number": number,
                    "name": name,
                    "quantifier": quantifier,
                })) + "\n"

                if reference and isinstance(reference, proto.Enum):
                    text += self.indent(render("""
                        , NULL // const OpProtobufMessage *message
                        , 0 // int message_id
                        , %(descriptorClass)s::_gen_EnumID_%(enumPath)s // unsigned enum_id
                        """, {
                            "enumPath": "_".join(reference.path()),
                            "descriptorClass": cpp_descriptor_class_name,
                        }), 6) + "\n"

                text += "						);\n"
                if field.type == Proto.Bytes:
                    datatypes = {"OpData": "Bytes_OpData",
                                }
                    datatype = field.options["cpp_datatype"].value
                    if datatype in datatypes:
                        text += self.indent(render("""
                                        fields[%(idx)s].SetBytesDataType(OpProtobufField::%(type)s);
                                    """, {
                                        "idx": idx,
                                        "type": datatypes[datatype],
                                    })) + "\n"
                if field.type == Proto.String:
                    datatypes = {"TempBuffer": "String_TempBuffer",
                                 "UniString": "String_UniString",
                                }
                    datatype = field.options["cpp_datatype"].value
                    if datatype in datatypes:
                        text += self.indent(render("""
                                        fields[%(idx)s].SetStringDataType(OpProtobufField::%(type)s);
                                    """, {
                                        "idx": idx,
                                        "type": datatypes[datatype],
                                    })) + "\n"
        else:
            text += self.indent(render("""
                OpProtobufField *fields = NULL;
                """)) + "\n"
        text += "\n"

        if len(offsets) > 0:
            text += self.indent(render("""
                int *offsets = OP_NEWA(int, FieldCount);
                if (offsets == NULL)
                {
                	OP_DELETEA(fields);
                	return NULL;
                }
            """)) + "\n"

            for idx, (classpath, membername) in enumerate(offsets):
                text += self.indent(render("""
                    offsets[%(idx)s] = OP_PROTO_OFFSETOF(
                    					%(classPath)s,
                    					%(memberName)s
                    					);
                    """, {
                        "idx": idx,
                        "classPath": classpath,
                        "memberName": membername,
                    })) + "\n"
        else:
            text += self.indent(render("""
                int *offsets = NULL;
            """)) + "\n"
        text += "\n"

        text += self.indent(render("""
            message = OP_NEW(OpProtobufMessage,
            				(%(descriptorClass)s::_gen_MsgID_%(messageVarPath)s, 0,
            				FieldCount, fields, offsets,
            				OP_PROTO_OFFSETOF(%(classPath)s, has_bits_),
            				OP_PROTO_OFFSETOF(%(classPath)s, encoded_size_),
            				%(messageNameQuoted)s,
            				OpProtobufMessageVector<%(classPath)s>::Make,
            				OpProtobufMessageVector<%(classPath)s>::Destroy));
            if (message == NULL)
            {
            	OP_DELETEA(fields);
            	OP_DELETEA(offsets);
            	return NULL;
            }
            message->SetIsInitialized(TRUE);
            """, {
                "messageVarPath": self.varPath(message.absPath()),
                "classPath": self.klass.symbol(),
                "messageNameQuoted": strQuote(message.name),
                "descriptorClass": cpp_descriptor_class_name,
            })) + "\n"

        for messagefield in messagefields:
            idx, className = messagefield[0:2]
            text += "	fields[%(idx)s].SetMessage(%(class)s::GetMessageDescriptor(descriptors));\n" % {
                "idx": idx,
                "class": className,
                }

        if isinstance(message.parent, proto.Message):
            text += self.indent(render("""
        const OpProtobufMessage *parent = %(parent_path)s::GetMessageDescriptor(descriptors);
        if (!parent)
        	return NULL;
        message->SetParentId(parent->GetInternalId());
        """, {
            "parent_path": parent_class_path,
        })) + "\n"

        text += "\n	return message;\n}\n"

        text += "\n" + render("""
            // BEGIN: %(message)s: Implementation
            """, {
                "message": ".".join(message.path()),
            }) + "\n\n"

        for child in self.klass.iterChildImplementations():
            text += child.implementation() + "\n";

        spacing = False
        exist_funcs = [func for func in self.exists if not func.is_inline]
        if exist_funcs:
            text += "// Checkers\n"
            for func in exist_funcs:
                if spacing:
                    text += "\n"
                else:
                    spacing = True
                text += self.renderFunctionImplementation(func) + "\n"

        getter_funcs = [func for func in self.getters if not func.is_inline]
        if getter_funcs:
            text += "// Getters\n"
            for func in getter_funcs:
                if spacing:
                    text += "\n"
                else:
                    spacing = True
                text += self.renderFunctionImplementation(func) + "\n"

        setter_funcs = [func for func in self.setters if not func.is_inline]
        if setter_funcs:
            text += "// Setters\n"
            for func in setter_funcs:
                if spacing:
                    text += "\n"
                else:
                    spacing = True
                text += self.renderFunctionImplementation(func) + "\n"


        text += "\n" + render("""
            // END: %(message)s: Implementation
            """, {
                "message": ".".join(message.path()),
            }) + "\n"

        return text

class CppMessageSetGenerator(TemplatedGenerator, CppGenerator):
    package = None

    def __init__(self, package, messages, class_name_base, opera_root):
        TemplatedGenerator.__init__(self)
        CppGenerator.__init__(self, opera_root=opera_root)

        self.package = package
        self._messages = package.messages
        self.isMembersInitialized = False
        self.methodGenerator = MethodGenerator()
        self.cpp_class_name = class_name_base
        if class_name_base.find("_MessageSet"):
            self.cpp_descriptor_class_name = class_name_base.replace("_MessageSet", "_Descriptors")
        else:
            self.cpp_descriptor_class_name = class_name_base + "_Descriptors"
        self.type_decl_includes = set()
        self.decl_includes = ["modules/protobuf/src/protobuf_utils.h",
                              "modules/protobuf/src/protobuf_message.h",
                              ]
        self.decl_forward_decls = ["OpProtobufField",
                                   "OpProtobufMessage",
                                   ]
        self.klass = self.package.cpp.cppMessageSet()#Class(self.cpp_class_name)

    def renderMethod(self, methodName, args):
        return self.methodGenerator.renderMethod(methodName, args)

    def varName(self, name):
        return _var_name(name)

    def varPath(self, path):
        return _var_path(path)

    @memoize
    def getInterface(self):
        return self.renderInterface()

    def messages(self, with_inline=False):
        existing = {}
        for message in iterTree(self._messages, existing, with_inline=with_inline):
            if message != defaultMessage:
                yield message

    def sortedMessages(self, with_inline=False):
        messages = list(self.messages(with_inline=with_inline))

        items = set()
        partial = set()
        ordered = []
        for msg in messages:
            if msg not in items:
                items.add(msg)
                ordered.append(msg)
            if isinstance(msg.iparent, Message):
                partial.add((msg.iparent, msg))
                if msg.iparent not in items:
                    items.add(msg.iparent)
                    ordered.append(msg.iparent)
            for field in msg.fields:
                if isinstance(field.message, Message):
                    if field.q == Quantifier.Required:
                        partial.add((msg, field.message))
                        if field.message not in items:
                            items.add(field.message)
                            ordered.append(field.message)

        messages = list(stable_topological_sort(ordered, partial))
        return reversed(messages)

    def messageGenerators(self, with_inline=False):
        messages = self.sortedMessages(with_inline=with_inline)
        for msg in messages:
            gen = generatorManager.getMessage(msg)
            if not gen:
                msg.cpp.klass().parent = self.klass
                gen = generatorManager.setMessage(msg, CppMessageGenerator(msg, self.cpp_class_name))
            yield gen

    def enums(self):
        enums = []
        package = self.package
        messages = {}
        def scan_message(msg):
            for field in msg.fields:
                item = field.message
                if not item:
                    continue
                if isinstance(item, Enum) and item not in enums:
                    enums.append(item)
                if isinstance(item, Message) and item not in messages:
                    messages[item] = True
                    scan_message(item)

        def scan_nested(root):
            for item in root.items:
                if isinstance(item, Enum) and item not in enums:
                    enums.append(item)
                elif isinstance(item, Message):
                    scan_message(item)
                    scan_nested(item)
        scan_nested(package)
        return enums

    def renderInterface(self):
        # all_messages are sorted topologically
        all_messages = list(self.sortedMessages(with_inline=True))
        # sorted_messages are sorted alphabetically
        sorted_messages = sorted(all_messages, key=lambda i: i.name)
        message_count = len(sorted_messages)
        # generators are sorted topologically to ensure class references compiles
        generators = list(self.messageGenerators(with_inline=True))
        enums = self.enums()
        message_set = self.package.cpp.cppMessageSet()

        text = "\n" + render("""
            class %(descriptorClass)s;

            class %(class)s
            {
            public:
            \t// BEGIN: ctor/dtor

            \t%(class)s();
            \tvirtual ~%(class)s();

            \t// END: ctor/dtor
            """, {
                "class": self.cpp_class_name,
                "descriptorClass": self.cpp_descriptor_class_name,
            }) + "\n"

        text += "\t// BEGIN: Enums\n\n"
        # TODO: This should be used to render all cpp elements, not just enums
        for child in message_set.iterChildDeclarations():
            text += self.indent(child.declaration()) + "\n"
        text += "\t// End: Enums\n\n"

        text += "\t// BEGIN: Message classes\n\n\t// Forward declarations\n"

        for msgGen in generators:
            message_defines = msgGen.message.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"

            text += "\tclass %(class)s;\n" % {"class": msgGen.klass.declarationName()}

            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"

        text += "\n"

        for msgGen in generators:
            message_defines = msgGen.message.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"

            text += self.indent(msgGen.getDeclaration()) + "\n\n"

            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"

        text += "\n\t// END: Message classes\n"

        text += "\n};\n\n"

        for msgGen in generators:
            message_defines = msgGen.message.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"

            text += self.indent(msgGen.getInlines(), 0)

            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"

        text += "\n"

        return text

    def renderDescriptorDeclaration(self):
        all_messages = list(self.sortedMessages(with_inline=True))
        sorted_messages = sorted(all_messages, key=lambda i: i.name)
        message_count = len(all_messages)
        enums = self.enums()

        text = render("""
            class OpProtobufMessage;

            // This class contains the descriptors of all messages in the given set
            // When a message descriptor is first requested it is allocated and put in this structure,
            // subsequent uses of the descriptor will just use the previously allocated object.
            class %(descriptorClass)s
            {
            public:
            \t%(descriptorClass)s();
            \t~%(descriptorClass)s();

            \tOP_STATUS Construct();

            \tconst OpProtobufMessage *Get(unsigned id) const;

            \t/**
            \t * Defines the number of messages in this descriptor set.
            \t * The exact value is computed compile time depending on which
            \t * features/defines are active.
            \t */
                """, {
                    "descriptorClass": self.cpp_descriptor_class_name,
                }) + "\n"

        # for message in all_messages:
            # message_defines = message.cpp.defines()
            # if message_defines:
                # text += self.renderDefines(message_defines) + "\n"

            # text += self.indent("+ 1\n", 6)

            # if message_defines:
                # text += self.renderEndifs(message_defines) + "\n"

        text += "\n" + self.indent(render("""
            // Each message consists of an ID and and offset. The ID is used in
            // introspection and sent to scope clients. The offset is used to find the
            // descriptor object for a given message.
            // IDs uses the prefix _gen_MsgID_ and offsets uses _gen_MessageOffs_
            // Each message is also converted to an identifier based on the path and
            // the name.
            //
            // For instance the message WindowID will get the suffix window_id_
            // The ID for the message will then be _gen_MsgID_window_id_ and
            // the offset for the message descriptor _gen_MessageOffs_window_id_

            enum // Message offset + count
            {
            \t// Defines the offsets for each message which is used
            \t// in the message_list arrays in the descriptor
            """)) + "\n"

        for msg in sorted_messages:

            message_defines = msg.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"

            text += "\t\t_gen_MessageOffs_%(messageVarPath)s,\n" % {
                "messageVarPath": self.varPath(msg.absPath()),
                }

            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"

        text += "\n\t\t// Message count is calculated at compile time\n\t\t_gen_Message_Count\n\t};\n"

        text += "\n" + self.indent(render("""
                // The IDs for all messages
                enum // message IDs
                {
                \t  _gen_void_MsgID // The Default/void message. Also ensures that all other IDs start at 1
                """)) + "\n"
        for msg in sorted_messages:
            message_defines = msg.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"

            text += "\t\t, _gen_MsgID_%(messageVarPath)s\n" % {"messageVarPath": self.varPath(msg.absPath())}

            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"

        text += "\t};\n"

        if enums:
            text += self.indent(render("""
                enum // Enum IDs
                {
                \tEnum_void_ID_, // Dummy entry, ensures IDs starts at 1
            """)) + "\n"

            for enum in enums:
                text += self.indent(render("""
                \t// Meta info for %(classPath)s
                \t_gen_EnumID_%(enumPath)s,
                """, {
                    "classPath": messageClassPath(enum)[0],
                    "enumPath": "_".join(enum.path()),
                }), 2) + "\n"

            text += "\n\t" + self.indent(render("""
                \t\t_gen_dummy_EnumID // Only present to allow preceding entries to always end in a comma
                };

                enum // Enum values
                {
            """)) + "\n"

            for enum in enums:
                text += self.indent(render("""
                \t// Meta info for %(classPath)s
                \t_gen_EnumValueCount_%(enumPath)s = %(enumValue)s,
                """, {
                    "classPath": messageClassPath(enum)[0],
                    "enumPath": "_".join(enum.path()),
                    "enumValue": len(enum.values),
                }), 2) + "\n"

            text += "\n\t\t_gen_dummy_EnumValue // Only present to allow preceding entries to always end in a comma\n\t};\n"

            text += self.indent(render("""
                enum // Enum names
                {
            """)) + "\n"

            for enum in enums:
                text += self.indent(render("""
                \t// Meta info for %(classPath)s
                \t_gen_Enum_%(enumPath)s,
                """, {
                    "classPath": messageClassPath(enum)[0],
                    "enumPath": "_".join(enum.path()),
                }), 2) + "\n"
            text += "\n\t\t// Enum count is calculated at compile time\n\t\t_gen_Enum_Count\n\t};\n"

        text += "\n"

        if message_count > 0:
            text += self.indent(render("""

                // List of message descriptors. They are initially NULL and assigned an object when the message is used.
                mutable OpProtobufMessage *message_list[_gen_Message_Count];
                """), 1) + "\n"
        text += "\n"

        if enums:
            text += self.indent(render("""
                // List of IDs for enums, assigned in Construct().
                unsigned enum_id_list[%(enumCount)d];
                """, {
                    "enumCount": len(enums),
                }), 1) + "\n\n"

        text += ("};\n\n")
        return text

    def renderDescriptorImplementation(self):
        # all_messages is sorted topologically
        all_messages = [msg for msg in self.sortedMessages(with_inline=True) if msg != defaultMessage]
        # sorted_message is sorted alphabetically
        sorted_messages = sorted(all_messages, key=lambda i: i.name)
        message_count = len(all_messages)
        enums = self.enums()
        # Generators are sorted by message name, this ensures the same order no matter how messages reference each other
        generators = sorted(self.messageGenerators(with_inline=True), key=lambda i: i.message.name)

        text = "\n" + render("""
            // BEGIN: %(descriptorClass)s

            %(descriptorClass)s::%(descriptorClass)s()
            {
            """, {
                "descriptorClass": self.cpp_descriptor_class_name,
            }) + "\n"

        if message_count > 0:
            text += self.indent(render("""
                for (unsigned i = 0; i < _gen_Message_Count; ++i)
                {
                	message_list[i] = NULL;
                }
                """, {
                    "class": self.cpp_class_name,
                })) + "\n"

        text += render("""
            }

            %(descriptorClass)s::~%(descriptorClass)s()
            {
            """, {
                "descriptorClass": self.cpp_descriptor_class_name,
            }) + "\n"

        if message_count > 0:
            text += self.indent(render("""
                for (unsigned i = 0; i < _gen_Message_Count; ++i)
                {
                	OP_DELETE(message_list[i]);
                }
                """, {
                    "class": self.cpp_class_name,
                })) + "\n"

        text += render("""
            }

            OP_STATUS
            %(descriptorClass)s::Construct()
            {
            """, {
                "descriptorClass": self.cpp_descriptor_class_name,
            }) + "\n"

        if enums:
            for idx, enum in enumerate(enums):
                text += "	enum_id_list[%(idx)s] = _gen_EnumID_%(enumPath)s;\n" % {
                    "idx": idx,
                    "enumPath": "_".join(enum.path()),
                }

        text += "	" + render("""
            	return OpStatus::OK;
            }

            const OpProtobufMessage *
            %(descriptorClass)s::Get(unsigned id) const
            {
            """, {
                "descriptorClass": self.cpp_descriptor_class_name,
            }) + "\n"

        if message_count > 0:
            text += "	%(descriptorClass)s *descriptors = const_cast<%(descriptorClass)s *>(this);\n" % {
                        "descriptorClass": self.cpp_descriptor_class_name,
                    }
            text += self.indent(render("""
                switch (id)
                {
                """)) + "\n"

        for idx, message in enumerate(sorted_messages):
            message_defines = message.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"

            text += self.indent(("case _gen_MsgID_%(messageVarPath)s:\n" +
                     "\treturn %(messageClassPath)s::GetMessageDescriptor(descriptors);\n\n") % {
                        "class": self.cpp_class_name,
                        "messageVarPath": self.varPath(message.absPath()),
                        "messageClassPath": message.cpp.klass().symbol(),
                    })

            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"

        text += ("\tdefault:\n"
                 "\t\treturn NULL;\n"
                 "\t}\n"
                 "}\n"
                 "\n"
                 "\n"
                 "// END: %(descriptorClass)s\n"
                 "\n"
                 ) % {
                    "descriptorClass": self.cpp_descriptor_class_name,
                 }
        return text

    @memoize
    def getImplementation(self):
        return self.renderImplementation()

    def renderImplementation(self):
        # all_messages is sorted topologically
        all_messages = [msg for msg in self.sortedMessages(with_inline=True) if msg != defaultMessage]
        # sorted_message is sorted alphabetically
        sorted_messages = sorted(all_messages, key=lambda i: i.name)
        message_count = len(sorted_messages)
        enums = self.enums()
        # Generators are sorted by message name, this ensures the same order no matter how messages reference each other
        generators = sorted(self.messageGenerators(with_inline=True), key=lambda i: i.message.name)
        text = ""

        text += "\n" + render("""
                // BEGIN: %(class)s

                %(class)s::%(class)s()
                {
                }

                /*virtual*/
                %(class)s::~%(class)s()
                {
                }

                // END: %(class)s
                """, {
                    "class": self.cpp_class_name,
                }) + "\n\n"

        text += ("// BEGIN: Messages\n\n") % {
                    "class": self.cpp_class_name,
                 }

        for messageGenerator in generators:
            message_defines = messageGenerator.message.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"

            text += render("""
                //   BEGIN: Message: %(messageName)s
                %(messageImplementation)s
                //   END: Message: %(messageName)s
                """, {
                    "messageName": messageGenerator.message.name,
                    "messageImplementation": messageGenerator.getImplementation(),
                }) + "\n\n"

            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"

        text += "\n// END: Messages\n\n"

        return text

class CppProtobufBase(CppScopeBase):
    def __init__(self, cppPackage, file_path, dependencies=None):
        # TODO: Adding cppPackage.package.filename shouldn't be necessary
        dependencies = DependencyList(set([cppPackage.package.filename]) | set(dependencies or []))
        CppScopeBase.__init__(self, file_path=file_path, dependencies=dependencies, opera_root=cppPackage.operaRoot)
        self.cppPackage = cppPackage

    def createFile(self):
        return CppScopeBase._createFile(self, build_module=self.cppPackage.buildModule, package=self.cppPackage.package)

class CppProtobufMessage(TemplatedGenerator, CppProtobufBase):
    def __init__(self, messageDeclarationPath, messageImplementationPath, descriptorDeclarationPath, descriptorImplementationPath, cppPackage, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppProtobufBase.__init__(self, cppPackage, file_path=messageDeclarationPath, dependencies=dependencies)
        self.messageDeclarationPath = messageDeclarationPath
        self.messageImplementationPath = messageImplementationPath
        self.descriptorDeclarationPath = descriptorDeclarationPath
        self.descriptorImplementationPath = descriptorImplementationPath

    def generateFiles(self):
        package = self.cppPackage.package
        defines = self.cppPackage.defines
        proto_file = self.cppPackage.makeRelativePath(package.filename)

        set_gen = CppMessageSetGenerator(package, package.messages, self.cppPackage.cppClass, self.cppPackage.operaRoot)

        def messageSetDeclaration(generated_file):
            includes = set(["modules/protobuf/src/protobuf.h",
                            "modules/protobuf/src/protobuf_message.h",
                            "modules/protobuf/src/protobuf_bitfield.h",
                            ])
            text = set_gen.getInterface()

            # Bring in includes needed by types or other generated code
            type_includes = set()
            for msgGen in list(set_gen.messageGenerators(with_inline=True)):
                type_includes |= msgGen.type_decl_includes
            includes |= type_includes
            includes = list(includes)
            includes.sort()

            return self.renderFile(generated_file.path, text, includes=includes, defines=defines, proto_file=proto_file)

        def messageSetImplementation(generated_file):
            includes = [self.cppPackage.makeRelativePath(self.messageDeclarationPath),
                        self.cppPackage.makeRelativePath(self.descriptorDeclarationPath),
                        self.makeRelativePath(self.descriptorDeclarationPath),
                        "modules/protobuf/src/protobuf_utils.h",
                        "modules/protobuf/src/protobuf_data.h",
                        ]

            text = set_gen.getImplementation()

            return self.renderFile(generated_file.path, text, includes=includes, defines=defines, proto_file=proto_file, is_header=False)

        def descriptorDeclaration(generated_file):
            text = set_gen.renderDescriptorDeclaration()
            return self.renderFile(generated_file.path, text, defines=defines, proto_file=proto_file)

        def descriptorImplementation(generated_file):
            includes = [self.cppPackage.makeRelativePath(self.descriptorDeclarationPath),
                        self.cppPackage.makeRelativePath(self.messageDeclarationPath),
                        ]
            text = set_gen.renderDescriptorImplementation()
            return self.renderFile(generated_file.path, text, includes=includes, defines=defines, proto_file=proto_file, is_header=False)

        return [GeneratedFile(self.messageDeclarationPath, build_module=self.cppPackage.buildModule, package=package, dependencies=self.dependencies, callback=messageSetDeclaration),
                GeneratedFile(self.messageImplementationPath, build_module=self.cppPackage.buildModule, package=package,  dependencies=self.dependencies, callback=messageSetImplementation),
                GeneratedFile(self.descriptorDeclarationPath, build_module=self.cppPackage.buildModule, package=package,  dependencies=self.dependencies, callback=descriptorDeclaration),
                GeneratedFile(self.descriptorImplementationPath, build_module=self.cppPackage.buildModule, package=package,  dependencies=self.dependencies, callback=descriptorImplementation),
                ]

class CppDescriptorDeclaration(TemplatedGenerator, CppScopeBase):
    def __init__(self, operaRoot, buildModule, declarationPath, implementationPath, protoList, dependencies=None, jumboComponent=None):
        TemplatedGenerator.__init__(self)
        dependencies = DependencyList(set([declarationPath, implementationPath]) | set(dependencies or []))
        CppScopeBase.__init__(self, file_path=declarationPath, dependencies=dependencies)
        self.operaRoot = operaRoot
        self.buildModule = buildModule
        self.protoList = protoList
        self.declarationPath = declarationPath
        self.implementationPath = implementationPath
        self.jumboComponent = jumboComponent

    def makeRelativePath(self, fname):
        """Returns the path of fname relative to the opera root"""
        return os.path.relpath(fname, self.operaRoot).replace(os.path.sep, '/')

    def generateFiles(self):
        includes = []
        implIncludes = [self.makeRelativePath(self.declarationPath)]

        main_defines = ["PROTOBUF_SUPPORT",
                   ]

        initializers = ""
        construct = ""
        destructor = ""
        for protoItem in self.protoList:
            implIncludes.extend(protoItem["includes"])
            defines = self.renderDefines(protoItem["defines"])
            endifs = self.renderEndifs(protoItem["defines"])
            initializers += self.indent(render("""
                %(defines)s
                , %(member)s(NULL)
                %(endifs)s
                """, {
                    "member": protoItem["descriptor_id"],
                    "defines": defines,
                    "endifs": endifs,
                }), 1) + "\n"
            construct += self.indent(render("""
                %(defines)s
                {
                \t%(member)s = OP_NEW(%(classPath)s, ());
                \tRETURN_OOM_IF_NULL(%(member)s);
                \tOP_STATUS status = %(member)s->Construct();
                \tOP_ASSERT(OpStatus::IsSuccess(status));
                \tif (OpStatus::IsError(status))
                \t\treturn status;
                }
                %(endifs)s
                """, {
                    "member": protoItem["descriptor_id"],
                    "classPath": protoItem["descriptor_path"],
                    "defines": defines,
                    "endifs": endifs,
                }), 1) + "\n"
            destructor += self.indent(render("""
                %(defines)s
                OP_DELETE(%(member)s);
                %(endifs)s
                """, {
                    "member": protoItem["descriptor_id"],
                    "defines": defines,
                    "endifs": endifs,
                }), 1) + "\n"

        def generateDeclaration(generated_file):
            guard = re.sub("_+", "_", re.sub("[^a-zA-Z0-9]+", "_", os.path.basename(generated_file.path))).strip("_").upper()

            forwardDecl = ""
            for protoItem in self.protoList:
                if protoItem["defines"]:
                    forwardDecl += self.renderDefines(protoItem["defines"]) + "\n"
                forwardDecl += render("""
                    class %(classPath)s;
                    """, {
                        "classPath": protoItem["descriptor_path"],
                    }) + "\n"
                if protoItem["defines"]:
                    forwardDecl += self.renderEndifs(protoItem["defines"]) + "\n"

            declText = "\n" + render("""
                // Forward declarations for descriptors
                %(forwardDeclarations)s

                /**
                 * Global container for all protobuf descriptor sets which
                 * are defined in the system. It provides initialization of
                 * the descriptor sets.
                 *
                 * This class is meant to be allocated an initialized in the
                 * component manager only.
                 *
                 * Accessing a descriptor set is done using the macro
                 * PROTOBUF_DESCRIPTOR() which returns a pointer to the set.
                 */
                class OpProtobufDescriptorSet
                {
                public:
                    /**
                     * Initialize all descriptor set pointers to NULL.
                     * @note Call Construct() to finish the construction.
                     */
                    OpProtobufDescriptorSet();
                    ~OpProtobufDescriptorSet();

                    /**
                     * Allocates and initializes each descriptor set.
                     * @return ERR_NO_MEMORY if it fails to allocate descriptor set
                     *         or OK on success.
                     */
                    OP_STATUS Construct();

                    /**
                     * Return @c TRUE if all descriptors were properly initialized, @c FALSE otherwise.
                     */
                    BOOL IsValid() const { return is_valid; }

                private:
                    BOOL is_valid;

                    /**
                     * Deallocate all descriptor sets and mark as invalid.
                     */
                    void Destroy();

                public:
                """, {
                    "forwardDeclarations": forwardDecl,
                }) + "\n"

            declText += self.indent("// All descriptors in the system\n\n")

            for protoItem in self.protoList:
                declText += "\n" + self.indent(render("""
                    // Generated from %(filename)s
                    %(defines)s

                    %(classPath)s *%(id)s;

                    %(endifs)s
                    """, {
                        "filename": self.makeRelativePath(protoItem["filename"]).replace("\\", "/"),
                        "id": protoItem["descriptor_id"],
                        "classPath": protoItem["descriptor_path"],
                        "defines": self.renderDefines(protoItem["defines"]),
                        "endifs": self.renderEndifs(protoItem["defines"]),
                    })) + "\n\n\n"

            declText += render("""
                };

                /**
                 * Defines the access point to the global object containing all descriptors.
                 * All generated protobuf code will use this identifier to access a specific descriptor.
                 * @return OpProtobufDescriptorSet *
                 */
                #define g_protobuf_descriptors g_component_manager->GetProtobufDescriptorSet()
                /**
                 * Provides access to a specific protobuf descriptor set.
                 * @param id The id of the descriptor as it's stored in g_protobuf_descriptors, e.g @c desc_hardcore_messages
                 * @return Pointer to descriptor set or @c NULL if the global descriptor object is not initialized or valid.
                 */
                #define PROTOBUF_DESCRIPTOR(id) (g_protobuf_descriptors ? g_protobuf_descriptors->id : NULL)

                """) + "\n"

            return self.renderFile(generated_file.path, declText, includes=includes, defines=main_defines)

        def generateImplementation(generated_file):
            implText = ""
            implText += "\n" + render("""
                OpProtobufDescriptorSet::OpProtobufDescriptorSet()
                \t: is_valid(FALSE)
                %(initializers)s
                {
                }

                OpProtobufDescriptorSet::~OpProtobufDescriptorSet()
                {
                \tDestroy();
                }

                void
                OpProtobufDescriptorSet::Destroy()
                {
                %(destructor)s

                \tis_valid = FALSE;
                }

                OP_STATUS
                OpProtobufDescriptorSet::Construct()
                {
                %(construct)s
                \tis_valid = TRUE;
                \treturn OpStatus::OK;
                }

                """, {
                    "initializers": initializers,
                    "construct": construct,
                    "destructor": destructor,
                }) + "\n"

            return self.renderFile(generated_file.path, implText, includes=implIncludes, defines=main_defines, is_header=False)

        declarationFile = GeneratedFile(self.declarationPath, build_module=self.buildModule, dependencies=self.dependencies, jumbo_component=self.jumboComponent, callback=generateDeclaration)
        implementationFile = GeneratedFile(self.implementationPath, build_module=self.buildModule, dependencies=self.dependencies, jumbo_component=self.jumboComponent, callback=generateImplementation)

        return [declarationFile, implementationFile]

class CppProtobufPackage(object):
    """Stores common settings for a protobuf packge used by generators
    """
    root = None
    modulePath = None
    basePath = None
    package = None
    define = None
    classPath = None
    includes = {}
    generateInstance = True

    def __init__(self, package, root, operaRoot, buildModule, modulePath, basePath, includes={}, class_path=None, generate_instance=True):
        self.package = package
        self.root = root
        self.operaRoot = operaRoot
        self.buildModule = buildModule
        self.modulePath = modulePath
        self.basePath = basePath
        module = os.path.basename(modulePath)
        self.includes = includes.copy()

        basename = os.path.splitext(os.path.basename(package.filename))[0]
        self.defines = package.cpp.defines()
        if hasattr(package, "cpp"):
            self.cppClass = package.cpp.cppMessageSet().name
        elif "cpp_class" in package.options:
            self.cppClass = package.options["cpp_class"].value
        else:
            self.cppClass = "Op" + upfirst(module) + utils.join_camelcase(basename.split("_"))
#        self.classPath = class_path or ""
        self.generateInstance = generate_instance

    def makeRelativePath(self, fname):
        """Returns the path of fname relative to the opera root"""
        return os.path.relpath(fname, self.operaRoot).replace(os.path.sep, '/')

class CppProtobufGenerator(object):
    """Main class for generating cpp/h files from protobuf files.

    @param opera_root The root of source code tree.
    @param module_path The relative path of the module, relative to opera_root.
    @param package The protobuf package to create code for.
    @param dependencies Optional list of files the all generated files depends on.
                        Any iterable type can be used, must contain strings.

    The actual generators is returned in generators().
    """
    package = None

    def __init__(self, opera_root, build_module, module_path, package, dependencies=None):
        self.package = package
        self.operaRoot = opera_root
        self.buildModule = build_module
        self.modulePath = module_path
        self.basePath = os.path.join(module_path, "src", "generated")
        self.root = os.path.join(self.operaRoot, self.basePath)
        self.dependencies = dependencies

    def generators(self):
        """Iterator over generator objects for the current package, may be empty."""

        includes = {
            "descriptorImplementation": self.package.cpp.filenames["descriptorImplementation"],
            "descriptorDeclaration": self.package.cpp.filenames["descriptorDeclaration"],
            "implementation": self.package.cpp.filenames["messageImplementation"],
            "declaration": self.package.cpp.filenames["messageDeclaration"],
        }
        if "cpp_generate" in self.package.options and self.package.options["cpp_generate"].value != "true":
            return

        cppPackage = CppProtobufPackage(self.package, self.root, self.operaRoot, self.buildModule, self.modulePath, self.basePath, includes, generate_instance=True)
        yield CppProtobufMessage(os.path.join(self.operaRoot, includes["declaration"]),
                                 os.path.join(self.operaRoot, includes["implementation"]),
                                 os.path.join(self.operaRoot, includes["descriptorDeclaration"]),
                                 os.path.join(self.operaRoot, includes["descriptorImplementation"]),
                                 cppPackage, dependencies=self.dependencies)

class CppProtobufDescriptorGenerator(object):
    """Main class for generating the global descriptor class containing all proto descriptors.

    @param opera_root The root of source code tree.
    @param module_path The relative path of the module, relative to opera_root.
    @param dependencies Optional list of files the all generated files depends on.
                        Any iterable type can be used, must contain strings.

    The actual generators is returned in generators().
    """

    def __init__(self, opera_root, build_module, module_path, proto_list, dependencies=None, jumbo_component=None):
        self.protoList = proto_list
        self.operaRoot = opera_root
        self.buildModule = build_module
        self.modulePath = module_path
        self.basePath = os.path.join(module_path, "src", "generated")
        self.root = os.path.join(self.operaRoot, self.basePath)
        self.dependencies = dependencies
        self.jumboComponent = jumbo_component

    def generators(self):
        """Iterator over generator objects for the current package, may be empty."""

        yield CppDescriptorDeclaration(self.operaRoot, self.buildModule,
                                       os.path.join(self.root, "g_protobuf_descriptors.h"),
                                       os.path.join(self.root, "g_protobuf_descriptors.cpp"),
                                       self.protoList, dependencies=self.dependencies,
                                       jumboComponent=self.jumboComponent)
