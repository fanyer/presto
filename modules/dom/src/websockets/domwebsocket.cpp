/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Erik Moller <emoller@opera.com>, Niklas Beischer <no@opera.com>
**
*/
#include "core/pch.h"

#ifdef WEBSOCKETS_SUPPORT

#include "modules/dom/src/websockets/domwebsocket.h"

#include "modules/dom/src/domglobaldata.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/url/protocols/websocket.h"
#include "modules/url/protocols/websocket_protocol.h"
#include "modules/url/protocols/websocket_manager.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/domutils.h"
#include "modules/dom/src/domfile/domblob.h"
#include "modules/dom/src/domfile/domfile.h"

/* How often to resample bufferedAmount when draining outgoing data.
   In milliseconds. */
#define DOM_WEBSOCKETS_BUFFEREDAMOUNT_UPDATE_INTERVAL 50

#define WS_NEW_DBG OP_NEW_DBG(__FUNCTION__, "dom_websocket")

/**
 * ------------- DOM_WebSocket::DOM_WebSocketThreadListener -------------------
 */

DOM_WebSocket::DOM_WebSocketThreadListener::DOM_WebSocketThreadListener(DOM_WebSocket* ws, DOM_WebSocketThreadListenerLink *l)
	: webSocket(ws)
	, link(l)
{
}

/* virtual */ OP_STATUS
DOM_WebSocket::DOM_WebSocketThreadListener::Signal(ES_Thread *thread, ES_ThreadSignal signal)
{
	WS_NEW_DBG;

	// If the script has finished executing we can perform our change.
	if (signal == ES_SIGNAL_FINISHED || signal == ES_SIGNAL_FAILED || signal == ES_SIGNAL_CANCELLED)
	{
		// Stop listening.
		DOM_WebSocket *ws = webSocket;
		ES_ThreadListener::Remove();
		link->Out();
		OP_DELETE(link);
		ws->ProcessTaskQueue();
	}
	return OpStatus::OK;
}


/**
 * ------------- DOM_WebSocket::DOM_WebSocketThreadListenerLink -------------------
 */

DOM_WebSocket::DOM_WebSocketThreadListenerLink::DOM_WebSocketThreadListenerLink(DOM_WebSocket* ws, ES_Thread *th)
	: thread(th)
	, listener(ws, this)
{
}

DOM_WebSocket::DOM_WebSocketThreadListenerLink::~DOM_WebSocketThreadListenerLink()
{
	listener.Remove();
}

/**
 * --------------------------- DOM_WebSocket ----------------------------------
 */

DOM_WebSocket_Constructor::DOM_WebSocket_Constructor()
	: DOM_BuiltInConstructor(DOM_Runtime::WEBSOCKET_PROTOTYPE)
{
}

int
DOM_WebSocket_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	WS_NEW_DBG;
	BOOL was_converted = ES_CONVERSION_COMPLETED(argc);
	if (was_converted)
	{
		argc = ES_CONVERSION_ARGC(argc);
		OP_ASSERT(argc > 1 && argv[0].type == VALUE_STRING && (argv[1].type == VALUE_STRING || argv[1].type == VALUE_OBJECT));
	}

	DOM_CHECK_ARGUMENTS("s");
	OpAutoVector<OpString> protocols;

	// If a protocol parameter was passed
	if (argc > 1)
	{
		if (argv[1].type == VALUE_STRING)
		{
			const uni_char *string = argv[1].value.string;
			unsigned length = argv[1].GetStringLength();

			if (!DOM_Utils::IsValidTokenValue(string, length))
				return CallDOMException(SYNTAX_ERR, return_value);

			OpString *arg = OP_NEW(OpString, ());
			if (!arg)
				return ES_NO_MEMORY;
			if (OpStatus::IsError(arg->Set(string, length)) || OpStatus::IsError(protocols.Add(arg)))
			{
				OP_DELETE(arg);
				return ES_NO_MEMORY;
			}
		}
		else if (argv[1].type == VALUE_OBJECT && !was_converted)
		{
			ES_CONVERT_ARGUMENTS_AS(return_value, "s[s]-");
			return ES_NEEDS_CONVERSION;
		}
		else if (argv[1].type == VALUE_OBJECT)
		{
			ES_Value value;
			ES_Object *array = argv[1].value.object;
			unsigned protocols_count;
			if (!DOMGetArrayLength(array, protocols_count))
				return CallDOMException(SYNTAX_ERR, return_value);

			for (unsigned i = 0; i < protocols_count; i++)
			{
				OP_BOOLEAN result;
				CALL_FAILED_IF_ERROR(result = GetRuntime()->GetIndex(array, i, &value));
				if (result == OpBoolean::IS_FALSE)
					return CallDOMException(SYNTAX_ERR, return_value);
				else if (value.type == VALUE_STRING)
				{
					unsigned length = value.GetStringLength();
					if (!DOM_Utils::IsValidTokenValue(value.value.string, length))
						return CallDOMException(SYNTAX_ERR, return_value);

					for (unsigned j = 0; j < protocols.GetCount(); j++)
						if (uni_str_eq(protocols.Get(j)->CStr(), value.value.string))
							return CallDOMException(SYNTAX_ERR, return_value);

					OpString *arg = OP_NEW(OpString, ());
					if (!arg)
						return ES_NO_MEMORY;
					if (OpStatus::IsError(arg->Set(value.value.string)) || OpStatus::IsError(protocols.Add(arg)))
					{
						OP_DELETE(arg);
						return ES_NO_MEMORY;
					}
				}
				else
					return CallDOMException(SYNTAX_ERR, return_value);
			}
		}
		else
		{
			ES_CONVERT_ARGUMENTS_AS(return_value, "ss-");
			return ES_NEEDS_CONVERSION;
		}
	}

	// If the URL string contains any hash characters, throw a SYNTAX_ERR.
	// If any null characters are encountered replace them with a %00 sequence.
	OpString escaped;

	const uni_char *url_string = argv[0].value.string;
	unsigned url_string_length = argv[0].GetStringLength();

	for (unsigned i = 0; i < url_string_length; ++i, ++url_string)
		if (*url_string == '#')
			return CallDOMException(SYNTAX_ERR, return_value);
		else if (*url_string == 0)
			CALL_FAILED_IF_ERROR(escaped.Append(UNI_L("%00"), 3));
		else
			CALL_FAILED_IF_ERROR(escaped.Append(url_string, 1));

	DOM_WebSocket *websocket;
	OP_STATUS status = DOM_WebSocket::Make(websocket, static_cast<DOM_Runtime *>(origining_runtime), escaped.CStr(), protocols);
	switch (status)
	{
	case OpStatus::OK:
	case OpStatus::ERR_NO_SUCH_RESOURCE:
		CALL_FAILED_IF_ERROR(websocket->AddThreadListener(static_cast<DOM_Runtime *>(origining_runtime)));
		DOMSetObject(return_value, websocket);
		if (status == OpStatus::ERR_NO_SUCH_RESOURCE)
		{
			/* Early connection fail for the socket made it not even progress to a
			   connecting state. Return the object (as required), but also fire a
			   close event. */
			websocket->CloseHard();
			OpStatus::Ignore(websocket->PostCloseEvent(TRUE, WebSocketProtocol::Frame::CLOSE_RESERVED_APPLICATION_LAYER_CLOSED_ABNORMALLY, UNI_L(""), DOM_WebSocket::DO_ALWAYS));
		}
		return ES_VALUE;

	case OpStatus::ERR_NO_MEMORY:
		return ES_ERROR_NO_MEMORY;

	case OpStatus::ERR_NO_ACCESS:
		return CallDOMException(SECURITY_ERR, return_value);

	case OpStatus::ERR_PARSING_FAILED:
	default:
		return CallDOMException(SYNTAX_ERR, return_value);
	}
}

/* static */ void
DOM_WebSocket_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_WebSocket_Constructor *websocket_constructor = OP_NEW_L(DOM_WebSocket_Constructor, ());
	object->PutConstructorL(websocket_constructor, "s(s|[s])-");
	DOM_WebSocket::ConstructWebSocketObjectL(*websocket_constructor, object->GetRuntime());
}

/* static */ OP_STATUS
DOM_WebSocket::Make(DOM_WebSocket *&websocket, DOM_Runtime *origining_runtime, const uni_char *url, const OpVector<OpString> &sub_protocols)
{
	WS_NEW_DBG;

	websocket = OP_NEW(DOM_WebSocket, ());

	RETURN_IF_ERROR(DOMSetObjectRuntime(websocket, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::WEBSOCKET_PROTOTYPE), "WebSocket"));

	origining_runtime->GetEnvironment()->AddWebSocket(websocket);

	websocket->messageHandler = g_main_message_handler;
	websocket->messageHandler->SetCallBack(websocket, MSG_DOM_PROCESS_WEBSOCKET_TASKS, 0);

	RETURN_IF_ERROR(OpWebSocket::Create(&websocket->socket, websocket, websocket->messageHandler));

	return websocket->socket->Open(url, origining_runtime->GetOriginURL(), sub_protocols);
}

DOM_WebSocket::DOM_WebSocket()
	: readyState(CONNECTING)
	, bufferedAmount(0)
	, binaryType(BINARY_BLOB)
	, socket(NULL)
	, messageHandler(NULL)
{
}

/* virtual */
DOM_WebSocket::~DOM_WebSocket()
{
	WS_NEW_DBG;
	if (socket)
		OpWebSocket::Destroy(&socket);
	if (messageHandler)
		messageHandler->UnsetCallBack(this, MSG_DOM_PROCESS_WEBSOCKET_TASKS, 0);

	GetEnvironment()->RemoveWebSocket(this);
}

/* static */ void
DOM_WebSocket::ConstructWebSocketObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "CONNECTING", CONNECTING, runtime);
	PutNumericConstantL(object, "OPEN", OPEN, runtime);
	PutNumericConstantL(object, "CLOSING", CLOSING, runtime);
	PutNumericConstantL(object, "CLOSED", CLOSED, runtime);
}

/* static */ int
DOM_WebSocket::close(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	WS_NEW_DBG;
	DOM_THIS_OBJECT(websocket, DOM_TYPE_WEBSOCKET, DOM_WebSocket);
	DOM_CHECK_ARGUMENTS("|ns");

	unsigned short status = 0;
	if (argc >= 1)
	{
		unsigned code = TruncateDoubleToUInt(argv[0].value.number);
		if (!(code == 1000 || code >= 3000 && code <= 4999))
			return DOM_CALL_DOMEXCEPTION(INVALID_ACCESS_ERR);

		status = code;
	}
	const uni_char *reason = NULL;
	if (argc >= 2)
	{
		if (argv[1].GetStringLength() > 123)
			return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

		reason = argv[1].value.string;
	}

	CALL_FAILED_IF_ERROR(websocket->AddThreadListener(origining_runtime));

	if (websocket->readyState == CLOSING || websocket->readyState == CLOSED)
		return ES_FAILED;

	websocket->readyState = CLOSING;

	websocket->socket->Close(status, reason);

	return ES_FAILED;
}

BOOL
DOM_WebSocket::CloseHard()
{
	WS_NEW_DBG;

	if (readyState != CLOSED)
	{
		readyState = CLOSED;
		socket->CloseHard();
		return TRUE;
	}
	return FALSE;
}

/* static */ int
DOM_WebSocket::send(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	WS_NEW_DBG;
	DOM_THIS_OBJECT(websocket, DOM_TYPE_WEBSOCKET, DOM_WebSocket);

	ES_Value *arg;
	const unsigned char *data = NULL;
	unsigned byte_length = 0;
	if (ES_CONVERSION_COMPLETED(argc))
	{
		argc = ES_CONVERSION_ARGC(argc);
		OP_ASSERT(argc >= 1 && argv[0].type == VALUE_STRING);
	}
	if (argc < 0)
	{
		OP_ASSERT(return_value->type == VALUE_OBJECT);
		DOM_Object *reader = static_cast<DOM_Object *>(ES_Runtime::GetHostObject(return_value->value.object));
		int result = DOM_FileReader::readBlob(reader, NULL, data, byte_length, return_value, origining_runtime);
		if (result != ES_VALUE)
			return result;
		else
		{
			OP_ASSERT(data);
			arg = return_value;
		}
	}
	else
	{
		DOM_CHECK_ARGUMENTS("-");
		arg = &argv[0];
	}

	// Don't allow anything to be sent before the socket is open.
	if (websocket->readyState == CONNECTING)
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	BOOL handled = FALSE;
	if (arg->type == VALUE_STRING)
	{
		const uni_char *str = arg->value.string;
		unsigned length = arg->GetStringLength();

		OpFileLength buffered_amount;
		CALL_FAILED_IF_ERROR(websocket->socket->SendMessage(str, length, buffered_amount));
		websocket->bufferedAmount += buffered_amount;
		handled = TRUE;
	}
	else if (arg->type == VALUE_OBJECT || data)
	{
		BOOL already_sent = FALSE;
		handled = TRUE;
		if (data)
			;
		else if (ES_Runtime::IsNativeArrayBufferObject(arg->value.object))
		{
			data = ES_Runtime::GetArrayBufferStorage(arg->value.object);
			byte_length = ES_Runtime::GetArrayBufferLength(arg->value.object);
		}
		else if (ES_Runtime::IsNativeTypedArrayObject(arg->value.object))
		{
			data = static_cast<unsigned char *>(ES_Runtime::GetStaticTypedArrayStorage(arg->value.object));
			byte_length = ES_Runtime::GetStaticTypedArraySize(arg->value.object);
		}
		else
		{
			DOM_HOSTOBJECT_SAFE(blob, arg->value.object, DOM_TYPE_BLOB, DOM_Blob);
			if (blob)
			{
				if (blob->IsA(DOM_TYPE_FILE))
				{
					DOM_HOSTOBJECT_SAFE(file_blob, arg->value.object, DOM_TYPE_BLOB, DOM_File);

					OpFileLength buffered_amount;
					CALL_FAILED_IF_ERROR(websocket->socket->SendFile(file_blob->GetFilePath(), TRUE, buffered_amount));
					websocket->bufferedAmount += buffered_amount;
					already_sent = TRUE;
				}
				else
				{
					double blob_length = blob->GetSize();

					/* Note: non-standard failure mode. */
					if (blob_length < 0 || blob_length > UINT_MAX)
						return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

					ES_Value blob_data;
					DOM_Object *file_reader;
					int result = DOM_FileReader::readBlob(file_reader, blob, data, byte_length, &blob_data, origining_runtime);
					if (result == ES_NO_MEMORY)
						return result;
					else if (result & ES_SUSPEND)
					{
						DOMSetObject(return_value, file_reader);
						return result;
					}
					OP_ASSERT(data);
				}
			}
			else
				handled = FALSE;
		}

		if (handled && !already_sent)
		{
			OpFileLength buffered_amount;
			CALL_FAILED_IF_ERROR(websocket->socket->SendMessage(data, byte_length, buffered_amount));
			 websocket->bufferedAmount += buffered_amount;
		}
	}

	if (!handled)
	{
		ES_CONVERT_ARGUMENTS_AS(return_value, "s-");
		return ES_NEEDS_CONVERSION;
	}
	else
		CALL_FAILED_IF_ERROR(websocket->AddThreadListener(origining_runtime));

	return ES_FAILED;
}

OP_STATUS
DOM_WebSocket::OnSocketOpen(OpWebSocket *socket)
{
	WS_NEW_DBG;
	RETURN_IF_ERROR(PostOpenEvent(DO_ALWAYS));
	return OpStatus::OK;
}

OP_STATUS
DOM_WebSocket::OnSocketClosing(OpWebSocket *socket)
{
	WS_NEW_DBG;
	RETURN_IF_ERROR(PostReadyStateChange(CLOSING));
	return OpStatus::OK;
}

void
DOM_WebSocket::OnSocketClosed(OpWebSocket *socket, BOOL wasClean, unsigned short code, const uni_char *reason)
{
	WS_NEW_DBG;
	OpStatus::Ignore(PostCloseEvent(wasClean, code, reason, DO_ALWAYS));
}

class OpMallocAnchor
{
public:
	OpMallocAnchor(void *ptr)
		: ptr(ptr)
	{
	}

	~OpMallocAnchor()
	{
		op_free(ptr);
	}

	void release()
	{
		ptr = NULL;
	}

private:
	void *ptr;
};

OP_STATUS
DOM_WebSocket::OnSocketMessage(OpWebSocket *socket, BOOL is_binary, BOOL is_file)
{
	WS_NEW_DBG;
	OpString lastEvent;

	OpFileLength size = socket->GetReceivedMessageBytes();
	if (!is_binary)
	{
		OP_ASSERT(!is_file || !"Currently no file support for text.");
		// Do a ReadMessage even if it's an empty message as we need the message to be
		// removed and the internal structures to be updated.

		uni_char* buffer = OP_NEWA(uni_char, static_cast<unsigned>(size));
		OpHeapArrayAnchor<uni_char> anchor_buffer(buffer);

		OpFileLength data_read;
		RETURN_IF_ERROR(socket->ReadMessage(buffer, size, data_read));
		return PostMessageEvent(buffer, static_cast<unsigned>(data_read), lastEvent, DO_WHEN_CONNECTED);
	}
	else
	{
		// Do a ReadMessage even if it's an empty message as we need the message to be
		// removed and the internal structures to be updated.
		switch (binaryType)
		{
		case BINARY_ARRAYBUFFER:
		{
			unsigned char *buffer = static_cast<unsigned char *>(op_malloc(static_cast<size_t>(size)));
			OpMallocAnchor anchor_buffer(buffer);

			OpFileLength data_read;
			if (!is_file)
				RETURN_IF_ERROR(socket->ReadMessage(buffer, size, data_read));
			else
			{
				OpString file_path;
				RETURN_IF_ERROR(socket->GetMessageFilePath(file_path));

				OpFile message_file;
				RETURN_IF_ERROR(message_file.Construct(file_path, OPFILE_ABSOLUTE_FOLDER));
				RETURN_IF_ERROR(message_file.Open(OPFILE_READ));

				OP_ASSERT(message_file.GetFileLength() == size);
				RETURN_IF_ERROR(message_file.Read(buffer, size, &data_read));

				OpStatus::Ignore(message_file.Close());
				OpStatus::Ignore(message_file.Delete());
			}

			ES_Object *message_buffer;
			RETURN_IF_ERROR(GetRuntime()->CreateNativeArrayBufferObject(&message_buffer, static_cast<unsigned>(size), buffer));
			anchor_buffer.release();

			ES_Value message_data_value;
			DOMSetObject(&message_data_value, message_buffer);
			return PostMessageEvent(message_data_value, lastEvent, DO_WHEN_CONNECTED);
		}
		case BINARY_BLOB:
		{
			ES_Value message_data_value;
			if (!is_file)
			{
				unsigned char *buffer = static_cast<unsigned char *>(op_malloc(static_cast<size_t>(size)));
				OpMallocAnchor anchor_buffer(buffer);

				OpFileLength data_read;
				RETURN_IF_ERROR(socket->ReadMessage(buffer, size, data_read));

				OP_ASSERT(data_read < UINT_MAX);

				// We create the blob from native buffer as it has better memory management than raw memory buffer (less copying).
				ES_Object *message_buffer;
				RETURN_IF_ERROR(GetRuntime()->CreateNativeArrayBufferObject(&message_buffer, static_cast<unsigned>(size), buffer));

				DOM_BlobBuffer *memory_blob;
				RETURN_IF_ERROR(DOM_BlobBuffer::Make(memory_blob, message_buffer, FALSE, GetRuntime()));
				anchor_buffer.release();

				DOMSetObject(&message_data_value, memory_blob);
				return PostMessageEvent(message_data_value, lastEvent, DO_WHEN_CONNECTED);
			}
			else
			{
				OpString file_path;
				RETURN_IF_ERROR(socket->GetMessageFilePath(file_path));

				DOM_File *file;
				OP_STATUS status = DOM_File::Make(file, file_path.CStr(), TRUE, TRUE, GetRuntime());
				if (OpStatus::IsError(status))
				{
					RETURN_IF_MEMORY_ERROR(status);
					OpFile file;
					if (OpStatus::IsSuccess(file.Construct(file_path.CStr())))
						OpStatus::Ignore(file.Delete(FALSE));
					return status;
				}

				/* Note: the file blob was created with delete_on_close set to TRUE,
				   meaning it will delete the underlying file upon destruction. */
				DOMSetObject(&message_data_value, file);
				return PostMessageEvent(message_data_value, lastEvent, DO_WHEN_CONNECTED);
			}
		}
		default:
			OP_ASSERT(!"Unrecognized binary type; cannot happen.");
			return OpStatus::ERR;
		}
	}
}

void
DOM_WebSocket::OnSocketError(OpWebSocket *socket, OP_STATUS error)
{
	WS_NEW_DBG;
	OpStatus::Ignore(PostErrorEvent(DO_ALWAYS));
}

void
DOM_WebSocket::OnSocketInfo(OpWebSocket *socket, WS_Event evt, const uni_char *str, const uni_char *str2, const uni_char *str3)
{
#ifdef OPERA_CONSOLE
	if (!g_console->IsLogging())
		return;

	OpConsoleEngine::Message msg(OpConsoleEngine::WebSockets, OpConsoleEngine::Error);
	if (OpStatus::IsError(GetRuntime()->GetDisplayURL(msg.url)))
		return;

	switch (evt)
	{
	case WSWrongSubprotocol:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Handshake")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("Server replied with subprotocol '%s', expected subprotocol '%s'."), str, str2));
		break;
	case WSMissingSubprotocol:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Handshake")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("Server replied without subprotocol, expected subprotocol '%s'."), str));
		break;
	case WSNonZeroSizedBinaryFrame:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Data framing")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("Frame type '0xFF' has non-zero size (%s)."), str));
		break;
	case WSDenormalizedBinaryFrame:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Data framing")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("Frame type '0xFF' has leading 0x80 bytes in frame size.")));
		break;
	case WSInvalidFrameType:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Data framing")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("Invalid frame type '%s' not supported."), str));
		break;
	case WSInvalidUTF8:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Data framing")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("Invalid UTF-8 '%s' received at character position %s."), str2, str));
		break;
	case WSFrameTooLong:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Data framing")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("Binary frame size too large.")));
		break;
	case WSFailedToConnect:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Connection")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("Failed to connect to '%s'."), str));
		break;
	case WSWrongResponseCode:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Connection")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("WebSocket handshake failure, invalid response code '%s'."), str));
		break;
	case WSHandshakeParsing:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Connection")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("WebSocket handshake failure, invalid first line.")));
		break;
	case WSHeaderParsing:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Connection")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("WebSocket handshake failure, could not parse headers.")));
		break;
	case WSFrameParsing:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Data framing")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("Could not parse websocket frame.")));
		break;
	case WS407WithNoProxy:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Handshake")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("WebSocket handshake failure, response code 407 but not using a proxy.")));
		break;
	case WS200WithNoProxy:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Handshake")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("WebSocket handshake failure, response code 200 but not using a proxy.")));
		break;
	case WSProxyAuthFailed:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Proxy")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("WebSocket could not authorize the proxy use.")));
		break;
	case WS403WithNoProxy:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Authorization")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("WebSocket was not authorized to access the requested URL.")));
		break;
	case WS403WithProxy:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Proxy")));
		RETURN_VOID_IF_ERROR(msg.message.Set(UNI_L("WebSocket was not authorized to connect through the proxy.")));
		break;
	case WSHeaderMissing:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Handshake")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("WebSocket handshake failure, required field '%s' missing."), str));
		break;
	case WSHeaderMismatch:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Handshake")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("WebSocket handshake failure, expected field '%s' to have value '%s', found '%s'."), str, str2, str3));
		break;
	case WSHeaderDuplicate:
		RETURN_VOID_IF_ERROR(msg.context.Set(UNI_L("Handshake")));
		RETURN_VOID_IF_ERROR(msg.message.AppendFormat(UNI_L("WebSocket handshake failure, duplicate field '%s'."), str));
		break;
	default:
		OP_ASSERT(!"UNOWN MESSAGE");
		return;
	}

	TRAPD(status, g_console->PostMessageL(&msg));
	OpStatus::Ignore(status);
#endif // OPERA_CONSOLE
}


OP_STATUS
DOM_WebSocket::PostOpenEvent(TaskCondition cond)
{
	WS_NEW_DBG;

	DOM_Event *open_event = OP_NEW(DOM_Event, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(open_event, GetRuntime(), GetRuntime()->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));
	open_event->InitEvent(ONOPEN, this);
	open_event->SetCurrentTarget(this);

	DOM_WebSocketTask *task = OP_NEW(DOM_WebSocketTask, (open_event, cond));
	if (!task)
		return OpStatus::ERR_NO_MEMORY;

	task->SetReadyState(OPEN);
	task->SetCookies();
	QueueTask(task);

	return OpStatus::OK;
}

OP_STATUS
DOM_WebSocket::PostMessageEvent(const uni_char *message_data, unsigned length, const OpString &lastEventId, TaskCondition cond)
{
	WS_NEW_DBG;

	ES_Value data_value;
	ES_ValueString string_holder;
	DOMSetStringWithLength(&data_value, &string_holder, message_data, length);

	DOM_MessageEvent *message_event = NULL;
	RETURN_IF_ERROR(DOM_MessageEvent::Make(message_event, GetRuntime()));

	URL origin_url = GetRuntime()->GetOriginURL();
	RETURN_IF_ERROR(message_event->SetOrigin(origin_url));
	RETURN_IF_ERROR(message_event->SetLastEventId(lastEventId.CStr()));

	RETURN_IF_ERROR(message_event->SetData(data_value));
	message_event->SetSynthetic();
	message_event->SetCurrentTarget(this);

	DOM_WebSocketTask *task = OP_NEW(DOM_WebSocketTask, (message_event, cond));
	if (!task)
		return OpStatus::ERR_NO_MEMORY;
	QueueTask(task);
	return OpStatus::OK;
}

OP_STATUS
DOM_WebSocket::PostMessageEvent(ES_Value &message_data_value, const OpString &lastEventId, TaskCondition cond)
{
	WS_NEW_DBG;
	ES_Value data_value;


	DOM_MessageEvent *message_event = NULL;
	RETURN_IF_ERROR(DOM_MessageEvent::Make(message_event, GetRuntime()));

	URL origin_url = GetRuntime()->GetOriginURL();
	RETURN_IF_ERROR(message_event->SetOrigin(origin_url));
	RETURN_IF_ERROR(message_event->SetLastEventId(lastEventId.CStr()));

	RETURN_IF_ERROR(message_event->SetData(message_data_value));
	message_event->SetSynthetic();
	message_event->SetCurrentTarget(this);

	DOM_WebSocketTask *task = OP_NEW(DOM_WebSocketTask, (message_event, cond));
	if (!task)
		return OpStatus::ERR_NO_MEMORY;
	QueueTask(task);
	return OpStatus::OK;
}

OP_STATUS
DOM_WebSocket::PostErrorEvent(TaskCondition cond)
{
	WS_NEW_DBG;
	DOM_Event *error_event = OP_NEW(DOM_Event, ());
	RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(error_event, GetRuntime(), GetRuntime()->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));
	error_event->InitEvent(ONERROR, this);
	error_event->SetCurrentTarget(this);

	DOM_WebSocketTask *task = OP_NEW(DOM_WebSocketTask, (error_event, cond));
	if (!task)
		return OpStatus::ERR_NO_MEMORY;

	task->SetReadyState(CLOSED);

	QueueTask(task);
	return OpStatus::OK;
}

OP_STATUS
DOM_WebSocket::PostCloseEvent(BOOL wasClean, unsigned short code, const uni_char *reason, TaskCondition cond)
{
	WS_NEW_DBG;
	DOM_CloseEvent *close_event = NULL;

	RETURN_IF_ERROR(DOM_CloseEvent::Make(close_event, this, wasClean, code, reason));
	close_event->SetCurrentTarget(this);

	DOM_WebSocketTask *task = OP_NEW(DOM_WebSocketTask, (close_event, cond));
	if (!task)
		return OpStatus::ERR_NO_MEMORY;
	task->SetReadyState(CLOSED);
	QueueTask(task);
	return OpStatus::OK;
}

void
DOM_WebSocket::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	WS_NEW_DBG;
	if (msg == MSG_DOM_PROCESS_WEBSOCKET_TASKS)
		ProcessTaskQueue();
}

OP_STATUS
DOM_WebSocket::AddThreadListener(DOM_Runtime *origining_runtime)
{
	ES_Thread *thread = GetCurrentThread(origining_runtime);
	thread = thread->GetRunningRootThread();

	for (DOM_WebSocketThreadListenerLink *l = threadListeners.First(); l != NULL; l = l->Suc())
		if (l->thread == thread)
			return OpStatus::OK;

	DOM_WebSocketThreadListenerLink *link = OP_NEW(DOM_WebSocketThreadListenerLink, (this, thread));
	if (!link)
		return OpStatus::ERR_NO_MEMORY;
	link->Into(&threadListeners);
	thread->AddListener(&link->listener);

	return OpStatus::OK;
}

void
DOM_WebSocket::QueueTask(DOM_WebSocketTask *task)
{
	WS_NEW_DBG;
	task->Into(&tasks);
	ProcessTaskQueue();
}

void
DOM_WebSocket::ProcessTaskQueue()
{
	WS_NEW_DBG;

	if (!threadListeners.Empty())
		return;

	// We update buffered amount to account for bytes that has been sent out to network.
	bufferedAmount = static_cast<double>(socket->GetBufferedAmount());

	DOM_WebSocketTask *task = tasks.First();
	BOOL first = TRUE;
	while (task)
	{
		if (!first && ((task->tasks & CHANGE_READY_STATE) == CHANGE_READY_STATE || (task->tasks & SET_COOKIES) == SET_COOKIES))
			break;

		if (task->tasks & CHANGE_READY_STATE)
		{
			OP_DBG(("readystate %d -> %d", readyState, task->data.readyState));
			readyState = task->data.readyState;
		}

		if (task->tasks & SET_COOKIES)
		{
			OP_DBG(("Setting cookies"));
			socket->SetCookies();
		}

		if (task->tasks & TRIGGER_EVENT)
		{
			if (((0x1 << readyState) & task->data.condition) != 0)
			{
				switch (task->data.evt->GetKnownType())
				{
				case ONMESSAGE:
					OP_DBG(("MESSAGE evt triggered"));
					break;
				case ONERROR:
					OP_DBG(("ERROR evt triggered"));
					break;
				case ONOPEN:
					OP_DBG(("OPEN evt triggered"));
					break;
				case ONCLOSE:
					OP_DBG(("CLOSE evt triggered"));
					break;
				default:
					break;
				}

				/* Detached runtime, cannot deliver. */
				if (!GetRuntime()->GetESScheduler())
					return;

				ES_Value retVal;
				ES_Value evtVal;
				DOMSetObject(&evtVal, task->data.evt);
				DOM_Node::dispatchEvent(this, &evtVal, 1, &retVal, GetRuntime());
			}
			else
			{
				OP_DBG(("evt didn't trigger"));
				task->data.evt = NULL;
			}
		}

		task->Out();
		OP_DELETE(task);

		task = tasks.First();
		first = FALSE;
	}

	if (!tasks.Empty() || bufferedAmount > 0 && (readyState == OPEN || readyState == CLOSING))
	{
		// Post message to wake up again.
		messageHandler->PostMessage(MSG_DOM_PROCESS_WEBSOCKET_TASKS, 0, 0, !tasks.Empty() ? 0 : DOM_WEBSOCKETS_BUFFEREDAMOUNT_UPDATE_INTERVAL);
	}
}

OP_STATUS
DOM_WebSocket::PostReadyStateChange(ReadyState state)
{
	WS_NEW_DBG;
	OP_DBG(("readystate=%d", state));
	DOM_WebSocketTask *task = OP_NEW(DOM_WebSocketTask, (state));
	if (task)
	{
		QueueTask(task);
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

/* virtual */ ES_GetState
DOM_WebSocket::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	WS_NEW_DBG;
	OP_DBG((DOM_AtomToString(property_name)));
	switch(property_name)
	{
	case OP_ATOM_url:
		if (value)
		{
			if(OpStatus::IsError(socket->GetURL(targetURL)))
				return GET_NO_MEMORY;
			DOMSetString(value, targetURL.CStr());
		}
		return GET_SUCCESS;

	case OP_ATOM_readyState:
		DOMSetNumber(value, readyState);
		return GET_SUCCESS;

	case OP_ATOM_bufferedAmount:
		DOMSetNumber(value, bufferedAmount);
		return GET_SUCCESS;

	case OP_ATOM_extensions:
		// The spec does currently (draft 2012-04-17) only
		// define this as the empty string.
		DOMSetString(value);
		return GET_SUCCESS;
	case OP_ATOM_protocol:
		DOMSetString(value, socket->GetNegotiatedSubProtocol());
		return GET_SUCCESS;

	case OP_ATOM_binaryType:
		DOMSetString(value, GetBinaryTypeString());
		return GET_SUCCESS;
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_WebSocket::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_GetState res = GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (res != GET_FAILED)
			return res;
	}
	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}


/* virtual */ ES_PutState
DOM_WebSocket::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	WS_NEW_DBG;
	OP_DBG((DOM_AtomToString(property_name)));

	switch(property_name)
	{
	case OP_ATOM_url:
	case OP_ATOM_readyState:
	case OP_ATOM_bufferedAmount:
	case OP_ATOM_extensions:
	case OP_ATOM_protocol:
		return PUT_READ_ONLY;

	case OP_ATOM_binaryType:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (!SetBinaryType(value->value.string))
			return PutNameDOMException(SYNTAX_ERR, value);
		else
			return PUT_SUCCESS;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_WebSocket::PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState res = PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
		if (res != PUT_FAILED)
			return res;
	}
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ void
DOM_WebSocket::GCTrace()
{
	WS_NEW_DBG;
	GCMark(event_target);

	for (DOM_WebSocketTask *task = tasks.First(); task; task = task->Suc())
		if (task->tasks & TRIGGER_EVENT)
			GetRuntime()->GCMark(task->data.evt);
}

void
DOM_WebSocket::GCTraceConditional()
{
	WS_NEW_DBG;

	BOOL onopen = FALSE;
	BOOL onmessage = FALSE;
	BOOL onerror = FALSE;
	BOOL onclose = FALSE;

	DOM_EventTarget *event_target = GetEventTarget();

	if (event_target)
	{
		onopen = event_target->HasListeners(ONOPEN, NULL, ES_PHASE_ANY);
		onerror = event_target->HasListeners(ONERROR, NULL, ES_PHASE_ANY);
		onclose = event_target->HasListeners(ONCLOSE, NULL, ES_PHASE_ANY);
		onmessage = event_target->HasListeners(ONMESSAGE, NULL, ES_PHASE_ANY);
	}

	BOOL collect = TRUE;
	switch (readyState)
	{
	case CONNECTING:
		collect = !(onopen || onmessage || onerror || onclose);
		break;
	case OPEN:
	case CLOSING:
		collect = !(onmessage || onerror || onclose);
		break;
	case CLOSED:
	default:
		break;
	}
	OP_DBG(("collect=%d", collect));
	if (!collect)
		DOM_Object::GCMark(*this);
}

/* virtual */ ES_GetState
DOM_WebSocket::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("onopen");
	enumerator->AddPropertyL("onerror");
	enumerator->AddPropertyL("onclose");
	enumerator->AddPropertyL("onmessage");

	return GET_SUCCESS;
}

const uni_char *
DOM_WebSocket::GetBinaryTypeString()
{
	switch (binaryType)
	{
	case BINARY_ARRAYBUFFER:
		return UNI_L("arraybuffer");
	default:
		OP_ASSERT(!"Unrecognized binary type; cannot happen.");
	case BINARY_BLOB:
		return UNI_L("blob");
	}
}

BOOL
DOM_WebSocket::SetBinaryType(const uni_char *new_type)
{
	if (uni_str_eq(new_type, "arraybuffer"))
	{
		binaryType = BINARY_ARRAYBUFFER;
		return TRUE;
	}
	else if (uni_str_eq(new_type, "blob"))
	{
		binaryType = BINARY_BLOB;
		return TRUE;
	}
	else
		return FALSE;
}

#ifdef DOM_WEBSOCKETS_TESTING
static int UncollectedWebSockets(ES_Value *argv, int argc, ES_Value *return_value, DOM_Environment::CallbackSecurityInfo *security_info)
{
	WS_NEW_DBG;
	g_ecmaManager->GarbageCollect();
	return_value->type = VALUE_NUMBER;
	return_value->value.number = g_webSocket_Manager->GetActiveWebSockets();
	OP_DBG(("uncollected=%d", (int)return_value->value.number));
	return ES_VALUE;
}
#endif //DOM_WEBSOCKETS_TESTING

/* static */ void
DOM_WebSocket::Init()
{
	WS_NEW_DBG;
#ifdef DOM_WEBSOCKETS_TESTING
	DOM_Environment::AddCallback(UncollectedWebSockets, DOM_Environment::OPERA_CALLBACK, "uncollectedWebSockets", "");
#endif //DOM_WEBSOCKETS_TESTING
}

DOM_FUNCTIONS_START(DOM_WebSocket)
	DOM_FUNCTIONS_FUNCTION(DOM_WebSocket, DOM_WebSocket::close, "close", "ns-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebSocket, DOM_WebSocket::send, "send", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_WebSocket, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_WebSocket)

DOM_FUNCTIONS_WITH_DATA_START(DOM_WebSocket)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WebSocket, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_WebSocket, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_WebSocket)

#endif // WEBSOCKETS_SUPPORT
