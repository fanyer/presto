// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef STORE_MESSAGE_H
#define STORE_MESSAGE_H

#include "adjunct/m2/src/engine/header.h"

/** @brief A class to help transfer data about a message in the mail store
  */
class StoreMessage
{
public:
	enum Flags {
		// status
		IS_READ=0,
		IS_REPLIED=1,
		IS_FORWARDED=2,
		IS_RESENT=3,
		IS_OUTGOING=4,
		IS_SENT=5,
		IS_QUEUED=6,
		IS_SEEN=7,
		IS_TIMEQUEUED=8,
		IS_DELETED=9,
		STATUS_FLAG_COUNT=10,

		// attachments
		HAS_ATTACHMENT=10,
		HAS_IMAGE_ATTACHMENT=11,
		HAS_AUDIO_ATTACHMENT=12,
		HAS_VIDEO_ATTACHMENT=13,
		HAS_ZIP_ATTACHMENT=14,
		ATTACHMENT_FLAG_COUNT=15,

		// features
		IS_ONEPART_MESSAGE=15,
		IS_WAITING_FOR_INDEXING=17,
		IS_NEWSFEED_MESSAGE=18,
		PARTIALLY_FETCHED=19,
		IS_NEWS_MESSAGE=20,
        ALLOW_8BIT=21,
		IS_IMPORTED=22,
		HAS_PRIORITY=23,     /// used by Get/SetPriority
		HAS_PRIORITY_LOW=24, /// used by Get/SetPriority
		HAS_PRIORITY_MAX=25, /// used by Get/SetPriority
		IS_CONTROL_MESSAGE=26,
		ALLOW_QUOTESTRING_QP=27,
		HAS_KEYWORDS=28,
		PERMANENTLY_REMOVED=29,
		DIR_FORCED_LTR=30,	/// if LTR and RTL are both FALSE, then it's automatic
		DIR_FORCED_RTL=31,
		ALLOW_EXTERNAL_EMBEDS=32,
		IS_FLAGGED=33
	};

	virtual ~StoreMessage() {}

	/** Initialize a message for use
	  * @param account_id Account this message should be associated with
	  */
	virtual OP_STATUS Init(const UINT16 account_id) = 0;

	/** Get/set the unique integer ID of the message
	  */
	virtual message_gid_t GetId() const = 0;
	virtual void          SetId(const message_gid_t myself) = 0;

	/** Get/set the value of a specified message header
	  * @param header_type
	  * @param value
	  */
	virtual OP_STATUS GetHeaderValue(Header::HeaderType header_type, Header::HeaderValue& value) const = 0;
	virtual OP_STATUS SetHeaderValue(Header::HeaderType header_type, const OpStringC16& value) = 0;

	/** Get a specified message header
	  * @param header_type
	  * @return A header
	  */
	virtual Header* GetHeader(Header::HeaderType header_type) const = 0;

	/** Get/set the value of a header that represents a date
	  * @param header_type
	  * @param time_utc
	  */
	virtual OP_STATUS GetDateHeaderValue(Header::HeaderType header_type, time_t& time_utc) const = 0;
	virtual OP_STATUS SetDateHeaderValue(Header::HeaderType header_type, time_t time_utc) = 0;

	/** Get the Message-Id for this message (globally unique string)
	  * @param value
	  */
	virtual OP_STATUS GetMessageId(OpString8& value) const = 0;

	/** Get/set the time at which this message was received
	  */
	virtual time_t GetRecvTime() const = 0;
	virtual void   SetRecvTime(time_t recv_time) = 0;

	/** Get/set the total size of this message (on the server) in bytes
	  * NB: can be different from the actual size of the locally saved message
	  */
	virtual UINT32 GetMessageSize() const = 0;
	virtual void   SetMessageSize(const UINT32 size) = 0;

	/** @return Actual size of the locally saved message
	  */
	virtual UINT32 GetLocalMessageSize() const = 0;

	/** Get/set bitset of Flags set on this message
	  */
	virtual BOOL   IsFlagSet(Flags flag) const = 0;
	virtual UINT64 GetAllFlags() const = 0;
	virtual void   SetAllFlags(const UINT64 flags) = 0;

	/** Get/set whether the message is marked as $NotSpam on the server, not stored in store
	 */
	virtual bool IsConfirmedNotSpam() const = 0;
	virtual void SetConfirmedNotSpam(bool is_not_spam) = 0;

	/** Get/set whether the message is marked as $Spam on the server or is located in the IMAP Spam folder, not stored in store
	 */
	virtual bool IsConfirmedSpam() const = 0;
	virtual void SetConfirmedSpam(bool is_spam) = 0;

	/** Get/set the outgoing message "create type" of this message
	 */
	virtual MessageTypes::CreateType  GetCreateType() const = 0;
	virtual void      SetCreateType(const MessageTypes::CreateType create_type) = 0;

	/** Get/set ID of the account this message is associated with
	  */
	virtual UINT16 GetAccountId() const = 0;
	virtual OP_STATUS SetAccountId(const UINT16 account_id) = 0;

	/** Get/set ID of parent message (in a thread) or 0 if it doesn't have one
	  */
	virtual message_gid_t GetParentId() const = 0;
	virtual void          SetParentId(const message_gid_t parent) = 0;

	/** Get/set the internet location for a message
	  * @param location String that uniquely identifies message on server
	  */
	virtual const OpStringC8& GetInternetLocation() const = 0;
	virtual OP_STATUS SetInternetLocation(const OpStringC8& location) = 0;

	/** Prepares the message for reading out the 'raw' message content (i.e. GetRaw*)
	  * If you have changed the message content, run this function before calling
	  * GetRaw* functions to make sure the content is in sync
	  */
	virtual OP_STATUS PrepareRawMessageContent() = 0;

	/** Get headers of the message in raw text form
	  */
	virtual const char* GetOriginalRawHeaders() const = 0;
	virtual int         GetOriginalRawHeadersSize() const = 0;

	/** Get body of the message in raw text form
	  */
	virtual const char* GetRawBody() const = 0;
	virtual int			GetRawBodySize() const = 0;

	/** Get complete message in raw text form
	  */
	virtual OP_STATUS GetRawMessage(OpString8& buffer) const = 0;

	/** Set the contents of the message from raw text form input
	  * @param buffer The message content to assign to this message
	  * @param force_content Create content for the body even if none is given in buffer
	  * @param strip_body If TRUE, doesn't take the body from buffer
	  * @param dont_copy_buffer If set equal to buffer, will take ownership of dont_copy_buffer and use it directly. Otherwise, set to NULL.
	  * @param dont_copy_buffer_length Length of the string in dont_copy_buffer
	  */
	virtual OP_STATUS SetRawMessage(const char* buffer, BOOL force_content=FALSE, BOOL strip_body=FALSE, char* dont_copy_buffer = NULL, int dont_copy_buffer_length = 0) = 0;

	/** @return preferred type of storage for this message, one of AccountTypes::MboxType
	  */
	virtual int GetPreferredStorage() const = 0;
};

#endif // STORE_MESSAGE_H
