/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef CHATINFO_H
#define CHATINFO_H

class ChatInfo
{
public:
	// Constructors.
	ChatInfo() : m_integer_data(-1) { }
	explicit ChatInfo(const OpStringC& chat_name,
		const OpStringC& string_data = OpStringC(), INT32 integer_data = -1)
	:	m_integer_data(integer_data)
	{
		m_chat_name.Set(chat_name);
		m_string_data.Set(string_data);
	}

	// Operators.
	ChatInfo& operator=(const ChatInfo& chat_info)
	{
		m_chat_name.Set(chat_info.m_chat_name);

		m_string_data.Set(chat_info.m_string_data);
		m_integer_data = chat_info.m_integer_data;

		return *this;
	}

	// Get methods.
	const OpStringC& ChatName() const { return m_chat_name; }

	const OpStringC& StringData() const { return m_string_data; }
	INT32 IntegerData() const { return m_integer_data; }

	// Set methods.
	OP_STATUS SetChatName(const OpStringC& chat_name)
	{
		RETURN_IF_ERROR(m_chat_name.Set(chat_name));
		return OpStatus::OK;
	}

	OP_STATUS SetStringData(const OpStringC& string_data)
	{
		RETURN_IF_ERROR(m_string_data.Set(string_data));
		return OpStatus::OK;
	}

	void SetIntegerData(INT32 integer_data) { m_integer_data = integer_data; }

	// Methods.
	BOOL HasName() const { return m_chat_name.HasContent(); }

private:
#ifndef _MACINTOSH_
	// No copy.
	ChatInfo(const ChatInfo& chat_info);
#endif

	// Members.
	OpString m_chat_name;

	OpString m_string_data;
	INT32 m_integer_data;
};

#endif // CHATINFO_H
