// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef IRC_URLPARSER_H
#define IRC_URLPARSER_H

class URL_Rep;

# include "modules/util/adt/opvector.h"

//****************************************************************************
//
//	IRCURLParser
//
//****************************************************************************

class IRCURLParser
{
public:
	// Construction.
	IRCURLParser();
	OP_STATUS Init(URL& url);

	// Methods.
	BOOL HasLocation() const { return m_location.HasContent(); }
	BOOL HasNickname() const { return m_nickname.HasContent(); }
	BOOL HasPassword() const { return m_password.HasContent(); }

	const OpStringC& Location() const { return m_location; }
	const OpStringC& Nickname() const { return m_nickname; }
	const OpStringC& Password() const { return m_password; }

	UINT ChannelCount() const { return m_channels.GetCount(); }
	OP_STATUS Channel(UINT index, OpString& name, OpString& key) const;

	UINT QueryCount() const { return m_queries.GetCount(); }
	OP_STATUS Query(UINT index, OpString& target) const;

	UINT Port() const { return m_port; }

private:
	// No copy or assignment.
	IRCURLParser(const IRCURLParser& parser);
	IRCURLParser& operator=(const IRCURLParser& parser);

	// Methods.
	OP_STATUS Unescape(OpString& text) const;

	// Constants.
	enum
	{
		DefaultSecurePort = 994,
		DefaultUnsecurePort = 6667,
	};

	// Internal classes.
	class ChannelInfo
	{
	public:
		// Construction.
		ChannelInfo() { }
		OP_STATUS Init(const OpStringC& name, const OpStringC& key);

		// Methods.
		const OpStringC& Name() const { return m_name; }
		const OpStringC& Key() const { return m_key; }

	private:
		// No copy or assignment.
		ChannelInfo(const ChannelInfo& other);
		ChannelInfo& operator=(const ChannelInfo& other);

		// Members.
		OpString m_name;
		OpString m_key;
	};

	class QueryInfo
	{
	public:
		// Construction.
		QueryInfo() { }
		OP_STATUS Init(const OpStringC& target);

		// Methods.
		const OpStringC& Target() const { return m_target; }

	private:
		// No copy or assignment.
		QueryInfo(const QueryInfo& other);
		QueryInfo& operator=(const QueryInfo& other);

		// Members.
		OpString m_target;
	};

	// Members.
	OpString m_location;
	OpString m_nickname;
	OpString m_password;

	OpAutoVector<ChannelInfo> m_channels;
	OpAutoVector<QueryInfo> m_queries;

	UINT m_port;
};

#endif
