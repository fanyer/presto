/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_SCOPE_TRANSPORT_MESSAGE_PARSER_H
#define OP_SCOPE_TRANSPORT_MESSAGE_PARSER_H

#include "modules/scope/src/scope_tp_message.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/util/adt/bytebuffer.h"

class TempBuffer;
class InputConverter;

// TODO: Start with a simple inheritance based setup, then see if
//       a proper IODevice system can be made

class OpScopeTPReader
{
public:
	// TODO: Make errors extend OP_STATUS
	enum Error
	{
		  NoError
		, StatusError
		, InvalidData
		, OutdatedProtocol
		, InvalidProtocolVersion
		, InvalidEncoding
	};

	struct STPStatus : public OpStatus
	{
		enum ExtraEnum
		{
			ERR_NOT_ENOUGH_DATA = USER_ERROR // There was not enough data available in stream
		};

	private:
		STPStatus();
		STPStatus(const STPStatus&);
		STPStatus& operator =(const STPStatus&);
	};

	OpScopeTPReader(BOOL continous=TRUE, unsigned int version=0);
	virtual ~OpScopeTPReader();

	OP_STATUS Construct(ByteBuffer *buffer);
	OP_STATUS Reset();

	/**
	 * Sets whether the reader is enabled or not.
	 * If it is enabled then any incoming byte data will be processed and
	 * OnMessageParsed called when a full message is parsed.
	 * Otherwise byte data is left in the incoming queue, any new data sent
	 * to OnDataReady() will be appended to the queue but not processed.
	 *
	 * @note Enabling the reader will not process data already in the queue.
	 *       Call ParseNextMessage() to process the queue.
	 *
	 * @param enable TRUE to enable, otherwise it is disabled
	 */
	OP_STATUS SetEnabled(BOOL enable) { enabled = enable; return OpStatus::OK; }

	/**
	 * Returns whether the reader is enabled or not.
	 * @see SetEnabled()
	 */
	BOOL IsEnabled() const { return enabled; }

	unsigned int ProtocolVersion() const { return version; }
	void SetProtocolVersion(unsigned int ver, BOOL force_change=FALSE);
	Error GetError() const { return error; }
	static OP_STATUS ErrorToText(OpString &text, Error error);

	/**
	 * Tries to parse the next message if the reader is enabled, otherwise
	 * does nothing. If there is enough data to parse a message the message
	 * will be sent via the OnMessageParsed() callback, otherwise nothing
	 * happens
	 *
	 * @return OK if message was parsed successfully or no message was parsed,
	 *         ERR_PARSING_FAILED if incoming data was incorrect,
	 *         ERR_NOT_ENOUGH_DATA if there is not enough data to fully parse
	 *         the message.
	 */
	OP_STATUS ParseNextMessage();

protected: // For subclasses to implement
	virtual OP_STATUS OnMessageParsed(const OpScopeTPMessage &message) = 0;
	/**<
	* Called when the message has been succesfully parsed.
	*/

	virtual OP_STATUS PrepareMessage(OpScopeTPMessage &message);
	/**<
	* Called before message is sent to OnMessageParsed(), allows for modifications.
	*/

	virtual void OnError(Error error) = 0;
	/**<
	* Called when the message has been succesfully parsed.
	*/

protected: // For subclasses to call
	OP_STATUS OnDataReady(const char *data, size_t len, BOOL parse_message=TRUE);
	OP_STATUS OnDataReady();

private:
	OP_STATUS ParseStream();
	OP_STATUS SetError(Error err);
	BOOL      ParseUntil(char terminator);
	BOOL3     ParseVarInt32(unsigned int &num);
	BOOL3     ParseFixed32(unsigned int &num);
	BOOL3     ParseFixed64(UINT64 &num);
	BOOL3     ParseString(OpString &str, unsigned int length);
	BOOL3     ParseSTP0Size(unsigned int &size);
	OP_STATUS ParseExtendedStp0(OpScopeTPMessage &message, const uni_char *data, int size);

	enum ParseState
	{
		  Message
	    , Prefix
		, MessageSize

		, STPType
		, Field
		, Service
		, ServiceString
		, Command
		, Type
		, Status
		, Tag
		, ChunkSize
		, ChunkData
		, MessageDone

		// States for ignoring data
		, IgnoreVarint
		, IgnoreFixed32
		, IgnoreFixed64
		, IgnoreLengthDelimited
		, IgnoreLengthDelimitedData
		, IgnoreMessageSize
		, IgnoreFields

		// STP/0 states
		, STP0_Message
		, STP0_Size
		, STP0_Init
		, STP0_Data
	};

	BOOL         enabled;
	BOOL         continous;
	Error        error;
	ParseState   state;
	ByteBuffer*  incoming;
	uni_char*    stp0_incoming;
	unsigned int chunksize;
	unsigned int string_length;

	OpScopeTPMessage message;
	ByteBuffer       payload;
	OpScopeTPHeader::MessageType payload_format;

	BOOL has_service_field, has_command_field, has_type_field, has_payload_field;
	unsigned int ignore_size;
	int          protobuf_limit; //< The current limit for reading protobuf fields, set to -1 to disable.
	unsigned int chunk_size;
	unsigned int body_size;
	unsigned int stp0_size; //< Number of UTF-16 characters to read, only used for STP/0

	unsigned int version, message_version;
};

#endif // OP_SCOPE_TRANSPORT_MESSAGE_PARSER_H
