/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/scope_tp_writer.h"

#include "modules/scope/src/scope_tp_message.h"
#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/protobuf/src/protobuf_utils.h"
#include "modules/protobuf/src/protobuf_output.h"
#include "modules/scope/src/generated/g_scope_service_interface.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/generated/g_scope_manager.h"
//#include "modules/encodings/encoders/outputconverter.h"

/* OpScopeTPWriter */

OpScopeTPWriter::OpScopeTPWriter(BOOL enabled, unsigned int version)
	: version(version)
	, outgoing_message(NULL)
	, bytes_remaining(0)
	, block(NULL)
	, chunk_len(0)
	, payload_length(0)
	, enabled(enabled)
	, deferred(TRUE)
	, state(State_Idle)
{
	OP_ASSERT(version == 0 || version == 1); // We only support STP/0 and STP/1
	prefix[0] = 'S';
	prefix[1] = 'T';
	prefix[2] = 'P';
}

OpScopeTPWriter::~OpScopeTPWriter()
{
	Reset();
}

OP_STATUS
OpScopeTPWriter::Reset()
{
	message_queue.Clear();

	return ClearOutgoingMessage();
}

OP_STATUS
OpScopeTPWriter::EnqueueMessage(OpAutoPtr<OpScopeTPMessage> &msg)
{
	OP_ASSERT(msg.get() != NULL && msg->Type() != OpScopeTPHeader::None);
	if (!(msg.get() != NULL && msg->Type() != OpScopeTPHeader::None))
		return OpStatus::ERR;
	msg->Into(&message_queue);
	msg.release();

	if (enabled)
	{
		while (!message_queue.Empty() && state == State_Idle)
		{
			deferred = FALSE;
			OP_STATUS status = ProcessOutgoingQueue();
			if (OpStatus::IsError(status))
			{
				deferred = TRUE;
				return status;
			}
		}
	}
	deferred = TRUE;
	return OpStatus::OK;
}

void
OpScopeTPWriter::SetProtocolVersion(unsigned int ver)
{
	version = ver;
}

OP_STATUS
OpScopeTPWriter::SetEnabled(BOOL enable)
{
	enabled = enable;
	if (enabled)
	{
		while (!message_queue.Empty() && state == State_Idle)
		{
			deferred = FALSE;
			OP_STATUS status = ProcessOutgoingQueue();
			if (OpStatus::IsError(status))
			{
				deferred = TRUE;
				return status;
			}
		}
	}
	deferred = TRUE;
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPWriter::ProcessOutgoingQueue()
{
	// TODO: If something fails the current message should be skipped
	//       and internal state reset.
	OpScopeTPMessage *msg = outgoing_message;
	if (msg == NULL)
	{
		outgoing_message = msg = (OpScopeTPMessage *)message_queue.First();
		if (msg == NULL)
		{
			state = State_Idle;
			return OpStatus::OK;
		}
		msg->Out();
		// Report that message has been taken out of queue
		RETURN_IF_ERROR(OnMessageDequeued(*msg));

		bytes_remaining = 0;
		header.Clear();

		if (version == 0)
			state = State_Stp0;
		else
			state = State_Header;
	}
	if (msg != NULL && state != State_Idle)
	{
		return ProcessMessage();
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeTPWriter::ProcessMessage()
{
	if (state == State_Idle || outgoing_message == NULL)
		return OpStatus::OK; // Cannot process message while being idle

	if (state == State_Stp0)
	{
		const OpString &name = outgoing_message->ServiceName();
		if (name.Length() == 0)
		{
			// There is no service name stored so we cannot send it as STP/0
			OP_DELETE(outgoing_message);
			outgoing_message = NULL;
			return OpStatus::ERR;
		}
		int buf_len = 0;
		if (outgoing_message->Version() >= 1)
		{
			// TODO: This needs to be optimized, there should be a JSON encoder which
			//       can encode to UTF-16 directly, that way there is no need to convert
			//       back and forth between UTF-16/UTF-8.

			// Compatibility layer, encodes STP/1 message over STP/0
			OpScopeService_SI::TransportMessage tm;

			// The service is transferred to the new header and the service name in the STP/0 message is set to always go to scope
			RETURN_IF_ERROR(tm.SetServiceName(outgoing_message->ServiceName()));
			RETURN_IF_ERROR(outgoing_message->SetServiceName(UNI_L("scope")));

			tm.SetFormat(outgoing_message->Type());
			tm.SetCommandID(outgoing_message->CommandID());
			if (outgoing_message->TransportType() == OpScopeTPMessage::STP_Response ||
				outgoing_message->TransportType() == OpScopeTPMessage::STP_Error)
				tm.SetTag(outgoing_message->Tag());
			if (outgoing_message->TransportType() == OpScopeTPMessage::STP_Error)
				tm.SetStatus(outgoing_message->Status());
			tm.SetStpType(outgoing_message->TransportType());

			ByteBuffer hdr_buf;
			OpProtobufInstanceProxy proxy(tm.GetMessageDescriptor(g_scope_manager->GetDescriptorSet().scope_service_), &tm);
			OpJSONOutputStream stream(hdr_buf);
			RETURN_IF_ERROR(stream.Write(proxy));

			// Convert header to UTF-16 so that we can calculate its size
			ByteBuffer hdr_buf_uni;
			RETURN_IF_ERROR(OpProtobufUtils::ConvertUTF8toUTF16(hdr_buf_uni, hdr_buf));
			OP_ASSERT(hdr_buf_uni.Length() % 2 == 0);

			// Add the header
			TempBuffer prefix;
			RETURN_IF_ERROR(prefix.Append(UNI_L("STP/")));
			RETURN_IF_ERROR(prefix.AppendUnsignedLong(outgoing_message->Version()));
			RETURN_IF_ERROR(prefix.Append(UNI_L(" ")));
			RETURN_IF_ERROR(prefix.AppendUnsignedLong(hdr_buf_uni.Length()/2));
			RETURN_IF_ERROR(prefix.Append(UNI_L(" ")));
			RETURN_IF_ERROR(OpProtobufUtils::Append(header, OpProtobufUtils::Bytes((const char *)prefix.GetStorage(), prefix.Length()*2)));
			RETURN_IF_ERROR(OpScopeCopy(hdr_buf_uni, header));

			// Convert UTF-8 data and update message
			if (outgoing_message->Data() != NULL)
				RETURN_IF_ERROR(OpProtobufUtils::ConvertUTF8toUTF16(header, *outgoing_message->Data()));

			// TODO: This should be replaced by a Move() function which ensure the most optimal way to move data from one container to another
			RETURN_IF_ERROR(outgoing_message->SetData(header, outgoing_message->Type()));
			header.Clear();

			OP_ASSERT(outgoing_message->Data()->Length() % 2 == 0); // Make sure it has even size
			buf_len = outgoing_message->Data()->Length() / 2;
		}
		else if (outgoing_message->Data() != NULL)
		{
			OP_ASSERT(outgoing_message->Data()->Length() % 2 == 0); // Make sure it has even size
			buf_len = outgoing_message->Data()->Length() / 2;
		}
		int data_len = name.Length() + 1 + buf_len;

		// Generate the number to figure out total size of data
		uni_char numbuf[12];	// ARRAY OK 2009-04-02 jborsodi
		uni_snprintf(numbuf, ARRAY_SIZE(numbuf), UNI_L("%d "), data_len);

		int num_len = uni_strlen(numbuf);
		int total_char_count = data_len + num_len;
		int total_len = total_char_count*2;
		OP_DELETEA(block);
		block = OP_NEWA(uni_char, total_char_count);
		if (block == NULL)
		{
			OP_DELETE(outgoing_message);
			outgoing_message = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		// Copy over number, name and data
		op_memcpy(block, numbuf, num_len*2);
		op_memcpy(block + num_len, name.CStr(), name.Length()*2);
		block[num_len + name.Length()] = ' ';
		const ByteBuffer *buf = outgoing_message->Data();
		if (buf != NULL)
		{
			// Extract data after number + name + SP
			buf->Extract(0, buf_len*2, (char *)(block + num_len + name.Length() + 1));
		}
#ifndef OPERA_BIG_ENDIAN
		OpProtobufUtils::ByteSwap(block, total_char_count);
#endif // OPERA_BIG_ENDIAN

		bytes_remaining = total_len;
		payload_length = 0;

		// Report that the message is about to be sent
		RETURN_IF_ERROR(OnMessageEncoded(*outgoing_message));

		RETURN_IF_ERROR(SendData((char *)block, total_len));
		return OpStatus::OK;
	}
	else if (state == State_Header)
	{
		// Prepare header
		OpProtobufByteBufferOutputRange out_range(header);
		OpProtobufBasicOutputStream out(&out_range);
		const OpString &name = outgoing_message->ServiceName();

		if (outgoing_message->Data() == NULL)
			payload_length = 0;
		else
			payload_length = outgoing_message->Data()->Length();

		// Write the type of message
		RETURN_IF_ERROR(out.WriteVarInt32(outgoing_message->TransportType()));

		// Write protobuf fields
		RETURN_IF_ERROR(out.WriteVarInt32(OpProtobufWireFormat::EncodeField32(OpProtobufWireFormat::LengthDelimited, OpScopeTPMessage::Field_ServiceName)));
		RETURN_IF_ERROR(out.WriteString(OpProtobufStringChunkRange(name)));

		RETURN_IF_ERROR(out.WriteVarInt32(OpProtobufWireFormat::EncodeField32(OpProtobufWireFormat::VarInt, OpScopeTPMessage::Field_CommandID)));
		RETURN_IF_ERROR(out.WriteVarInt32(outgoing_message->CommandID()));

		RETURN_IF_ERROR(out.WriteVarInt32(OpProtobufWireFormat::EncodeField32(OpProtobufWireFormat::VarInt, OpScopeTPMessage::Field_Type)));
		RETURN_IF_ERROR(out.WriteVarInt32(outgoing_message->Type()));

		if (outgoing_message->TransportType() == OpScopeTPMessage::STP_Error)
		{
			// Minor precaution: Error message should have a status
			OP_ASSERT(outgoing_message->Status() != OpScopeTPMessage::OK);
			RETURN_IF_ERROR(out.WriteVarInt32(OpProtobufWireFormat::EncodeField32(OpProtobufWireFormat::VarInt, OpScopeTPMessage::Field_Status)));
			RETURN_IF_ERROR(out.WriteVarInt32(outgoing_message->Status()));
		}
		else
		{
			// Minor precaution: Only error message should have a status
			OP_ASSERT(outgoing_message->Status() == OpScopeTPMessage::OK);
		}

		if (outgoing_message->TransportType() == OpScopeTPMessage::STP_Call ||
			outgoing_message->TransportType() == OpScopeTPMessage::STP_Response ||
			outgoing_message->TransportType() == OpScopeTPMessage::STP_Error)
		{
			RETURN_IF_ERROR(out.WriteVarInt32(OpProtobufWireFormat::EncodeField32(OpProtobufWireFormat::VarInt, OpScopeTPMessage::Field_Tag)));
			RETURN_IF_ERROR(out.WriteVarInt32(outgoing_message->Tag()));
		}
		else
		{
			// Minor precaution: Only calls, responses and errors should have a tag.
			OP_ASSERT(outgoing_message->Tag() == 0);
		}

		// Writes the field data + size of payload, the data for the payload is handled at a later stage
		RETURN_IF_ERROR(out.WriteVarInt32(OpProtobufWireFormat::EncodeField32(OpProtobufWireFormat::LengthDelimited, OpScopeTPMessage::Field_Payload)));
		RETURN_IF_ERROR(out.WriteVarInt32(payload_length));

		// Calculate size of message
		int header_length = header.Length() + payload_length;

		// Fill in STP outer header
		prefix[3] = version;
		int header_varint_size = 0;
		OpProtobufWireFormat::EncodeVarInt64(prefix + 4, sizeof(prefix) - 4, header_varint_size, header_length);

		// Report that the message is about to be sent
		RETURN_IF_ERROR(OnMessageEncoded(*outgoing_message));

		// Send STP outer header
		bytes_remaining = 4 + header_varint_size;
		return SendData(prefix, bytes_remaining);
	}
	else if (state == State_HeaderData)
	{
		// Send out all the chunks of the header
		OP_ASSERT(bytebuffer_chunk_idx < header.GetChunkCount());
		unsigned int bytebuffer_size;
		char *bytebuffer_chunk = header.GetChunk(bytebuffer_chunk_idx, &bytebuffer_size);
		bytes_remaining = bytebuffer_size;
		return SendData(bytebuffer_chunk, bytes_remaining);
	}
	else if (state == State_Chunk)
	{
		const ByteBuffer *buf = outgoing_message->Data();
		OP_ASSERT(buf != NULL);

		// Send out a chunk from the body
		OP_ASSERT(bytebuffer_chunk_idx < buf->GetChunkCount());
		unsigned int bytebuffer_size;
		char *bytebuffer_chunk = buf->GetChunk(bytebuffer_chunk_idx, &bytebuffer_size);
		bytes_remaining = bytebuffer_size;
		chunk_len = bytebuffer_size;
		return SendData(bytebuffer_chunk, bytes_remaining);
	}

	OP_ASSERT(!"Invalid state, should not happen");
	return OpStatus::ERR; // Invalid state
}

OP_STATUS
OpScopeTPWriter::OnDataSent(size_t len)
{
	OP_ASSERT(bytes_remaining != 0);

	if (state != State_Idle)
	{
		OP_ASSERT(len <= bytes_remaining);
		bytes_remaining -= len;
		if (bytes_remaining != 0)
			return OpStatus::OK;
	}

	if (state == State_Stp0)
	{
		OP_DELETEA(block);
		block = NULL;
		payload_length = 0;
		state = State_Idle;
	}
	else if (state == State_Header)
	{
		bytebuffer_chunk_idx = 0;
		state = State_HeaderData;
	}
	else if (state == State_HeaderData)
	{
		if (++bytebuffer_chunk_idx >= header.GetChunkCount())
		{
			if (payload_length == 0)
				state = State_Idle;
			else
			{
				bytebuffer_chunk_idx = 0;
				state = State_Chunk;
			}
		}
	}
	else if (state == State_Chunk)
	{
		OP_ASSERT(chunk_len <= (int)payload_length);
		payload_length -= chunk_len;
		OP_ASSERT(outgoing_message->Data() != NULL);
		if (++bytebuffer_chunk_idx >= outgoing_message->Data()->GetChunkCount())
		{
			if (payload_length == 0)
				state = State_Idle;
		}
	}

	if (state == State_Idle)
	{
		OnMessageSent(*outgoing_message);
		ClearOutgoingMessage();
		if (enabled && deferred)
			RETURN_IF_ERROR(ProcessOutgoingQueue());
	}
	else
	{
		if (enabled)
			RETURN_IF_ERROR(ProcessMessage());
	}

	return OpStatus::OK;
}

OP_STATUS
OpScopeTPWriter::ClearOutgoingMessage()
{
	OP_DELETE(outgoing_message);
	header.Clear();
	OP_DELETEA(block);
	block = NULL;
	outgoing_message = NULL;
	bytes_remaining = payload_length = 0;
	chunk_len = 0;
	bytebuffer_chunk_idx = 0;
	state = State_Idle;
	return OpStatus::OK;
}

#endif // SCOPE_SUPPORT
