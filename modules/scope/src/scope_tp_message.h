/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_SCOPE_TRANSPORT_MESSAGE_H
#define OP_SCOPE_TRANSPORT_MESSAGE_H

class ByteBuffer;
class ES_Object;

class OpScopeTPHeader : public Link
{
public:
	enum Field
	{
		  Field_ServiceName = 1
		, Field_CommandID = 2
		, Field_Type = 3
		, Field_Status = 4
		, Field_Tag = 5
		, Field_Payload = 8
	};
	enum STPType
	{
		STP_Call = 1,
		STP_Response = 2,
		STP_Event = 3,
		STP_Error = 4
	};
	enum MessageType
	{
		None = -1,
		ECMAScript = -2,

		ProtocolBuffer = 0,
		JSON = 1,
		XML = 2,

		MessageTypeMin = 0,
		MessageTypeMax = 3,
		MessageTypeCount = MessageTypeMax - MessageTypeMin + 1
	};
	enum MessageStatus
	{
		OK = 0
		, BadRequest = 3
		, InternalError = 4
		, CommandNotFound = 5
		, ServiceNotFound = 6
		, OutOfMemory = 7
		, ServiceNotEnabled = 8
		, ServiceAlreadyEnabled = 9

		, MessageStatusMin = 0
		, MessageStatusMax = 9
	};
	enum MessageVersion
	{
		Version_0 = 0, // STP/0
		Version_1 = 1,  // STP/1
		DefaultVersion = Version_1
	};
	inline OpScopeTPHeader(STPType stype, OpString &service_name, unsigned int command_id,
		MessageType type, MessageStatus status, unsigned int tag, MessageVersion version);
	inline OpScopeTPHeader(STPType stype, const uni_char *service_name, unsigned int command_id,
		MessageType type, MessageStatus status, unsigned int tag, MessageVersion version);
	OpScopeTPHeader()
		: stp_type(STP_Call)
		, service_name()
		, command_id(0)
		, type(None)
		, status(OK)
		, tag(0)
		, version(DefaultVersion) {}
	inline OP_STATUS Copy(const OpScopeTPHeader &msg);
	inline void Clear();

	STPType TransportType() const { return stp_type; }
	const OpString &ServiceName() const { return service_name; }
	OpString &ServiceName() { return service_name; }
	unsigned int  CommandID() const { return command_id; }
	MessageType   Type() const { return type; }
	MessageStatus Status() const { return status; }
	unsigned int  Tag() const { return tag; }
	MessageVersion Version() const { return version; }

	void SetTransportType(STPType t) { stp_type = t; }
	OP_STATUS SetServiceName(const OpString &service) { return service_name.Set(service); }
	OP_STATUS SetServiceName(const uni_char *service) { return service_name.Set(service); }
	void SetCommandID(unsigned int c) { command_id = c; }
	void SetStatus(MessageStatus s) { status = s; }
	void SetVersion(MessageVersion ver) { version = ver; }
	void SetTag(unsigned int t) { tag = t; }

protected:
	void SetType(MessageType t) { type = t; }
	OP_STATUS SetType(int t);

private:
	STPType       stp_type;
	OpString      service_name;
	unsigned int  command_id;
	MessageType   type;
	MessageStatus status;
	unsigned int  tag;
	MessageVersion version;

	// Disable copy-constructor and assignment operator
	OpScopeTPHeader(const OpScopeTPHeader &msg);
	OpScopeTPHeader &operator =(const OpScopeTPHeader &msg);
};

OpScopeTPHeader::OpScopeTPHeader(STPType stype, OpString &service_name, unsigned int command_id,
							     MessageType type, MessageStatus status, unsigned int tag, MessageVersion version)
								 : stp_type(stype)
								 , service_name(service_name)
								 , command_id(command_id)
								 , type(type)
								 , status(status)
								 , tag(tag)
								 , version(version)
{
}

OpScopeTPHeader::OpScopeTPHeader(STPType stype, const uni_char *service_name, unsigned int command_id,
							     MessageType type, MessageStatus status, unsigned int tag, MessageVersion version)
								 : stp_type(stype)
								 , service_name()
								 , command_id(command_id)
								 , type(type)
								 , status(status)
								 , tag(tag)
								 , version(version)
{
	this->service_name.SetL(service_name);
}

void
OpScopeTPHeader::Clear()
{
	stp_type = STP_Call;
	service_name.Empty();
	command_id = 0;
	type = ProtocolBuffer;
	status = OK;
	tag = 0;
	version = DefaultVersion;
}

OP_STATUS
OpScopeTPHeader::Copy(const OpScopeTPHeader &msg)
{
	stp_type = msg.stp_type;
	RETURN_IF_ERROR(service_name.Set(msg.service_name));
	command_id = msg.command_id;
	type = msg.type;
	status = msg.status;
	tag = msg.tag;
	version = msg.version;
	return OpStatus::OK;
}

class OpScopeTPMessage : public OpScopeTPHeader
{
public:
	inline OpScopeTPMessage(STPType stype, const OpString &service_name, unsigned int command_id,
							MessageStatus status, unsigned int tag, MessageVersion version);
	inline OpScopeTPMessage(STPType stype, const uni_char *service_name, unsigned int command_id,
							MessageStatus status, unsigned int tag, MessageVersion version);
	OpScopeTPMessage() : OpScopeTPHeader(), data(NULL), es_object(NULL), es_runtime(NULL) {}
	OP_STATUS Construct(const ByteBuffer &msg, MessageType type);
	virtual ~OpScopeTPMessage();

	void Clear();
	/**
	 * Frees the current byte buffer or ECMAScript object
	 * and sets the current type to None.
	 */
	OP_STATUS Free();

	OP_STATUS Copy(const OpScopeTPMessage &msg, BOOL include_data = TRUE);
	OP_STATUS Copy(const OpScopeTPHeader &hdr);
	static OP_STATUS Clone(OpAutoPtr<OpScopeTPMessage> &out, const OpScopeTPMessage &in, BOOL include_data = TRUE);
	static OP_STATUS Clone(OpAutoPtr<OpScopeTPMessage> &out, const OpScopeTPHeader &in);

	ByteBuffer   *Data();
	const ByteBuffer *Data() const;

	OP_STATUS SetData(const ByteBuffer &msg, MessageType type);
	/**
	 * Creates a new (empty) byte buffer and sets the current type to @a type.
	 * The new byte buffer can be accessed with Data() afterwards.
	 */
	OP_STATUS CreateEmptyData(MessageType type);

	ES_Object *GetESObject() const;
	ES_Runtime *GetESRuntime() const;
	/**
	 * Unprotects the ECMAScript object and sets the type to @c None.
	 * @returns The ECMASCript object or NULL if it does not contain an object
	 */
	ES_Object *ReleaseESObject();
	/**
	 * Sets an ECMAScript object as the current payload, also updates type accordingly.
	 * @note The ES object will be protected from garbage collection.
	 */
	OP_STATUS SetESObject(ES_Object *obj, ES_Runtime *runtime);

private:
	ByteBuffer *data;      // type == ProtocolBuffer, JSON, XML
	ES_Object  *es_object; // type == ECMAScript
	ES_Runtime *es_runtime;// type == ECMAScript

	// Disable copy-constructor and assignment operator
	OpScopeTPMessage &operator =(const OpScopeTPMessage &msg);
	OpScopeTPMessage(const OpScopeTPHeader &msg);
};

OpScopeTPMessage::OpScopeTPMessage(STPType stype, const OpString &service_name, unsigned int command_id,
								   MessageStatus status, unsigned int tag, MessageVersion version)
	: OpScopeTPHeader(stype, service_name, command_id, None, status, tag, version)
	, data(NULL)
	, es_object(NULL)
	, es_runtime(NULL)
{
}

OpScopeTPMessage::OpScopeTPMessage(STPType stype, const uni_char *service_name, unsigned int command_id,
								   MessageStatus status, unsigned int tag, MessageVersion version)
	: OpScopeTPHeader(stype, service_name, command_id, None, status, tag, version)
	, data(NULL)
	, es_object(NULL)
	, es_runtime(NULL)
{
}

class OpScopeTPError
{
public:
	explicit OpScopeTPError()
		: status(OpScopeTPHeader::OK), static_description(NULL), line(-1), col(-1), offset(-1)
	{}
	explicit OpScopeTPError(OpScopeTPHeader::MessageStatus status, const uni_char *description = NULL, int line = -1, int col = -1, int offset = -1)
		: status(status), static_description(description), line(line), col(col), offset(offset)
	{}

	OP_STATUS Copy(const OpScopeTPError &error);

	/**
	 * Format an error description using printf-style formatting.
	 *
	 * Examples:
	 *
	 *  SetFormattedDescription(UNI_L("Invalid object ID: %i"), 1337);
	 *  SetFormattedDescription(UNI_L("Invalid name: %s"), UNI_L("Somename"));
	 *
	 * @param format The formatting string.
	 * @param ... Formatting parameters.
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS SetFormattedDescription(const uni_char *format, ...);

	OpScopeTPHeader::MessageStatus GetStatus() const { return status; }
	const uni_char *GetDescription() const { return static_description == NULL ? description.CStr() : static_description; }
	int GetLine() const { return line; }
	int GetColumn() const { return col; }
	int GetOffset() const { return offset; }

	void SetStatus(OpScopeTPHeader::MessageStatus s) { status = s; }
	void SetStaticDescription(const uni_char *description) { static_description = description; }
	OP_STATUS SetDescription(const uni_char *description_) { return description.Set(description_); }
	OP_STATUS SetDescription(const OpString &description_) { return description.Set(description_); }
	void SetLine(int line_) { line = line_; }
	void SetColumn(int column_) { col = column_; }
	void SetOffset(int offset_) { offset = offset_; }

private:
	OpScopeTPHeader::MessageStatus status;
	const uni_char *static_description;
	OpString        description;
	int line;
	int col;
	int offset;
};

class OpScopeCommand
{
public:
	enum CommandType
	{
		Type_None  = 0,
		Type_Call  = 1,
		Type_Event = 2
	};
	OpScopeCommand()
		: name(NULL)
		, number(-1)
		, type(Type_None)
	{}
	OpScopeCommand(const char *name, int number, CommandType type, unsigned message_id, unsigned response_id)
		: name(name)
		, number(number)
		, message_id(message_id)
		, response_id(response_id)
		, type(type)
	{}

	const char *name;
	int         number;
	unsigned    message_id;
	unsigned    response_id;
	CommandType type;
};

#endif // OP_SCOPE_NETWORK_H
