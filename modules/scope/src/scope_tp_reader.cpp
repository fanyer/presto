/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_tp_reader.h"

#include "modules/scope/src/scope_tp_message.h"
#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/protobuf_input.h"
#include "modules/scope/src/generated/g_scope_service_interface.h"
#include "modules/scope/src/generated/g_scope_manager.h"
//#include "modules/encodings/decoders/inputconverter.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/scope/src/scope_manager.h"

/* OpScopeTPReader */

OpScopeTPReader::OpScopeTPReader(BOOL continous, unsigned int version)
	: enabled(TRUE)
	, continous(continous)
	, error(NoError)
	, state(version == 0 ? STP0_Message : Message)
	, incoming(NULL)
	, stp0_incoming(NULL)
	, chunksize(0)
	, string_length(0)
	, payload_format(OpScopeTPHeader::None)
	, has_service_field(FALSE)
	, has_command_field(FALSE)
	, has_type_field(FALSE)
	, has_payload_field(FALSE)
	, ignore_size(0)
	, protobuf_limit(-1)
	, chunk_size(0)
	, body_size(0)
	, stp0_size(0)
	, version(version)
	, message_version(0)
{
}

OpScopeTPReader::~OpScopeTPReader()
{
	OP_DELETEA(stp0_incoming);
}

OP_STATUS
OpScopeTPReader::Construct(ByteBuffer *buffer)
{
	if (buffer == NULL)
		return OpStatus::ERR_NULL_POINTER;
	incoming = buffer;
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPReader::Reset()
{
	error = NoError;
	state = version == 0 ? STP0_Message : Message;
	OP_ASSERT(incoming != NULL);
	payload.Clear();
	incoming->Clear();
	OP_DELETEA(stp0_incoming);
	stp0_incoming = NULL;
	stp0_size = 0;
	chunksize = 0;
	message_version = 0;
	return OpStatus::OK;
}

void
OpScopeTPReader::SetProtocolVersion(unsigned int ver, BOOL force_change)
{
	version = ver;
	if (force_change)
		state = version == 0 ? STP0_Message : Message;
}

OP_STATUS
OpScopeTPReader::OnDataReady(const char *data, size_t len, BOOL parse_message)
{
	OP_ASSERT(data != NULL);
	OP_ASSERT(len > 0);
	RETURN_IF_ERROR(incoming->AppendBytes(data, len));
	if (enabled && parse_message)
	{
		do
		{
			RETURN_IF_ERROR(ParseStream());
		}
		while (continous && enabled);
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPReader::OnDataReady()
{
	if (!enabled)
		return OpStatus::OK;

	do
	{
		RETURN_IF_ERROR(ParseStream());
	}
	while (continous && enabled);
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeTPReader::PrepareMessage(OpScopeTPMessage &)
{
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPReader::ParseNextMessage()
{
	if (!enabled)
		return OpStatus::OK;

	return ParseStream();
}

OP_STATUS
OpScopeTPReader::ParseStream()
{
	OP_ASSERT(enabled);

	while (true)
	{
		switch (state)
		{
		case Message:
		{
			if (version == 0)
			{
				state = STP0_Message;
				continue;
			}
			else
				state = Prefix;
			message_version = 1;
		}

		case Prefix:
		{
			if (incoming->Length() < 4)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			char buffer[4]; // "STP" + ver // ARRAY OK 2009-05-05 jhoff
			incoming->Extract(0, 4, buffer);
			if (buffer[0] != 'S' || buffer[1] != 'T' || buffer[2] != 'P')
			{
				return OpStatus::ERR_PARSING_FAILED;
			}
			message.Clear();
			incoming->Consume(4);
			unsigned int incoming_version = buffer[3];
			// Check if we support the message
			if (incoming_version == 0)
			{
				// Support for STP/0 message wrapped in a STP/1 message
				message_version = 0;
			}
			else if (incoming_version > 1)
			{
				// TODO: Report it somewhere/somehow?
				state = IgnoreMessageSize;
				continue;
			}
			message.SetVersion((OpScopeTPMessage::MessageVersion)incoming_version);
			state = MessageSize;
		}

		case MessageSize:
		{
			unsigned int field_size;
			// FIXME: Field size is 64 bit
			BOOL3 result = ParseVarInt32(field_size);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			OP_ASSERT(field_size < (1u << 31));
			protobuf_limit = field_size;
			has_service_field = has_command_field = has_type_field = has_payload_field = FALSE;
			if (message_version == 0)
			{
				// Support for STP/0 message wrapped in a STP/1 message
				// The exception is the size entry for STP/0 which is skipped
				body_size = field_size;
				OP_ASSERT((body_size % 2) == 0);
				stp0_size = body_size / 2;
				state = STP0_Init;
				continue;
			}
			state = STPType;
		}

		case STPType:
		{
			if (incoming->Length() < 1)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			unsigned int type = incoming->Extract1(0);
			protobuf_limit -= 1;
			incoming->Consume(1);
			if (type == OpScopeTPMessage::STP_Call)
				message.SetTransportType(OpScopeTPMessage::STP_Call);
			else if (type == OpScopeTPMessage::STP_Response)
				message.SetTransportType(OpScopeTPMessage::STP_Response);
			else if (type == OpScopeTPMessage::STP_Event)
				message.SetTransportType(OpScopeTPMessage::STP_Event);
			else if (type == OpScopeTPMessage::STP_Error)
				message.SetTransportType(OpScopeTPMessage::STP_Error);
			else
			{
				state = IgnoreFields;
				continue;
			}
			state = Field;
		}

		case Field:
		{
			if (protobuf_limit == 0) // Did we reach the end of the field data?
			{
				protobuf_limit = -1; // Remove limit
				state = MessageDone;
				continue;
			}

			unsigned int field;
			BOOL3 result = ParseVarInt32(field);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			int type = OpProtobufWireFormat::DecodeFieldType32(field);
			int number = OpProtobufWireFormat::DecodeFieldNumber32(field);
			if (number == OpScopeTPMessage::Field_ServiceName)
			{
				if (type == OpProtobufWireFormat::LengthDelimited)
					state = Service;
			}
			else if (number == OpScopeTPMessage::Field_CommandID)
			{
				if (type == OpProtobufWireFormat::VarInt)
					state = Command;
			}
			else if (number == OpScopeTPMessage::Field_Type)
			{
				if (type == OpProtobufWireFormat::VarInt)
					state = Type;
			}
			else if (number == OpScopeTPMessage::Field_Status)
			{
				if (type == OpProtobufWireFormat::VarInt)
					state = Status;
			}
			else if (number == OpScopeTPMessage::Field_Tag)
			{
				if (type == OpProtobufWireFormat::VarInt)
					state = Tag;
			}
			else if (number == OpScopeTPMessage::Field_Payload)
			{
				if (type == OpProtobufWireFormat::LengthDelimited)
					state = ChunkSize;
				payload.Clear();
			}

			if (state == Field)
			{
				if (type == OpProtobufWireFormat::VarInt)
					state = IgnoreVarint;
				else if (type == OpProtobufWireFormat::Fixed32)
					state = IgnoreFixed32;
				else if (type == OpProtobufWireFormat::Fixed64)
					state = IgnoreFixed64;
				else if (type == OpProtobufWireFormat::LengthDelimited)
					state = IgnoreLengthDelimited;
				else
					return OpStatus::ERR_PARSING_FAILED;
			}
			continue;
		}

		case Service:
		{
			BOOL3 result = ParseVarInt32(string_length);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			message.ServiceName().Empty();
			state = ServiceString;
		}

		case ServiceString:
		{
			BOOL3 result = ParseString(message.ServiceName(), string_length);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			has_service_field = TRUE;
			state = Field;
			continue;
		}

		case Command:
		{
			unsigned command_id;
			BOOL3 result = ParseVarInt32(command_id);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			message.SetCommandID(command_id);
			has_command_field = TRUE;
			state = Field;
			continue;
		}

		case Type:
		{
			unsigned int type;
			BOOL3 result = ParseVarInt32(type);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			if (type > OpScopeTPHeader::MessageTypeMax)
				return OpStatus::ERR;
			payload_format = static_cast<OpScopeTPMessage::MessageType>(type);
			has_type_field = TRUE;
			state = Field;
			continue;
		}

		case Status:
		{
			unsigned int status;
			BOOL3 result = ParseVarInt32(status);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			message.SetStatus(static_cast<OpScopeTPMessage::MessageStatus>(status));
			state = Field;
			continue;
		}

		case Tag:
		{
			unsigned int tag;
			BOOL3 result = ParseVarInt32(tag);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			message.SetTag(tag);
			state = Field;
			continue;
		}

		case ChunkSize:
		{
			BOOL3 result = ParseVarInt32(chunk_size);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			if (chunk_size == 0)
			{
				chunk_size = 0;
				state = Field;
				has_payload_field = TRUE;
				continue;
			}
			body_size = chunk_size;
			state = ChunkData;
		}

		case ChunkData:
		{
			if (incoming->Length() < chunk_size)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			RETURN_IF_ERROR(OpScopeCopy(*incoming, payload, chunk_size));
			incoming->Consume(chunk_size);
			protobuf_limit -= chunk_size;
			chunk_size = 0;
			has_payload_field = TRUE;
			state = Field;
			continue;
		}

		case MessageDone:
		{
			// TODO: Check for missing required fields
			if (!has_service_field || !has_command_field || !has_type_field || !has_payload_field)
			{
				// Missing fields, ignore this message
				// TODO: Should report about bad message to a callback.
				state = Message;
				continue;
			}
			RETURN_IF_ERROR(message.SetData(payload, payload_format));
			OP_ASSERT(message.Type() != OpScopeTPHeader::None);
			OP_ASSERT(message.Data() != NULL);
			PrepareMessage(message);
			OnMessageParsed(message);
			RETURN_IF_ERROR(message.Free());
			state = Message;
			return OpStatus::OK;
		}

		case STP0_Message:
		{
			if (version > 0)
			{
				state = Message;
				continue;
			}
			message.Clear();
			state = STP0_Size;
			message_version = 0;
		}

		case STP0_Size:
		{
			BOOL3 result = ParseSTP0Size(stp0_size);
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			else if (result == NO)
				return STPStatus::ERR_PARSING_FAILED;
			body_size = stp0_size*2;
			state = STP0_Init;
		}

		case STP0_Init:
		{
			OP_DELETEA(stp0_incoming);
			stp0_incoming = OP_NEWA(uni_char, stp0_size);
			RETURN_OOM_IF_NULL(stp0_incoming);
			message.SetVersion(OpScopeTPMessage::Version_0);
			message.SetTransportType(OpScopeTPMessage::STP_Call);
			message.SetServiceName(UNI_L(""));
			message.SetCommandID(0);
			message.SetStatus(OpScopeTPMessage::OK);
			RETURN_IF_ERROR(message.Free());
			message.SetTag(0);
			payload.Clear();
			state = STP0_Data;
		}

		case STP0_Data:
		{
			OP_ASSERT(stp0_incoming != NULL);
			if (incoming->Length() < body_size)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			incoming->Extract(0, body_size, reinterpret_cast<char *>(stp0_incoming));
			incoming->Consume(body_size);
#ifndef OPERA_BIG_ENDIAN
			OpProtobufUtils::ByteSwap(stp0_incoming, stp0_size);
#endif // OPERA_BIG_ENDIAN

			// Now find the service name and separate it from the data that follows
			const uni_char *stp0_data = NULL;
			for (unsigned int i = 0; i < stp0_size; ++i)
			{
				if (stp0_incoming[i] == ' ')
				{
					stp0_incoming[i] = '\0';
					stp0_data = stp0_incoming + i + 1;
					break;
				}
			}
			OP_STATUS status = OpStatus::OK;
			if (stp0_data != NULL)
			{
				unsigned int data_size = stp0_size - (stp0_data - stp0_incoming);
				RETURN_IF_ERROR(message.SetServiceName(stp0_incoming));

				// Check if it contains an STP/x header, if so decode extra headers.
				if (stp0_incoming[0] != '*' && data_size >= 4 && uni_strncmp(stp0_data, UNI_L("STP/"), 4) == 0)
				{
					status = ParseExtendedStp0(message, stp0_data, data_size);
				}
				else
				{
					RETURN_IF_ERROR(payload.AppendBytes(reinterpret_cast<const char *>(stp0_data), data_size*2));

					// Sniff information
					if (stp0_incoming[0] == '*')
						payload_format = OpScopeTPMessage::JSON; // FIXME: The JSON type is just a temporary solution as the Scope type is removed
					else if (data_size > 0)
					{
						if (uni_strncmp(stp0_data, "<?xml", 5) == 0)
							payload_format = OpScopeTPMessage::XML;
						else if (stp0_data[0] == '[')
							payload_format = OpScopeTPMessage::JSON;
					}
				}
			}
			OP_DELETEA(stp0_incoming);
			stp0_incoming = NULL;

			chunk_size = stp0_size = 0;
			state = version == 0 ? STP0_Message : Message;

			if (stp0_data != NULL && OpStatus::IsSuccess(status))
			{
				RETURN_IF_ERROR(message.SetData(payload, payload_format));
				payload.Clear();
				PrepareMessage(message);
				OnMessageParsed(message);
				RETURN_IF_ERROR(message.Free());
			}

			if (stp0_data == NULL)
				return OpStatus::ERR_PARSING_FAILED;
			return OpStatus::OK;
		}

		case IgnoreVarint:
		{
			unsigned int tmp;
			BOOL3 result = ParseVarInt32(tmp);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			state = Field;
			continue;
		}

		case IgnoreFixed32:
		{
			unsigned int tmp;
			BOOL3 result = ParseFixed32(tmp);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			state = Field;
			continue;
		}

		case IgnoreFixed64:
		{
			UINT64 tmp;
			BOOL3 result = ParseFixed64(tmp);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			state = Field;
			continue;
		}

		case IgnoreLengthDelimited:
		{
			BOOL3 result = ParseVarInt32(ignore_size);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			state = IgnoreLengthDelimitedData;
		}

		case IgnoreLengthDelimitedData:
		{
			if (protobuf_limit >= 0)
			{
				OP_ASSERT(protobuf_limit >= (int)ignore_size);
				if (protobuf_limit < (int)ignore_size) // Cannot skip length-delimited data as it is crosses the limit
					return OpStatus::ERR_PARSING_FAILED;
			}
			if (incoming->Length() < ignore_size)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			incoming->Consume(ignore_size);
			if (protobuf_limit > 0)
				protobuf_limit -= ignore_size;
			state = Field;
			continue;
		}

		case IgnoreMessageSize:
		{
			// TODO: Should consume data as they arrive, better for iterative parsing
			unsigned int field_size;
			// FIXME: Field size is 64 bit
			BOOL3 result = ParseVarInt32(field_size);
			if (result == NO)
				return OpStatus::ERR_PARSING_FAILED;
			if (result == MAYBE)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			OP_ASSERT(field_size < (1u << 31));
			protobuf_limit = field_size;
			state = IgnoreFields;
		}

		case IgnoreFields:
		{
			// TODO: Should consume data as they arrive, better for iterative parsing
			if ((int)incoming->Length() < protobuf_limit)
				return STPStatus::ERR_NOT_ENOUGH_DATA;
			incoming->Consume(protobuf_limit);
			protobuf_limit = -1;
			state = Message;
			continue;
		}

		default:
			OP_ASSERT(!"Invalid parser state");
		}
		break;
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPReader::ParseExtendedStp0(OpScopeTPMessage &message, const uni_char *str, int len)
{
	// Skip "STP/"
	const uni_char *hdr = str + 4;
	len -= 4;

	int chars_read;
	int stp_ver = OpProtobufUtils::ParseDelimitedInteger(hdr, len, ' ', chars_read);
	if (chars_read <= 0)
		return OpStatus::ERR_PARSING_FAILED;
	hdr += chars_read;
	len -= chars_read;

	int hdr_size = OpProtobufUtils::ParseDelimitedInteger(hdr, len, ' ', chars_read);
	if (chars_read <= 0)
		return OpStatus::ERR_PARSING_FAILED;
	hdr += chars_read;
	len -= chars_read;

	if (stp_ver != 1)
		return OpStatus::ERR_PARSING_FAILED; // We only support version 1
	if (hdr_size > len)
		return OpStatus::ERR_PARSING_FAILED; // Header size is incorrect

	if (hdr[0] != '[' && hdr[0] != '{')
		return OpStatus::ERR_PARSING_FAILED; // Invalid header data

	const uni_char *stp0_payload = hdr + hdr_size;
	unsigned int stp0_payload_len = len - hdr_size;

	ByteBuffer hdr_buf;
	OpProtobufUtils::Convert(hdr_buf, hdr, hdr_size);
	TempBuffer hdr_data;
	RETURN_IF_ERROR(OpProtobufUtils::ConvertUTF8toUTF16(hdr_data, hdr_buf));

	OpJSONInputStream stream;
	RETURN_IF_ERROR(stream.Construct(hdr_data.GetStorage(), hdr_data.Length()));
	OpScopeService_SI::TransportMessage tm;
	OpProtobufInstanceProxy proxy(tm.GetMessageDescriptor(g_scope_manager->GetDescriptorSet().scope_service_), &tm);
	RETURN_IF_ERROR(stream.Read(proxy));

	if (tm.GetStpType() != OpScopeTPMessage::STP_Call &&
		tm.GetStpType() != OpScopeTPMessage::STP_Error &&
		tm.GetStpType() != OpScopeTPMessage::STP_Event &&
		tm.GetStpType() != OpScopeTPMessage::STP_Response)
	{
		// TODO: Respond with error message.
		return OpStatus::ERR_NOT_SUPPORTED;
	}

	RETURN_IF_ERROR(message.SetServiceName(tm.GetServiceName()));
	message.SetTransportType(OpScopeTPHeader::STPType(tm.GetStpType()));
	message.SetVersion((OpScopeTPHeader::MessageVersion)stp_ver);
	unsigned format = tm.GetFormat();
	if (format > OpScopeTPHeader::MessageTypeMax)
		return OpStatus::ERR;
	payload_format = (OpScopeTPHeader::MessageType)format;
	message.SetCommandID(tm.GetCommandID());
	if (tm.HasTag())
		message.SetTag(tm.GetTag());

	// Convert from UTF-16 to UTF-8
	payload.Clear();
	RETURN_IF_ERROR(OpProtobufUtils::Convert(payload, stp0_payload, stp0_payload_len));
	return OpStatus::OK;
}

BOOL3
OpScopeTPReader::ParseVarInt32(unsigned int &num)
{
	if (incoming->Length() < 1)
		return MAYBE;
	char buffer[5]; // ARRAY OK 2009-05-05 jhoff
	unsigned int buffer_len = MIN(sizeof(buffer), incoming->Length());
	incoming->Extract(0, buffer_len, buffer);
	int len;
	int value;
	OP_STATUS status = OpProtobufWireFormat::DecodeVarInt32(buffer, buffer + buffer_len, len, value);
	if (OpStatus::IsError(status))
	{
		if (len == (int)buffer_len) // If we reached the end of the buffer we need to wait for more data
			return MAYBE;
		else
			return NO;
	}
	if (protobuf_limit >= 0 && protobuf_limit < len) // Cannot read varint value in current limit
		return NO;

	OP_ASSERT((unsigned int)len <= incoming->Length());
	num = value;
	incoming->Consume(len);
	if (protobuf_limit >= 0)
		protobuf_limit -= len;
	return YES;
}

BOOL3
OpScopeTPReader::ParseFixed32(unsigned int &num)
{
	if (incoming->Length() < 4)
		return MAYBE;
	if (protobuf_limit >= 0)
	{
		if (protobuf_limit < 4) // Cannot read 32bit value in current limit
			return NO;
	}
	char buffer[4]; // ARRAY OK 2009-05-05 jhoff
	incoming->Extract(0, 4, buffer);
	OP_STATUS status = OpProtobufWireFormat::DecodeFixed32(buffer, num);
	if (OpStatus::IsError(status))
		return NO;
	incoming->Consume(4);
	if (protobuf_limit > 0)
	{
		OP_ASSERT(4 <= protobuf_limit);
		protobuf_limit -= 4;
	}
	return YES;
}

BOOL3
OpScopeTPReader::ParseFixed64(UINT64 &num)
{
	if (incoming->Length() < 8)
		return MAYBE;
	if (protobuf_limit >= 0)
	{
		if (protobuf_limit < 8) // Cannot read 64bit value in current limit
			return NO;
	}
	char buffer[8]; // ARRAY OK 2009-05-05 jhoff
	incoming->Extract(0, 8, buffer);
	OP_STATUS status = OpProtobufWireFormat::DecodeFixed64(buffer, num);
	if (OpStatus::IsError(status))
		return NO;
	incoming->Consume(8);
	if (protobuf_limit >= 0)
		protobuf_limit -= 8;
	return YES;
}

BOOL3
OpScopeTPReader::ParseString(OpString &str, unsigned int len)
{
	if (protobuf_limit >= 0)
	{
		if (protobuf_limit < (int)len) // Cannot read string value in current limit
			return NO;
	}
	if (incoming->Length() < len)
		return MAYBE;
	OpHeapArrayAnchor<char> buffer(OP_NEWA(char, len));
	RETURN_VALUE_IF_NULL(buffer.Get(), NO);
	incoming->Extract(0, len, buffer.Get());
	RETURN_VALUE_IF_ERROR(OpProtobufUtils::Convert(str, buffer.Get(), len), NO);
	incoming->Consume(len);
	if (protobuf_limit > 0)
		protobuf_limit -= len;
	return YES;
}

BOOL3
OpScopeTPReader::ParseSTP0Size(unsigned int &size)
{
	if (incoming->Length() < 4) // Need at least two UTF-16 characters (4 bytes)
		return MAYBE;
	const unsigned int max_size = (10+1);
	uni_char buffer[max_size]; // ARRAY OK 2009-05-05 jhoff
	unsigned int buffer_len = MIN(max_size, incoming->Length()/2);
	incoming->Extract(0, buffer_len*2, reinterpret_cast<char *>(&buffer[0]));
#ifndef OPERA_BIG_ENDIAN
	OpProtobufUtils::ByteSwap(buffer, buffer_len);
#endif // OPERA_BIG_ENDIAN

	// TODO: Should scan for SPACE character, if it is not found there is no need to try and parse an integer

	int chars_read;
	size = OpProtobufUtils::ParseDelimitedInteger(buffer, buffer_len, ' ', chars_read);
	if (chars_read == 0)
		return MAYBE;
	else if (chars_read < 0)
		return NO;
	unsigned int len = chars_read*2;
	incoming->Consume(len);
	return YES;
}

/*	unsigned int tmp_size = 0;
	unsigned int i;
	for (i = 0; i < buffer_len; ++i)
	{
		if (buffer[i] == ' ')
			break;
		if (buffer[i] < '0' || buffer[i] > '9')
			return NO;
		tmp_size = tmp_size*10 + (buffer[i] - '0');
	}
	if (i == buffer_len) // We cannot know the size until the space terminator is reached
		return MAYBE;

	value = tmp_size;

	chars_read = i + 1;
	return YES;*/

OP_STATUS 
OpScopeTPReader::SetError(Error err)
{
	error = err;
	OnError(err);
	return OpStatus::ERR;
}

/*static*/
OP_STATUS
OpScopeTPReader::ErrorToText(OpString &text, Error error)
{
	const uni_char *error_text = NULL;
	switch(error)
	{
	case NoError:
		error_text = UNI_L("NoError");
		break;
	case StatusError:
		error_text = UNI_L("StatusError");
		break;
	case InvalidData:
		error_text = UNI_L("InvalidData");
		break;
	case OutdatedProtocol:
		error_text = UNI_L("OutdatedProtocol");
		break;
	case InvalidProtocolVersion:
		error_text = UNI_L("InvalidProtocolVersion");
		break;
	case InvalidEncoding:
		error_text = UNI_L("InvalidEncoding");
		break;
	default:
		OP_ASSERT(!"Missing case for Error value");
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(text.Set(error_text));
	return OpStatus::OK;
}

#endif // SCOPE_SUPPORT
