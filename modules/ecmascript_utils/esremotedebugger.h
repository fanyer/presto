/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESREMOTEDEBUGGER_H
#define ES_UTILS_ESREMOTEDEBUGGER_H

#ifdef ECMASCRIPT_REMOTE_DEBUGGER

#include "modules/ecmascript_utils/esdebugger.h"

#include "modules/pi/network/OpSocketAddress.h"
#include "modules/pi/network/OpSocket.h"

#include "modules/util/simset.h"
#include "modules/util/tempbuf.h"

class ES_DebugMessagePart
	: public Link
{
public:
	enum Type
	{
		/* BODY_* are the main message's body part.  Never more than one per
		   message and always the first part. */
		BODY_HELLO,
		BODY_SET_CONFIGURATION,
		BODY_NEW_SCRIPT,
		BODY_THREAD_STARTED,
		BODY_THREAD_FINISHED,
		BODY_STOPPED_AT,
		BODY_CONTINUE,
		BODY_EVAL,
		BODY_EVAL_REPLY,
		BODY_EXAMINE,
		BODY_EXAMINE_REPLY,
		BODY_BACKTRACE,
		BODY_BACKTRACE_REPLY,
		BODY_ADD_BREAKPOINT,
		BODY_REMOVE_BREAKPOINT,
		BODY_BREAK,

		/* AUX_* are auxiliary parts.  Varying number per message and never
		   the first part.  Not all message types can have auxiliary parts
		   and not all auxiliary part types are allowed in every message
		   that can have auxiliary parts. */
		AUXILIARY_OBJECT_INFO,
		AUXILIARY_WATCH_UPDATE,
		AUXILIARY_STACK,
		AUXILIARY_BREAKPOINT_TRIGGERED,
		AUXILIARY_RETURN_VALUE,
		AUXILIARY_EXCEPTION_THROWN,
		AUXILIARY_EVAL_VARIABLE,
		AUXILIARY_RUNTIME_INFORMATION,
		AUXILIARY_HEAP_STATISTICS
	};

	enum { REQUIRED_BUFFER_SIZE = 64 };

	ES_DebugMessagePart(Type type);

	Type GetType();
	/**< Returns the message part type. */

	virtual ~ES_DebugMessagePart();

	virtual unsigned GetLength() = 0;
	/**< Number of items in message part. */

	virtual void GetItemData(unsigned index, const char *&data, unsigned &length) = 0;
	/**< If data==NULL, only return length.  If not, data should initially
	     point to a buffer at least REQUIRED_BUFFER_SIZE characters large. */

	virtual void ParseL(const char *&data, unsigned &size, unsigned &length) = 0;
	/**< Parse from 'data'.  Should be updated to reflect bytes and items
	     consumed for this message part. */

	static void SetString8L(char *&stored_data, unsigned &stored_length, const char *data, unsigned length = ~0u);
	/**< Store 8-bit string data.  Stored data will contain the header ("sXXXX"
	     or "SXXXXXXXX", but 'stored_data' will be set to point to after the
	     header.  GetItemDataString8() should be called to get a pointer to the
	     start of the header. */

	static void SetString16L(char *&stored_data, unsigned &stored_length, const uni_char *data, unsigned length = ~0u);
	/**< Store 16-bit string data.  Stored data will contain the header ("sXXXX"
	     or "SXXXXXXXX", but 'stored_data' will be set to point to after the
	     header.  GetItemDataString8() should be called to get a pointer to the
	     start of the header.  Data will be converted to utf-8. */

	static void SetValueL(ES_DebugValue &stored_value, const ES_DebugValue &value);

protected:
	/* The four next functions: the 'data' argument should point to a buffer
	   of at least REQUIRED_BUFFER_SIZE characters. */

	void GetItemDataBoolean(const char *data, unsigned &length, BOOL value);
	void GetItemDataInteger(const char *data, unsigned &length, int value);
	void GetItemDataUnsigned(const char *data, unsigned &length, unsigned value);
	void GetItemDataDouble(const char *data, unsigned &length, double value);

	void GetItemDataString(const char *&data, unsigned &length);
	/**< Moves 'data' so that it points to the start of the string header. */

	void GetItemDataValueType(const char *data, unsigned &length, ES_DebugValue &value);
	void GetItemDataValueValue(const char *&data, unsigned &length, ES_DebugValue &value);

	void FreeItemDataString(const char *data, unsigned length);

private:
	Type type;
};

class ES_DebugMessage
	: public Link
{
public:
	enum ProtocolVersion
	{
		PROTOCOL_VERSION = 3
	};

	enum Type
	{
		TYPE_HELLO = 1,
		TYPE_NEW_SCRIPT,
		TYPE_THREAD_STARTED,
		TYPE_THREAD_FINISHED,
		TYPE_STOPPED_AT,
		TYPE_CONTINUE,
		TYPE_EVAL,
		TYPE_EVAL_REPLY,
		TYPE_EXAMINE,
		TYPE_EXAMINE_REPLY,
		TYPE_ADD_BREAKPOINT,
		TYPE_REMOVE_BREAKPOINT,
		TYPE_SET_CONFIGURATION,
		TYPE_BACKTRACE,
		TYPE_BACKTRACE_REPLY,
		TYPE_BREAK
	};

	enum AuxiliaryType
	{
		AUXILIARY_OBJECT_INFO = 1,
		AUXILIARY_WATCH_UPDATE,
		AUXILIARY_STACK,
		AUXILIARY_BREAKPOINT_TRIGGERED,
		AUXILIARY_RETURN_VALUE,
		AUXILIARY_EXCEPTION_THROWN,
		AUXILIARY_EVAL_VARIABLE,
		AUXILIARY_RUNTIME_INFORMATION,
		AUXILIARY_HEAP_STATISTICS
	};

	enum ValueType
	{
		VALUE_TYPE_NULL      = 1,
		VALUE_TYPE_UNDEFINED,
		VALUE_TYPE_BOOLEAN,
		VALUE_TYPE_NUMBER,
		VALUE_TYPE_STRING,
		VALUE_TYPE_OBJECT
	};

	ES_DebugMessage();
	~ES_DebugMessage();

	unsigned GetSize();
	/**< Message size in bytes, not including the message header. */

	unsigned GetLength();
	/**< Number of items in the message. */

	ES_DebugMessagePart *GetFirstPart();

	void GetItemDataL(unsigned index, const char *&data, unsigned &length);
	/**< If data==NULL, only return length.  If not, data should initially
	     point to a buffer at least REQUIRED_BUFFER_SIZE characters large. */

	void ParseL(const char *data, unsigned size, unsigned length);
	/**< Parse from 'data'.  LEAVEs on OOM or if parsing failed. */

	void AddPart(ES_DebugMessagePart *part);

	void AddObjectInfoL(const ES_DebugObject &object);

	static void ParseBooleanL(const char *&data, unsigned &size, unsigned &length, BOOL &value);
	static void ParseIntegerL(const char *&data, unsigned &size, unsigned &length, int &value);
	static void ParseUnsignedL(const char *&data, unsigned &size, unsigned &length, unsigned &value);
	static void ParseDoubleL(const char *&data, unsigned &size, unsigned &length, double &value);
	static void ParseStringL(const char *&data, unsigned &size, unsigned &length, const char *&value, unsigned &value_length);
	static void ParseValueL(const char *&data, unsigned &size, unsigned &length, ES_DebugValue &value);

protected:
	Head parts;
	TempBuffer buffer;
};

class ES_RemoteDebugConnection
	: public OpSocketListener
{
public:
	ES_RemoteDebugConnection();
	~ES_RemoteDebugConnection();

	BOOL IsConnected();

	OP_STATUS Connect(const OpStringC &address, int port);
	OP_STATUS Listen(int port);
	OP_STATUS Close();
	void CloseL();

	// Call to send a message to the other side.
	void SendL(ES_DebugMessage *message);

	virtual OP_STATUS Connected() = 0;

	// Called when a message is received from the other side.
	virtual OP_STATUS Received(ES_DebugMessage *message) = 0;

	// Called when the socket is closed.
	virtual OP_STATUS Closed() = 0;

private:
	void SendMoreL();
	void ReceiveMoreL();
	BOOL ReceiveDataL();

#ifdef CORESOCKETACCOUNTING
	virtual void OnSocketInstanceNumber(int instance_nr);
#endif // CORESOCKETACCOUNTING

	virtual void OnSocketConnected(OpSocket* socket);
	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent);
	virtual void OnSocketClosed(OpSocket* socket);
#ifdef SOCKET_LISTEN_SUPPORT
	virtual void OnSocketConnectionRequest(OpSocket* socket);
#endif // SOCKET_LISTEN_SUPPORT
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketDataSendProgressUpdate(OpSocket* aSocket, unsigned int aBytesSent);

	OpSocketAddress *socketaddress;
	OpSocket *socket;

	enum { INITIAL, CONNECTING, LISTENING, CONNECTED, CLOSED } state;

	OpSocket::Error connect_error;

	Head send_queue;
	ES_DebugMessage *send_message;
	BOOL send_head, send_tail, is_sending;
	char send_buffer[ES_DebugMessagePart::REQUIRED_BUFFER_SIZE];
	unsigned send_item, send_length, send_offset;
	OpSocket::Error send_error;

	ES_DebugMessage *receive_message;
	char receive_buffer[12], *receive_message_buffer;
	BOOL receive_head, receive_tail;
	unsigned receive_buffer_used, receive_message_size, receive_message_received, receive_message_length, receive_item, receive_item_offset;
	OpSocket::Error receive_error;
};

class ES_RemoteDebugFrontend
	: public ES_DebugFrontend,
	  public ES_RemoteDebugConnection
{
public:
	ES_RemoteDebugFrontend();
	virtual ~ES_RemoteDebugFrontend();

	OP_STATUS Construct(BOOL active, const OpStringC &address, int port);

	virtual OP_STATUS NewScript(unsigned dbg_runtime_id, ES_DebugScriptData *data, const ES_DebugRuntimeInformation *runtimeinfo);
	virtual OP_STATUS ThreadStarted(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo);
	virtual OP_STATUS ThreadFinished(unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus status, ES_DebugReturnValue *dbg_return_value);
	virtual OP_STATUS StoppedAt(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data);
	virtual OP_STATUS BacktraceReply(unsigned tag, unsigned length, ES_DebugStackFrame *stack);
	virtual OP_STATUS EvalReply(unsigned tag, EvalStatus status, const ES_DebugValue &result);
	virtual OP_STATUS ExamineReply(unsigned tag, unsigned objects_count, ES_DebugObjectProperties *objects);

	virtual OP_STATUS Connected();
	virtual OP_STATUS Received(ES_DebugMessage *message);
	virtual OP_STATUS Closed();

private:
	void NewScriptL(unsigned dbg_runtime_id, ES_DebugScriptData *data, const ES_DebugRuntimeInformation *runtimeinfo);
	void ThreadStartedL(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo);
	void ThreadFinishedL(unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus status, ES_DebugReturnValue *dbg_return_value);
	void StoppedAtL(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data);
	void BacktraceReplyL(unsigned tag, unsigned length, ES_DebugStackFrame *stack);
	void EvalReplyL(unsigned tag, EvalStatus status, const ES_DebugValue &result);
	void ExamineReplyL(unsigned tag, unsigned objects_count, ES_DebugObjectProperties *objects);

	void SendHelloL();

	ES_DebugMessage *current_message;
	ES_DebugMessagePart *current_message_part;
};

class ES_RemoteDebugBackend
	: public ES_DebugBackend,
	  public ES_RemoteDebugConnection
{
public:
	ES_RemoteDebugBackend();
	virtual ~ES_RemoteDebugBackend();

	OP_STATUS Construct(ES_DebugFrontend *frontend, BOOL active, const OpStringC &address, int port);

	/* From ES_DebugBackend. */
	virtual OP_STATUS AttachToAll();
	virtual OP_STATUS AttachTo(ES_Runtime *runtime, BOOL debug_window);

	virtual OP_STATUS Continue(unsigned dbg_runtime_id, ES_DebugFrontend::Mode new_mode);
	virtual OP_STATUS Backtrace(unsigned tag, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned max_frames);
	virtual OP_STATUS Eval(unsigned tag, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned frame_index, const uni_char *string, unsigned string_length, ES_DebugVariable *variables, unsigned variables_count);
	virtual OP_STATUS AddBreakpoint(unsigned id, const ES_DebugBreakpointData &data);
	virtual OP_STATUS RemoveBreakpoint(unsigned id);
	virtual OP_STATUS GetStopAt(StopType stop_type, BOOL value);
	virtual OP_STATUS SetStopAt(StopType stop_type, BOOL &return_value);

	/* From ES_RemoteDebugConnection. */
	virtual OP_STATUS Connected();
	virtual OP_STATUS Received(ES_DebugMessage *message);
	virtual OP_STATUS Closed();

private:
};

#endif /* ECMASCRIPT_REMOTE_DEBUGGER */
#endif /* ES_UTILS_ESREMOTEDEBUGGER_H */
