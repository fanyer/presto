import os
from copy import copy
import re

from hob import utils, proto
from hob.proto import Request, Event, Proto, Quantifier, defaultMessage, iterTree, Target, Package, Service, Command, Message, Field, Enum, Option
from cppgen.cpp import TemplatedGenerator, CppScopeBase, CppGenerator, memoize, DependencyList, render, strQuote, _var_name, _var_path, message_className, messageClassPath, template, currentCppConfig, getDefaultOptions, generatorManager, upfirst, enum_name, enum_valueName, message_inlineClassName, iterOpMessages
from cppgen.cpp import Class, Method, Parameter, path_string, AllocationStrategy
from cppgen.cpp import fileHeader, cpp_path_norm, commaList, root as getPackage
from cppgen.message import CppMessageGenerator
from cppgen.utils import stable_topological_sort

class CppOpMessageSetGenerator(TemplatedGenerator, CppGenerator):
    package = None

    def __init__(self, package, messages, options):
        TemplatedGenerator.__init__(self)
        CppGenerator.__init__(self, opera_root=options.operaRoot)

        self.package = package
        self.options = options
        self.messages = messages

    @memoize
    def getInterface(self):
        return self.renderInterface()

    def renderInterface(self):
        # all_messages are sorted topologically
        all_messages = [msg for msg in self.messages if msg != defaultMessage]
        # sorted_messages are sorted alphabetically
        sorted_messages = sorted(all_messages, key=lambda i: i.name)
        message_count = len(sorted_messages)
        # generators are sorted topologically to ensure class references compiles
        message_set = self.package.cpp.cppOpMessageSet()
        text = ""

        if self.package.cpp.useOpMessageSet:
            text += "\n" + render("""
                class %(class)s
                {
                public:
                """, {
                    "class": message_set.name,
                }) + "\n\n"

        text += "\n	// BEGIN: Message classes\n"

        for msg in sorted_messages:
            text += self.indent(self.renderMessage(msg)) + "\n\n"

        text += "\n	// END: Message classes\n"

        if self.package.cpp.useOpMessageSet:
            text += "\n};\n\n"

        return text

    def renderMessage(self, message):
        klass = message.cpp.opMessage()
        protobuf_klass = message.cpp.klass()

        message_gen = CppMessageGenerator(message, self.package.cpp.cppMessageSet().symbol())
        message_gen.initMembers()

        members = ""
        for member in message_gen.redirect_members:
            members += self.indent(message_gen.renderFunctionDefinition(member, use_rtype_outer=True)) + "\n\n"

        all_args = message_gen.construct + message_gen.constructor
        all_args.sort(key=lambda i: i[0].default != None)
        args = "\n	".join([arg.declaration(relative=klass) + "," for arg, idx in all_args])
        constructor_args = "\n			".join([", " + arg.declaration(relative=klass) for arg, idx in sorted(message_gen.constructor, key=lambda i: i[0].default != None)])
        construct_args = "\n		".join([arg[0].declaration(relative=klass) + comma for arg, comma in commaList(sorted(message_gen.construct, key=lambda i: i[0].default != None))])
        arg_call = ", ".join([arg.symbol(relative=klass) for arg, idx in sorted(message_gen.constructor, key=lambda i: i[0].default != None)])
        arg_call_comma = arg_call and ", " + arg_call
        arg_call2 = ", ".join([arg.symbol(relative=klass) for arg, idx in sorted(message_gen.construct, key=lambda i: i[0].default != None)])

        allocateDefs = ""
        if message.cpp.opMessageAlloc == AllocationStrategy.ALLOCATE_OPMEMORYPOOL:
            allocateDefs = render("""
                /** Use OpMemoryPool allocation with OP_NEW */
                OP_USE_MEMORY_POOL_DECL;
                """, { "class": klass.name }) + "\n"

        custom_create = ""
        default_create = ""
        need_default_create = not all_args
        if all_args:
            # See if we need a default Create() method with no args, if all args
            # to the custom Create() method have default values then it is not
            # needed
            for arg in all_args:
                if not arg[0].default:
                    need_default_create = True
                    break

            custom_create = self.indent(render("""
                	static %(class)s* Create(
                		%(args)s
                		const OpMessageAddress& src = OpMessageAddress(),
                		const OpMessageAddress& dst = OpMessageAddress(),
                		double delay = 0)
                	{
                		%(class)s *obj = OP_NEW(%(class)s, (src, dst, delay%(arg_call_comma)s));
                		if (obj)
                		{
                			if (OpStatus::IsSuccess(obj->Construct(%(arg_call2)s)))
                				return obj;

                			OP_DELETE(obj);
                		}
                		return NULL;
                	}
                """, {
                    "class": klass.name,
                    "args": args,
                    "arg_call2": arg_call2,
                    "arg_call_comma": arg_call_comma,
                }))
        if need_default_create:
            makeCode = "OP_NEW(%(class)s, (src, dst, delay))" % {
                    "class": klass.name,
                    }
            default_create = self.indent(render("""
                static %(class)s* Create()
                {
                    OpMessageAddress src;
                    OpMessageAddress dst;
                    double delay = 0;
                    return %(makeCode)s;
                }
                """, {
                    "class": klass.name,
                    "makeCode": makeCode,
                }))

        defaultConstructor = ""
        for arg in message_gen.constructor:
            if not arg[0].default:
                defaultConstructor = self.indent(render("""
                	%(class)s(
                			const OpMessageAddress& src,
                			const OpMessageAddress& dst,
                			double delay
                			)
                		: OpTypedMessage(Type, src, dst, delay)
                		, protobuf_data()
                	{
                	}
                """, {
                    "class": klass.name,
                }))
                break

        doc = ""
        if klass.doc:
            doc = klass.doc.toComment() + "\n"

        text = ""
        message_defines = message.cpp.defines()
        if message_defines:
            text += self.renderDefines(message_defines) + "\n"

        text = render("""
            %(doc)sclass %(class)s
            	: public OpTypedMessage
            {
            public:
            \t/**
            \t * The type for this message class.
            \t *
            \t * @note The reason this contains a integer value and not an enum
            \t *       is to avoid uneccesary dependencies and helps avoid
            \t *       re-compiles whenever a message is added, removed or renamed.
            \t */
            \tstatic const unsigned Type = %(messageIdentifier)du;

            	virtual ~%(class)s();

            %(default_create)s

            	static %(class)s* Create(
            		const OpMessageAddress& src,
            		const OpMessageAddress& dst = OpMessageAddress(),
            		double delay = 0)
            	{
            		return OP_NEW(%(class)s, (src, dst, delay));
            	}

            %(custom_create)s

            	static %(class)s* Cast(const OpTypedMessage* msg);

            	static %(class)s* Deserialize(
            		const OpMessageAddress& src,
            		const OpMessageAddress& dst,
            		double delay,
            		const OpData& data);

            	virtual OP_STATUS Serialize(OpData& data) const;

            	virtual const char *GetTypeString() const;

            	// Accessors/modifiers comes here
            %(members)s

            protected:
            %(allocateDefs)s
            	%(class)s(
            			const OpMessageAddress& src,
            			const OpMessageAddress& dst,
            			double delay
            			%(constructor_args)s
            			)
            		: OpTypedMessage(Type, src, dst, delay)
            		, protobuf_data(%(arg_call)s)
            	{
            	}

            %(defaultConstructor)s

            	OP_STATUS Construct(
            		%(construct_args)s
            	)
            	{
            		return protobuf_data.Construct(%(arg_call2)s);
            	}

            private:
            	%(protobuf_class)s protobuf_data;
            };

            """, {
                "class": klass.name,
                "protobuf_class": protobuf_klass.symbol(),
                "messageIdentifier": message.cpp.opMessageID,
                "doc": doc,
                "protobuf_message": "TODO: Fill in protobuf message here",
                "members": members,
                "args": args,
                "arg_call": arg_call,
                "arg_call_comma": arg_call_comma,
                "arg_call2": arg_call2,
                "constructor_args": constructor_args,
                "construct_args": construct_args,
                "default_create": default_create,
                "custom_create": custom_create,
                "defaultConstructor": defaultConstructor,
                "allocateDefs": self.indent(allocateDefs),
            }) + "\n"

        if message_defines:
            text += self.renderEndifs(message_defines) + "\n"

        return text

    @memoize
    def getImplementation(self):
        return self.renderImplementation()

    def renderImplementation(self):
        # all_messages are sorted topologically
        all_messages = [msg for msg in self.messages if msg != defaultMessage]
        # sorted_messages are sorted alphabetically
        sorted_messages = sorted(all_messages, key=lambda i: i.name)
        message_count = len(sorted_messages)
        # generators are sorted topologically to ensure class references compiles
        message_set = self.package.cpp.cppOpMessageSet()
        text = ""

        text += "\n// BEGIN: Message classes\n"

        for msg in sorted_messages:
            classPath = msg.cpp.opMessage().symbol()

            message_defines = msg.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"

            text += "\n// BEGIN: Message %(classPath)s\n\n" % {"classPath": classPath,}
            text += self.renderMessageImplementation(msg) + "\n\n"
            text += "\n// END: Message %(classPath)s\n\n" % {"classPath": classPath,}

            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"

        text += "\n// END: Message classes\n"

        return text

    def renderMessageImplementation(self, message):
        klass = message.cpp.opMessage()
        protoKlass = message.cpp.klass()

        message_gen = CppMessageGenerator(message, self.package.cpp.cppMessageSet().symbol())
        message_gen.initMembers()
        message_gen.cpp_class_path = klass.symbol()

        members = ""
        for member in message_gen.redirect_members:
            if not member.is_inline:
                members += message_gen.renderFunctionImplementation(member) + "\n\n"

        text = render("""
            %(classPath)s::~%(class)s()
            {
            }

            /* static */ %(classPath)s*
            %(classPath)s::Cast(const OpTypedMessage* msg)
            {
            	OP_ASSERT(msg && msg->GetType() == Type);
                return static_cast<%(classPath)s*>(const_cast<OpTypedMessage*>(msg));
            }

            /* static */ %(classPath)s*
            %(classPath)s::Deserialize(
            		const OpMessageAddress& src,
            		const OpMessageAddress& dst,
            		double delay,
            		const OpData& data)
            {
            	OpProtobufInputStream stream;
            	RETURN_VALUE_IF_ERROR(stream.Construct(data), NULL);
            	%(classPathRelative)s *msg = %(classPathRelative)s::Create(src, dst, delay);
            	if (!msg)
            		return NULL;
            	OpProtobufInstanceProxy instance(%(protoClassPath)s::GetMessageDescriptor(PROTOBUF_DESCRIPTOR(%(descriptorID)s)), &msg->protobuf_data);
            	if (OpStatus::IsError(stream.Read(instance)))
            	{
            		OP_DELETE(msg);
            		return NULL;
            	}
            	return msg;
            }

            OP_STATUS
            %(classPath)s::Serialize(OpData& data) const
            {
            	OpProtobufOpDataOutputRange range(data);
            	OpProtobufOutputStream stream(&range);
            	%(protoClassPath)s *proto = const_cast<%(protoClassPath)s *>(&protobuf_data);
            	void *proto_ptr = proto;
            	OpProtobufInstanceProxy instance(%(protoClassPath)s::GetMessageDescriptor(PROTOBUF_DESCRIPTOR(%(descriptorID)s)), proto_ptr);
            	return stream.Write(instance);
            }

            const char *
            %(classPath)s::GetTypeString() const
            {
            	const OpProtobufMessage *d = %(protoClassPath)s::GetMessageDescriptor(PROTOBUF_DESCRIPTOR(%(descriptorID)s));
            	return d ? d->GetName() : NULL;
            }
            """, {
                "classPath": klass.symbol(),
                "classPathRelative": klass.symbol(relative=klass),
                "class": klass.name,
                "protoClassPath": protoKlass.symbol(),
                "globalModule": "g_%s" % self.options.module,
                "descriptorID": self.package.cpp.descriptorIdentifier(),
            })

        text += members

        return text

class CppModuleBase(object):
    """Stores common settings for a single module used by generators
    """
    root = None
    modulePath = None
    basePath = None
    defines = None
    classPath = None

    def __init__(self, root, operaRoot, buildModule, modulePath, basePath):
        self.root = root
        self.operaRoot = operaRoot
        self.buildModule = buildModule
        self.modulePath = modulePath
        self.basePath = basePath
        self.module = module = os.path.basename(modulePath)
        self.defines = []

    def makeRelativePath(self, fname):
        """Returns the path of fname relative to the opera root"""
        return os.path.relpath(fname, self.operaRoot).replace(os.path.sep, '/')

class CppOpMessagePackage(CppModuleBase):
    """Stores common settings for a protobuf packge used by generators
    """
    package = None
    classPath = None
    includes = {}
    generateInstance = True

    def __init__(self, package, messages, root, operaRoot, buildModule, modulePath, basePath, includes={}, class_path=None, generate_instance=True):
        super(CppOpMessagePackage, self).__init__(root, operaRoot, buildModule, modulePath, basePath)
        self.package = package
        self.messages = list(messages)
        self.includes = includes.copy()

        basename = os.path.splitext(os.path.basename(package.filename))[0]
        self.defines.extend(package.cpp.defines())
#        self.defines.insert(0, "OP_MULTIPROCESS_MESSAGE")
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

class CppOpMessageBase(CppScopeBase):
    def __init__(self, cppPackage, file_path, dependencies=None):
        dependencies = DependencyList(set([cppPackage.package.filename]) | set(dependencies or []))
        CppScopeBase.__init__(self, file_path=file_path, dependencies=dependencies)
        self.cppPackage = cppPackage

    def createFile(self):
        return CppOpMessageBase._createFile(self, build_module=self.cppPackage.buildModule, package=self.cppPackage.package)

class CppOpMessageDeclaration(TemplatedGenerator, CppOpMessageBase):
    def __init__(self, file_path, cppPackage, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppOpMessageBase.__init__(self, cppPackage, file_path=file_path, dependencies=dependencies)

    def generate(self):
        package = self.cppPackage.package
        guard = re.sub("_+", "_", re.sub("[^a-zA-Z0-9]+", "_", os.path.basename(self.filePath))).strip("_").upper()

        includes = [cpp_path_norm(package.cpp.filenames["messageDeclaration"]),
                    "modules/hardcore/component/OpTypedMessage.h",
                    ]

        text = fileHeader()
        text += "\n" + render("""
            // Generated from %(proto_file)s
            
            #ifndef %(guard)s
            #define %(guard)s

            %(defines)s

            %(includes)s
            
            """, {
                "proto_file": self.cppPackage.makeRelativePath(package.filename),
                "guard": guard,
                "defines": self.renderDefines(self.cppPackage.defines),
                "includes": self.renderIncludes(includes),
            }) + "\n"

        set_gen = CppOpMessageSetGenerator(package, self.cppPackage.messages, options=self.cppPackage)
        text += set_gen.getInterface()

        text += render("""

            %(endifs)s

            #endif // %(guard)s
            """, {
                "endifs": self.renderEndifs(self.cppPackage.defines),
                "guard": guard,
            }) + "\n"

        return text

class CppOpMessageImplementation(TemplatedGenerator, CppOpMessageBase):
    def __init__(self, file_path, cppPackage, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppOpMessageBase.__init__(self, cppPackage, file_path=file_path, dependencies=dependencies)

    def generate(self):
        package = self.cppPackage.package
        guard = re.sub("_+", "_", re.sub("[^a-zA-Z0-9]+", "_", os.path.basename(self.filePath))).strip("_").upper()

        includes = [cpp_path_norm(package.cpp.filenames["opMessageDeclaration"]),
                    "modules/protobuf/src/protobuf_input.h",
                    "modules/protobuf/src/protobuf_output.h",
                    "modules/protobuf/src/protobuf_message.h",
                    "modules/protobuf/src/protobuf_data.h",
                    "modules/protobuf/src/generated/g_protobuf_descriptors.h",
                    ]

        text = fileHeader()
        text += "\n" + render("""
            // Generated from %(proto_file)s

            #include "core/pch.h"
            
            #ifndef %(guard)s
            #define %(guard)s

            %(defines)s

            %(includes)s

            """, {
                "proto_file": self.cppPackage.makeRelativePath(package.filename),
                "guard": guard,
                "defines": self.renderDefines(self.cppPackage.defines),
                "includes": self.renderIncludes(includes),
            }) + "\n"

        set_gen = CppOpMessageSetGenerator(package, self.cppPackage.messages, options=self.cppPackage)
        text += set_gen.getImplementation()

        text += render("""

            %(endifs)s

            #endif // %(guard)s
            """, {
                "endifs": self.renderEndifs(self.cppPackage.defines),
                "guard": guard,
            }) + "\n"

        return text

class CppOpTypedMessageBaseDeclaration(TemplatedGenerator, CppScopeBase, CppModuleBase):
    def __init__(self, file_path, root, operaRoot, buildModule, modulePath, basePath, jumboComponent, messages, enums, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppScopeBase.__init__(self, file_path=file_path, dependencies=dependencies)
        CppModuleBase.__init__(self, root, operaRoot, buildModule, modulePath, basePath)
        self.jumboComponent = jumboComponent
        self.messages = messages
        self.enums = enums

    def generate(self):
        enums = self.enums
        guard = re.sub("_+", "_", re.sub("[^a-zA-Z0-9]+", "_", os.path.basename(self.filePath))).strip("_").upper()

        text = fileHeader()
        text += "\n" + render("""
            // Generated from proto files in the system

            #ifndef %(guard)s
            #define %(guard)s

            %(defines)s

            class OpTypedMessage;
            struct OpMessageAddress;
            class OpData;

            class OpTypedMessageBase
            {
            public:
            """, {
                "defines": self.renderDefines(self.defines),
                "guard": guard,
            }) + "\n"

        text += self.indent(self.renderVerify(enums)) + "\n"

        text += self.indent(render("""
            /**
             * Deserializes the message data in @p data based on the @p type.
             * It will call the Class::Deserialize() method on the class that
             * belongs to the type.
             *
             * @param type The type of message data to decode. This determines
             *        the class of the returned instance.
             * @param src Source address of the message. This will be placed in
             *        the returned object.
             * @param dst Destination address of the message. This will be
             *        placed in the returned object.
             * @param delay Number of milliseconds until this message should
             *        be processed. This will be recorded in the returned object.
             * @param data The data-stream from which to de-serialize the
             *        message object.
             * @return The deserialized message object or @c NULL on failure.
             */
            static OpTypedMessage*
            DeserializeData(
                int type,
                const OpMessageAddress& src,
                const OpMessageAddress& dst,
                double delay,
                const OpData& data);
            """))
        text += render("""
            };

            %(endifs)s

            #endif // %(guard)s
            """, {
                "endifs": self.renderEndifs(self.defines),
                "guard": guard,
            }) + "\n"

        return text

    def renderVerify(self, enums):
        text = "\n" + render("""
            /**
             * Check if the type value @a type can be mapped to a type in an OpTypedMessage sub-class.
             * @param type Integer value corresponding to type.
             * @return @c TRUE if the value is valid, @c FALSE otherwise.
             */
            static BOOL VerifyType(int type);
            """) + "\n"
        return text

    def createFile(self):
        return CppScopeBase._createFile(self, build_module=self.buildModule, jumbo_component=self.jumboComponent)

class CppOpTypedMessageBaseImplementation(TemplatedGenerator, CppScopeBase, CppModuleBase):
    def __init__(self, file_path, root, operaRoot, buildModule, modulePath, basePath, jumboComponent, messages, enums, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppScopeBase.__init__(self, file_path=file_path, dependencies=dependencies)
        CppModuleBase.__init__(self, root, operaRoot, buildModule, modulePath, basePath)
        self.jumboComponent = jumboComponent
        self.messages = messages[:]
        self.messages.sort(key=lambda i: "%s:%s" % (getPackage(i).filename, ".".join(i.path())))

    def generate(self):
        includes = ["modules/hardcore/src/generated/OpTypedMessageBase.h",
                    ]
        includes_set = set(includes)
        for message in self.messages:
            package = getPackage(message)
            for include in package.cpp.opMessageIncludes():
                if include not in includes_set:
                    includes_set.add(include)
                    includes.append(include)

        text = fileHeader()
        text += "\n" + render("""
            // Generated from proto files in the system

            #include "core/pch.h"
            
            %(defines)s

            %(includes)s

            """, {
                "defines": self.renderDefines(self.defines),
                "includes": self.renderIncludes(includes),
            }) + "\n"

        text += self.renderVerify(self.messages) + "\n"

        text += self.renderDeserialize(self.messages) + "\n"

        text += render("""

            %(endifs)s
            """, {
                "endifs": self.renderEndifs(self.defines),
            }) + "\n"

        return text

    def renderVerify(self, messages):
        text = "\n" + render("""
            /*static*/ BOOL
            %(class)s::VerifyType(int type)
            {
            \tswitch(type)
            \t{
            """, {
                "class": "OpTypedMessageBase",
            }) + "\n"
        for message in messages:
            message_defines = getPackage(message).cpp.defines() + message.cpp.defines()
            if message_defines:
                text += self.renderDefines(message_defines) + "\n"
            text += self.indent(render("""
                case %(class)s::Type:
                // %(file)s: %(messagePath)s
                """, {
                    "opMessageID": message.cpp.opMessageID,
                    "messagePath": ".".join(message.path()),
                    "file": os.path.normpath(getPackage(message).filename).replace("\\", "/"),
                    "class": message.cpp.opMessage().symbol(),
                }), 2) + "\n\n"
            if message_defines:
                text += self.renderEndifs(message_defines) + "\n"
        text += "\n\t\t\t" + render("""
            \t\t\treturn TRUE;

            \t\tdefault:
            \t\t\treturn FALSE;
            \t}
            }
            """, {
                "class": "OpTypedMessageBase",
            }) + "\n"
        return text

    def renderDeserialize(self, messages):
        def code():
            text = ""
            for message in messages:
                message_defines = getPackage(message).cpp.defines() + message.cpp.defines()
                if message_defines:
                    text += self.renderDefines(message_defines) + "\n"
                text += self.indent(render("""
                    case %(class)s::Type:
                        // %(file)s: %(messagePath)s
                        return %(class)s::Deserialize(src, dst, delay, data);
                    """, {
                        "opMessageID": message.cpp.opMessageID,
                        "messagePath": ".".join(message.path()),
                        "file": os.path.normpath(getPackage(message).filename).replace("\\", "/"),
                        "class": message.cpp.opMessage().symbol(),
                    })) + "\n\n"
                if message_defines:
                    text += self.renderEndifs(message_defines) + "\n"
            return text

        text = "\n" + render("""
            /*static*/ OpTypedMessage*
            OpTypedMessageBase::DeserializeData(
                int type,
                const OpMessageAddress& src,
                const OpMessageAddress& dst,
                double delay,
                const OpData& data)
            {
            \t// Decode the serialized data by matching @a type against the Type in one of the sub-classes of OpTypedMessage
            \tswitch(type)
            \t{
            %(code)s
            
            \t\tdefault:
            \t\t\tOP_ASSERT(!"Unknown message type, cannot decode");
            \t\t\treturn NULL;
            \t}
            }
            """, {
                "code": self.indent(code()),
            }) + "\n"
        return text

    def createFile(self):
        return CppScopeBase._createFile(self, build_module=self.buildModule, jumbo_component=self.jumboComponent)

class CppOpMessageGenerator(object):
    """Main class for generating cpp/h files for OpMessage sub-classes.

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
            "implementation": self.package.cpp.filenames["opMessageImplementation"],
            "declaration": self.package.cpp.filenames["opMessageDeclaration"],
        }
        if "cpp_generate" in self.package.options and self.package.options["cpp_generate"].value != "true":
            return

        cppPackage = CppOpMessagePackage(self.package, iterOpMessages(self.package), self.root, self.operaRoot, self.buildModule, self.modulePath, self.basePath, includes, generate_instance=True)
        yield CppOpMessageImplementation(os.path.join(self.operaRoot, includes["implementation"]), cppPackage, dependencies=self.dependencies)
        yield CppOpMessageDeclaration(os.path.join(self.operaRoot, includes["declaration"]), cppPackage, dependencies=self.dependencies)

class CppOpMessageGlobalGenerator(object):
    """Main class for generating global information on OpMessage classes

    @param opera_root The root of source code tree.
    @param module_path The relative path of the module, relative to opera_root.
    @param enums The enums to identify each OpMessage
    @param dependencies Optional list of files the all generated files depends on.
                        Any iterable type can be used, must contain strings.

    The actual generators is returned in generators().
    """
    enums = None

    def __init__(self, opera_root, build_module, module_path, jumbo_component, messages, enums, dependencies=None):
        self.messages = messages
        self.enums = enums
        self.operaRoot = opera_root
        self.buildModule = build_module
        self.modulePath = module_path
        self.basePath = os.path.join(module_path, "src", "generated")
        self.root = os.path.join(self.operaRoot, self.basePath)
        self.dependencies = dependencies
        self.jumboComponent = jumbo_component

    def generators(self):
        """Iterator over generator objects for the current package, may be empty."""

        decl_path = os.path.join(self.basePath, "OpTypedMessageBase.h")
        impl_path = os.path.join(self.basePath, "OpTypedMessageBase.cpp")

        if self.enums:
            yield CppOpTypedMessageBaseDeclaration(os.path.join(self.operaRoot, decl_path), self.root, self.operaRoot, self.buildModule, self.modulePath, self.basePath, self.jumboComponent, self.messages, self.enums, dependencies=self.dependencies)
            yield CppOpTypedMessageBaseImplementation(os.path.join(self.operaRoot, impl_path), self.root, self.operaRoot, self.buildModule, self.modulePath, self.basePath, self.jumboComponent, self.messages, self.enums, dependencies=self.dependencies)
