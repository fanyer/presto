// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "irc-urlparser.h"

#include "modules/formats/uri_escape.h"
#include "modules/url/url_enum.h"
#include "modules/url/url_man.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/desktop_util/string/stringutils.h"

//****************************************************************************
//
//	IRCURLParser
//
//****************************************************************************

IRCURLParser::IRCURLParser()
:	m_port(0)
{
}


OP_STATUS IRCURLParser::Init(URL& url)
{
	// An irc URL takes the form:
	// type "://" location "/" [ channel ] [ options ]
	// (http://www.ietf.org/internet-drafts/draft-butcher-irc-url-02.tx.txt)
	if ((URLType)url.GetAttribute(URL::KType) != URL_IRC)
		return OpStatus::ERR;

	// Fetch the host.
	RETURN_IF_ERROR(m_location.Set(url.GetAttribute(URL::KHostName)));
	RETURN_IF_ERROR(Unescape(m_location));

	// Fetch the port number.
	m_port = url.GetAttribute(URL::KServerPort);
	if (m_port == 0)
		m_port = DefaultUnsecurePort;

	// Fetch the preferred nickname and password, if existing.

	OpString nicknames;
	RETURN_IF_ERROR(nicknames.Set(url.GetAttribute(URL::KUserName)));

	// There might be multiple nicknames listed. Use only the first one.
	if (nicknames.HasContent())
	{
		StringTokenizer tokenizer(nicknames, UNI_L(","));
		RETURN_IF_ERROR(tokenizer.NextToken(m_nickname));
	}
	RETURN_IF_ERROR(Unescape(m_nickname));

	RETURN_IF_ERROR(m_password.Set(url.GetAttribute(URL::KPassWord)));
	RETURN_IF_ERROR(Unescape(m_password));

	OpString path;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniPathAndQuery_Escaped, path));
	
	if (path.HasContent())
	{
		OpString options;

		if (path[0] == '/')
			RETURN_IF_ERROR(options.Set(path.CStr() + 1));
		else
			RETURN_IF_ERROR(options.Set(path));

		// Options are specified as "options  = "?" option *( "&" option )".
		RETURN_IF_ERROR(StringUtils::Strip(options, UNI_L("?"), TRUE, FALSE));

		StringTokenizer option_tokenizer(options, UNI_L("&"));
		while (option_tokenizer.HasMoreTokens())
		{
			OpString option;
			RETURN_IF_ERROR(option_tokenizer.NextToken(option));
			RETURN_IF_ERROR(Unescape(option));

			if (option.IsEmpty())
				break;

			// Figure out the option type.
			enum OptionType { OptionTypeNone, OptionTypeChannel, OptionTypeQuery };
			OptionType option_type = OptionTypeNone;

			if (option.CompareI("channel", 7) == 0)
				option_type = OptionTypeChannel;
			else if (option.CompareI("query", 5) == 0)
				option_type = OptionTypeQuery;
			else
				option_type = OptionTypeChannel;

			// Find the option value.
			OpString option_value;
			RETURN_IF_ERROR(option_value.Set(option));

			const int equal_pos = option_value.Find(UNI_L("="));
			if (equal_pos != KNotFound)
				option_value.Delete(0, equal_pos + 1);

			switch (option_type)
			{
				case OptionTypeChannel :
				{
					// Channel and key will be separated by a comma.
					OpString channel;
					OpString key;

					StringTokenizer tokenizer(option_value, UNI_L(","));
					OpStatus::Ignore(tokenizer.NextToken(channel));

					if (tokenizer.HasMoreTokens())
						OpStatus::Ignore(tokenizer.NextToken(key));

					if (channel.HasContent())
					{
						ChannelInfo *chan_inf = OP_NEW(ChannelInfo, ());
						if (!chan_inf)
							return OpStatus::ERR_NO_MEMORY;

						OpAutoPtr<ChannelInfo> channel_info(chan_inf);

						RETURN_IF_ERROR(chan_inf->Init(channel, key));
						RETURN_IF_ERROR(m_channels.Add(chan_inf));

						channel_info.release();
					}

					break;
				}
				case OptionTypeQuery :
				{
					// Only allow one query target.
					OpString target;

					StringTokenizer tokenizer(option_value, UNI_L(","));
					OpStatus::Ignore(tokenizer.NextToken(target));

					if (target.HasContent())
					{
						QueryInfo *qinf = OP_NEW(QueryInfo, ());
						if (!qinf)
							return OpStatus::ERR_NO_MEMORY;

						OpAutoPtr<QueryInfo> query_info(qinf);

						RETURN_IF_ERROR(qinf->Init(target));
						RETURN_IF_ERROR(m_queries.Add(qinf));

						query_info.release();
					}

					break;
				}
				case OptionTypeNone :
					break;
				default :
					OP_ASSERT(0);
					break;
			}
		}
	}

	return OpStatus::OK;
}


OP_STATUS IRCURLParser::Channel(UINT index, OpString& name, OpString& key) const
{
	OP_ASSERT(index < ChannelCount());

	ChannelInfo* channel_info = m_channels.Get(index);
	OP_ASSERT(channel_info != 0);

	RETURN_IF_ERROR(name.Set(channel_info->Name()));
	RETURN_IF_ERROR(key.Set(channel_info->Key()));

	return OpStatus::OK;
}


OP_STATUS IRCURLParser::Query(UINT index, OpString& target) const
{
	OP_ASSERT(index < QueryCount());

	QueryInfo* query_info = m_queries.Get(index);
	OP_ASSERT(query_info != 0);

	RETURN_IF_ERROR(target.Set(query_info->Target()));
	return OpStatus::OK;
}


OP_STATUS IRCURLParser::Unescape(OpString& text) const
{
	int len = text.Length();
	if(len)
	{
		UriUnescape::ReplaceChars(text.CStr(), len, OPSTR_MAIL);
		text.CStr()[len] = 0;
	}
	return OpStatus::OK;
}


//****************************************************************************
//
//	IRCURLParser::ChannelInfo
//
//****************************************************************************

OP_STATUS IRCURLParser::ChannelInfo::Init(const OpStringC& name,
	const OpStringC& key)
{
	RETURN_IF_ERROR(m_name.Set(name));
	RETURN_IF_ERROR(m_key.Set(key));

	return OpStatus::OK;
}


//****************************************************************************
//
//	IRCURLParser::QueryInfo
//
//****************************************************************************

OP_STATUS IRCURLParser::QueryInfo::Init(const OpStringC& target)
{
	RETURN_IF_ERROR(m_target.Set(target));
	return OpStatus::OK;
}

#endif // IRC_SUPPORT
