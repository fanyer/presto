/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#if defined(M2_SUPPORT) && defined(SELFTEST)

#include "adjunct/m2/selftest/overrides/ST_StoreMessage.h"
#include "adjunct/m2/src/include/enums.h"


/// Constants to use for the test class
namespace TestStoreMessageConstants
{
	const uni_char*		From    = UNI_L("testfrom@opera.com");
	const uni_char*		To      = UNI_L("testto@opera.com");
	const uni_char*		Subject = UNI_L("test subject");
	const char*			MsgId   = "<op.uk988pu0t0fgli@test.oslo.osa>";
	const char*			MsgId2	= "<op.afafafet5455di@test.oslo.osa>";
	const char*			Headers = "From: testfrom@opera.com\r\n"
								  "To: testto@opera.com\r\n"
								  "Subject: test subject\r\n"
								  "Message-ID: <op.uk988pu0t0fgli@test.oslo.osa>\r\n";
	const char*			Body	= "Test body\r\n";
	const char*			Unique  = "BLA:12";
	const OpStringC8    UniqueOpString(Unique);
	const time_t		Date    = 1515151515;
	const UINT16		AccId	= 1;
};


///////////////////////////////////////////
// ST_StoreMessage
///////////////////////////////////////////


OP_STATUS ST_StoreMessage::GetHeaderValue(Header::HeaderType header_type, Header::HeaderValue& value) const
{
	// Sets value to predefined values
	switch (header_type)
	{
		case Header::FROM:
			return value.Set(TestStoreMessageConstants::From);
		case Header::TO:
			return value.Set(TestStoreMessageConstants::To);
		case Header::SUBJECT:
			return value.Set(TestStoreMessageConstants::Subject);
	}

	return OpStatus::OK;
}


OP_STATUS ST_StoreMessage::SetHeaderValue(Header::HeaderType header_type, const OpStringC16& value)
{
	// Returns OK if we're setting headers to the expected values (and expected
	// values only)
	switch (header_type)
	{
		case Header::FROM:
			m_errors += value.Compare(TestStoreMessageConstants::From);
			break;
		case Header::TO:
			m_errors += value.Compare(TestStoreMessageConstants::To);
			break;
		case Header::SUBJECT:
			m_errors += value.Compare(TestStoreMessageConstants::Subject);
			break;
		default:
			if (value.HasContent())
				m_errors++;
	}

	return OpStatus::OK;
}


OP_STATUS ST_StoreMessage::GetDateHeaderValue(Header::HeaderType header_type, time_t& time_utc) const
{
	time_utc = TestStoreMessageConstants::Date;
	return OpStatus::OK;
}


OP_STATUS ST_StoreMessage::SetDateHeaderValue(Header::HeaderType header_type, time_t time_utc)
{
	if (time_utc != TestStoreMessageConstants::Date)
		m_errors++;
	return OpStatus::OK;
}


OP_STATUS ST_StoreMessage::GetMessageId(OpString8& value) const
{
	return value.Set(TestStoreMessageConstants::MsgId);
}


time_t ST_StoreMessage::GetRecvTime() const
{
	return TestStoreMessageConstants::Date;
}


UINT32 ST_StoreMessage::GetLocalMessageSize() const
{
	return op_strlen(TestStoreMessageConstants::Headers) + 2 + op_strlen(TestStoreMessageConstants::Body);
}


UINT16 ST_StoreMessage::GetAccountId() const
{
	return TestStoreMessageConstants::AccId;
}


OP_STATUS ST_StoreMessage::SetAccountId(const UINT16 account_id)
{
	if (account_id != TestStoreMessageConstants::AccId)
		m_errors++;
	return OpStatus::OK;
}


const OpStringC8& ST_StoreMessage::GetInternetLocation() const
{
	return TestStoreMessageConstants::UniqueOpString;
}


OP_STATUS ST_StoreMessage::SetInternetLocation(const OpStringC8& location)
{
	m_errors += location.Compare(TestStoreMessageConstants::Unique);
	return OpStatus::OK;
}


const char* ST_StoreMessage::GetOriginalRawHeaders() const
{
	return TestStoreMessageConstants::Headers;
}


int ST_StoreMessage::GetOriginalRawHeadersSize() const
{
	return op_strlen(TestStoreMessageConstants::Headers);
}


const char* ST_StoreMessage::GetRawBody() const
{
	return TestStoreMessageConstants::Body;
}


int ST_StoreMessage::GetRawBodySize() const
{
	return op_strlen(TestStoreMessageConstants::Body);
}


OP_STATUS ST_StoreMessage::GetRawMessage(OpString8& buffer) const
{
	RETURN_IF_ERROR(buffer.Set(TestStoreMessageConstants::Headers));
	RETURN_IF_ERROR(buffer.Append("\r\n"));
	return buffer.Append(TestStoreMessageConstants::Body);
}


OP_STATUS ST_StoreMessage::SetRawMessage(const char* buffer, BOOL force_content, BOOL strip_body, char* dont_copy_buffer, int dont_copy_buffer_length)
{
	OpString8 preset_buffer;
	RETURN_IF_ERROR(GetRawMessage(preset_buffer));

	m_errors += preset_buffer.Compare(buffer);
	return OpStatus::OK;
}


int ST_StoreMessage::GetPreferredStorage() const
{
	return AccountTypes::MBOX_NONE;
}


///////////////////////////////////////////
// ST_StoreMessageReply
///////////////////////////////////////////


ST_StoreMessageReply::ST_StoreMessageReply()
	: m_header(0, Header::REFERENCES)
{
	m_header.AddMessageId(TestStoreMessageConstants::MsgId);
}


Header* ST_StoreMessageReply::GetHeader(Header::HeaderType header_type) const
{
	if (header_type == Header::REFERENCES)
		return const_cast<Header*>(&m_header);

	return 0;
}


OP_STATUS ST_StoreMessageReply::GetMessageId(OpString8& value) const
{
	return value.Set(TestStoreMessageConstants::MsgId2);
}


#endif // M2_SUPPORT && SELFTEST
