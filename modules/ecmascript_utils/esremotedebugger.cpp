/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ECMASCRIPT_REMOTE_DEBUGGER

#include "modules/ecmascript_utils/esremotedebugger.h"
#include "modules/ecmascript_utils/esremotemessageparts.h"

#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/uamanager/ua.h"
#include "modules/url/url_enum.h"
#include "modules/url/url_socket_wrapper.h"
#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/stdlib/include/double_format.h"

static BOOL
ES_RemoteCharIsHex(char ch)
{
	return ch >= '0' && ch <= '9' || ch >= 'a' && ch <= 'f';
}

static BOOL
ES_RemoteStringIsHex(const char *string, unsigned length)
{
	for (const char *stop = string + length; string != stop; ++string)
		if (!ES_RemoteCharIsHex(*string))
			return FALSE;
	return TRUE;
}

static unsigned
ES_RemoteCharToUnsigned(char ch)
{
	if (ch < 'a')
		return ch - '0';
	else
		return ch - 'a' + 10;
}

static unsigned
ES_RemoteStringToUnsigned2(const char *string)
{
	return (ES_RemoteCharToUnsigned(string[0]) << 4) + ES_RemoteCharToUnsigned(string[1]);
}

static unsigned
ES_RemoteStringToUnsigned4(const char *string)
{
	return (ES_RemoteStringToUnsigned2(string) << 8) + ES_RemoteStringToUnsigned2(string + 2);
}

static unsigned
ES_RemoteStringToUnsigned8(const char *string)
{
	return (ES_RemoteStringToUnsigned4(string) << 16) + ES_RemoteStringToUnsigned4(string + 4);
}

ES_DebugMessagePart::ES_DebugMessagePart(Type type)
	: type(type)
{
}

ES_DebugMessagePart::Type
ES_DebugMessagePart::GetType()
{
	return type;
}

/* virtual */
ES_DebugMessagePart::~ES_DebugMessagePart()
{
}

/* static */ void
ES_DebugMessagePart::SetString8L(char *&stored_data, unsigned &stored_length, const char *data, unsigned length)
{
	if (length == ~0u)
		stored_length = data ? op_strlen(data) : 0;
	else
		stored_length = length;

	char header[10], *header_format;
	unsigned header_length;

	if (stored_length <= 65535)
		header_format = "s%04x";
	else
		header_format = "S%08x";

	header_length = op_sprintf(header, header_format, stored_length);
	stored_data = new (ELeave) char[header_length + stored_length];

	op_memcpy(stored_data, header, header_length);
	stored_data += header_length;

	if (data)
		op_memcpy(stored_data, data, stored_length);
}

/* static */ void
ES_DebugMessagePart::SetString16L(char *&stored_data, unsigned &stored_length, const uni_char *data, unsigned length)
{
	if (!data)
		data = UNI_L("");

	if (length == ~0u)
		length = uni_strlen(data);

	UTF16toUTF8Converter converter;
	int read;

	SetString8L(stored_data, stored_length, NULL, converter.BytesNeeded((char*)data, UNICODE_SIZE(length)));
	converter.Reset();
	converter.Convert(data, UNICODE_SIZE(length), stored_data, stored_length, &read);
}

/* static */ void
ES_DebugMessagePart::SetValueL(ES_DebugValue &stored_value, const ES_DebugValue &value)
{
	stored_value = value;

	if (value.type == VALUE_STRING)
	{
		if (value.is_8bit)
		{
			char *new_value = 0;
			SetStrL(new_value, value.value.string8.value, value.value.string8.length);
			stored_value.value.string8.value = new_value;
		}
		else
		{
			uni_char *new_value = 0;
			LEAVE_IF_ERROR(UniSetStrN(new_value, value.value.string16.value, value.value.string16.length));
			stored_value.value.string16.value = new_value;
		}
	}
	else if (value.type == VALUE_OBJECT)
		stored_value.value.object.info = NULL;

	stored_value.owns_value = TRUE;
}

void
ES_DebugMessagePart::GetItemDataBoolean(const char *data, unsigned &length, BOOL value)
{
	length = 1;
	if (data) ((char *) data)[0] = value ? 't' : 'f';
}

void
ES_DebugMessagePart::GetItemDataInteger(const char *data, unsigned &length, int value)
{
	char sign = value < 0 ? '-' : '+';
	unsigned uvalue = value < 0 ? -value : value;

	if (value >= -65535 && value <= 65535)
	{
		length = 6;
		if (data) op_sprintf((char *) data, "i%c%04x", sign, uvalue);
	}
	else
	{
		length = 10;
		if (data) op_sprintf((char *) data, "I%c%08x", sign, uvalue);
	}
}

void
ES_DebugMessagePart::GetItemDataUnsigned(const char *data, unsigned &length, unsigned value)
{
	if (value <= 65535)
	{
		length = 5;
		if (data) op_sprintf((char *) data, "u%04x", value);
	}
	else
	{
		length = 9;
		if (data) op_sprintf((char *) data, "U%08x", value);
	}
}

void
ES_DebugMessagePart::GetItemDataDouble(const char *data, unsigned &length, double value)
{
	char buffer[REQUIRED_BUFFER_SIZE], *result;

	result = OpDoubleFormat::ToString(buffer, value);
	if (!result)
	{
		length = 1;
		buffer[0] = '0';
	}
	else
		length = op_strlen(buffer);

	if (data)
	{
		op_sprintf((char *) data, "d%02x", length);
		op_memcpy((char *) data + 3, buffer, length);
	}

	length += 3;
}

void
ES_DebugMessagePart::GetItemDataString(const char *&data, unsigned &length)
{
	if (length <= 65535)
	{
		data -= 5;
		length += 5;
		OP_ASSERT(data[0] == 's');
	}
	else
	{
		data -= 9;
		length += 9;
		OP_ASSERT(data[0] == 'S');
	}
}

void
ES_DebugMessagePart::GetItemDataValueType(const char *data, unsigned &length, ES_DebugValue &value)
{
	unsigned type;

	switch (value.type)
	{
	case VALUE_UNDEFINED:
		type = ES_DebugMessage::VALUE_TYPE_UNDEFINED;
		break;

	case VALUE_NULL:
		type = ES_DebugMessage::VALUE_TYPE_NULL;
		break;

	case VALUE_BOOLEAN:
		type = ES_DebugMessage::VALUE_TYPE_BOOLEAN;
		break;

	case VALUE_NUMBER:
		type = ES_DebugMessage::VALUE_TYPE_NUMBER;
		break;

	case VALUE_STRING:
		type = ES_DebugMessage::VALUE_TYPE_STRING;
		break;

	default:
		type = ES_DebugMessage::VALUE_TYPE_OBJECT;
	}

	GetItemDataUnsigned(data, length, type);
}

void
ES_DebugMessagePart::GetItemDataValueValue(const char *&data, unsigned &length, ES_DebugValue &value)
{
	switch (value.type)
	{
	case VALUE_NULL:
	case VALUE_UNDEFINED:
		GetItemDataBoolean(data, length, FALSE);
		return;

	case VALUE_BOOLEAN:
		GetItemDataBoolean(data, length, value.value.boolean);
		return;

	case VALUE_NUMBER:
		GetItemDataDouble(data, length, value.value.number);
		return;

	case VALUE_STRING:
		length = ~0u;
		if (data)
		{
			char *ptr = (char *) data;

			*((BOOL *) ptr) = value.is_8bit;
			ptr += sizeof(BOOL);

			if (value.is_8bit)
			{
				*((unsigned *) ptr) = value.value.string8.length;
				ptr += sizeof(unsigned);
				*((const char **) ptr) = value.value.string8.value;
			}
			else
			{
				*((unsigned *) ptr) = value.value.string16.length;
				ptr += sizeof(unsigned);
				*((const uni_char **) ptr) = value.value.string16.value;
			}
		}
		return;

	default:
		GetItemDataUnsigned(data, length, value.value.object.id);
	}
}

void
ES_DebugMessagePart::FreeItemDataString(const char *data, unsigned length)
{
	if (data)
	{
		GetItemDataString(data, length);
		delete[] (char *) data;
	}
}

ES_DebugMessage::ES_DebugMessage()
{
}

ES_DebugMessage::~ES_DebugMessage()
{
	parts.Clear();
}

unsigned
ES_DebugMessage::GetSize()
{
	unsigned size = 0;

	for (ES_DebugMessagePart *part = (ES_DebugMessagePart *) parts.First(); part; part = (ES_DebugMessagePart *) part->Suc())
		for (unsigned index = 0, length = part->GetLength(); index < length; ++index)
		{
			const char *data = NULL;
			unsigned itemsize;
			part->GetItemData(index, data, itemsize);

			if (itemsize == ~0u)
			{
				char buffer[32]; /* ARRAY OK jl 2008-02-07 */
				data = buffer;

				part->GetItemData(index, data, itemsize);

				BOOL is_8bit = *((BOOL *) data);
				data += sizeof(BOOL);

				unsigned length = *((unsigned *) data);
				data += sizeof(unsigned);

				if (is_8bit)
					itemsize = length;
				else
				{
					UTF16toUTF8Converter converter;
					itemsize = converter.BytesNeeded(*((const char **) data), UNICODE_SIZE(length));
				}

				if (itemsize <= 65535)
					itemsize += 5;
				else
					itemsize += 9;
			}

			size += itemsize;
		}

	return size;
}

unsigned
ES_DebugMessage::GetLength()
{
	unsigned length = 0;

	for (ES_DebugMessagePart *part = (ES_DebugMessagePart *) parts.First(); part; part = (ES_DebugMessagePart *) part->Suc())
		length += part->GetLength();

	return length;
}

ES_DebugMessagePart *
ES_DebugMessage::GetFirstPart()
{
	return (ES_DebugMessagePart *) parts.First();
}

void
ES_DebugMessage::GetItemDataL(unsigned index, const char *&data, unsigned &length)
{
	for (ES_DebugMessagePart *part = (ES_DebugMessagePart *) parts.First(); part; part = (ES_DebugMessagePart *) part->Suc())
	{
		unsigned partlength = part->GetLength();

		if (index < partlength)
		{
			part->GetItemData(index, data, length);

			if (length == ~0u)
			{
				BOOL is_8bit = *((BOOL *) data);
				data += sizeof(BOOL);

				unsigned itemsize, header_length;

				length = *((unsigned *) data);
				data += sizeof(unsigned);

				if (is_8bit)
					itemsize = length;
				else
				{
					UTF16toUTF8Converter converter;
					itemsize = converter.BytesNeeded(*((const char **) data), UNICODE_SIZE(length));
				}

				UTF16toUTF8Converter converter;
				header_length = itemsize <= 65535 ? 5 : 9;
				buffer.ExpandL((header_length + itemsize + 1) / 2);

				if (itemsize <= 65535)
					op_sprintf((char *) buffer.GetStorage(), "s%04x", itemsize);
				else
					op_sprintf((char *) buffer.GetStorage(), "S%08x", itemsize);

				if (is_8bit)
					op_memcpy(((char *) buffer.GetStorage()) + header_length, *((const char **) data), itemsize);
				else
				{
					int read;
					converter.Convert(*((const char **) data), UNICODE_SIZE(itemsize), ((char *) buffer.GetStorage()) + header_length, UNICODE_SIZE(buffer.GetCapacity()) - header_length, &read);
				}

				data = (char *) buffer.GetStorage();
				length = itemsize + header_length;
			}

			return;
		}
		else
			index -= partlength;
	}

	OP_ASSERT(FALSE);
}

void
ES_DebugMessage::ParseL(const char *data, unsigned size, unsigned length)
{
	unsigned message_type;

	ParseUnsignedL(data, size, length, message_type);

	ES_DebugMessagePart *body = NULL;

	if (message_type == TYPE_NEW_SCRIPT)
		body = new (ELeave) ES_NewScriptMessagePart;
	else if (message_type == TYPE_THREAD_STARTED)
		body = new (ELeave) ES_ThreadStartedMessagePart;
	else if (message_type == TYPE_THREAD_FINISHED)
		body = new (ELeave) ES_ThreadFinishedMessagePart;
	else if (message_type == TYPE_STOPPED_AT)
		body = new (ELeave) ES_StoppedAtMessagePart;
	else if (message_type == TYPE_CONTINUE)
		body = new (ELeave) ES_ContinueMessagePart;
	else if (message_type == TYPE_EVAL)
		body = new (ELeave) ES_EvalMessagePart;
	else if (message_type == TYPE_EXAMINE)
		body = new (ELeave) ES_ExamineMessagePart;
	else if (message_type == TYPE_ADD_BREAKPOINT)
		body = new (ELeave) ES_ChangeBreakpointMessagePart(TRUE);
	else if (message_type == TYPE_REMOVE_BREAKPOINT)
		body = new (ELeave) ES_ChangeBreakpointMessagePart(FALSE);
	else if (message_type == TYPE_SET_CONFIGURATION)
		body = new (ELeave) ES_SetConfigurationMessagePart;
	else if (message_type == TYPE_BACKTRACE)
		body = new (ELeave) ES_BacktraceMessagePart;
	else if (message_type == TYPE_BREAK)
		body = new (ELeave) ES_BreakMessagePart;
	else
		LEAVE(OpStatus::ERR);

	body->Into(&parts);
	body->ParseL(data, size, length);

	while (size != 0 && length != 0)
	{
		unsigned auxiliary_type;

		ParseUnsignedL(data, size, length, auxiliary_type);

		ES_DebugMessagePart *auxiliary = NULL;

		if (auxiliary_type == AUXILIARY_OBJECT_INFO)
			auxiliary = new (ELeave) ES_ObjectInfoMessagePart;
		else if (auxiliary_type == AUXILIARY_WATCH_UPDATE)
			auxiliary = new (ELeave) ES_WatchUpdateMessagePart;
		else if (auxiliary_type == AUXILIARY_STACK)
			auxiliary = new (ELeave) ES_StackMessagePart;
		else if (auxiliary_type == AUXILIARY_BREAKPOINT_TRIGGERED)
			auxiliary = new (ELeave) ES_BreakpointTriggeredMessagePart;
		else if (auxiliary_type == AUXILIARY_RETURN_VALUE)
			auxiliary = new (ELeave) ES_ReturnValueMessagePart;
		else if (auxiliary_type == AUXILIARY_EXCEPTION_THROWN)
			auxiliary = new (ELeave) ES_ExceptionThrownMessagePart;
		else if (auxiliary_type == AUXILIARY_EVAL_VARIABLE)
			auxiliary = new (ELeave) ES_EvalVariableMessagePart;
		else
			LEAVE(OpStatus::ERR);

		auxiliary->Into(&parts);
		auxiliary->ParseL(data, size, length);
	}

	if (size != 0 || length != 0)
		LEAVE(OpStatus::ERR);
}

void
ES_DebugMessage::AddPart(ES_DebugMessagePart *part)
{
	part->Into(&parts);
}

void
ES_DebugMessage::AddObjectInfoL(const ES_DebugObject &object)
{
	if (object.id && object.info)
	{
		for (ES_DebugMessagePart *existing = (ES_DebugMessagePart *) parts.First(); existing; existing = (ES_DebugMessagePart *) existing->Suc())
			if (existing->GetType() == ES_DebugMessagePart::AUXILIARY_OBJECT_INFO)
				if (((ES_ObjectInfoMessagePart *) existing)->object_id == object.id)
					return;

		ES_ObjectInfoMessagePart *part = new (ELeave) ES_ObjectInfoMessagePart;
		part->Into(&parts);

		part->object_id = object.id;
		part->prototype_id = object.info->prototype.id;
		part->is_callable = object.info->flags.packed.is_callable;
		part->is_function = object.info->flags.packed.is_function;

		if (part->is_function)
			ES_DebugMessagePart::SetString16L(part->name, part->name_length, object.info->data.function_name);
		else
			ES_DebugMessagePart::SetString8L(part->name, part->name_length, object.info->data.class_name);

		AddObjectInfoL(object.info->prototype);
	}
}

/* static */ void
ES_DebugMessage::ParseBooleanL(const char *&data, unsigned &size, unsigned &length, BOOL &value)
{
	if (length == 0 || size == 0 || !(*data == 't' || *data == 'f'))
		LEAVE(OpStatus::ERR);

	value = *data++ == 't';

	--size;
	--length;
}

/* static */ void
ES_DebugMessage::ParseIntegerL(const char *&data, unsigned &size, unsigned &length, int &value)
{
	if (length == 0 || size < 2 || !(*data == 'i' && size >= 6 && ES_RemoteStringIsHex(data + 2, 4) || *data == 'I' && size >= 10 && ES_RemoteStringIsHex(data + 2, 8)) || !(data[1] == '-' || data[1] == '+'))
		LEAVE(OpStatus::ERR);

	unsigned uvalue;
	char type = *data++;
	char sign = *data++;

	if (type == 'i')
	{
		uvalue = ES_RemoteStringToUnsigned4(data);
		data += 4;
		size -= 6;
	}
	else
	{
		uvalue = ES_RemoteStringToUnsigned8(data);
		data += 8;
		size -= 10;
	}

	if (sign == '-')
	{
		value = -(int) uvalue;

		if (value > 0)
			LEAVE(OpStatus::ERR);
	}
	else
	{
		value = (int) uvalue;

		if (value < 0)
			LEAVE(OpStatus::ERR);
	}

	--length;
}

/* static */ void
ES_DebugMessage::ParseUnsignedL(const char *&data, unsigned &size, unsigned &length, unsigned &value)
{
	if (length == 0 || size == 0 || !(*data == 'u' && size >= 5 && ES_RemoteStringIsHex(data + 1, 4) || *data == 'U' && size >= 9 && ES_RemoteStringIsHex(data + 1, 8)))
		LEAVE(OpStatus::ERR);

	if (*data++ == 'u')
	{
		value = ES_RemoteStringToUnsigned4(data);
		data += 4;
		size -= 5;
	}
	else
	{
		value = ES_RemoteStringToUnsigned8(data);
		data += 8;
		size -= 9;
	}

	--length;
}

/* static */ void
ES_DebugMessage::ParseDoubleL(const char *&data, unsigned &size, unsigned &length, double &value)
{
	if (length == 0 || size < 4 || !(*data == 'd' && ES_RemoteStringIsHex(data, 2)))
		LEAVE(OpStatus::ERR);

	unsigned dlength = ES_RemoteStringToUnsigned2(data + 1);
	data += 3;
	size -= 3;

	if (dlength >= ES_DebugMessagePart::REQUIRED_BUFFER_SIZE || size < dlength)
		LEAVE(OpStatus::ERR);

	char buffer[ES_DebugMessagePart::REQUIRED_BUFFER_SIZE], *endptr;
	op_memcpy(buffer, data, dlength);
	((char *) buffer)[dlength] = 0;

	data += dlength;
	size -= dlength;

	value = op_strtod(buffer, &endptr);

	--length;
}

/* static */ void
ES_DebugMessage::ParseStringL(const char *&data, unsigned &size, unsigned &length, const char *&value, unsigned &value_length)
{
	if (length == 0 || size == 0 || !(*data == 's' && size >= 5 && ES_RemoteStringIsHex(data + 1, 4) || *data == 'S' && size >= 65545 && ES_RemoteStringIsHex(data + 1, 8)))
		LEAVE(OpStatus::ERR);

	if (*data++ == 's')
	{
		value_length = ES_RemoteStringToUnsigned4(data);
		data += 4;
		size -= 5;
	}
	else
	{
		value_length = ES_RemoteStringToUnsigned8(data);
		data += 8;
		size -= 9;
	}

	value = data;

	data += value_length;
	size -= value_length;
	--length;
}

/* static */ void
ES_DebugMessage::ParseValueL(const char *&data, unsigned &size, unsigned &length, ES_DebugValue &value)
{
	unsigned type;

	ParseUnsignedL(data, size, length, type);

	value.owns_value = FALSE;

	switch (type)
	{
	case VALUE_TYPE_NULL:
	case VALUE_TYPE_UNDEFINED:
	case VALUE_TYPE_BOOLEAN:
		if (type == VALUE_TYPE_NULL)
			value.type = VALUE_NULL;
		else if (type == VALUE_TYPE_UNDEFINED)
			value.type = VALUE_UNDEFINED;
		else
			value.type = VALUE_BOOLEAN;
		ParseBooleanL(data, size, length, value.value.boolean);
		break;

	case VALUE_TYPE_NUMBER:
		value.type = VALUE_NUMBER;
		ParseDoubleL(data, size, length, value.value.number);
		break;

	case VALUE_TYPE_STRING:
		value.type = VALUE_STRING;
		ParseStringL(data, size, length, value.value.string8.value, value.value.string8.length);
		value.is_8bit = TRUE;
		break;

	case VALUE_TYPE_OBJECT:
		value.type = VALUE_OBJECT;
		ParseUnsignedL(data, size, length, value.value.object.id);
		value.value.object.info = NULL;
	}
}

ES_RemoteDebugConnection::ES_RemoteDebugConnection()
	: socketaddress(NULL),
	  socket(NULL),
	  state(INITIAL),
	  connect_error(OpSocket::NETWORK_NO_ERROR),
	  send_message(NULL),
	  send_head(TRUE),
	  send_tail(TRUE),
	  is_sending(FALSE),
	  send_item(0),
	  send_length(0),
	  send_offset(0),
	  send_error(OpSocket::NETWORK_NO_ERROR),
	  receive_message(NULL),
	  receive_message_buffer(NULL),
	  receive_buffer_used(0),
	  receive_error(OpSocket::NETWORK_NO_ERROR)
{
}

ES_RemoteDebugConnection::~ES_RemoteDebugConnection()
{
	delete socket;
	delete socketaddress;
}

BOOL
ES_RemoteDebugConnection::IsConnected()
{
	return state == CONNECTED;
}

OP_STATUS
ES_RemoteDebugConnection::Connect(const OpStringC &address, int port)
{
	OP_ASSERT(state == INITIAL);

	delete socketaddress;
	socketaddress = NULL;

	RETURN_IF_ERROR(OpSocketAddress::Create(&socketaddress));

	socketaddress->FromString(address);
	socketaddress->SetPort(port);

	RETURN_IF_ERROR(SocketWrapper::CreateTCPSocket(&socket, this, SocketWrapper::ALLOW_ALL_WRAPPERS));
	RETURN_IF_ERROR(socket->Connect(socketaddress));

	state = CONNECTING;
	return OpStatus::OK;
}

OP_STATUS
ES_RemoteDebugConnection::Listen(int port)
{
#ifdef SOCKET_LISTEN_SUPPORT
	OP_ASSERT(state == INITIAL);

	RETURN_IF_ERROR(SocketWrapper::CreateTCPSocket(&socket, this, SocketWrapper::NO_WRAPPERS));
	RETURN_IF_ERROR(socket->SetLocalPort(port));
	RETURN_IF_ERROR(socket->Listen(1));

	state = LISTENING;
	return OpStatus::OK;
#else // SOCKET_LISTEN_SUPPORT
	return OpStatus::ERR;
#endif // SOCKET_LISTEN_SUPPORT
}

OP_STATUS
ES_RemoteDebugConnection::Close()
{
	delete socket;
	socket = NULL;
	delete socketaddress;
	socketaddress = NULL;

	state = CLOSED;
	return Closed();
}

void
ES_RemoteDebugConnection::CloseL()
{
	LEAVE_IF_ERROR(Close());
}

void
ES_RemoteDebugConnection::SendL(ES_DebugMessage *message)
{
	OP_ASSERT(state == CONNECTED);

	if (!send_message)
	{
		send_message = message;
		SendMoreL();
	}
	else
		message->Into(&send_queue);
}

void
ES_RemoteDebugConnection::SendMoreL()
{
	while (send_message)
	{
		if (send_message->InList())
			send_message->Out();

		while (send_head)
		{
			op_sprintf(send_buffer, "%08x%04x", send_message->GetSize(), send_message->GetLength());

			if (send_length == 0)
			{
				send_length = 12;
				send_offset = 0;
			}

			is_sending = TRUE;
			OP_STATUS status = socket->Send(send_buffer + send_offset, send_length);
			is_sending = FALSE;

			if (OpStatus::IsSuccess(status))
				continue;
			else if (send_error == OpSocket::SOCKET_BLOCKING)
				return;
			else
			{
				OP_ASSERT(FALSE);
				LEAVE(status);
			}
		}

		while (send_item < send_message->GetLength())
		{
			const char *value = send_buffer;
			unsigned length;

			send_message->GetItemDataL(send_item, value, length);

			if (send_length == 0)
			{
				send_length = length;
				send_offset = 0;
			}

			is_sending = TRUE;
			OP_STATUS status = socket->Send(value + send_offset, MIN(send_length, 1024));
			is_sending = FALSE;

			if (OpStatus::IsSuccess(status))
				continue;
			else if (send_error == OpSocket::SOCKET_BLOCKING)
				return;
			else
			{
				OP_ASSERT(FALSE);
				LEAVE(status);
			}
		}

		delete send_message;
		send_message = (ES_DebugMessage *) send_queue.First();
		send_head = TRUE;
		send_tail = TRUE;
		send_item = 0;
		send_length = 0;
		send_offset = 0;
	}
}

void
ES_RemoteDebugConnection::ReceiveMoreL()
{
	while (TRUE)
	{
		if (!ReceiveDataL())
			return;

		if (!receive_message)
		{
			if (receive_buffer_used < 12)
				return;

			receive_tail = TRUE;
			receive_item = receive_item_offset = 0;

			if (!ES_RemoteStringIsHex(receive_buffer, 12))
			{
				CloseL();
				return;
			}

			receive_message_size = ES_RemoteStringToUnsigned8(receive_buffer);
			receive_message_length = ES_RemoteStringToUnsigned4(receive_buffer + 8);
			receive_message_buffer = new (ELeave) char[receive_message_size];
			receive_message = new (ELeave) ES_DebugMessage;

			receive_buffer_used = 0;
			receive_message_received = 0;
			continue;
		}
		else if (receive_message_received < receive_message_size)
			continue;
		else
		{
			receive_message->ParseL(receive_message_buffer, receive_message_size, receive_message_length);
			LEAVE_IF_ERROR(Received(receive_message));

			delete[] receive_message_buffer;
			receive_message_buffer = NULL;

			receive_message = NULL;
		}
	}
}

BOOL
ES_RemoteDebugConnection::ReceiveDataL()
{
	char *buffer;
	unsigned buffer_size, *buffer_used, received;

	if (receive_message_buffer)
	{
		buffer = receive_message_buffer;
		buffer_size = receive_message_size;
		buffer_used = &receive_message_received;
	}
	else
	{
		buffer = receive_buffer;
		buffer_size = 12;
		buffer_used = &receive_buffer_used;
	}

	OP_STATUS status = socket->Recv(buffer + *buffer_used, buffer_size - *buffer_used, &received);
	if (OpStatus::IsError(status))
	{
		if (receive_error == OpSocket::SOCKET_BLOCKING)
			return FALSE;

		OP_ASSERT(FALSE);
		LEAVE(status);
	}

	*buffer_used += received;
	return received > 0;
}

#ifdef CORESOCKETACCOUNTING

/* virtual */ void
ES_RemoteDebugConnection::OnSocketInstanceNumber(int instance_nr)
{
}

#endif // CORESOCKETACCOUNTING

/* virtual */ void
ES_RemoteDebugConnection::OnSocketConnected(OpSocket* socket)
{
	OP_ASSERT(state == CONNECTING);

	state = CONNECTED;
	LEAVE_IF_ERROR(Connected());
}

/* virtual */ void
ES_RemoteDebugConnection::OnSocketDataReady(OpSocket* socket)
{
	TRAPD(status, ReceiveMoreL());
	OpStatus::Ignore(status);
}

/* virtual */ void
ES_RemoteDebugConnection::OnSocketDataSent(OpSocket* socket, unsigned int bytes_sent)
{
	send_length -= bytes_sent;
	send_offset += bytes_sent;

	if (send_length == 0)
		if (send_head)
			send_head = FALSE;
		else
			++send_item;

	if (!is_sending)
	{
		TRAPD(status, SendMoreL());
		OpStatus::Ignore(status);
	}
}

/* virtual */ void
ES_RemoteDebugConnection::OnSocketClosed(OpSocket* socket)
{
	TRAPD(status, CloseL());
	OpStatus::Ignore(status);
}

#ifdef SOCKET_LISTEN_SUPPORT

/* virtual */ void
ES_RemoteDebugConnection::OnSocketConnectionRequest(OpSocket* socket)
{
	OpSocket *new_socket;

	if (OpStatus::IsSuccess(SocketWrapper::CreateTCPSocket(&new_socket, this, SocketWrapper::NO_WRAPPERS)))
		if (OpStatus::IsSuccess(socket->Accept(new_socket)))
		{
			delete socket;
			socket = new_socket;

			state = CONNECTED;

			OpStatus::Ignore(Connected());
		}
}

#endif // SOCKET_LISTEN_SUPPORT

/* virtual */ void
ES_RemoteDebugConnection::OnSocketConnectError(OpSocket* socket, OpSocket::Error error)
{
	connect_error = error;
}

/* virtual */ void
ES_RemoteDebugConnection::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
	receive_error = error;
}

/* virtual */ void
ES_RemoteDebugConnection::OnSocketSendError(OpSocket* socket, OpSocket::Error error)
{
	send_error = error;
}

/* virtual */ void
ES_RemoteDebugConnection::OnSocketCloseError(OpSocket* socket, OpSocket::Error error)
{
}

/* virtual */ void
ES_RemoteDebugConnection::OnSocketDataSendProgressUpdate(OpSocket* aSocket, unsigned int aBytesSent)
{
}

ES_RemoteDebugFrontend::ES_RemoteDebugFrontend()
{
}

/* virtual */
ES_RemoteDebugFrontend::~ES_RemoteDebugFrontend()
{
	Detach();
}

OP_STATUS
ES_RemoteDebugFrontend::Construct(BOOL active, const OpStringC &address, int port)
{
	if (active)
		RETURN_IF_ERROR(Connect(address, port));
	else
		RETURN_IF_ERROR(Listen(port));

	return ES_DebugFrontend::ConstructEngineBackend();
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::NewScript(unsigned dbg_runtime_id, ES_DebugScriptData *data, const ES_DebugRuntimeInformation *runtimeinfo)
{
	if (!IsConnected())
		return OpStatus::OK;

	TRAPD(status, NewScriptL(dbg_runtime_id, data, runtimeinfo));
	return status;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::ThreadStarted(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo)
{
	if (!IsConnected())
		return OpStatus::OK;

	TRAPD(status, ThreadStartedL(dbg_runtime_id, dbg_thread_id, dbg_parent_thread_id, type, namespace_uri, event_type, runtimeinfo));
	return status;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::ThreadFinished(unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus thread_status, ES_DebugReturnValue *dbg_return_value)
{
	if (!IsConnected())
		return OpStatus::OK;

	TRAPD(status, ThreadFinishedL(dbg_runtime_id, dbg_thread_id, thread_status, dbg_return_value));
	return status;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::StoppedAt(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data)
{
	if (!IsConnected())
		return Continue(dbg_runtime_id, RUN);

	TRAPD(status, StoppedAtL(dbg_runtime_id, dbg_thread_id, data));
	return status;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::BacktraceReply(unsigned tag, unsigned length, ES_DebugStackFrame *stack)
{
	TRAPD(status, BacktraceReplyL(tag, length, stack));
	return status;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::EvalReply(unsigned tag, EvalStatus evalstatus, const ES_DebugValue &result)
{
	TRAPD(status, EvalReplyL(tag, evalstatus, result));
	return status;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::ExamineReply(unsigned tag, unsigned objects_count, ES_DebugObjectProperties *objects)
{
	TRAPD(status, ExamineReplyL(tag, objects_count, objects));
	return status;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::Connected()
{
	TRAPD(status, SendHelloL());
	if (OpStatus::IsSuccess(status))
		return AttachToAll();
	else
		return status;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::Received(ES_DebugMessage *message)
{
	OpAutoPtr<ES_DebugMessage> anchor(message);

	ES_DebugMessagePart *part = message->GetFirstPart();

	if (part->GetType() == ES_DebugMessagePart::BODY_CONTINUE)
	{
		ES_ContinueMessagePart *body = (ES_ContinueMessagePart *) part;
		return Continue(body->runtime_id, (ES_DebugFrontend::Mode) body->mode);
	}
	else if (part->GetType() == ES_DebugMessagePart::BODY_EVAL)
	{
		ES_EvalMessagePart *body = (ES_EvalMessagePart *) part;

		OpString script;
		RETURN_IF_ERROR(script.SetFromUTF8(body->script, body->script_length));

		unsigned variables_count = 0;

		ES_DebugMessagePart *iter = (ES_DebugMessagePart *) part->Suc();
		while (iter)
		{
			if (iter->GetType() == ES_DebugMessagePart::AUXILIARY_EVAL_VARIABLE)
				++variables_count;
			iter = (ES_DebugMessagePart *) iter->Suc();
		}

		ES_DebugVariable *variables;
		if (variables_count != 0)
		{
			variables = new ES_DebugVariable[variables_count];
			if (variables)
			{
				unsigned index = 0;

				iter = (ES_DebugMessagePart *) part->Suc();
				while (iter)
				{
					if (iter->GetType() == ES_DebugMessagePart::AUXILIARY_EVAL_VARIABLE)
					{
						ES_EvalVariableMessagePart *evalvariable = (ES_EvalVariableMessagePart *) iter;
						variables[index].name = evalvariable->name;
						variables[index].name_length = evalvariable->name_length;
						variables[index].value = evalvariable->value;
						++index;
					}
					iter = (ES_DebugMessagePart *) iter->Suc();
				}
			}
			else
				variables_count = 0;
		}
		else
			variables = NULL;

		OP_STATUS status = Eval(body->tag, body->runtime_id, body->thread_id, body->frame_index, script.CStr(), script.Length(), variables, variables_count);
		delete[] variables;
		if (status == OpStatus::ERR)
		{
			ES_DebugValue value;
			return EvalReply(body->tag, EVAL_STATUS_ABORTED, value);
		}
		else
			return status;
	}
	else if (part->GetType() == ES_DebugMessagePart::BODY_EXAMINE)
	{
		ES_ExamineMessagePart *body = (ES_ExamineMessagePart *) part;
		return Examine(body->tag, body->runtime_id, body->in_scope, body->thread_id, body->frame_index, body->objects_count, body->object_ids);
	}
	else if (part->GetType() == ES_DebugMessagePart::BODY_BACKTRACE)
	{
		ES_BacktraceMessagePart *body = (ES_BacktraceMessagePart *) part;
		return Backtrace(body->tag, body->runtime_id, body->thread_id, body->max_frames);
	}
	else if (part->GetType() == ES_DebugMessagePart::BODY_ADD_BREAKPOINT || part->GetType() == ES_DebugMessagePart::BODY_REMOVE_BREAKPOINT)
	{
		ES_ChangeBreakpointMessagePart *body = (ES_ChangeBreakpointMessagePart *) part;

		if (body->add)
			return AddBreakpoint(body->id, body->bpdata);
		else
			return RemoveBreakpoint(body->id);
	}
	else if (part->GetType() == ES_DebugMessagePart::BODY_SET_CONFIGURATION)
	{
		ES_SetConfigurationMessagePart *body = (ES_SetConfigurationMessagePart *) part;
		return SetStopAt(body->stop_at_script, body->stop_at_exception, body->stop_at_error, body->stop_at_abort, body->stop_at_gc);
	}
	else if (part->GetType() == ES_DebugMessagePart::BODY_BREAK)
	{
		ES_BreakMessagePart *body = (ES_BreakMessagePart *) part;
		return Break(body->runtime_id, body->thread_id);
	}

	OP_ASSERT(FALSE);

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugFrontend::Closed()
{
	return Detach();
}

static void
ES_RemoteAddRuntimeInformationL(ES_DebugMessage *message, const ES_DebugRuntimeInformation *runtimeinfo)
{
	if (runtimeinfo)
	{
		ES_RuntimeInformationMessagePart *part = new (ELeave) ES_RuntimeInformationMessagePart;
		message->AddPart(part);

		part->dbg_runtime_id = runtimeinfo->dbg_runtime_id;
		part->dbg_window_id = runtimeinfo->dbg_window_id;
		ES_DebugMessagePart::SetString16L(part->framepath, part->framepath_length, runtimeinfo->framepath);
		ES_DebugMessagePart::SetString16L(part->documenturi, part->documenturi_length, runtimeinfo->documenturi);
	}
}

void
ES_RemoteDebugFrontend::NewScriptL(unsigned dbg_runtime_id, ES_DebugScriptData *data, const ES_DebugRuntimeInformation *runtimeinfo)
{
	ES_DebugMessage *message = current_message = new (ELeave) ES_DebugMessage;

	ES_NewScriptMessagePart *body = new (ELeave) ES_NewScriptMessagePart;
	message->AddPart(body);

	body->runtime_id = dbg_runtime_id;
	body->script_id = data->script_no;

	if (uni_str_eq(data->type, "inline"))
		body->script_type = ES_NewScriptMessagePart::SCRIPT_TYPE_INLINE;
	else if (uni_str_eq(data->type, "linked"))
		body->script_type = ES_NewScriptMessagePart::SCRIPT_TYPE_LINKED;
	else if (uni_str_eq(data->type, "generated"))
		body->script_type = ES_NewScriptMessagePart::SCRIPT_TYPE_GENERATED;
	else
		body->script_type = ES_NewScriptMessagePart::SCRIPT_TYPE_OTHER;

	ES_DebugMessagePart::SetString16L(body->source, body->source_length, data->source);

	if (body->script_type == ES_NewScriptMessagePart::SCRIPT_TYPE_LINKED)
		ES_DebugMessagePart::SetString16L(body->uri, body->uri_length, data->url);

	ES_RemoteAddRuntimeInformationL(message, runtimeinfo);

	current_message = NULL;

	SendL(message);
}

void
ES_RemoteDebugFrontend::ThreadStartedL(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo)
{
	ES_DebugMessage *message = current_message = new (ELeave) ES_DebugMessage;

	ES_ThreadStartedMessagePart *body = new (ELeave) ES_ThreadStartedMessagePart;
	message->AddPart(body);

	body->runtime_id = dbg_runtime_id;
	body->thread_id = dbg_thread_id;
	body->parent_thread_id = dbg_parent_thread_id;
	body->thread_type = type;

	if (type == THREAD_TYPE_EVENT)
	{
		ES_DebugMessagePart::SetString16L(body->event_namespace_uri, body->event_namespace_uri_length, namespace_uri);
		ES_DebugMessagePart::SetString16L(body->event_type, body->event_type_length, event_type);
	}

	ES_RemoteAddRuntimeInformationL(message, runtimeinfo);

	current_message = NULL;

	SendL(message);
}

static void
ES_RemoteAddReturnValuesL(ES_DebugMessage *message, ES_DebugReturnValue *dbg_return_value, BOOL is_exception)
{
	ES_DebugReturnValue *iter = dbg_return_value;

	while (iter)
	{
		if (iter->function.id == 0 && is_exception)
		{
			ES_ExceptionThrownMessagePart *exceptionthrown = new (ELeave) ES_ExceptionThrownMessagePart;
			message->AddPart(exceptionthrown);

			ES_DebugMessagePart::SetValueL(exceptionthrown->exception, iter->value);

			if (iter->value.type == VALUE_OBJECT)
				message->AddObjectInfoL(iter->value.value.object);
		}
		else
		{
			ES_ReturnValueMessagePart *returnvalue = new (ELeave) ES_ReturnValueMessagePart;
			message->AddPart(returnvalue);

			returnvalue->function_object_id = iter->function.id;
			ES_DebugMessagePart::SetValueL(returnvalue->value, iter->value);

			message->AddObjectInfoL(iter->function);

			if (iter->value.type == VALUE_OBJECT)
				message->AddObjectInfoL(iter->value.value.object);
		}

		iter = iter->next;
	}
}

void
ES_RemoteDebugFrontend::ThreadFinishedL(unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus status, ES_DebugReturnValue *dbg_return_value)
{
	ES_DebugMessage *message = current_message = new (ELeave) ES_DebugMessage;

	ES_ThreadFinishedMessagePart *body = new (ELeave) ES_ThreadFinishedMessagePart;
	message->AddPart(body);

	body->runtime_id = dbg_runtime_id;
	body->thread_id = dbg_thread_id;
	body->status = status;

	ES_RemoteAddReturnValuesL(message, dbg_return_value, status == THREAD_STATUS_EXCEPTION);
	delete dbg_return_value;

	current_message = NULL;

	SendL(message);
}

void
ES_RemoteDebugFrontend::StoppedAtL(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data)
{
	ES_DebugMessage *message = current_message = new (ELeave) ES_DebugMessage;

	ES_StoppedAtMessagePart *body = new (ELeave) ES_StoppedAtMessagePart;
	message->AddPart(body);

	body->runtime_id = dbg_runtime_id;
	body->thread_id = dbg_thread_id;
	body->position = data.position;

	ES_RemoteAddReturnValuesL(message, data.dbg_return_value, FALSE);
	delete data.dbg_return_value;

	if (data.exception)
	{
		ES_ExceptionThrownMessagePart *exceptionthrown = new (ELeave) ES_ExceptionThrownMessagePart;
		message->AddPart(exceptionthrown);

		ES_DebugMessagePart::SetValueL(exceptionthrown->exception, *data.exception);

		if (data.exception->type == VALUE_OBJECT)
			message->AddObjectInfoL(data.exception->value.object);
	}

	if (data.breakpoint_id != 0)
	{
		ES_BreakpointTriggeredMessagePart *breakpointtriggered = new (ELeave) ES_BreakpointTriggeredMessagePart;
		message->AddPart(breakpointtriggered);

		breakpointtriggered->id = data.breakpoint_id;
	}

	if (data.heapdata)
	{
		ES_HeapStatisticsMessagePart *heapstatistics = new (ELeave) ES_HeapStatisticsMessagePart;
		message->AddPart(heapstatistics);

		heapstatistics->load_factor = data.heapdata->load_factor;
		heapstatistics->unsigneds[0] = data.heapdata->num_collections;
		heapstatistics->unsigneds[1] = data.heapdata->ms_collecting;
		heapstatistics->unsigneds[2] = data.heapdata->bytes_heap_limit;
		heapstatistics->unsigneds[3] = data.heapdata->bytes_allocated;
		heapstatistics->unsigneds[4] = data.heapdata->bytes_reclaimed;
		heapstatistics->unsigneds[5] = data.heapdata->bytes_in_heap;
		heapstatistics->unsigneds[6] = data.heapdata->bytes_in_heap_peak;
		heapstatistics->unsigneds[7] = data.heapdata->bytes_live_after_gc;
		heapstatistics->unsigneds[8] = data.heapdata->bytes_free_after_gc;
		heapstatistics->unsigneds[9] = data.heapdata->objects_live_after_gc;
		heapstatistics->unsigneds[10] = data.heapdata->objects_free_after_gc;
	}

	current_message = NULL;

	SendL(message);
}

void
ES_RemoteDebugFrontend::BacktraceReplyL(unsigned tag, unsigned length, ES_DebugStackFrame *stack)
{
	ES_DebugMessage *message = current_message = new (ELeave) ES_DebugMessage;

	ES_BacktraceReplyMessagePart *body = new (ELeave) ES_BacktraceReplyMessagePart;
	message->AddPart(body);

	body->tag = tag;
	body->stack_length = length;
	body->stack = new (ELeave) ES_DebugStackFrame[length];

	for (unsigned index = 0; index < length; ++index)
	{
		body->stack[index] = stack[index];
		body->stack[index].function.info = NULL;

		message->AddObjectInfoL(stack[index].function);
	}

	current_message = NULL;

	SendL(message);
}

void
ES_RemoteDebugFrontend::EvalReplyL(unsigned tag, EvalStatus status, const ES_DebugValue &result)
{
	ES_DebugMessage *message = current_message = new (ELeave) ES_DebugMessage;

	ES_EvalReplyMessagePart *body = new (ELeave) ES_EvalReplyMessagePart;
	message->AddPart(body);

	body->tag = tag;
	body->status = status;

	if (status == EVAL_STATUS_FINISHED || status == EVAL_STATUS_EXCEPTION)
	{
		ES_DebugReturnValue returnvalue;

		returnvalue.function.id = 0;
		returnvalue.value = result;
		returnvalue.next = NULL;

		ES_RemoteAddReturnValuesL(message, &returnvalue, status == EVAL_STATUS_EXCEPTION);
	}

	current_message = NULL;

	SendL(message);
}

void
ES_RemoteDebugFrontend::ExamineReplyL(unsigned tag, unsigned objects_count, ES_DebugObjectProperties *objects)
{
	ES_DebugMessage *message = current_message = new (ELeave) ES_DebugMessage;

	ES_ExamineReplyMessagePart *body = new (ELeave) ES_ExamineReplyMessagePart;
	message->AddPart(body);

	body->tag = tag;

	for (unsigned oindex = 0; oindex < objects_count; ++oindex)
	{
		ES_DebugObjectProperties &properties = objects[oindex];
		unsigned properties_count = properties.properties_count;

		body->AddObjectL(properties.object.id, properties_count);
		message->AddObjectInfoL(properties.object);

		for (unsigned pindex = 0; pindex < properties_count; ++pindex)
		{
			body->SetPropertyL(properties.properties[pindex].name, properties.properties[pindex].value);

			if (properties.properties[pindex].value.type == VALUE_OBJECT)
				message->AddObjectInfoL(properties.properties[pindex].value.value.object);
		}
	}

	current_message = NULL;

	SendL(message);
}

void
ES_RemoteDebugFrontend::SendHelloL()
{
	ES_DebugMessage *message = current_message = new (ELeave) ES_DebugMessage;

	ES_HelloMessagePart *body = new (ELeave) ES_HelloMessagePart;
	message->AddPart(body);

	char buffer[256]; /* ARRAY OK jl 2008-02-07 */
	unsigned length = g_uaManager->GetUserAgentStr(buffer, sizeof buffer, UA_Opera);

	ES_DebugMessagePart::SetString8L(body->os, body->os_length, g_op_system_info->GetOSStr(UA_Opera));
	ES_DebugMessagePart::SetString16L(body->platform, body->platform_length, g_op_system_info->GetPlatformStr());
	ES_DebugMessagePart::SetString8L(body->useragent, body->useragent_length, buffer, length);

	current_message = NULL;

	SendL(message);
}

ES_RemoteDebugBackend::ES_RemoteDebugBackend()
{
}

/* virtual */
ES_RemoteDebugBackend::~ES_RemoteDebugBackend()
{
}

OP_STATUS
ES_RemoteDebugBackend::Construct(ES_DebugFrontend *new_frontend, BOOL active, const OpStringC &address, int port)
{
	frontend = new_frontend;

	if (active)
		return Connect(address, port);
	else
		return Listen(port);
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::AttachToAll()
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::AttachTo(ES_Runtime *runtime, BOOL debug_window)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::Continue(unsigned dbg_runtime_id, ES_DebugFrontend::Mode new_mode)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::Backtrace(unsigned tag, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned max_frames)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::Eval(unsigned tag, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned frame_index, const uni_char *string, unsigned string_length, ES_DebugVariable *variables, unsigned variables_count)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::AddBreakpoint(unsigned id, const ES_DebugBreakpointData &data)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::RemoveBreakpoint(unsigned id)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::SetStopAt(StopType stop_type, BOOL value)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::GetStopAt(StopType stop_type, BOOL &return_value)
{
	return_value = FALSE;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::Connected()
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::Received(ES_DebugMessage *message)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_RemoteDebugBackend::Closed()
{
	return OpStatus::OK;
}

#endif // ECMASCRIPT_REMOTE_DEBUGGER
