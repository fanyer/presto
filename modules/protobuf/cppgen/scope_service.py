import os
from copy import copy
import string
import re

from hob import utils, proto
from hob.proto import Request, Event, Proto, Quantifier, defaultMessage, iterTree, Target, Package, Service, Command, Message, Field, Enum, Option
from cppgen.cpp import TemplatedGenerator, CppScopeBase, memoize, DependencyList, render, strQuote, _var_name, _var_path, message_className, messageClassPath, generatorManager, enum_name, enum_valueName, fileHeader, cpp_path_norm, GeneratedFile
from cppgen.message import CppMessageGenerator, CppMessageSetGenerator

def commandEnumName(name):
    return "Command_" + name

def enumName(cmd):
    return cmd.options["cpp_enum_name"].value

def requiresAsyncResponse(cmd):
    return cmd.options.get("cpp_response", "direct") == "deferred"

def event_functionName(command):
    return "Send%s" % command.name

def is_deferred(command):
    return command.options.get("cpp_response", "direct") == "deferred"

def event_functionSignature(command):
    if issubclass(type(command), Request):
        message = command.response
    else:
        message = command.message
    params = []
    if message.isNonEmpty():
        params.append("const %s &msg" % message_className(message))
    if is_deferred(command):
        params.append("unsigned int tag")
    out = "OP_STATUS %s(%s);" % (event_functionName(command), ", ".join(params))
    return out

def request_functionCall(command):
    out = "Do" + command.name + "("
    args = []
#    if command.cpp_send_message:
#        args.append("msg")
    if command.message.isNonEmpty():
        args.append("in")
    if hasattr(command, 'response') and command.response.isNonEmpty():
        args.append("out")
    if is_deferred(command):
        args.append("async_tag")
    out += ", ".join(args)
    out += ")"
    return out

def request_functionSignature(command):
    out = "virtual OP_STATUS Do" + command.name + "("
    args = []
#    if command.cpp_send_message:
#        args.append("const OpScopeTPHeader &msg")
    if command.message.isNonEmpty():
        args.append("const %s &in" % message_className(command.message))
    if command.response.isNonEmpty():
        args.append("%s &out" % message_className(command.response))
    if is_deferred(command):
        args.append("unsigned int async_tag")
    out += ", ".join(args)
    out += ") = 0;"
    return out

def serviceMessages(service, with_inline=False):
    existing = {}
    for command in service.commands:
        if command.message:
            for message in iterTree([command.message], existing, with_inline=with_inline):
                yield message
        if hasattr(command, 'response') and command.response:
            for message in iterTree([command.response], existing, with_inline=with_inline):
                yield message

def serviceMessages2(service, with_inline=False):
    existing = {}
    for command in service.commands:
        if command.message:
            yield command.message
        if hasattr(command, 'response') and command.response:
            yield command.response

class CppServiceGenerator(TemplatedGenerator):
    """
    C++ code generator for the implementation and declaration of a scope service.

    TODO: This code is old-style and should be updated to use the new generateFiles() API, see CppServiceInterfaceDeclaration for an example.
    """
    service = None

    # Generated text
#    decl_include = None
#    decl_public = None
#    decl_private = None
#    impl = None

    def __init__(self, service):
        super(CppServiceGenerator, self).__init__()

        self.service = service
        self.package = service.iparent
        self.generate()

    def varName(self, name):
        return _var_name(name)

    def varPath(self, path):
        return _var_path(path)

    def messages(self, with_inline=False):
        return serviceMessages(self.service, with_inline)

    def sortedMessages(self, with_inline=False):
        messages = list(self.messages(with_inline=with_inline))
        references = {}
        for e in messages:
            for msg in iterTree([e], {}, with_global=False):
                for field in msg.fields:
                    if field.type != Proto.Message:
                        continue
                    references.setdefault(field.message, set()).add(msg)
        def compare(a, b):
            if a == b:
                return 0
            if a in references:
                if b in references[a]:
                    return -1
            if b in references:
                if a in references[b]:
                    return 1
            return 0
        messages.sort(cmp=compare)
        return messages

    def enums(self):
        enums = []
        package = self.service.iparent
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

        for item in package.items:
            if isinstance(item, Enum) and item not in enums:
                enums.append(item)
            elif isinstance(item, Message):
                scan_message(item)
        return enums

    def messageGenerators(self, with_inline=False):
        messages = self.sortedMessages(with_inline=with_inline)
        if "cpp_class" not in self.service.options:
            raise CppError("Missing 'cpp_class' option in service definition")
        cpp_class = self.service.options["cpp_class"].value
        for msg in messages:
            if msg.isNonEmpty():
                gen = generatorManager.getMessage(msg)
                if not gen:
                    gen = generatorManager.setMessage(msg, CppMessageGenerator(msg, cpp_class + "_SI"))
                yield gen

    def commands(self):
        for command in self.service.commands:
            yield command

    def sortedCommands(self):
        commands = list(self.commands())
        commands.sort(key=lambda item: item.id)
        return commands

    def getVersion(self):
        if "version" not in self.service.options:
            raise CppError("Missing 'version' option in the service definition")

        v = string.split(self.service.options["version"].value, ".")

        if not v or len(v) < 2 or len(v) > 3:
            raise CppError("Incorrect service version format (expected: x.y[.z])")

        if len(v) <= 2:
            v.append("0")

        return v

    def generate(self):
        self.decl_includes = ["modules/protobuf/src/protobuf_utils.h",
                              "modules/protobuf/src/protobuf_message.h",
                              ]
        self.decl_forward_decls = ["OpScopeTPMessage",
                                   "OpProtobufField",
                                   "OpProtobufMessage",
                                   ]
        self.impl_includes = ["modules/protobuf/src/protobuf_utils.h",
                              "modules/scope/src/scope_tp_message.h",
                              "modules/scope/src/scope_default_message.h",
                              "modules/scope/src/scope_manager.h",
                              "modules/scope/src/generated/g_scope_manager.h",
                              ]
        if "cpp_file" in self.service.options:
            cpp_file = self.service.options["cpp_file"].value
        else:
            cpp_file = utils.join_underscore(utils.split_camelcase(self.service.name))
        files = self.service.cpp.files()
        self.impl_includes.append(files["generatedServiceDeclaration"])
        self.impl_includes.append(files["serviceDeclaration"])

        commands  = copy(self.sortedCommands())
        for command in commands:
            command.options["cpp_enum_name"] = commandEnumName(command.name)
        requests  = [command for command in commands if issubclass(type(command), Request) and not is_deferred(command)]
        events    = [command for command in commands if issubclass(type(command), Event)]
        async     = [(command.toRequest(), command.toEvent()) for command in commands if issubclass(type(command), Request) and is_deferred(command)]
        requests += [req[0] for req in async]
        events   += [req[1] for req in async]

        self.service.version = self.getVersion()


        import hob.proto
        self.commands = commands
        self.requests = requests
        self.events = events

    @memoize
    def getInterface(self, generate_instance=True):
        return self.renderServiceInterface(self.service, self.commands, generate_instance)

    @memoize
    def getDeclaration(self):
        return self.renderServiceDeclaration(self.service, self.requests, self.events)

    @memoize
    def getInterfaceIncludes(self):
        return self.renderDeclarations(self.decl_includes, self.decl_forward_decls)

    def renderDeclarations(self, includes, forwardDeclarations):
        text = "\n"
        for include in includes:
            text += "#include %s\n" % strQuote(include)
        text += "\n"

        for decl in forwardDeclarations:
            text += "class %s;\n" % decl
        text += "\n"
        return text

    def renderServiceInterface(self, service, commands, generate_instance):
        text = "\n" + render("""
            class %(class)s_SI
            	: public OpScopeService
            	, public %(class)s_MessageSet
            {
            public:
            	%(class)s_SI(const uni_char* id = NULL, OpScopeServiceManager *manager = NULL, ControlType control = CONTROL_MANUAL);
            	virtual ~%(class)s_SI();

            	enum CommandEnum
            	{
            """, {
                "class": service.options["cpp_class"].value, # TODO: Store fully generated class name on service object
            }) + "\n"

        for command in commands[:1]:
            text += "		  %(enumName)s = %(enumValue)s\n" % {
                "enumName": command.options["cpp_enum_name"].value,
                "enumValue": command.id,
                }

        for command in commands[1:]:
            text += "		, %(enumName)s = %(enumValue)s\n" % {
                "enumName": command.options["cpp_enum_name"].value,
                "enumValue": command.id,
                }


        text += "\n		, Command_Count = %(commandCount)s // Number of commands in this service\n" % {
                "commandCount": len(commands),
                }

        text += self.indent(render("""
            };
            """))

        text += "\n" + self.indent(render("""
            // This class contains the descriptors or all commands of this service
            class Descriptors
                : public %(descriptorClass)s
            {
            public:
            \tDescriptors();
            \t~Descriptors();

            \tOP_STATUS Construct();
                """, {
                "class": service.options["cpp_class"].value,
                "descriptorClass": self.package.cpp.descriptorClass().symbol(),
            })) + "\n\n"

        text += "\n"

        text += ("		// List of commands\n" +
                 "		OpScopeCommand command_list[Command_Count];\n" +
                 "	};\n\n")

        if generate_instance:
            text += "\n// Service API implementation: BEGIN\n\n"
            text += self.renderServiceDeclaration(self.service, self.requests, self.events) + "\n\n"
            text += "\n// Service API implementation: END\n"

        text += "\n};\n\n"

        text += "\n"
        return text

    def renderServiceDeclaration(self, service, requests, events):
        enums = self.enums()

        text = "\n" + self.indent(render("""
            	virtual OP_STATUS             DoReceive( OpScopeClient *client, const OpScopeTPMessage &msg );
            	virtual int                   GetCommandCount() const;
            	virtual CommandRange          GetCommands() const;

            	virtual const OpProtobufMessage *GetMessage(unsigned int message_id) const;
            	virtual unsigned                 GetMessageCount() const;
            	virtual MessageIDRange           GetMessageIDs() const;
            	virtual MessageRange             GetMessages() const;
            """)) + "\n\n"

        if enums:
            text += self.indent(render("""
                // Introspection of enums
                virtual EnumIDRange GetEnumIDs() const;
                virtual unsigned  GetEnumCount() const;
                virtual BOOL      HasEnum(unsigned enum_id) const;
                virtual OP_STATUS GetEnum(unsigned enum_id, const uni_char *&name, unsigned &value_count) const;
                virtual OP_STATUS GetEnumValue(unsigned enum_id, unsigned idx, const uni_char *&value_name, unsigned &value_number) const;
                """)) + "\n\n"

        text += self.indent(render("""
            virtual const char *GetVersionString() const;
            virtual int         GetMajorVersion() const;
            virtual int         GetMinorVersion() const;
            virtual const char *GetPatchVersion() const;
            """)) + "\n\n"

        if requests:
            text += "	// Request/Response functions\n"
            for command in requests:
                text += "	%(function)s\n" % {"function": request_functionSignature(command)}
            text += "\n"

        if events:
            text += "	// Event functions\n"
            for command in events:
                text += "	%(function)s\n" % {"function": event_functionSignature(command)}
            text += "\n"

        text += "\n	Descriptors *GetDescriptors() const;\n"
        return text

    @memoize
    def getInterfaceImplementation(self):
        return self.renderInterfaceImplementation(self.service, self.impl_includes, self.commands)

    def renderRequestDecode(self, command):
        text = "\n		OP_STATUS status = OpStatus::OK;\n"

        if requiresAsyncResponse(command):
            text += self.indent(render("""
                unsigned async_tag;
                status = InitAsyncCommand(msg, async_tag); // Needed for async requests, the response will use this info when request is done
                if (OpStatus::IsError(status))
                {
                	RETURN_IF_ERROR(SetCommandError(OpScopeTPHeader::InternalError, GetInitAsyncCommandFailedText()));
                	return status;
                }
                """), 2) + "\n"

        if command.message.isNonEmpty():
            text += "		%(requestMessage)s in;\n" % {"requestMessage": message_className(command.message)}

        if hasattr(command, 'response') and command.response.isNonEmpty():
            text += "		%(responseMessage)s out;\n" % {"responseMessage": message_className(command.response)}
        text += "\n"

        if command.message.isNonEmpty():
            text += self.indent(render("""
                OpProtobufInstanceProxy in_proxy(%(requestMessage)s::GetMessageDescriptor(GetDescriptors()), &in);
                if (in_proxy.GetProtoMessage() == NULL)
                	return OpStatus::ERR_NO_MEMORY;
                status = ParseMessage(client, msg, in_proxy);
                if (OpStatus::IsError(status))
                {
                	if (GetCommandError().GetStatus() == OpScopeTPHeader::OK) // Set a generic error if none was set by a parser
                		RETURN_IF_ERROR(SetCommandError(OpScopeTPHeader::InternalError, GetParseCommandMessageFailedText()));
                	return status;
                }
                """, {
                    "requestMessage": message_className(command.message),
                }), 2) + "\n\n"


        text += self.indent(render("""
            status = %(commandCall)s;
            if (OpStatus::IsError(status))
            {
            	if (GetCommandError().GetStatus() == OpScopeTPHeader::OK) // Set a generic error if none was set by command
            """) % {
                "commandCall": request_functionCall(command),
            }, 2) + "\n"

        if requiresAsyncResponse(command):
            text += self.indent(render("""
                {
                	OP_STATUS tmp_status = SetCommandError(OpScopeTPHeader::InternalError, GetCommandExecutionFailedText());
                	OpStatus::Ignore(tmp_status);
                }
                // Remove the async command since the command failed to send an async response
                CleanupAsyncCommand(async_tag);
            """), 3) + "\n"
        else:
            text += "			RETURN_IF_ERROR(SetCommandError(OpScopeTPHeader::InternalError, GetCommandExecutionFailedText()));\n"

        text += ("			return status;\n" +
                 "		}\n")

        if hasattr(command, 'response') and command.response.isNonEmpty():
            text += "\n" + self.indent(render("""
                OpProtobufInstanceProxy out_proxy(%(responseMessage)s::GetMessageDescriptor(GetDescriptors()), &out);
                if (out_proxy.GetProtoMessage() == NULL)
                	return OpStatus::ERR_NO_MEMORY;
                RETURN_IF_ERROR(SendResponse(client, msg, out_proxy));
            """, {
                "responseMessage": message_className(command.response),
            }), 2) + "\n"
        elif not requiresAsyncResponse(command):
            text += "\n" + self.indent(render("""
                // The request does not define a response message so we send the default response which is an empty message
                OpProtobufInstanceProxy out_proxy(OpScopeDefaultMessage::GetMessageDescriptor(), &g_scope_manager->default_message_instance);
                if (out_proxy.GetProtoMessage() == NULL)
                	return OpStatus::ERR_NO_MEMORY;
                status = SendResponse(client, msg, out_proxy);
                if (OpStatus::IsError(status))
                {
                	if (!IsResponseSent() && GetCommandError().GetStatus() == OpScopeTPHeader::OK) // Set a generic error if response sending failed or no error was set by response code
                		RETURN_IF_ERROR(SetCommandError(OpScopeTPHeader::InternalError, GetCommandResponseFailedText()));
                	return status;
                }
            """, {
            }), 2) + "\n"
        return text

    def renderMessageIdentifier(self, message):
        if message and message != defaultMessage:
            text = "id_list[" + self.messageIdentifier(message) + "]"
        else:
            text = "0"
        return text

    def renderMessageID(self, message):
        if message and message != defaultMessage:
            text = "_gen_MsgID_%s" % self.varPath(message.absPath())
        else:
            text = "_gen_void_MsgID"
        return text


    def renderInterfaceImplementation(self, service, includes, commands):
        messages = [msg for msg in self.sortedMessages() if msg != defaultMessage]
        all_messages = [msg for msg in self.sortedMessages(with_inline=True) if msg != defaultMessage]
        enums = self.enums()

        text = "\n"
        for include in includes:
            text += "#include %(include)s\n" % {"include": strQuote(include)}

        text += "\n" + render("""
            // BEGIN: %(class)s_SI::Descriptors

            %(class)s_SI::Descriptors::Descriptors()
            {
            """, {
                "class": service.options["cpp_class"].value,
            }) + "\n"

        text += render("""
            }

            %(class)s_SI::Descriptors::~Descriptors()
            {
            """, {
                "class": service.options["cpp_class"].value,
            }) + "\n"

        text += render("""
            }

            OP_STATUS
            %(class)s_SI::Descriptors::Construct()
            {
                RETURN_IF_ERROR(%(descriptorClass)s::Construct());
            """, {
                "class": service.options["cpp_class"].value,
                "descriptorClass": self.package.cpp.descriptorClass().symbol(),
            }) + "\n"

        text += "	// Initialize commands\n"

        for idx, command in enumerate(commands):
            text += self.indent(render("""
                command_list[%(idx)s] = OpScopeCommand(%(command)s,
                					%(commandEnum)s
                """, {
                    "idx": idx,
                    "command": strQuote(command.name),
                    "commandEnum": enumName(command),
                }))
            if isinstance(command, proto.Event):
                text += ",\n						OpScopeCommand::Type_Event"
            else:
                text += ",\n						OpScopeCommand::Type_Call"

            if isinstance(command, proto.Event):
                text += render("""
                    ,
                    						_gen_void_MsgID,
                    						%(requestIdentifier)s
                    """, {
                        "requestIdentifier": self.renderMessageID(command.message),
                    })
            else:
                text += render("""
                    ,
                    						%(requestIdentifier)s,
                    						%(responseIdentifier)s
                    """, {
                        "requestIdentifier": self.renderMessageID(command.message),
                        "responseIdentifier": self.renderMessageID(command.response),
                    })
            text += "\n						);\n"
        text += "	" + render("""
            	return OpStatus::OK;
            }

            // END: %(class)s_SI::Descriptors
            """, {
                "class": service.options["cpp_class"].value,
            })

        return text

    @memoize
    def getServiceImplementation(self, generate_instance=True):
        return self.renderServiceImplementation(self.service, self.requests, self.events, generate_instance)

    def renderServiceImplementation(self, service, requests, events, generate_instance):
        class_name = service.options["cpp_class"].value + "_SI"

        class_name_base = service.iparent.cpp.cppMessageSet().symbol()
        if class_name_base.find("_MessageSet"):
            cpp_descriptor_class_name = class_name_base.replace("_MessageSet", "_Descriptors")
        else:
            cpp_descriptor_class_name = class_name_base + "_Descriptors"

        text = "\n"

        text += "\n" + render("""
            // Service implementation

            %(class)s::%(class)s(const uni_char* id, OpScopeServiceManager *manager, ControlType control)
            		: OpScopeService(id == NULL ? UNI_L(%(protocol_name_quoted)s) : id, manager, control)
            {
            }

            /*virtual*/
            %(class)s::~%(class)s()
            {
            }
            """, {
                "class": class_name,
                "protocol_name_quoted": strQuote(utils.join_dashed(utils.split_camelcase(service.name))),
                "serviceVarName": self.varName(service.name),
            }) + "\n"
        if not generate_instance:
            return text

        text += "\n" + render("""
            /*virtual*/
            int
            %(class)s::GetCommandCount() const
            {
            	return Command_Count;
            }

            /*virtual*/
            OpScopeService::CommandRange
            %(class)s::GetCommands() const
            {
            	Descriptors *descriptors = g_scope_manager->GetDescriptorSet().%(serviceVarName)s;
            	OP_ASSERT(descriptors);
            	return CommandRange(descriptors ? descriptors->command_list : NULL, Command_Count);
            }

            /*virtual*/
            OP_STATUS
            %(class)s::DoReceive( OpScopeClient *client, const OpScopeTPMessage &msg )
            {
            	// Check for invalid request data
            	if (msg.Status() != OpScopeTPMessage::OK) // All calls must have status OK
            		return SetCommandError(OpScopeTPMessage::BadRequest, GetInvalidStatusFieldText());
            	if (msg.ServiceName().Compare(GetName()) != 0) // Make sure the service name matches the one in the message
            		return SetCommandError(OpScopeTPMessage::InternalError, GetIncorrectServiceText());
            """, {
                "class": class_name,
                "protocol_name_quoted": strQuote(utils.join_dashed(utils.split_camelcase(service.name))),
                "serviceVarName": self.varName(service.name),
            }) + "\n\n"

        if requests:
            text += self.indent(render("""
                if (msg.CommandID() == %(requestID)s) // %(requestName)s
                {
                """, {
                    "requestID": enumName(requests[0]),
                    "requestName": requests[0].name,
                })) + self.renderRequestDecode(requests[0]) + "	}\n"

            for command in requests[1:]:
                text += self.indent(render("""
                    else if (msg.CommandID() == %(requestID)s) // %(requestName)s
                    {
                    """, {
                        "requestID": enumName(command),
                        "requestName": command.name,
                    })) + self.renderRequestDecode(command) + "	}\n"

            text += self.indent(render("""
                else
                {
                	SetCommandError(OpScopeTPMessage::CommandNotFound, GetCommandNotFoundText());
                	return OpStatus::ERR;
                }
                """)) + "\n"

        text += "	return OpStatus::OK;\n}\n\n"

        messages = [msg for msg in service.messages() if msg != defaultMessage]
        enums = self.enums()

        text += render("""
            /*virtual*/
            const OpProtobufMessage *
            %(class)s::GetMessage(unsigned message_id) const
            {
            	Descriptors *descriptors = GetDescriptors();
            	if (!descriptors)
            		return NULL;
            	return descriptors->Get(message_id);
            }

            %(class)s::Descriptors *
            %(class)s::GetDescriptors() const
            {
            	Descriptors *d = g_scope_manager->GetDescriptorSet().%(serviceVarName)s;
            	OP_ASSERT(d);
            	return d;
            }

            /*virtual*/
            unsigned
            %(class)s::GetMessageCount() const
            {
            	return %(descriptorClass)s::_gen_Message_Count;
            }

            /*virtual*/
            OpScopeService::MessageIDRange
            %(class)s::GetMessageIDs() const
            {
            	unsigned start = %(descriptorClass)s::_gen_Message_Count > 0 ? 1 : 0;
            	return MessageIDRange(start, start + %(descriptorClass)s::_gen_Message_Count);
            }

            /*virtual*/
            OpScopeService::MessageRange
            %(class)s::GetMessages() const
            {
            	unsigned start = %(descriptorClass)s::_gen_Message_Count > 0 ? 1 : 0;
            	return MessageRange(this, start, start + %(descriptorClass)s::_gen_Message_Count);
            }
            """, {
                "class": class_name,
                "serviceVarName": self.varName(service.name),
                "descriptorClass": cpp_descriptor_class_name,
            }) + "\n\n"

        text += render("""
            /* virtual */
            const char *
            %(class)s::GetVersionString() const
            {
            	return "%(version)s";
            }

            /* virtual */
            int
            %(class)s::GetMajorVersion() const
            {
            	return %(versionMajor)s;
            }

            /* virtual */
            int
            %(class)s::GetMinorVersion() const
            {
            	return %(versionMinor)s;
            }

            /* virtual */
            const char *
            %(class)s::GetPatchVersion() const
            {
            	return "%(versionPatch)s";
            }
            """, {
                "class": class_name,
                "version": service.options["version"].value,
                "versionMajor": service.version[0],
                "versionMinor": service.version[1],
                "versionPatch": service.version[2],
            }) + "\n"

        if events:
            for event in events:
                params = []
                if event.message.isNonEmpty():
                    params.append("const %s &msg" % message_className(event.message))
                if requiresAsyncResponse(event):
                    params.append("unsigned int tag")

                text += "\n" + render("""
                    OP_STATUS
                    %(class)s::%(eventMethod)s(%(eventParams)s) // %(eventName)s
                    {
                    """, {
                        "class": class_name,
                        "eventMethod": event_functionName(event),
                        "eventParams": ", ".join(params),
                        "eventName": event.name,
                    }) + "\n"

                if event.message.isNonEmpty():
                    text += "	OpProtobufInstanceProxy proxy(%(messageClass)s::GetMessageDescriptor(GetDescriptors()), const_cast<%(messageClass)s *>(&msg));\n" % {
                            "messageClass": message_className(event.message),
                        }
                else:
                    text += "	OpProtobufInstanceProxy proxy(OpScopeDefaultMessage::GetMessageDescriptor(), &g_scope_manager->default_message_instance);\n"

                text += ("	if (proxy.GetProtoMessage() == NULL)\n" +
                         "		return OpStatus::ERR_NO_MEMORY;\n")

                if requiresAsyncResponse(event):
                    text += "	return SendAsyncResponse(tag, proxy, %(eventEnum)s);\n" % {"eventEnum": enumName(event)}
                else:
                    text += "	return SendEvent(proxy, %(eventEnum)s);\n" % {"eventEnum": enumName(event)}

                text += "}\n\n"

        if enums:
            text += render("""
                // Enum introspection: BEGIN

                /*virtual*/
                OpScopeService::EnumIDRange
                %(class)s::GetEnumIDs() const
                {
                	Descriptors *descriptors = GetDescriptors();
                	return EnumIDRange(descriptors ? descriptors->enum_id_list : NULL, %(descriptorClass)s::_gen_Enum_Count);
                }

                /*virtual*/
                unsigned
                %(class)s::GetEnumCount() const
                {
                	return %(descriptorClass)s::_gen_Enum_Count;
                }

                /*virtual*/
                BOOL
                %(class)s::HasEnum(unsigned enum_id) const
                {
                	switch (enum_id)
                	{
                """, {
                    "class": class_name,
                    "descriptorClass": cpp_descriptor_class_name,
                }) + "\n"
            for enum in enums:
                text += "		case %(descriptorClass)s::_gen_EnumID_%(enumPath)s:\n" % {
                    "enumPath": "_".join(enum.path()),
                    "descriptorClass": cpp_descriptor_class_name,
                    }

            text += ("			return TRUE;\n"
                     "	}\n"
                     "	return FALSE;\n"
                     "}\n"
                     "\n")
            text += render("""
                /*virtual*/
                OP_STATUS
                %(class)s::GetEnum(unsigned enum_id, const uni_char *&name, unsigned &value_count) const
                {
                	switch (enum_id)
                	{
                """, {
                    "class": class_name,
                }) + "\n"

            for enum in enums:
                text += self.indent(render("""
                    // %(messageClassPath)s
                    case %(descriptorClass)s::_gen_EnumID_%(enumPath)s:
                    	name = UNI_L(%(enum)s);
                    	value_count = %(descriptorClass)s::_gen_EnumValueCount_%(enumPath)s;
                    	return OpStatus::OK;
                    """, {
                        "messageClassPath": messageClassPath(enum)[1],
                        "enumPath": "_".join(enum.path()),
                        "enum": strQuote(enum.name),
                        "descriptorClass": cpp_descriptor_class_name,
                    }), 2) + "\n"

            text += ("	}\n"
                     "	return OpStatus::ERR_NO_SUCH_RESOURCE;\n"
                     "}\n"
                     "\n")
            text += render("""
                /*virtual*/
                OP_STATUS
                %(class)s::GetEnumValue(unsigned enum_id, unsigned idx, const uni_char *&value_name, unsigned &value_number) const
                {
                	switch (enum_id)
                	{
                """, {
                    "class": class_name,
                }) + "\n"

            for enum in enums:
                text += self.indent(render("""
                    // %(messageClassPath)s
                    case %(descriptorClass)s::_gen_EnumID_%(enumPath)s:
                    {
                    """, {
                        "messageClassPath": messageClassPath(enum)[1],
                        "enumPath": "_".join(enum.path()),
                        "descriptorClass": cpp_descriptor_class_name,
                    }), 2) + "\n"
                if enum.values:
                    text += self.indent(render("""
                        if (idx > %(descriptorClass)s::_gen_EnumValueCount_%(enumPath)s)
                        	return OpStatus::ERR_OUT_OF_RANGE;
                        // Values for %(messageClassPath)s
                        const unsigned enum_numbers[] = {
                        	  %(enumValue)s
                        """, {
                            "enumPath": "_".join(enum.path()),
                            "messageClassPath": messageClassPath(enum)[1],
                            "enumValue": enum.values[0].value,
                            "descriptorClass": cpp_descriptor_class_name,
                        }), 3) + "\n"
                    for value in enum.values[1:]:
                        text += "				, %s\n" % value.value

                    text += self.indent(render("""
                        }; // ARRAY OK 2010-04-14 jborsodi
                        const unsigned name_offsets[] = {
                        	  0
                        """), 3) + "\n"
                    for idx in range(1, len(enum.values)):
                        text += "				, %(offset)s\n" % {"offset": sum([len(value.name) + 1 for value in enum.values[:idx]])}

                    text += self.indent(render("""
                        }; // ARRAY OK 2010-04-14 jborsodi
                        const uni_char *names = UNI_L( %(enumStrings)s );
                        OP_ASSERT(idx < sizeof(name_offsets));
                        OP_ASSERT(idx < sizeof(enum_numbers));
                        value_name = names + name_offsets[idx];
                        value_number = enum_numbers[idx];
                        return OpStatus::OK;
                        """, {
                            "enumStrings": strQuote("".join([value.name + "\0" for value in enum.values[:-1]] + [enum.values[-1].name]))
                        }), 3) + "\n"
                else:
                    text += "			return OpStatus::ERR; // No values so return error\n"

                text += "		}\n\n"

            text += ("	}\n"
                     "	return OpStatus::ERR_NO_SUCH_RESOURCE;\n"
                     "}\n"
                     "// Enum introspection: END\n")
        return text

class CppScopeServiceBase(CppScopeBase):
    cppService = None
    service = None
    define = None
    classPath = None
    includes = {}

    def __init__(self, file_path, cpp_service, dependencies=None):
        self.cppService = cpp_service
        self.service = cpp_service.service
        self.define = cpp_service.define
        self.defines = cpp_service.defines
        self.classPath = cpp_service.classPath
        self.includes = cpp_service.includes
        dependencies = DependencyList(set([self.service.iparent.filename]) | set(dependencies or []))
        CppScopeBase.__init__(self, file_path, dependencies, opera_root=cpp_service.operaRoot)

    def createFile(self):
        return CppScopeServiceBase._createFile(self, build_module=self.cppService.buildModule, package=self.cppService.package)

class CppServiceInterfaceImplementation(TemplatedGenerator, CppScopeServiceBase):
    """
    C++ code generator for the implementation of the service interface.

    TODO: This code uses the old generate() API, it should be updated to use the new generateFiles() API, see CppServiceInterfaceDeclaration for an example.
    """
    def __init__(self, file_path, cpp_service, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppScopeServiceBase.__init__(self, file_path=file_path, cpp_service=cpp_service, dependencies=dependencies)

    def generate(self):
        text = fileHeader()
        text += "\n" + render("""
            #include "core/pch.h"

            %(defines)s

            #include "%(interfaceInclude)s"

            """, {
                "defines": self.renderDefines(self.defines),
                "interfaceInclude": cpp_path_norm(self.includes["interfaceDeclaration"]),
                "serviceStr": strQuote(self.service.name),
            }) + "\n"

        className = self.service.iparent.cpp.cppMessageSet().name
        set_gen = CppMessageSetGenerator(self.service.iparent, serviceMessages2(self.service), className, self.operaRoot)
        text += set_gen.renderDescriptorImplementation()
        text += set_gen.getImplementation()

        gen = CppServiceGenerator(self.service)
        text += gen.getInterfaceImplementation()

        text += "\n// Service implementation: BEGIN\n"
        text += gen.getServiceImplementation(generate_instance=self.cppService.generateInstance)
        text += "\n// Service implementation: END\n\n"

        text += render("""

            %(endifs)s
            """, {
                "endifs": self.renderEndifs(self.defines),
            }) + "\n"

        return text

class CppServiceInterfaceDeclaration(TemplatedGenerator, CppScopeServiceBase):
    def __init__(self, file_path, cpp_service, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppScopeServiceBase.__init__(self, file_path=file_path, cpp_service=cpp_service, dependencies=dependencies)

    def generateFiles(self):
        package = self.service.iparent
        proto_file = self.makeRelativePath(package.filename)

        def service(generated_file):
            includes = set(["modules/scope/src/scope_service.h"])

            text = ""
            gen = CppServiceGenerator(self.service)
            text += gen.getInterfaceIncludes()

            className = self.service.iparent.cpp.cppMessageSet().name
            set_gen = CppMessageSetGenerator(package, serviceMessages2(self.service), className, self.operaRoot)

            text += set_gen.getInterface()

            text += set_gen.renderDescriptorDeclaration()

            text += gen.getInterface(generate_instance=self.cppService.generateInstance)

            # Bring in includes needed by types or other generated code
            type_includes = set()
            for msgGen in list(set_gen.messageGenerators(with_inline=True)):
                type_includes |= msgGen.type_decl_includes
            includes |= type_includes
            includes = list(includes)
            includes.sort()

            return self.renderFile(generated_file.path, text, includes=includes, defines=self.defines, proto_file=proto_file)


        return [GeneratedFile(self.filePath, build_module=self.cppService.buildModule, dependencies=self.dependencies, callback=service),
               ]

class CppScopeService(object):
    """Stores common settings for a scope service used by generators
    """
    root = None
    service = None
    define = None
    classPath = None
    includes = {}
    generateInstance = True

    def __init__(self, package, service, build_module, opera_root, root, includes={}, class_path=None, generate_instance=True):
        self.package = package
        self.service = service
        self.buildModule = build_module
        self.operaRoot = opera_root
        self.root = root
        self.includes = includes.copy()
        if "cpp_feature" in service.options:
            self.define = service.options["cpp_feature"].value
        else:
            self.define = None
        if "cpp_defines" in service.options:
            self.defines = list(map(str.strip, service.options["cpp_defines"].value.split(",")))
        else:
            self.defines = []
        if self.define:
            self.defines.insert(0, self.define)
        self.classPath = class_path or ""
        self.generateInstance = generate_instance

class CppScopeServiceGenerator(object):
    """Main class for generating cpp/h files from service descriptors.
    It checks options on the service and returns specific generators needed for that service.

    @param opera_root The root of source code tree.
    @param module_path The relative path of the module, relative to opera_root.
    @param service The service object to generate files for.
    @param dependencies Optional list of files the all generated files depends on.
                        Any iterable type can be used, must contain strings.

    The actual generators is returned in generators().
    """
    service = None

    def __init__(self, opera_root, build_module, module_path, package, service, dependencies=None):
        self.service = service
        self.package = package
        self.moduleType = "modules"
        self.module = "scope"
        self.operaRoot = opera_root
        self.buildModule = build_module
        self.modulePath = module_path
        self.basePath = os.path.join(module_path, "src", "generated")
        self.root = os.path.join(self.operaRoot, self.basePath)
        self.dependencies = dependencies

    def generators(self):
        """Iterator over generator objects for the current service, may be empty."""

        files = self.service.cpp.files()
        includes = {
            "interfaceImplementation": files["generatedServiceImplementation"],
            "interfaceDeclaration": files["generatedServiceDeclaration"],
        }
        generate_service = True
        generate_service_interface = True
        if "cpp_instance" in self.service.options:
            generate_service = self.service.options["cpp_instance"].value == "true"
        if "cpp_generate" in self.service.options and self.service.options["cpp_generate"].value != "true":
            return

        cppService = CppScopeService(self.package, self.service, self.buildModule, self.operaRoot, self.root, includes, generate_instance=generate_service)
        if generate_service_interface:
            yield CppServiceInterfaceImplementation(os.path.join(self.operaRoot, includes["interfaceImplementation"]), cppService, dependencies=self.dependencies)
            yield CppServiceInterfaceDeclaration(os.path.join(self.operaRoot, includes["interfaceDeclaration"]), cppService, dependencies=self.dependencies)
