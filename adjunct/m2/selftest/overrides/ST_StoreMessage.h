/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef ST_STORE_MESSAGE_H
#define ST_STORE_MESSAGE_H

#include "adjunct/m2/src/engine/store/storemessage.h"

/** @brief A StoreMessage implementation for testing
  * How it works: All getter functions return constant values, defined in the
  * .cpp file. When setting a property, the value that is being set will be
  * checked against that constant value; if they are different, GetStatus()
  * will return an error from this moment on.
  *
  * iow, GetStatus() only returns OpStatus::OK if all Set* functions that were
  * called were setting the expected values.
  */
class ST_StoreMessage : public StoreMessage
{
public:
	ST_StoreMessage() : m_id(0), m_parent_id(0), m_errors(0) {}

	/** Get status of ST_StoreMessage object
	  * @return OpStatus::OK if there were no errors, OpStatus::ERR otherwise
	  */
	OP_STATUS GetStatus() { return m_errors == 0 ? OpStatus::OK : OpStatus::ERR; }

	// From StoreMessage
	virtual OP_STATUS Init(const UINT16 account_id) { return OpStatus::OK; }

	virtual message_gid_t GetId() const { return m_id; }
	virtual void          SetId(const message_gid_t myself) { m_id = myself; }

	virtual OP_STATUS GetHeaderValue(Header::HeaderType header_type, Header::HeaderValue& value) const;
	virtual OP_STATUS SetHeaderValue(Header::HeaderType header_type, const OpStringC16& value);

	virtual Header* GetHeader(Header::HeaderType header_type) const { return 0; }

	virtual OP_STATUS GetDateHeaderValue(Header::HeaderType header_type, time_t& time_utc) const;
	virtual OP_STATUS SetDateHeaderValue(Header::HeaderType header_type, time_t time_utc);

	virtual OP_STATUS GetMessageId(OpString8& value) const;

	virtual time_t GetRecvTime() const;
	virtual void   SetRecvTime(time_t recv_time) {}

	virtual UINT32 GetMessageSize() const { return GetLocalMessageSize(); }
	virtual void   SetMessageSize(const UINT32 size) {}

	virtual UINT32 GetLocalMessageSize() const;

	virtual BOOL   IsFlagSet(Flags flag) const { return FALSE; }
	virtual UINT64 GetAllFlags() const { return 0; }
	virtual void   SetAllFlags(const UINT64 flags) {}

	virtual MessageTypes::CreateType	GetCreateType() const { return MessageTypes::NEW; }
	virtual void						SetCreateType(const MessageTypes::CreateType create_type) {}

	virtual UINT32 GetLabel() const { return 0; }
	virtual void   SetLabel(const UINT32 label) {}

	virtual UINT16 GetAccountId() const;
	virtual OP_STATUS SetAccountId(const UINT16 account_id);

	virtual message_gid_t GetParentId() const { return m_parent_id; }
	virtual void          SetParentId(const message_gid_t parent) { m_parent_id = parent; }

	virtual const OpStringC8& GetInternetLocation() const;
	virtual OP_STATUS SetInternetLocation(const OpStringC8& location);

	virtual OP_STATUS PrepareRawMessageContent() { return OpStatus::OK; }

	virtual const char* GetOriginalRawHeaders() const;
	virtual int         GetOriginalRawHeadersSize() const;

	virtual const char* GetRawBody() const;
	virtual int			GetRawBodySize() const;

	virtual OP_STATUS GetRawMessage(OpString8& buffer) const;

	virtual OP_STATUS SetRawMessage(const char* buffer, BOOL force_content, BOOL strip_body, char* dont_copy_buffer, int dont_copy_buffer_length);

	virtual int GetPreferredStorage() const;

private:
	message_gid_t m_id;
	message_gid_t m_parent_id;
	unsigned m_errors;
};

/** @brief A reply to ST_StoreMessage
  */
class ST_StoreMessageReply : public ST_StoreMessage
{
public:
	ST_StoreMessageReply();

	// From ST_StoreMessage
	virtual Header* GetHeader(Header::HeaderType header_type) const;

	virtual OP_STATUS GetMessageId(OpString8& value) const;

private:
	Header m_header;
};

#endif // ST_STORE_MESSAGE_H
