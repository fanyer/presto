/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "irc-protocol.h"

#include "color-parser.h"
#include "identd.h"
#include "irc-module.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/chatinfo.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2/src/util/misc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url_man.h"

//****************************************************************************
//
//	ModeInfo
//
//****************************************************************************

/*! Class keeping track of which modes that requires a parameter. */
class ModeInfo
{
public:
	// Constructor.
	ModeInfo(const OpStringC &modes_requiring_parameter,
		const OpStringC &modes_requiring_parameter_when_set,
		const OpStringC &modes_not_requiring_parameter)
	{
		m_always_parameter.Set(modes_requiring_parameter);
		m_only_parameter_when_set.Set(modes_requiring_parameter_when_set);
		m_never_parameter.Set(modes_not_requiring_parameter);
	}

	// Methods.
	void AddModeRequiringParameter(uni_char mode);
	void AddModeRequiringParameterOnlyWhenSet(uni_char mode);
	void AddModeNotRequiringParameter(uni_char mode);

	BOOL HasMode(uni_char mode) const;
	BOOL RequiresParameter(uni_char mode, BOOL should_set) const;

private:
	// Members.
	OpString m_always_parameter;
	OpString m_only_parameter_when_set;
	OpString m_never_parameter;
};


void ModeInfo::AddModeRequiringParameter(uni_char mode)
{
	OP_ASSERT(HasMode(mode) == FALSE);
	m_always_parameter.Append(&mode, 1);
}


void ModeInfo::AddModeRequiringParameterOnlyWhenSet(uni_char mode)
{
	OP_ASSERT(HasMode(mode) == FALSE);
	m_only_parameter_when_set.Append(&mode, 1);
}


void ModeInfo::AddModeNotRequiringParameter(uni_char mode)
{
	OP_ASSERT(HasMode(mode) == FALSE);
	m_never_parameter.Append(&mode, 1);
}


BOOL ModeInfo::HasMode(uni_char mode) const
{
	bool have_mode = FALSE;

	if (m_always_parameter.FindFirstOf(mode) != KNotFound)
		have_mode = TRUE;
	else if (m_only_parameter_when_set.FindFirstOf(mode) != KNotFound)
		have_mode = TRUE;
	else if (m_never_parameter.FindFirstOf(mode) != KNotFound)
		have_mode = TRUE;

	return have_mode;
}


BOOL ModeInfo::RequiresParameter(uni_char mode, BOOL should_set) const
{
	BOOL requires_parameter = FALSE;

	if (m_always_parameter.FindFirstOf(mode) != KNotFound)
		requires_parameter = TRUE;
	else if ((should_set) &&
		(m_only_parameter_when_set.FindFirstOf(mode) != KNotFound))
	{
		requires_parameter = TRUE;
	}

	return requires_parameter;
}


//****************************************************************************
//
//	LocalInfo
//
//****************************************************************************

class LocalInfo
:	public OpHostResolverListener
{
public:
	// Constructor / destructor.
	LocalInfo();
	virtual ~LocalInfo();

	OP_STATUS Init(const OpStringC &userhost);

	// Methods.
	unsigned long LocalIPAsLongIP(const OpStringC& receiver_host) const;

private:
	// OpHostResolverObserver.
	virtual void OnHostResolved(OpHostResolver* host_resolver);
	virtual void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error);

	// Methods.
	unsigned long AsLongIP(const OpStringC &ip_address) const;
	void Cleanup();

	// Members.
	OpString m_local_resolved_ip;
	OpString m_server_resolved_ip;
	OpString m_server_resolved_userhost;

	OpHostResolver *m_host_resolver;
};


LocalInfo::LocalInfo()
:	m_host_resolver(0)
{
}


LocalInfo::~LocalInfo()
{
	Cleanup();
}


OP_STATUS LocalInfo::Init(const OpStringC &userhost)
{
	OP_ASSERT(m_host_resolver == 0);

	// Look up our ip locally.
	g_op_system_info->GetSystemIp(m_local_resolved_ip);

	// Store the userhost we received as parameter.
	{
		OpString userhost_copy;
		RETURN_IF_ERROR(userhost_copy.Set(userhost));
		RETURN_IF_ERROR(StringUtils::Strip(userhost_copy));

		const int find_pos = userhost_copy.FindFirstOf('@');
		if (find_pos != KNotFound)
			m_server_resolved_userhost.Set(userhost_copy.CStr() + find_pos + 1);
	}

	// Attempt to resolve the userhost.
	if (m_server_resolved_userhost.IsEmpty())
		return OpStatus::OK;

	m_host_resolver = MessageEngine::GetInstance()->GetGlueFactory()->CreateHostResolver(*this);
	if (m_host_resolver == 0)
	{
		Cleanup();
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(m_host_resolver->Resolve(m_server_resolved_userhost.CStr()));
	return OpStatus::OK;
}


void LocalInfo::OnHostResolved(OpHostResolver* host_resolver)
{
	OP_ASSERT(host_resolver == m_host_resolver);

	if (host_resolver->GetAddressCount() > 0)
	{
		OpSocketAddress* socket_address = 0;
		if (OpStatus::IsSuccess(OpSocketAddress::Create(&socket_address)))
		{
			if (OpStatus::IsSuccess(host_resolver->GetAddress(socket_address, 0)))
			{
				socket_address->ToString(&m_server_resolved_ip);
			}
			OP_DELETE(socket_address);
		}
	}

	Cleanup();
}


void LocalInfo::OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error)
{
	Cleanup();
}


unsigned long LocalInfo::LocalIPAsLongIP(const OpStringC& receiver_host) const
{
	unsigned long long_ip = 0;

	if (m_server_resolved_ip.HasContent())
	{
		// Ok, we have an ip address resolved through the server. First we
		// check if we should send the file to someone who is somewhat on the
		// same network as ourselves. If that is the case, we want to
		// broadcast the locally resolved ip.
		if (m_server_resolved_userhost.CompareI(receiver_host) == 0 ||
			m_server_resolved_ip.CompareI(receiver_host) == 0)
		{
			long_ip = AsLongIP(m_local_resolved_ip);
		}
		else
		{
			BOOL is_public;
			if (OpStatus::IsError(OpMisc::IsPublicIP(is_public, m_server_resolved_ip)))
				return long_ip;

			// Broadcast this ip if it's public.
			if (is_public)
				long_ip = AsLongIP(m_server_resolved_ip);
			else
			{
				// The server resolved ip is bogus, so this is the best we
				// can do. Might very well be a good ip though.
				long_ip = AsLongIP(m_local_resolved_ip);
			}
		}
	}
	else
	{
		// We only have the ip resolved locally.
		long_ip = AsLongIP(m_local_resolved_ip);
	}

	return long_ip;
}


unsigned long LocalInfo::AsLongIP(const OpStringC &ip_address) const
{
	unsigned long long_ip = 0;

	// Find the individual parts of the ip-address.
	int a = -1;
	int b = -1;
	int c = -1;
	int d = -1;

	StringTokenizer tokenizer(ip_address, UNI_L("."));
	OpString token;

	if (tokenizer.HasMoreTokens())
	{
		tokenizer.NextToken(token);
		a = uni_atoi(token.CStr());
	}
	if (tokenizer.HasMoreTokens())
	{
		tokenizer.NextToken(token);
		b = uni_atoi(token.CStr());
	}
	if (tokenizer.HasMoreTokens())
	{
		tokenizer.NextToken(token);
		c = uni_atoi(token.CStr());
	}
	if (tokenizer.HasMoreTokens())
	{
		tokenizer.NextToken(token);
		d = uni_atoi(token.CStr());
	}

	if ((a != -1) && (b != -1) && (c != -1) && (d != -1))
	{
		// Compute the long ip.
		long_ip = a << 24 | b << 16 | c << 8 | d;
	}

	return long_ip;
}


void LocalInfo::Cleanup()
{
	if (m_host_resolver != 0)
	{
		MessageEngine::GetInstance()->GetGlueFactory()->DestroyHostResolver(m_host_resolver);
		m_host_resolver = 0;
	}
}


//****************************************************************************
//
//	IRCIncomingHandler
//
//****************************************************************************

IRCIncomingHandler::~IRCIncomingHandler()
{
}


IRCIncomingHandler::IRCIncomingHandler(IRCProtocol& protocol, UINT seconds_timeout)
:	m_protocol(protocol),
	m_done(FALSE),
	m_raw_message_count(1),
	m_timeout_secs(seconds_timeout)
{
}


void IRCIncomingHandler::FirstMessageSent()
{
	if (m_timeout_secs > 0)
	{
		OpTimer* timer = OP_NEW(OpTimer, ());
		if (timer)
		{
			m_timer = timer;
			m_timer->SetTimerListener(this);
			m_timer->Start(m_timeout_secs * 1000);
		}
	}
}


void IRCIncomingHandler::CompleteMessageProcessed()
{
	OP_ASSERT(m_raw_message_count > 0);
	--m_raw_message_count;

	if (m_raw_message_count == 0)
		HandlerDone();
}


void IRCIncomingHandler::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(m_timer.get() == timer);

	HandlerDone();
	HandleTimeout();
}


//****************************************************************************
//
//	IRCReplyHandler
//
//****************************************************************************

BOOL IRCReplyHandler::NeedsDummyHandler(const IRCMessage &message)
{
	if (message.Command() == "ISON" || message.Command() == "WHO" ||
		message.Command() == "AWAY" || message.Command() == "LIST")
	{
		return TRUE;
	}

	return FALSE;
}


void IRCReplyHandler::RepliesNeeded(const IRCMessage &message, OpINT32Set &replies)
{
	if (message.Command() == "ISON")
		replies.Add(IRCReply::RPL_ISON);
	else if (message.Command() == "WHO")
		replies.Add(IRCReply::RPL_ENDOFWHO);
	else if (message.Command() == "AWAY")
	{
		replies.Add(IRCReply::RPL_UNAWAY);
		replies.Add(IRCReply::RPL_NOWAWAY);
	}
	else if (message.Command() == "LIST")
		replies.Add(IRCReply::RPL_LISTEND);
	else
		OP_ASSERT(0);
}


IRCReplyHandler::IRCReplyHandler(IRCProtocol &protocol, UINT seconds_timeout)
:	IRCIncomingHandler(protocol, seconds_timeout)
{
}


//****************************************************************************
//
//	DummyReplyHandler
//
//****************************************************************************

class DummyReplyHandler
:	public IRCReplyHandler
{
public:
	// Constructor.
	DummyReplyHandler(IRCProtocol &protocol, const IRCMessage &message);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "DummyReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);

	// Members.
	OpINT32Set m_replies_needed;
};


DummyReplyHandler::DummyReplyHandler(IRCProtocol &protocol, const IRCMessage &message)
:	IRCReplyHandler(protocol)
{
	IRCReplyHandler::RepliesNeeded(message, m_replies_needed);
}


BOOL DummyReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (m_replies_needed.Contains(numeric_reply))
		CompleteMessageProcessed();

	return handled;
}


//****************************************************************************
//
//	UserhostReplyHandler
//
//****************************************************************************

class UserhostReplyHandler
:	public IRCReplyHandler
{
public:
	// Constructor.
	UserhostReplyHandler(IRCProtocol &protocol);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "UserhostReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);
};


UserhostReplyHandler::UserhostReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol)
{
}


BOOL UserhostReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_USERHOST)
	{
		OP_ASSERT(reply.ParameterCount() >= 2);

		Protocol().m_local_info->Init(reply.Parameter(1));

		CompleteMessageProcessed();
		handled = TRUE;
	}
	else if (numeric_reply == IRCReply::ERR_UNKNOWNCOMMAND)
	{
		// The USERHOST request was not understood.
		Protocol().m_local_info->Init(OpStringC());

		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	WhoisReplyHandler
//
//****************************************************************************

class WhoisReplyHandler
:	public IRCReplyHandler
{
public:
	// Constructor.
	WhoisReplyHandler(IRCProtocol &protocol);

	// IRCIncomingHandler.
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "WhoisReplyHandler"; }
#endif

	// Members.
	OpString m_nick;
	OpString m_user_id;
	OpString m_host;
	OpString m_real_name;
	OpString m_server;
	OpString m_server_info;
	OpString m_away_message;
	OpString m_logged_in_as;
	BOOL m_is_irc_operator;
	int m_seconds_idle;
	int m_signed_on;
	OpString m_channels;
};


WhoisReplyHandler::WhoisReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol),
	m_is_irc_operator(FALSE),
	m_seconds_idle(-1),
	m_signed_on(-1)
{
}


BOOL WhoisReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_WHOISUSER)
	{
		if (reply.ParameterCount() >= 6)
		{
			m_nick.Set(reply.Parameter(1));
			m_user_id.Set(reply.Parameter(2));
			m_host.Set(reply.Parameter(3));
			m_real_name.Set(reply.Parameter(5));
		}

		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_AWAY)
	{
		if (reply.ParameterCount() >= 3)
		{
			OP_ASSERT(m_nick.CompareI(reply.Parameter(1)) == 0);
			m_away_message.Set(reply.Parameter(2));
		}

		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_WHOISSERVER)
	{
		if (reply.ParameterCount() >= 4)
		{
			OP_ASSERT(m_nick.CompareI(reply.Parameter(1)) == 0);

			m_server.Set(reply.Parameter(2));
			m_server_info.Set(reply.Parameter(3));
		}

		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_WHOISOPERATOR)
	{
		if (reply.ParameterCount() >= 3)
		{
			OP_ASSERT(m_nick.CompareI(reply.Parameter(1)) == 0);
			m_is_irc_operator = TRUE;
		}

		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_WHOISIDLE)
	{
		if (reply.ParameterCount() >= 5)
		{
			OP_ASSERT(m_nick.CompareI(reply.Parameter(1)) == 0);

			m_seconds_idle = uni_atoi(reply.Parameter(2).CStr());
			m_signed_on = uni_atoi(reply.Parameter(3).CStr());
		}

		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_WHOISCHANNELS)
	{
		if (reply.ParameterCount() >= 3)
		{
			OP_ASSERT(m_nick.CompareI(reply.Parameter(1)) == 0);

			// Quick and dirty removal of # from channel names.
			OpString channel_string;
			channel_string.Set(reply.Parameter(2));
			channel_string.Strip();

			StringUtils::Replace(channel_string, UNI_L("#"), UNI_L(""));

			// Insert a comma between each channel name.
			StringUtils::Replace(channel_string, UNI_L(" "), UNI_L(", "));

			m_channels.Set(channel_string);
		}

		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_LOGGEDINAS)
	{
		// Is logged in as.
		if (reply.ParameterCount() >= 4)
		{
			OP_ASSERT(m_nick.CompareI(reply.Parameter(1)) == 0);
			m_logged_in_as.Set(reply.Parameter(2));
		}

		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_SECURECONNECTION)
		handled = TRUE;

	else if (numeric_reply == IRCReply::RPL_ENDOFWHOIS)
	{
		if (reply.ParameterCount() >= 2)
		{
			if (m_nick.CompareI(reply.Parameter(1)) == 0)
			{
				Protocol().m_owner.OnWhoisReply(m_nick, m_user_id, m_host,
					m_real_name, m_server, m_server_info, m_away_message,
					m_logged_in_as, m_is_irc_operator, m_seconds_idle,
					m_signed_on, m_channels);
			}
		}

		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	ListReplyHandler
//
//****************************************************************************

class ListReplyHandler
:	public IRCReplyHandler
{
public:
	// Constructor.
	ListReplyHandler(IRCProtocol &protocol);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "ListReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);

	// Methods.
	void SendNormalList();

	// Members.
	BOOL m_inside_liststart;
	BOOL m_normal_list_pending;
	BOOL m_normal_list_sent;

	UINT m_list_items;
};


ListReplyHandler::ListReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol),
	m_inside_liststart(FALSE),
	m_normal_list_pending(FALSE),
	m_normal_list_sent(FALSE),
	m_list_items(0)
{
}


BOOL ListReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_LISTSTART)
	{
		m_inside_liststart = TRUE;
		handled = TRUE;
	}
	else if (numeric_reply == IRCReply::RPL_LIST)
	{
		if (reply.ParameterCount() >= 4)
		{
			const OpStringC& channel = reply.Parameter(1);
			const int user_count = uni_atoi(reply.Parameter(2).CStr());

			if (channel.HasContent() && user_count > 0)
			{
				OpString room;
				Protocol().StripChannelPrefix(channel, room);

				IRCColorStripper color_stripper;
				color_stripper.Init(reply.Parameter(3));

				ChatInfo chat_info(room, channel);
				Protocol().m_owner.OnChannelListReply(chat_info, user_count, color_stripper.StrippedText());
			}
		}

		++m_list_items;
		handled = TRUE;
	}
	else if (numeric_reply == IRCReply::RPL_LISTEND)
	{
		m_inside_liststart = FALSE;

		if (!m_normal_list_sent && m_list_items == 0)
			m_normal_list_pending = TRUE;

		if (m_normal_list_pending)
			SendNormalList();
		else
		{
			Protocol().m_owner.OnChannelListDone();
			CompleteMessageProcessed();

			if (m_normal_list_sent)
				Protocol().m_use_extended_list_command = FALSE;
		}

		handled = TRUE;
	}
	else if (IRCReply::IsError(numeric_reply) ||
		numeric_reply == IRCReply::RPL_TRYAGAIN)
	{
		if (m_normal_list_sent)
		{
			Protocol().m_owner.OnChannelListDone();
			CompleteMessageProcessed();
		}
		else if (!m_normal_list_pending)
		{
			// Perhaps this server doesn't understand LIST with arguments.
			if (m_inside_liststart)
				m_normal_list_pending = TRUE;
			else
				SendNormalList();
		}

		handled = TRUE;
	}

	return handled;
}


void ListReplyHandler::SendNormalList()
{
	IRCMessage raw_list;
	raw_list.SetCommand(UNI_L("LIST"));

	// Wait 8 seconds so the server won't complain too much.
	Protocol().SendIRCMessage(raw_list, IRCMessageSender::NORMAL_PRIORITY, 0, FALSE, 8);

	m_normal_list_pending = FALSE;
	m_normal_list_sent = TRUE;
}


//****************************************************************************
//
//	PresenceIsOnReplyHandler
//
//****************************************************************************

class PresenceIsOnReplyHandler
:	public IRCReplyHandler
{
public:
	// Construction.
	PresenceIsOnReplyHandler(IRCProtocol &protocol);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "PresenceIsOnReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);
};


PresenceIsOnReplyHandler::PresenceIsOnReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol)
{
}


BOOL PresenceIsOnReplyHandler::HandleReply(const IRCMessage &reply,
	UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_ISON)
	{
		OP_ASSERT(reply.ParameterCount() >= 2);

		StringTokenizer tokenizer(reply.Parameter(1));
		while (tokenizer.HasMoreTokens())
		{
			OpString cur_nickname;
			tokenizer.NextToken(cur_nickname);

			Protocol().HandlePresenceISONReply(cur_nickname);
		}

		CompleteMessageProcessed();

		if (IsDone())
		{
			Protocol().CheckForOfflineContacts();
			Protocol().SendISONMessages(30);
		}

		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	PresenceWhoReplyHandler
//
//****************************************************************************

class PresenceWhoReplyHandler
:	public IRCReplyHandler
{
public:
	// Construction.
	PresenceWhoReplyHandler(IRCProtocol &protocol);
	OP_STATUS Init(const OpStringC& expected_nick);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "PresenceWhoReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);

	// Members.
	OpString m_expected_nick;
};


PresenceWhoReplyHandler::PresenceWhoReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol)
{
}


OP_STATUS PresenceWhoReplyHandler::Init(const OpStringC& expected_nick)
{
	RETURN_IF_ERROR(m_expected_nick.Set(expected_nick));
	return OpStatus::OK;
}


BOOL PresenceWhoReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_WHOREPLY)
	{
		OP_ASSERT(reply.ParameterCount() >= 8);

		const OpStringC& nick = reply.Parameter(5);

		if (m_expected_nick.CompareI(nick) == 0)
			Protocol().HandlePresenceWHOReply(nick, reply.Parameter(6));

		handled = TRUE;
	}
	else if (numeric_reply == IRCReply::RPL_ENDOFWHO)
	{
		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	PresenceWhoisReplyHandler
//
//****************************************************************************

class PresenceWhoisReplyHandler
:	public IRCReplyHandler
{
public:
	// Constructor.
	PresenceWhoisReplyHandler(IRCProtocol &protocol);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "PresenceWhoisReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);

	// Members.
	OpString m_away_message;
};


PresenceWhoisReplyHandler::PresenceWhoisReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol)
{
}


BOOL PresenceWhoisReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_WHOISUSER)
		handled = TRUE;

	else if (numeric_reply == IRCReply::RPL_AWAY)
	{
		if (reply.ParameterCount() >= 3)
			m_away_message.Set(reply.Parameter(2));

		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_WHOISSERVER)
		handled = TRUE;

	else if (numeric_reply == IRCReply::RPL_WHOISOPERATOR)
		handled = TRUE;

	else if (numeric_reply == IRCReply::RPL_WHOISIDLE)
		handled = TRUE;

	else if (numeric_reply == IRCReply::RPL_WHOISCHANNELS)
		handled = TRUE;

	else if (numeric_reply == IRCReply::RPL_LOGGEDINAS)
		handled = TRUE;

	else if (numeric_reply == IRCReply::RPL_SECURECONNECTION)
		handled = TRUE;

	else if (numeric_reply == IRCReply::RPL_ENDOFWHOIS)
	{
		OP_ASSERT(reply.ParameterCount() >= 2);

		Protocol().HandlePresenceWHOISReply(reply.Parameter(1), m_away_message);

		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	DCCWhoReplyHandler
//
//****************************************************************************

class DCCWhoReplyHandler
:	public IRCReplyHandler
{
public:
	// Construction.
	DCCWhoReplyHandler(IRCProtocol &protocol, const OpStringC& nick, UINT port, UINT id, BOOL is_sender);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "DCCWhoReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);

	// Members.
	OpString m_nick;
	UINT m_port;
	UINT m_id;
	BOOL m_is_sender;
};


DCCWhoReplyHandler::DCCWhoReplyHandler(IRCProtocol &protocol,
	const OpStringC& nick, UINT port, UINT id, BOOL is_sender)
:	IRCReplyHandler(protocol),
	m_port(port),
	m_id(id),
	m_is_sender(is_sender)
{
	OpStatus::Ignore(m_nick.Set(nick));
}


BOOL DCCWhoReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_WHOREPLY)
	{
		// Get the userhost of this user, if we have received reply for the
		// correct person.
		OP_ASSERT(reply.ParameterCount() >= 8);
		const OpStringC& nick = reply.Parameter(5);

		if (m_nick.CompareI(nick) == 0)
		{
			Protocol().DCCHandleWhoReply(reply.Parameter(3), m_port, m_id, m_is_sender);
		}

		handled = TRUE;
	}
	else if (numeric_reply == IRCReply::RPL_ENDOFWHO)
	{
		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	SuppressAwayReplyHandler
//
//****************************************************************************

class SuppressAwayReplyHandler
:	public IRCReplyHandler
{
public:
	// Constructor.
	SuppressAwayReplyHandler(IRCProtocol &protocol);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "SuppressAwayReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);
};


SuppressAwayReplyHandler::SuppressAwayReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol)
{
}


BOOL SuppressAwayReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_UNAWAY)
	{
		CompleteMessageProcessed();
		handled = TRUE;
	}

	else if (numeric_reply == IRCReply::RPL_NOWAWAY)
	{
		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	SuppressMOTDReplyHandler
//
//****************************************************************************

class SuppressMOTDReplyHandler
:	public IRCReplyHandler
{
public:
	// Constructor.
	SuppressMOTDReplyHandler(IRCProtocol &protocol);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "SuppressMOTDReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);
};


SuppressMOTDReplyHandler::SuppressMOTDReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol)
{
}


BOOL SuppressMOTDReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_MOTDSTART)
		handled = TRUE;
	else if (numeric_reply == IRCReply::RPL_MOTD)
		handled = TRUE;
	else if (numeric_reply == IRCReply::RPL_ENDOFMOTD ||
		numeric_reply == IRCReply::ERR_NOMOTD)
	{
		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	InitialModeReplyHandler
//
//****************************************************************************

class InitialModeReplyHandler
:	public IRCReplyHandler
{
public:
	// Constructor.
	InitialModeReplyHandler(IRCProtocol &protocol);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "InitialModeReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);
};


InitialModeReplyHandler::InitialModeReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol)
{
}


BOOL InitialModeReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_CHANNELMODEIS)
	{
		Protocol().ProcessModeChanges(reply, TRUE);

		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	BanWhoReplyHandler
//
//****************************************************************************

class BanWhoReplyHandler
:	public IRCReplyHandler
{
public:
	// Construction.
	BanWhoReplyHandler(IRCProtocol &protocol);
	OP_STATUS Init(const OpStringC& nick, const OpStringC& channel);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "BanWhoReplyHandler"; }
#endif
	virtual BOOL HandleReply(const IRCMessage &reply, UINT numeric_reply);

	// Members.
	OpString m_nick;
	OpString m_channel;
};


BanWhoReplyHandler::BanWhoReplyHandler(IRCProtocol &protocol)
:	IRCReplyHandler(protocol)
{
}


OP_STATUS BanWhoReplyHandler::Init(const OpStringC& nick,
	const OpStringC& channel)
{
	RETURN_IF_ERROR(m_nick.Set(nick));
	RETURN_IF_ERROR(m_channel.Set(channel));

	return OpStatus::OK;
}


BOOL BanWhoReplyHandler::HandleReply(const IRCMessage &reply, UINT numeric_reply)
{
	BOOL handled = FALSE;

	if (numeric_reply == IRCReply::RPL_WHOREPLY)
	{
		OP_ASSERT(reply.ParameterCount() >= 8);

		if (m_nick.CompareI(reply.Parameter(5)) == 0)
		{
			OpString ban_address;
			if (OpStatus::IsSuccess(Protocol().CreateBanAddress(ban_address, reply.Parameter(2), reply.Parameter(3))))
			{
				// Ban this user.
				IRCMessage ban_message;
				ban_message.SetCommand(UNI_L("MODE"));
				ban_message.AddParameter(m_channel);
				ban_message.AddParameter(UNI_L("+b"));
				ban_message.AddParameter(ban_address);

				Protocol().SendIRCMessage(ban_message, IRCMessageSender::NORMAL_PRIORITY, 0);
			}
		}

		handled = TRUE;
	}
	else if (numeric_reply == IRCReply::RPL_ENDOFWHO)
	{
		CompleteMessageProcessed();
		handled = TRUE;
	}

	return handled;
}


//****************************************************************************
//
//	CTCPRequestReplyHandler
//
//****************************************************************************

class CTCPRequestReplyHandler
:	public IRCMessageHandler
{
public:
	// Constructor.
	CTCPRequestReplyHandler(IRCProtocol& protocol, UINT max_ctcp_requests, UINT max_ctcp_request_interval);

	// IRCMessageHandler.
	virtual BOOL HandleMessage(const IRCMessage& message, const OpStringC& command);

private:
	// IRCIncomingHandler.
#ifdef _DEBUG
	virtual const char* Name() const { return "CTCPRequestReplyHandler"; }
#endif

	// Members.
	UINT m_ctcp_request_count;
	UINT m_max_ctcp_requests;
	UINT m_max_ctcp_request_interval;
};


CTCPRequestReplyHandler::CTCPRequestReplyHandler(IRCProtocol& protocol,
	UINT max_ctcp_requests, UINT max_ctcp_request_interval)
:	IRCMessageHandler(protocol, max_ctcp_request_interval),
	m_ctcp_request_count(0),
	m_max_ctcp_requests(max_ctcp_requests),
	m_max_ctcp_request_interval(max_ctcp_request_interval)
{
}


BOOL CTCPRequestReplyHandler::HandleMessage(const IRCMessage& message,
	const OpStringC& command)
{
	BOOL handled = FALSE;

	if (message.IsCTCPMessage() && command == "PRIVMSG")
	{
		if (m_ctcp_request_count <= m_max_ctcp_requests)
		{
			BOOL requires_reply = FALSE;
			Protocol().RespondToCTCPRequest(message, command, requires_reply, handled);

			if (requires_reply)
				++m_ctcp_request_count;
		}
		else
		{
			OP_ASSERT(0); // Flooded.
			handled = TRUE;
		}

		if (m_max_ctcp_request_interval == 0)
			CompleteMessageProcessed();
	}

	return handled;
}


//****************************************************************************
//
//	IRCMessageHandler
//
//****************************************************************************

IRCMessageHandler::IRCMessageHandler(IRCProtocol &protocol,
	UINT seconds_timeout)
:	IRCIncomingHandler(protocol, seconds_timeout)
{
}


//****************************************************************************
//
//	DCCResumeMessageHandler
//
//****************************************************************************

class DCCResumeMessageHandler
:	public IRCMessageHandler
{
public:
	// Constructor.
	DCCResumeMessageHandler(IRCProtocol& protocol, UINT port_number, UINT id);

private:
	// IRCIncomingHandler.
	virtual void HandleTimeout();
#ifdef _DEBUG
	virtual const char* Name() const { return "DCCResumeMessageHandler"; }
#endif

	// IRCMessageHandler.
	virtual BOOL HandleMessage(const IRCMessage& message, const OpStringC& command);

	// Members.
	UINT m_port_number;
	UINT m_id;
};


DCCResumeMessageHandler::DCCResumeMessageHandler(IRCProtocol& protocol,
	UINT port_number, UINT id)
:	IRCMessageHandler(protocol, 15), // 15 seconds timeout.
	m_port_number(port_number),
	m_id(id)
{
}


void DCCResumeMessageHandler::HandleTimeout()
{
	DCCFileReceiver* receiver = 0;
	OpStatus::Ignore(Protocol().m_dcc_sessions.FileReceiver(m_port_number, m_id, receiver));

	if (receiver != 0)
	{
		// Alert about the fact that the resume request timed out.
		Protocol().m_owner.OnFileReceiveFailed(*receiver);
		Protocol().m_dcc_sessions.RemoveFileReceiver(receiver);
	}
}


BOOL DCCResumeMessageHandler::HandleMessage(const IRCMessage& message,
	const OpStringC& command)
{
	BOOL handled = FALSE;

	if (message.IsCTCPMessage() && command == "PRIVMSG")
	{
		CTCPInformation ctcp_info;
		message.CTCPInfo(ctcp_info);

		if (ctcp_info.Type() == "DCC" && (ctcp_info.ParameterCount() > 0))
		{
			const OpStringC& dcc_type = ctcp_info.Parameter(0);
			if (dcc_type.CompareI("ACCEPT") == 0)
			{
				Protocol().DoReceiveResume(ctcp_info);

				CompleteMessageProcessed();
				handled = TRUE;
			}
		}
	}

	return handled;
}


//****************************************************************************
//
//	PassiveDCCSendMessageHandler
//
//****************************************************************************

class PassiveDCCSendMessageHandler
:	public IRCMessageHandler
{
public:
	// Constructor.
	PassiveDCCSendMessageHandler(IRCProtocol& protocol, DCCFileSender* sender);

private:
	// IRCIncomingHandler.
	virtual void HandleTimeout();
#ifdef _DEBUG
	virtual const char* Name() const { return "PassiveDCCSendMessageHandler"; }
#endif

	// IRCMessageHandler.
	virtual BOOL HandleMessage(const IRCMessage& message, const OpStringC& command);

	// Members.
	DCCFileSender* m_sender;
};


PassiveDCCSendMessageHandler::PassiveDCCSendMessageHandler(IRCProtocol& protocol,
	DCCFileSender* sender)
:	IRCMessageHandler(protocol, 60), // 60 seconds timeout.
	m_sender(sender)
{
	OP_ASSERT(m_sender != 0);
}


void PassiveDCCSendMessageHandler::HandleTimeout()
{
	// Alert about the fact that send timed out.
	Protocol().m_owner.OnFileSendFailed(*m_sender);
	Protocol().m_dcc_sessions.RemoveFileSender(m_sender);
}


BOOL PassiveDCCSendMessageHandler::HandleMessage(const IRCMessage& message,
	const OpStringC& command)
{
	BOOL handled = FALSE;

	if (message.IsCTCPMessage() && command == "PRIVMSG")
	{
		CTCPInformation ctcp_info;
		message.CTCPInfo(ctcp_info);

		if (ctcp_info.Type() == "DCC" && (ctcp_info.ParameterCount() > 5))
		{
			const OpStringC& dcc_type = ctcp_info.Parameter(0);
			if (dcc_type.CompareI("SEND") == 0)
			{
				// Extract the information we care about.
				const UINT port = uni_atoi(ctcp_info.Parameter(3).CStr());
				const UINT id = uni_atoi(ctcp_info.Parameter(5).CStr());

				if (id == m_sender->Id())
				{
					RETURN_IF_ERROR(m_sender->InitClient(ctcp_info.Parameter(2), port));
					RETURN_IF_ERROR(m_sender->StartTransfer());

					CompleteMessageProcessed();
					handled = TRUE;
				}
			}
		}
	}

	return handled;
}


//****************************************************************************
//
//	IRCContacts
//
//****************************************************************************

OP_STATUS IRCContacts::AddContacts(INT32 id, const OpStringC& nicknames)
{
 	// The nicknames can be separated by a comma.
 	StringTokenizer tokenizer(nicknames, UNI_L(","));
 	while (tokenizer.HasMoreTokens())
 	{
 		OpString nick;
 		if (OpStatus::IsSuccess(tokenizer.NextToken(nick)))
		{
 			nick.Strip();

			IRCContact *contact = OP_NEW(IRCContact, ());
			if (!contact)
				return OpStatus::ERR_NO_MEMORY;

			OpAutoPtr<IRCContact> new_contact(contact);

			RETURN_IF_ERROR(contact->Init(nick));
			RETURN_IF_ERROR(m_contacts_by_nick.Add(contact->Nick().CStr(), contact));

			OpVector<IRCContact>* contact_vector = 0;
			OpStatus::Ignore(m_contacts_by_id.GetData(id, &contact_vector));

			if (contact_vector == 0)
			{
				OpVector<IRCContact> *cont_vec = OP_NEW(OpVector<IRCContact>, ());
				if (!cont_vec)
					return OpStatus::ERR_NO_MEMORY;

				OpAutoPtr<OpVector<IRCContact> > new_vector(cont_vec);

				RETURN_IF_ERROR(m_contacts_by_id.Add(id, cont_vec));
				contact_vector = new_vector.release();
			}

			RETURN_IF_ERROR(contact_vector->Add(contact));
			new_contact.release();
		}
	}

	return OpStatus::OK;
}


OP_STATUS IRCContacts::RemoveContacts(INT32 id)
{
	OpVector<IRCContact>* contact_vector = 0;
	RETURN_IF_ERROR(m_contacts_by_id.GetData(id, &contact_vector));

	for (UINT32 index = 0, count = contact_vector->GetCount(); index < count;
		++index)
	{
		IRCContact* contact = contact_vector->Get(index);

		OpStatus::Ignore(m_contacts_by_nick.Remove(contact->Nick().CStr(), &contact));
		m_contacts_by_nick.Delete(contact);
	}

	RETURN_IF_ERROR(m_contacts_by_id.Remove(id, &contact_vector));
	m_contacts_by_id.Delete(contact_vector);

	return OpStatus::OK;
}


OP_STATUS IRCContacts::ContactList(INT32 id, OpAutoVector<OpString> &nicknames) const
{
	IRCContacts* this_ptr = (IRCContacts *)(this);

	OpVector<IRCContact>* id_contacts = 0;
	RETURN_IF_ERROR(this_ptr->m_contacts_by_id.GetData(id, &id_contacts));

	for (UINT32 index = 0, count = id_contacts->GetCount(); index < count;
		++index)
	{
		IRCContact* contact = id_contacts->Get(index);

		OpString *nickn = OP_NEW(OpString, ());
		if (!nickn)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpString> nickname(nickn);

		RETURN_IF_ERROR(nickn->Set(contact->Nick()));
		RETURN_IF_ERROR(nicknames.Add(nickn));

		nickname.release();
	}

	return OpStatus::OK;
}


BOOL IRCContacts::HasContact(const OpStringC& nick) const
{
	IRCContacts* this_ptr = (IRCContacts *)(this);
	return this_ptr->m_contacts_by_nick.Contains(nick.CStr());
}


OP_STATUS IRCContacts::Contact(const OpStringC& nick, IRCContact*& contact) const
{
	IRCContacts* this_ptr = (IRCContacts *)(this);

	IRCContact* internal_contact = 0;
	RETURN_IF_ERROR(this_ptr->m_contacts_by_nick.GetData(nick.CStr(), &internal_contact));

	contact = internal_contact;
	return OpStatus::OK;
}


OP_STATUS IRCContacts::PrepareNicknameList(OpVector<OpStringC>& nicknames)
{
	for (StringHashIterator<IRCContact> it(m_contacts_by_nick); it; it++)
	{
		IRCContact* contact = it.GetData();
		OpStatus::Ignore(nicknames.Add((OpStringC *)(&contact->Nick())));

		contact->SetAwaitingOnlineReply(TRUE);
	}

	return OpStatus::OK;
}


OP_STATUS IRCContacts::OfflineContacts(OpVector<OpStringC>& nicknames)
{
	for (StringHashIterator<IRCContact> it(m_contacts_by_nick); it; it++)
	{
		IRCContact* contact = it.GetData();
		if (contact->IsAwaitingOnlineReply())
		{
			contact->SetOnline(FALSE);
			OpStatus::Ignore(nicknames.Add((OpStringC *)(&contact->Nick())));
		}
	}

	return OpStatus::OK;
}


//****************************************************************************
//
//	IRCMessageSender
//
//****************************************************************************

IRCMessageSender::IRCMessageSender(Owner& owner)
:	m_owner(owner),
	m_penalty_count(MaxPenaltyCount)
{
}


OP_STATUS IRCMessageSender::Send(const IRCMessage& message,
	const OpString8& raw_message, Priority priority,
	IRCIncomingHandler* handler, UINT delay_secs)
{
	if (m_timer.get() == 0)
	{
		OpTimer *new_timer = OP_NEW(OpTimer, ());

		if (new_timer)
		{
			m_timer = new_timer;
			m_timer->SetTimerListener(this);
			m_timer->Start(1000);
		}
	}

	OpAutoPtr<IRCIncomingHandler> handler_holder(handler);

	const int message_length = raw_message.Length();
	const int message_weight = CalculateWeight(message, message_length);

	if ((SendAllowed() && delay_secs == 0) || priority == HIGH_PRIORITY)
		RETURN_IF_ERROR(InternalSend(raw_message, message_weight, handler_holder, message_length));
	else
	{
		QueuedIRCMessage *queued_msg = OP_NEW(QueuedIRCMessage, (message_weight, priority, handler_holder, delay_secs));
		if (!queued_msg)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<QueuedIRCMessage> queued_message(queued_msg);
		RETURN_IF_ERROR(queued_msg->Init(raw_message));

		RETURN_IF_ERROR(AddToInternalQueue(queued_msg, delay_secs > 0));
		queued_message.release();
	}

	return OpStatus::OK;
}


void IRCMessageSender::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(m_timer.get() == timer);

	if (m_penalty_count	< MaxPenaltyCount)
		++m_penalty_count;

	while (SendAllowed())
	{
		// Send the first message in the queue.
		OpAutoPtr<QueuedIRCMessage> queued_message;

		if (m_normal_priority_queue.GetCount() > 0)
		{
			queued_message = m_normal_priority_queue.Remove(0);
		}
		else if (m_low_priority_queue.GetCount() > 0)
		{
			queued_message = m_low_priority_queue.Remove(0);
		}
		else
			break;

		OP_ASSERT(queued_message.get() != 0);

		OpAutoPtr<IRCIncomingHandler> handler_holder(queued_message->ReleaseHandler());
		InternalSend(queued_message->RawMessage(), queued_message->Weight(), handler_holder);
	}

	// Check if any messages in the delay queue should be moved to a normal queue.
	for (UINT32 idx = 0; idx < m_delayed_messages_queue.GetCount(); ++idx)
	{
		QueuedIRCMessage* delayed_message = m_delayed_messages_queue.Get(idx);
		OP_ASSERT(delayed_message != 0);

		const time_t current_time = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();
		if (current_time > delayed_message->Timeout())
		{
			if (OpStatus::IsSuccess(AddToInternalQueue(delayed_message, FALSE)))
			{
				m_delayed_messages_queue.RemoveByItem(delayed_message);
				idx = 0;
			}
		}
	}

	m_timer->Start(1000);
}


OP_STATUS IRCMessageSender::AddToInternalQueue(QueuedIRCMessage* queued_message, BOOL delay_message)
{
	if (queued_message == 0)
		return OpStatus::ERR_NULL_POINTER;

	if (delay_message)
		RETURN_IF_ERROR(m_delayed_messages_queue.Add(queued_message));
	else
	{
		switch (queued_message->GetPriority())
		{
			case NORMAL_PRIORITY :
			{
				RETURN_IF_ERROR(m_normal_priority_queue.Add(queued_message));
				break;
			}
			case LOW_PRIORITY :
			{
				RETURN_IF_ERROR(m_low_priority_queue.Add(queued_message));
				break;
			}
		}
	}

	return OpStatus::OK;
}


int IRCMessageSender::CalculateWeight(const IRCMessage& message,
	int message_length) const
{
	// Based on the algorithm described in "ircd flood control algorithm"
	// <URL: http://larve.net/people/hugo/2002/06/ircd-flood-control>.
	int weight = 1;

	// First give a value according to the length of the message.
	weight += message_length / 100;

	// Then according to the message type.
	if (message.Command() == "NOTICE" ||
		message.Command() == "PRIVMSG")
	{
		weight += 0;
	}
	else if (message.Command() == "TOPIC" ||
		message.Command() == "KICK" ||
		message.Command() == "MODE" ||
		message.Command() == "ISON" ||
		message.Command() == "WHO")
	{
		weight += 2;
	}
	else if (message.Command() == "WHOIS")
	{
		weight += 3;
	}
	else
		weight += 1;

	return weight;
}


OP_STATUS IRCMessageSender::InternalSend(const OpStringC8& raw_message,
	int weight, OpAutoPtr<IRCIncomingHandler> handler, int message_length)
{
	if (message_length == -1)
		message_length = raw_message.Length();

	char* send_buffer = OP_NEWA(char, message_length + 3);
	if (send_buffer == 0)
		return OpStatus::ERR_NO_MEMORY;

	sprintf(send_buffer, "%s\r\n", raw_message.CStr());

	OpString8 message_str;
	message_str.Set(send_buffer);

	RETURN_IF_ERROR(m_owner.SendDataToConnection(send_buffer, message_length + 2)); // deletes send_buffer!
	OpStatus::Ignore(m_owner.OnMessageSent(message_str, handler));

	m_penalty_count -= weight;
	return OpStatus::OK;
}


//****************************************************************************
//
//	IRCProtocol
//
//****************************************************************************

IRCProtocol::IRCProtocol(IRCProtocolOwner &owner)
:	m_owner(owner),
	// m_port ?
	m_state(OFFLINE),
	m_message_sender(*this),
	m_chat_status(AccountTypes::OFFLINE),
	m_is_connected(FALSE),
	m_use_extended_list_command(TRUE)
{
	// Set these to the RFC 1459 default values.
	m_allowed_channel_prefixes.Set("#&");

	m_allowed_user_mode_changes.Set("ov");
	m_allowed_user_prefixes.Set("@+");
}


IRCProtocol::~IRCProtocol()
{
}


OP_STATUS IRCProtocol::Init(const OpStringC8& servername, UINT16 port,
	const OpStringC8& password, const OpStringC& nick,
	const OpStringC& user_name, const OpStringC& full_name, BOOL use_ssl)
{
	// Init variables.
	OpStatus::Ignore(m_servername.Set(servername));
	OpStatus::Ignore(m_password.Set(password));
	OpStatus::Ignore(m_nick.Set(nick));
	OpStatus::Ignore(m_user_name.Set(user_name));
	OpStatus::Ignore(m_full_name.Set(full_name));
	m_port = port;

	// Add all the default rfc1459 modes.
	ModeInfo *mode_inf = OP_NEW(ModeInfo, (UNI_L("obkv"), UNI_L("l"), UNI_L("psitnm")));
	m_mode_info = mode_inf;
	if (!mode_inf)
		return OpStatus::ERR_NO_MEMORY;

	// Create the local info object.
	LocalInfo *loc_inf = OP_NEW(LocalInfo, ());
	m_local_info = loc_inf;
	if (!loc_inf)
		return OpStatus::ERR_NO_MEMORY;

	// Get the chat contacts we need to keep track of.
	BrowserUtils *browserutils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
 	if (browserutils != 0)
	{
 		for (INT32 index = 0, contact_count = browserutils->GetContactCount();
			index < contact_count; ++index)
 		{
			INT32 contact_id = 0;
 			OpString contact_nicks;

 			browserutils->GetContactByIndex(index, contact_id, contact_nicks);
			OpStatus::Ignore(m_contacts.AddContacts(contact_id, contact_nicks));
	 	}
	}

	// Start connecting.
	m_state = CONNECTING;
	RETURN_IF_ERROR(StartLoading(servername.CStr(), "irc", port, use_ssl, TRUE, 5 * 60, TRUE));

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::Cancel(const OpStringC& quit_message, BOOL nice)
{
	if (m_state == ONLINE && nice)
	{
		// Send QUIT.
		IRCMessage irc_message;
		irc_message.SetCommand(UNI_L("QUIT"));

		if (quit_message.HasContent())
			irc_message.AddParameter(quit_message);

		SendIRCMessage(irc_message, IRCMessageSender::HIGH_PRIORITY, 0);
	}

	m_nick.Empty();
	m_chat_status = AccountTypes::OFFLINE;

	m_state = OFFLINE;
	return StopLoading();
}


OP_STATUS IRCProtocol::JoinChatRoom(const OpStringC& room, const OpStringC& password)
{
	OpString channel;
	PrefixChannel(room, channel);

	IRCMessage join_message;
	join_message.SetCommand(UNI_L("JOIN"));
	join_message.AddParameter(channel);
	if (password.HasContent())
		join_message.AddParameter(password);

	SendIRCMessage(join_message, IRCMessageSender::NORMAL_PRIORITY, 0);
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::LeaveChatRoom(const ChatInfo& room)
{
	// OpString channel;
	// PrefixChannel(room, channel);

	IRCMessage part_message;
	part_message.SetCommand(UNI_L("PART"));
	part_message.AddParameter(room.StringData());

	SendIRCMessage(part_message, IRCMessageSender::NORMAL_PRIORITY, 0);
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::ListChatRooms()
{
	IRCMessage list_message;
	RETURN_IF_ERROR(list_message.SetCommand(UNI_L("LIST")));

	if (m_use_extended_list_command)
		RETURN_IF_ERROR(list_message.AddParameter(UNI_L("<10000")));

	ListReplyHandler* reply_handler = OP_NEW(ListReplyHandler, (*this));
	SendIRCMessage(list_message, IRCMessageSender::NORMAL_PRIORITY, reply_handler, FALSE, 2);

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::SendChatMessage(EngineTypes::ChatMessageType type,
	const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter)
{
	IRCMessage irc_message;
	IRCReplyHandler* reply_handler = 0;

	switch (type)
	{
		case EngineTypes::WHOIS_COMMAND :
		{
			irc_message.SetCommand(UNI_L("WHOIS"));
			irc_message.AddParameter(chatter);
			irc_message.AddParameter(chatter);
			break;
		}
		case EngineTypes::OP_COMMAND :
		{
			irc_message.SetCommand(UNI_L("MODE"));
			irc_message.AddParameter(chat_info.StringData());
			irc_message.AddParameter(UNI_L("+o"));
			irc_message.AddParameter(chatter);
			break;
		}
		case EngineTypes::DEOP_COMMAND :
		{
			irc_message.SetCommand(UNI_L("MODE"));
			irc_message.AddParameter(chat_info.StringData());
			irc_message.AddParameter(UNI_L("-o"));
			irc_message.AddParameter(chatter);
			break;
		}
		case EngineTypes::VOICE_COMMAND :
		{
			irc_message.SetCommand(UNI_L("MODE"));
			irc_message.AddParameter(chat_info.StringData());
			irc_message.AddParameter(UNI_L("+v"));
			irc_message.AddParameter(chatter);
			break;
		}
		case EngineTypes::DEVOICE_COMMAND :
		{
			irc_message.SetCommand(UNI_L("MODE"));
			irc_message.AddParameter(chat_info.StringData());
			irc_message.AddParameter(UNI_L("-v"));
			irc_message.AddParameter(chatter);
			break;
		}
		case EngineTypes::KICK_COMMAND :
		{
			irc_message.SetCommand(UNI_L("KICK"));
			irc_message.AddParameter(chat_info.StringData());
			irc_message.AddParameter(chatter);

			if (message.HasContent())
				irc_message.AddParameter(message);

			break;
		}
		case EngineTypes::BAN_COMMAND :
		{
			// As of yet, we don't know the hostmask of this user, so we look
			// it up.
			irc_message.SetCommand(UNI_L("WHO"));
			irc_message.AddParameter(chatter);

			OpAutoPtr<BanWhoReplyHandler> ban_reply_handler(OP_NEW(BanWhoReplyHandler, (*this)));
			if (ban_reply_handler.get() == 0)
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_ERROR(ban_reply_handler->Init(chatter, chat_info.StringData()));
			reply_handler = ban_reply_handler.release();

			break;
		}
		case EngineTypes::ROOM_MESSAGE :
		{
			irc_message.SetCommand(UNI_L("PRIVMSG"));
			irc_message.AddParameter(chat_info.StringData());
			irc_message.AddParameter(message);
			break;
		}
		case EngineTypes::PRIVATE_MESSAGE :
		{
			if (chat_info.IntegerData() != -1)
			{
				// This could be a dcc chat message.
			}
			else
			{
				irc_message.SetCommand(UNI_L("PRIVMSG"));
				irc_message.AddParameter(chatter);
				irc_message.AddParameter(message);
			}
			break;
		}
		case EngineTypes::ROOM_ACTION_MESSAGE :
		{
			irc_message.SetIsCTCP();
			irc_message.SetCommand(UNI_L("PRIVMSG"));
			irc_message.AddParameter(chat_info.StringData());

			OpString action_message;
			action_message.AppendFormat(UNI_L("ACTION %s"), message.CStr());
			irc_message.AddParameter(action_message);

			break;
		}
		case EngineTypes::PRIVATE_ACTION_MESSAGE :
		{
			irc_message.SetIsCTCP();
			irc_message.SetCommand(UNI_L("PRIVMSG"));
			irc_message.AddParameter(chatter);

			OpString action_message;
			action_message.AppendFormat(UNI_L("ACTION %s"), message.CStr());
			irc_message.AddParameter(action_message);
			break;
		}
		default:
			OP_ASSERT(0);
			break;
	}

	SendIRCMessage(irc_message, IRCMessageSender::NORMAL_PRIORITY, reply_handler);
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::SendUserMessage(const IRCMessage& message, BOOL notify_owner)
{
	BOOL send_message = TRUE;
	UINT raw_messages_count = 0;

	// Special handling of /operairc commands.
	if (message.Command() == "OPERAIRC")
	{
		// No sub-commands here at the moment.
		send_message = FALSE;
	}

	// Special treatment of a couple of ctcp cases.
	else if (message.IsCTCPMessage())
	{
		CTCPInformation ctcp_info;
		RETURN_IF_ERROR(message.CTCPInfo(ctcp_info));

		// Don't allow anything with DCC.
		if (ctcp_info.Type().Compare("DCC", 3) == 0)
			return OpStatus::ERR;

		// Add the timestamp if this is a ping request (mIRC has made this a
		// custom. Not really a fan of it though).
		else if (ctcp_info.Type() == "PING")
		{
			if (ctcp_info.ParameterCount() == 0 && message.ParameterCount() > 0)
			{
				const time_t current_time = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();

				IRCMessage new_message;
				new_message.SetCommand(message.Command());
				new_message.SetIsCTCP(message.IsCTCPMessage());
				new_message.AddParameter(message.Parameter(0));

				OpString ping;
				ping.AppendFormat(UNI_L("PING %u"), current_time);
				new_message.AddParameter(ping);

				raw_messages_count = SendIRCMessage(new_message, IRCMessageSender::NORMAL_PRIORITY, 0, notify_owner);
				send_message = FALSE;
			}
		}
	}

	if (send_message)
	{
		DummyReplyHandler* reply_handler = 0;
		if (IRCReplyHandler::NeedsDummyHandler(message))
			reply_handler = OP_NEW(DummyReplyHandler, (*this, message));

		raw_messages_count = SendIRCMessage(message, IRCMessageSender::NORMAL_PRIORITY, reply_handler, notify_owner);
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::SetChatProperty(const ChatInfo& room,
	const OpStringC& chatter, EngineTypes::ChatProperty property,
	const OpStringC& property_string, INT32 property_value)
{
	// OpString channel;
	// PrefixChannel(room, channel);
	const OpStringC& channel = room.StringData();

	IRCMessage message;

	switch (property)
	{
		case EngineTypes::ROOM_TOPIC :
		{
			message.SetCommand(UNI_L("TOPIC"));
			message.AddParameter(channel);
			message.AddParameter(property_string);
			break;
		}
		case EngineTypes::ROOM_MODERATED :
		{
			message.SetCommand(UNI_L("MODE"));
			message.AddParameter(channel);

			if (property_value > 0)
				message.AddParameter(UNI_L("+m"));
			else
				message.AddParameter(UNI_L("-m"));
			break;
		}
		case EngineTypes::ROOM_CHATTER_LIMIT :
		{
			message.SetCommand(UNI_L("MODE"));
			message.AddParameter(channel);

			if (property_value > 0)
			{
				message.AddParameter(UNI_L("+l"));
				message.AddParameter(property_string);
			}
			else
				message.AddParameter(UNI_L("-l"));
			break;
		}
		case EngineTypes::ROOM_TOPIC_PROTECTION :
		{
			message.SetCommand(UNI_L("MODE"));
			message.AddParameter(channel);

			if (property_value > 0)
				message.AddParameter(UNI_L("+t"));
			else
				message.AddParameter(UNI_L("-t"));
			break;
		}
		case EngineTypes::ROOM_PASSWORD :
		{
			message.SetCommand(UNI_L("MODE"));
			message.AddParameter(channel);

			if (property_value > 0)
				message.AddParameter(UNI_L("+k"));
			else
				message.AddParameter(UNI_L("-k"));

			message.AddParameter(property_string);
			break;
		}
		case EngineTypes::ROOM_SECRET :
		{
			message.SetCommand(UNI_L("MODE"));
			message.AddParameter(channel);

			if (property_value > 0)
				message.AddParameter(UNI_L("+s"));
			else
				message.AddParameter(UNI_L("-s"));
			break;
		}
		default :
			OP_ASSERT(0);
			break;
	}

	SendIRCMessage(message, IRCMessageSender::NORMAL_PRIORITY, 0);
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::SetNick(const OpStringC& nick)
{
	IRCMessage nick_message;
	nick_message.SetCommand(UNI_L("NICK"));
	nick_message.AddParameter(nick);

	SendIRCMessage(nick_message, IRCMessageSender::NORMAL_PRIORITY, 0);
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::SetAway(const OpStringC &message)
{
	IRCMessage irc_message;
	irc_message.SetCommand(UNI_L("AWAY"));

	if (message.HasContent())
		irc_message.AddParameter(message);

	SuppressAwayReplyHandler* reply_handler = OP_NEW(SuppressAwayReplyHandler, (*this));
	SendIRCMessage(irc_message, IRCMessageSender::NORMAL_PRIORITY, reply_handler);

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::DccSend(const OpStringC& nick, const OpStringC& filename,
	BOOL active_send)
{
	// Construct a dcc sender.
	DCCFileSender *dcc_sender = OP_NEW(DCCFileSender, (*this));
	if (!dcc_sender)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DCCFileSender> sender(dcc_sender);

	RETURN_IF_ERROR(dcc_sender->InitCommon(nick, filename));

	if (active_send)
	{
		// Initialize the sender.
		UINT start_port = 0, end_port = 0;
		m_owner.GetTransferPortRange(start_port, end_port);

		RETURN_IF_ERROR(dcc_sender->InitServer(start_port, end_port));

		// Add sender to the internal list.
		RETURN_IF_ERROR(m_dcc_sessions.AddActiveFileSender(dcc_sender));

		// Send a WHO message for the receiver, in order to determine the
		// userhost of the person.
		IRCMessage who_message;
		who_message.SetCommand(UNI_L("WHO"));
		who_message.AddParameter(nick);

		DCCWhoReplyHandler* reply_handler = OP_NEW(DCCWhoReplyHandler, (*this, nick, dcc_sender->PortNumber(), dcc_sender->Id(), TRUE));
		SendIRCMessage(who_message, IRCMessageSender::NORMAL_PRIORITY, reply_handler);
	}
	else
	{
		// Add sender to the internal list.
		RETURN_IF_ERROR(m_dcc_sessions.AddPassiveFileSender(dcc_sender));

		// We can't start a server, so we must ask the receiver to do it for
		// us by sending a passive dcc send request.
		IRCMessage send_message;
		RETURN_IF_ERROR(CreateDCCSendMessage(nick, dcc_sender->FileName(),
			UNI_L(""), 0, (UINT32)dcc_sender->GetFileSize(), dcc_sender->Id(), send_message));

		PassiveDCCSendMessageHandler* message_handler = OP_NEW(PassiveDCCSendMessageHandler, (*this, dcc_sender));
		SendIRCMessage(send_message, IRCMessageSender::NORMAL_PRIORITY, message_handler);
	}

	// Notify the owner.
	m_owner.OnFileSendInitializing(*dcc_sender);
	sender.release();

	return OpStatus::OK;
}

OP_STATUS IRCProtocol::AcceptDCCRequest(UINT port_number, UINT id)
{
	// Look up the dcc receiver.
	DCCFileReceiver* receiver = 0;
	RETURN_IF_ERROR(m_dcc_sessions.FileReceiver(port_number, id, receiver));

	if (receiver == 0)
		return OpStatus::ERR_NULL_POINTER;

	if (receiver->CurrentStatus() != DCCFileReceiver::DCC_STATUS_STARTING)
		return OpStatus::OK;

	m_owner.OnFileReceiveInitializing(*receiver, OP_NEW(ChatFileTransferManager::FileReceiveInitListener, (receiver, this)));
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::IRCContactAdded(INT32 contact_id, const OpStringC& nicknames)
{
	RETURN_IF_ERROR(m_contacts.AddContacts(contact_id, nicknames));

	if (m_contacts.ContactCount() == 1)
		SendISONMessages(0);

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::IRCContactChanged(INT32 contact_id, const OpStringC& nicknames)
{
	// Perhaps a bit unecessary, but ok.
	RETURN_IF_ERROR(IRCContactRemoved(contact_id, nicknames));
	RETURN_IF_ERROR(m_contacts.AddContacts(contact_id, nicknames));

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::IRCContactRemoved(INT32 contact_id, const OpStringC& nicknames)
{
	OpAutoVector<OpString> contact_list;
	OpStatus::Ignore(m_contacts.ContactList(contact_id, contact_list));

	for (UINT32 index = 0, count = contact_list.GetCount();
		index < count; ++index)
	{
		m_owner.OnPresenceOffline(*contact_list.Get(index));
	}

	RETURN_IF_ERROR(m_contacts.RemoveContacts(contact_id));
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::ProcessReceivedData()
{
	{
		const int network_buf_size = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;

		OpString8 buffer;
		if (buffer.Reserve(network_buf_size) == 0)
			return OpStatus::ERR_NO_MEMORY;

		const unsigned int content_loaded = ReadData(buffer.CStr(), network_buf_size);
		m_remaining_buffer.Append(buffer.CStr(), content_loaded);
	}

	if (m_remaining_buffer.IsEmpty())
		return OpStatus::OK;

	int newline_pos;
	while ((newline_pos = m_remaining_buffer.FindFirstOf('\n')) != KNotFound)
	{
		int delete_len = newline_pos + 1;
		if (newline_pos > 0 && m_remaining_buffer[newline_pos-1] == '\r')
			newline_pos--;

		m_remaining_buffer[newline_pos] = '\0';
		ParseResponseLine(m_remaining_buffer.CStr());
		m_remaining_buffer[newline_pos] = '\n';
		m_remaining_buffer.Delete(0, delete_len);
	}

	return OpStatus::OK;
}


void IRCProtocol::RequestMoreData()
{
	// start logon..
	if (!m_is_connected)
	{
		m_is_connected = TRUE;

		// Start identd server.
		IdentdServer *identd_serv = OP_NEW(IdentdServer, (*this));
		m_identd_server = identd_serv;

		if (identd_serv)
			identd_serv->Init(m_user_name);

		// Send password, if we have one.
		if (m_password.HasContent())
		{
			IRCMessage pass_message;
			pass_message.SetCommand(UNI_L("PASS"));
			pass_message.AddParameter(m_password);

			SendIRCMessage(pass_message, IRCMessageSender::NORMAL_PRIORITY, 0);
		}

		// Send nick.
		SetNick(m_nick);

		// Send user.
		IRCMessage user_message;
		user_message.SetCommand(UNI_L("USER"));
		user_message.AddParameter(m_user_name);
		user_message.AddParameter(UNI_L("8")); // bitmask 1000, set +i on the user.
		user_message.AddParameter(UNI_L("*"));
		user_message.AddParameter(m_full_name);

		SendIRCMessage(user_message, IRCMessageSender::NORMAL_PRIORITY, 0);
	}
}


void IRCProtocol::OnClose(OP_STATUS rc)
{
	if (m_state != OFFLINE)
	{
		Cancel(0, FALSE);
		m_owner.OnConnectionLost(rc != OpStatus::OK);
	}
}


void IRCProtocol::OnRestartRequested()
{
	m_owner.OnRestartRequested();
}


OP_STATUS IRCProtocol::ParseResponseLine(const char *line)
{
	// Log it.
	m_owner.OnRawMessageReceived(line);

	// Parse it.
	OpString8 charset;
	m_owner.GetCurrentCharset(charset);

	IRCMessage incoming_message;
	RETURN_IF_ERROR(incoming_message.FromRawLine(line, charset, m_servername));

	if (incoming_message.IsNumericReply())
		OpStatus::Ignore(HandleIncomingReply(incoming_message, incoming_message.NumericReplyValue()));
	else
		OpStatus::Ignore(HandleIncomingMessage(incoming_message, incoming_message.Command()));

	return OpStatus::OK;
}


void IRCProtocol::OnDCCReceiveBegin(DCCFileReceiver* receiver)
{
	BOOL transfer_failed = FALSE;
	m_owner.OnFileReceiveBegin(*receiver, transfer_failed);

	if (transfer_failed)
		m_dcc_sessions.RemoveFileReceiver(receiver);
}


void IRCProtocol::OnDCCDataReceived(DCCFileReceiver* receiver,
	unsigned char* received, UINT bytes_received)
{
	BOOL transfer_failed = FALSE;
	m_owner.OnFileReceiveProgress(*receiver, received, bytes_received, transfer_failed);

	if (transfer_failed)
		m_dcc_sessions.RemoveFileReceiver(receiver);
}


void IRCProtocol::OnDCCReceiveComplete(DCCFileReceiver* receiver)
{
	m_owner.OnFileReceiveCompleted(*receiver);
	m_dcc_sessions.RemoveFileReceiver(receiver);
}


void IRCProtocol::OnDCCReceiveFailed(DCCFileReceiver* receiver)
{
	m_owner.OnFileReceiveFailed(*receiver);
	m_dcc_sessions.RemoveFileReceiver(receiver);
}


void IRCProtocol::OnDCCSendBegin(DCCFileSender* sender)
{
	BOOL transfer_failed = FALSE;
	m_owner.OnFileSendBegin(*sender, transfer_failed);

	if (transfer_failed)
		m_dcc_sessions.RemoveFileSender(sender);
}


void IRCProtocol::OnDCCDataSent(DCCFileSender* sender, UINT bytes_sent)
{
	BOOL transfer_failed = FALSE;
	m_owner.OnFileSendProgress(*sender, bytes_sent, transfer_failed);

	if (transfer_failed)
		m_dcc_sessions.RemoveFileSender(sender);
}


void IRCProtocol::OnDCCSendComplete(DCCFileSender* sender)
{
	m_owner.OnFileSendCompleted(*sender);
	m_dcc_sessions.RemoveFileSender(sender);
}


void IRCProtocol::OnDCCSendFailed(DCCFileSender* sender)
{
	m_owner.OnFileSendFailed(*sender);
	m_dcc_sessions.RemoveFileSender(sender);
}


void IRCProtocol::OnDCCChatBegin(DCCChat* chat)
{
}


void IRCProtocol::OnDCCChatDataSent(DCCChat* chat, const OpStringC8& data)
{
}


void IRCProtocol::OnDCCChatDataReceived(DCCChat* chat, const OpStringC8& data)
{
}


void IRCProtocol::OnDCCChatClosed(DCCChat* chat)
{
}


void IRCProtocol::OnConnectionEstablished(const OpStringC &connection_to)
{
	m_owner.OnIdentdConnectionEstablished(connection_to);
}


void IRCProtocol::OnIdentConfirmed(const OpStringC8 &request, const OpStringC8 &response)
{
	m_owner.OnIdentdIdentConfirmed(request, response);
}


OP_STATUS IRCProtocol::SendDataToConnection(char* data, UINT data_size)
{
	if (data == 0)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(SendData(data, data_size));
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::OnMessageSent(const OpStringC8& message,
	OpAutoPtr<IRCIncomingHandler> handler)
{
	// Notify owner.
	m_owner.OnRawMessageSent(message);

	if (handler.get() != 0)
	{
		// Add the handler to our list of handlers.
		if (handler->IsReplyHandler())
			RETURN_IF_ERROR(m_reply_handlers.Add(handler.get()));
		else if (handler->IsMessageHandler())
			RETURN_IF_ERROR(m_message_handlers.Add(handler.get()));
		else
			OP_ASSERT(FALSE);

		// Notify the handler that we have sent the message.
		handler->FirstMessageSent();

		handler.release(); // Vector owns it now.
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::HandleIncomingMessage(IRCMessage const &message,
	const OpStringC& command)
{
	// See if the message handlers want to have a go at this message.
	for (UINT index = 0; index < m_message_handlers.GetCount(); ++index)
	{
		IRCIncomingHandler *handler = m_message_handlers.Get(index);
		OP_ASSERT(handler != 0);

		const BOOL handled = handler->HandleMessage(message, command);
		if (handler->IsDone())
		{
			// This handler is done. Delete it from the list.
			m_message_handlers.Delete(handler);

			index = 0;
		}

		if (handled)
			return OpStatus::OK;
	}

	if (command == "PING")
	{
		// PING? PONG!
		IRCMessage pong_message;
		pong_message.SetCommand(UNI_L("PONG"));
		pong_message.AddParameter(message.Parameter(0));

		SendIRCMessage(pong_message, IRCMessageSender::NORMAL_PRIORITY, 0);
		return OpStatus::OK;
	}

	// Get some common values we will need for several of the messages.
	OpString sender;
	message.MessageSender(sender);

	OpStringC channel(message.ParameterCount() >= 1 ? message.Parameter(0).CStr() : NULL);

	OpString room;
	StripChannelPrefix(channel, room);

	ChatInfo chat_info(room, channel);

	BOOL is_privmsg = (command == "PRIVMSG");
	if (is_privmsg || command == "NOTICE")
	{
		if (message.ParameterCount() < 2)
			return OpStatus::ERR;

		OpString received_text;

		if (!message.IsCTCPMessage())
			received_text.Set(message.Parameter(1));
		else
		{
			if (is_privmsg) // We should reply / respond to a CTCP request.
			{
				UINT max_ctcp_requests = 0;
				UINT max_ctcp_request_interval = 0;
				m_owner.GetCTCPLimitations(max_ctcp_requests, max_ctcp_request_interval);

				CTCPRequestReplyHandler *msg_han = OP_NEW(CTCPRequestReplyHandler, (*this, max_ctcp_requests, max_ctcp_request_interval));
				if (!msg_han)
					return OpStatus::ERR_NO_MEMORY;

				OpAutoPtr<CTCPRequestReplyHandler> message_handler(msg_han);

				RETURN_IF_ERROR(m_message_handlers.Add(msg_han));

				msg_han->HandleMessage(message, command);
				message_handler.release();
			}
			else // This is a CTCP reply.
			{
				CTCPInformation ctcp_info;
				RETURN_IF_ERROR(message.CTCPInfo(ctcp_info));

				if (ctcp_info.Type() == "PING" && ctcp_info.ParameterCount() > 0)
				{
					const time_t current_time = m_owner.GetCurrentTime();
					const time_t reply_time = uni_atoi(ctcp_info.Parameter(0).CStr());
					const UINT seconds_diff = current_time - reply_time;

					OpString reply;
					reply.AppendFormat(UNI_L("PING reply: %u second(s)"), seconds_diff);
					RETURN_IF_ERROR(received_text.Set(reply));
				}
				else
				{
					OpString reply;
					reply.AppendFormat(UNI_L("%s %s"),
						ctcp_info.Type().CStr(), ctcp_info.Parameters().CStr());

					RETURN_IF_ERROR(received_text.Set(reply));
				}
			}
		}

		if (received_text.HasContent() && sender.HasContent())
		{
			 if (message.SenderIsServer())
				m_owner.OnServerInformation(received_text);
			 else
			 {
				// Filter away those ugly DCC notices that mIRC send.
				// The format is as follows: DCC Send <filename> (ip)
				BOOL is_mirc_dcc_notice = FALSE;

				if ((received_text.FindI("DCC Send") == 0) ||
					(received_text.FindI("DCC Chat") == 0))
				{
					// Match the ip-address at the end.
					OpString regexp;
					regexp.Set("\\(\\d+\\.\\d+\\.\\d+\\.\\d+\\)$");

					is_mirc_dcc_notice = GetFactory()->GetBrowserUtils()->MatchRegExp(regexp, received_text);
				}

				if (!is_mirc_dcc_notice)
				{
					if (IsChannel(channel))
					{
						m_owner.OnChannelMessage(chat_info, received_text, sender);
					}
					else
					{
						ChatInfo chat_info_private(sender);
						m_owner.OnPrivateMessage(chat_info_private, received_text, FALSE);
					}
				}
			}
		}
		return OpStatus::OK;
	}

	BOOL join = (command == "JOIN");
	BOOL quit = (command == "QUIT");
	if (join || quit || command == "PART")
	{
		if (sender == m_nick)
		{
			OpStringC empty;
			if (join)
			{
				// Retrieve the channel modes.
				IRCMessage mode_message;
				mode_message.SetCommand(UNI_L("MODE"));
				mode_message.AddParameter(channel);

				InitialModeReplyHandler* reply_handler = OP_NEW(InitialModeReplyHandler, (*this));
				SendIRCMessage(mode_message, IRCMessageSender::NORMAL_PRIORITY, reply_handler);

				// Notify owner.
				m_owner.OnJoinedChannel(chat_info, empty);
			}
			else
				m_owner.OnLeftChannel(chat_info, empty, empty);
		}
		else
		{
			if (join)
			{
				// Notify owner.
				m_owner.OnJoinedChannel(chat_info, sender);

				if (m_contacts.HasContact(sender))
				{
					IRCContacts::IRCContact* contact = 0;
					m_contacts.Contact(sender, contact);

					if (contact->IsOffline())
						m_owner.OnInitialPresence(sender);
				}
			}
			else
			{
				if (quit)
				{
					m_owner.OnQuit(sender, message.ParameterCount() >= 1 ? message.Parameter(0) : OpStringC());

					if (m_contacts.HasContact(sender))
						m_owner.OnPresenceOffline(sender);
				}
				else
					m_owner.OnLeftChannel(chat_info, sender, message.ParameterCount() >= 2 ? message.Parameter(1) : OpStringC());
			}
		}
		return OpStatus::OK;
	}

	if (command == "KICK")
	{
		if (message.ParameterCount() >= 2)
		{
			const OpStringC& kicked_nick = message.Parameter(1);

			m_owner.OnKickedFromChannel(chat_info,
										kicked_nick == m_nick ? OpStringC() : kicked_nick,
										sender,
										message.ParameterCount() >= 3 ? message.Parameter(2) : OpStringC());
		}
		return OpStatus::OK;
	}

	if (command == "TOPIC")
	{
		if (message.ParameterCount() >= 2)
		{
			ChatInfo chat_info(room, channel);
			m_owner.OnTopicChanged(chat_info,
				message.SenderIsServer() ? OpStringC() : sender,
				message.Parameter(1));
		}
		return OpStatus::OK;
	}

	if (command == "NICK")
	{
		const OpStringC& new_nick = message.Parameter(0);

		if (sender == m_nick)
			m_nick.Set(new_nick);

		m_owner.OnNickChanged(sender, new_nick);
		return OpStatus::OK;
	}

	if (command == "INVITE")
	{
		if (message.ParameterCount() >= 2)
		{
			OpString room;
			StripChannelPrefix(message.Parameter(1), room);

			ChatInfo chat_info(room, channel);
			m_owner.OnInvite(sender, chat_info);
		}
		return OpStatus::OK;
	}

	if (command == "MODE")
	{
		ProcessModeChanges(message, FALSE);
		return OpStatus::OK;
	}

	if (command == "WALLOPS")
	{
		if (message.ParameterCount() > 0)
			m_owner.OnServerInformation(message.Parameter(0));

		return OpStatus::OK;
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::HandleIncomingReply(const IRCMessage &reply, UINT numeric_reply)
{
	// See if the reply handlers want to have a go at this reply.
	for (UINT index = 0; index < m_reply_handlers.GetCount(); ++index)
	{
		IRCIncomingHandler *handler = m_reply_handlers.Get(index);
		OP_ASSERT(handler != 0);

		const BOOL handled = handler->HandleReply(reply, numeric_reply);
		if (handler->IsDone())
		{
			// This handler is done. Delete it from the list.
			m_reply_handlers.Delete(handler);

			index = 0;
		}

		if (handled)
			return OpStatus::OK;
	}

	// Get some common values we will need for several of the messages.
	OpString sender;
	reply.MessageSender(sender);

	OpStringC channel(reply.ParameterCount() >= 2 ? reply.Parameter(1).CStr() : NULL);

	OpString room;
	StripChannelPrefix(channel, room);

	if (numeric_reply == IRCReply::ERR_NICKNAMEINUSE)
	{
		if (reply.ParameterCount() >= 2)
		{
			m_owner.OnNicknameInUse(reply.Parameter(1));
		}
	}

	else if (numeric_reply == IRCReply::RPL_WELCOME ||
		numeric_reply == IRCReply::RPL_YOURHOST)
	{
		if (m_state != ONLINE)
		{
			// Get the nickname we were assigned (it may be that the one we
			// typed in was too long, and was chopped off, or something).
			if (reply.ParameterCount() >= 1)
			{
				const OpStringC& assigned_nick = reply.Parameter(0);

				if (assigned_nick.CompareI(m_nick) != 0)
				{
					m_owner.OnNickChanged(m_nick, assigned_nick);
					m_nick.Set(assigned_nick);
				}
			}

			// Shut down the identd server.
			m_identd_server = 0;

			// Add a reply handler that will suppress the initial motd.
			{
				SuppressMOTDReplyHandler *rep_han = OP_NEW(SuppressMOTDReplyHandler, (*this));
				if (!rep_han)
					return OpStatus::ERR_NO_MEMORY;

				OpAutoPtr<SuppressMOTDReplyHandler> reply_handler(rep_han);

				RETURN_IF_ERROR(m_reply_handlers.Insert(0, rep_han));
				reply_handler.release();
			}

			// Send USERHOST, in order to try to determine our address.
			{
				IRCMessage userhost_message;
				userhost_message.SetCommand(UNI_L("USERHOST"));
				userhost_message.AddParameter(m_nick);

				UserhostReplyHandler* reply_handler = OP_NEW(UserhostReplyHandler, (*this));
				SendIRCMessage(userhost_message, IRCMessageSender::NORMAL_PRIORITY, reply_handler);
			}

			// Presence updates.
			SendISONMessages(5);

			// Update state.
			m_state = ONLINE;
			m_chat_status = AccountTypes::ONLINE;
			m_owner.OnProtocolReady();
		}
	}

	else if (numeric_reply == IRCReply::RPL_ISUPPORT)
	{
		ProcessServerSupportMessage(reply);
	}

	else if (numeric_reply == IRCReply::RPL_WHOISUSER)
	{
		// Create a reply handler to handle this.
		WhoisReplyHandler *rep_han = OP_NEW(WhoisReplyHandler, (*this));
		if (!rep_han)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<WhoisReplyHandler> reply_handler(rep_han);

		RETURN_IF_ERROR(m_reply_handlers.Insert(0, rep_han));

		rep_han->HandleReply(reply, numeric_reply);
		reply_handler.release();
	}

	else if (numeric_reply == IRCReply::RPL_TOPIC)
	{
		if (reply.ParameterCount() >= 3)
		{
			ChatInfo chat_info(room, channel);
			m_owner.OnTopicChanged(chat_info,
				reply.SenderIsServer() ? OpStringC() : sender,
				reply.Parameter(2));
		}
	}

	else if (numeric_reply == IRCReply::RPL_AWAY)
	{
		if (reply.ParameterCount() >= 3)
			m_owner.OnNickAway(reply.Parameter(1), reply.Parameter(2));
	}

	else if (numeric_reply == IRCReply::RPL_NAMREPLY)
	{
		if (reply.ParameterCount() >= 4)
		{
			// For some reason, parameter 2 is just a "=" for this reply.
			StripChannelPrefix(reply.Parameter(2), room);

			StringTokenizer tokenizer(reply.Parameter(3));
			while (tokenizer.HasMoreTokens())
			{
				OpString token;
				tokenizer.NextToken(token);

				BOOL is_operator = FALSE;
				BOOL is_voiced = FALSE;

				OpString prefix;
				NicknamePrefix(token, prefix);

				if (prefix.HasContent())
				{
					if (PrefixIsOpOrBetter(prefix))
						is_operator = TRUE;
					else if (PrefixIsVoiceOrBetter(prefix))
						is_voiced = TRUE;
				}

				OpString nickname;
				StripNicknamePrefix(token, nickname);

				if (!nickname.IsEmpty())
				{
					ChatInfo chat_info(room, channel);
					m_owner.OnChannelUserReply(chat_info, nickname, is_operator, is_voiced, prefix);
				}
			}
		}
	}

	else if (numeric_reply == IRCReply::RPL_ENDOFNAMES)
	{
		ChatInfo chat_info(room, channel);
		m_owner.OnChannelUserReplyDone(chat_info);
	}

	else if (numeric_reply == IRCReply::ERR_BADCHANNELKEY)
	{
		ChatInfo chat_info(room, channel);
		m_owner.OnChannelPasswordRequired(chat_info);
	}

	else
	{
		// Check if the number is inside the generic range we print out replies in.
		if (numeric_reply == IRCReply::RPL_ADMINME ||
			numeric_reply == IRCReply::RPL_ADMINLOC1 ||
			numeric_reply == IRCReply::RPL_ADMINLOC2 ||
			numeric_reply == IRCReply::RPL_ADMINEMAIL ||
			numeric_reply == IRCReply::RPL_TRYAGAIN ||
			numeric_reply == IRCReply::RPL_USERHOST ||
			numeric_reply == IRCReply::RPL_UNAWAY ||
			numeric_reply == IRCReply::RPL_NOWAWAY ||
			numeric_reply == IRCReply::RPL_WHOWASUSER ||
			numeric_reply == IRCReply::RPL_UNIQOPIS ||
			numeric_reply == IRCReply::RPL_INVITELIST ||
			numeric_reply == IRCReply::RPL_WHOISSERVER ||
			numeric_reply == IRCReply::RPL_ISON ||
			numeric_reply == IRCReply::RPL_EXCEPTLIST ||
			numeric_reply == IRCReply::RPL_VERSION ||
			numeric_reply == IRCReply::RPL_WHOREPLY ||
			numeric_reply == IRCReply::RPL_LINKS ||
			numeric_reply == IRCReply::RPL_BANLIST ||
			numeric_reply == IRCReply::RPL_YOUREOPER ||
			numeric_reply == IRCReply::RPL_REHASHING ||
			numeric_reply == IRCReply::RPL_TIME ||
			numeric_reply == IRCReply::RPL_MOTDSTART ||
			numeric_reply == IRCReply::RPL_MOTD ||
			numeric_reply == IRCReply::RPL_CHANNELMODEIS ||
			numeric_reply == IRCReply::RPL_LISTSTART ||
			numeric_reply == IRCReply::RPL_LIST ||
			numeric_reply >= 400) // All error replies.
		{
			// Format the text message nicely and send it as a server message.
			OpString text_message;
			for (UINT index = 1; index < reply.ParameterCount(); ++index)
			{
				if (index > 1)
					text_message.Append(" ");

				text_message.Append(reply.Parameter(index));
			}

			m_owner.OnServerMessage(text_message);
		}
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::CreateDCCAcceptMessage(const DCCFile& file,
	UINT32 resume_position, BOOL with_id, IRCMessage& message) const
{
	message.SetIsCTCP();
	RETURN_IF_ERROR(message.SetCommand(UNI_L("PRIVMSG")));
	RETURN_IF_ERROR(message.AddParameter(file.OtherParty()));

	OpString parameter;
	RETURN_IF_ERROR(parameter.AppendFormat(UNI_L("DCC ACCEPT \"%s\" %u %u"),
		file.FileName().CStr(),
		file.PortNumber(),
		resume_position));

	if (with_id)
		RETURN_IF_ERROR(parameter.AppendFormat(UNI_L(" %u"), file.Id()));

	RETURN_IF_ERROR(message.AddParameter(parameter));
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::CreateDCCSendMessage(const OpStringC& nick,
	const OpStringC& filename, const OpStringC& to_userhost, UINT port_number,
	UINT32 file_size, UINT id, IRCMessage& message) const
{
	IRCProtocol* this_ptr = (IRCProtocol *)(this);

	// Get the file part of the filename.
	OpString file_part;
	RETURN_IF_ERROR(this_ptr->GetFactory()->GetBrowserUtils()->FilenamePartFromFullFilename(filename, file_part));

	// Spaces in the filename is no good for DCC send. Replace a space with an underscore,
	// just like mIRC does.
	RETURN_IF_ERROR(StringUtils::Replace(file_part, UNI_L(" "), UNI_L("_")));

	// Construct a DCC SEND message.
	message.SetIsCTCP();
	RETURN_IF_ERROR(message.SetCommand(UNI_L("PRIVMSG")));
	RETURN_IF_ERROR(message.AddParameter(nick));

	OpString dcc_send;
	RETURN_IF_ERROR(dcc_send.AppendFormat(UNI_L("DCC SEND %s %u %u %u"),
		file_part.CStr(), m_local_info->LocalIPAsLongIP(to_userhost),
		port_number, file_size));

	if (id != 0)
		RETURN_IF_ERROR(dcc_send.AppendFormat(UNI_L(" %u"), id));

	RETURN_IF_ERROR(message.AddParameter(dcc_send));
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::SendISONMessages(UINT delay_secs)
{
	OpVector<OpStringC> nicknames;
	RETURN_IF_ERROR(m_contacts.PrepareNicknameList(nicknames));

	if (nicknames.GetCount() == 0)
		return OpStatus::OK;

	IRCMessage ison_message;
	RETURN_IF_ERROR(ison_message.SetCommand(UNI_L("ISON")));

	for (UINT32 nick_index = 0, count = nicknames.GetCount();
		nick_index < count; ++nick_index)
	{
		OpStatus::Ignore(ison_message.AddParameter(*nicknames.Get(nick_index)));
	}

	PresenceIsOnReplyHandler* reply_handler = OP_NEW(PresenceIsOnReplyHandler, (*this));
	SendIRCMessage(ison_message, IRCMessageSender::LOW_PRIORITY, reply_handler, FALSE, delay_secs);

	return OpStatus::OK;
}

OP_STATUS IRCProtocol::DCCInitReceiveCheckResume(DCCFileReceiver* receiver, OpFileLength resume_position, BOOL initialization_failed)
{
/*	UINT32 resume_position = 0;
	BOOL initialization_failed = FALSE;
	m_owner.OnFileReceiveInitializing(*receiver, resume_position, initialization_failed);
*/
	if (initialization_failed)
	{
		m_dcc_sessions.RemoveFileReceiver(receiver);
		return OpStatus::ERR;
	}

	if (resume_position > 0)
	{
		IRCMessage resume_message;
		resume_message.SetCommand(UNI_L("PRIVMSG"));
		resume_message.SetIsCTCP(TRUE);
		resume_message.AddParameter(receiver->OtherParty());

		OpString resume_parameter;
		resume_parameter.AppendFormat(UNI_L("DCC RESUME \"%s\" %u %u"),
			receiver->FileName().CStr(),
			receiver->PortNumber(),
			resume_position);

		resume_message.AddParameter(resume_parameter);

		DCCResumeMessageHandler* message_handler = OP_NEW(DCCResumeMessageHandler, (*this, receiver->PortNumber(), receiver->Id()));
		SendIRCMessage(resume_message, IRCMessageSender::NORMAL_PRIORITY, message_handler);

		receiver->SetResumePosition(resume_position);
	}
	else
	{
		if (receiver->IsActive())
		{
			// About time to send a who request to figure out which ip-address
			// we should broadcast back to the sender of the file.
			IRCMessage who_message;
			who_message.SetCommand(UNI_L("WHO"));
			who_message.AddParameter(receiver->OtherParty());

			DCCWhoReplyHandler* reply_handler = OP_NEW(DCCWhoReplyHandler, (*this, receiver->OtherParty(), 0, receiver->Id(), FALSE));
			SendIRCMessage(who_message, IRCMessageSender::NORMAL_PRIORITY, reply_handler);
		}
		else
			RETURN_IF_ERROR(receiver->StartTransfer());
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::DCCHandleResumeRequest(const CTCPInformation& ctcp_info)
{
	if (ctcp_info.ParameterCount() < 4)
		return OpStatus::ERR;

	const UINT port = uni_atoi(ctcp_info.Parameter(2).CStr());
	const UINT32 resume_position = uni_atoi(ctcp_info.Parameter(3).CStr());

	UINT id = 0;
	if (ctcp_info.ParameterCount() > 4)
		id = uni_atoi(ctcp_info.Parameter(4).CStr());

	if (resume_position == 0)
		return OpStatus::ERR;

	// Look up the sender accociated with this resume request.
	DCCFileSender* sender = 0;
	RETURN_IF_ERROR(m_dcc_sessions.FileSender(port, id, sender));

	if (sender != 0)
	{
		RETURN_IF_ERROR(sender->SetResumePosition(resume_position));

		// Ok, let's send a message back to the sender, telling him that we
		// will allow the resume.
		IRCMessage resume_message;
		RETURN_IF_ERROR(CreateDCCAcceptMessage(*sender, resume_position,
			sender->PortNumber() == 0, resume_message));

		SendIRCMessage(resume_message, IRCMessageSender::NORMAL_PRIORITY, 0);
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::DCCHandleWhoReply(const OpStringC& userhost,
	UINT port, UINT id, BOOL is_sender)
{
	if (is_sender)
	{
		// Fetch the sender.
		DCCFileSender* sender = 0;
		OpStatus::Ignore(m_dcc_sessions.FileSender(port, id, sender));

		if (sender != 0)
		{
			// All we need to do now is to send the send message.
			IRCMessage send_message;
			RETURN_IF_ERROR(CreateDCCSendMessage(sender->OtherParty(),
				sender->FileName(), userhost, sender->PortNumber(),
				(UINT32)sender->GetFileSize(), 0, send_message));

			SendIRCMessage(send_message, IRCMessageSender::NORMAL_PRIORITY);
		}
	}
	else
	{
		// Fetch the receiver.
		DCCFileReceiver* receiver = 0;
		OpStatus::Ignore(m_dcc_sessions.FileReceiver(port, id, receiver));

		if (receiver != 0)
		{
			// Send a message back to the sender, to indicate that we have a
			// server ready.
			IRCMessage send_message;
			RETURN_IF_ERROR(CreateDCCSendMessage(receiver->OtherParty(),
				receiver->FileName(), userhost, receiver->PortNumber(),
				(UINT32)receiver->GetFileSize(), receiver->Id(), send_message));

			SendIRCMessage(send_message, IRCMessageSender::NORMAL_PRIORITY);
		}
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::DoReceiveResume(const CTCPInformation& ctcp_info)
{
	if (ctcp_info.ParameterCount() < 4)
		return OpStatus::ERR;

	const UINT port_number = uni_atoi(ctcp_info.Parameter(2).CStr());
	if (port_number == 0)
		return OpStatus::ERR;

	UINT id = 0;
	if (ctcp_info.ParameterCount() > 4)
		id = uni_atoi(ctcp_info.Parameter(4).CStr());

	// Look up the receiver from this information.
	DCCFileReceiver* receiver = 0;
	RETURN_IF_ERROR(m_dcc_sessions.FileReceiver(port_number, id, receiver));

	if (receiver == 0)
		return OpStatus::ERR_NULL_POINTER;

	if (receiver->IsActive())
	{
		// Look up the user address of the person who are sending the file.
		IRCMessage who_message;
		who_message.SetCommand(UNI_L("WHO"));
		who_message.AddParameter(receiver->OtherParty());

		DCCWhoReplyHandler* reply_handler = OP_NEW(DCCWhoReplyHandler, (*this, receiver->OtherParty(), 0, receiver->Id(), FALSE));
		SendIRCMessage(who_message, IRCMessageSender::NORMAL_PRIORITY, reply_handler);
	}
	else
		receiver->StartTransfer();

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::HandlePresenceISONReply(const OpStringC& nick)
{
	// Get the contact we're receiving a reply for.
	IRCContacts::IRCContact* contact = 0;
	if (OpStatus::IsError(m_contacts.Contact(nick, contact)))
	{
		m_owner.OnPresenceOffline(nick);
		return OpStatus::ERR;
	}

	// Notify of initial detection of online presence if needed.
	if (contact->IsOffline())
		m_owner.OnInitialPresence(nick);

	contact->SetAwaitingOnlineReply(FALSE);
	contact->SetOnline(TRUE);

	// Send WHO.
	IRCMessage who_message;
	RETURN_IF_ERROR(who_message.SetCommand(UNI_L("WHO")));
	RETURN_IF_ERROR(who_message.AddParameter(nick));

	PresenceWhoReplyHandler *rep_han = OP_NEW(PresenceWhoReplyHandler, (*this));
	if (rep_han)
	{
		OpAutoPtr<PresenceWhoReplyHandler> reply_handler(rep_han);
		RETURN_IF_ERROR(rep_han->Init(nick));
		reply_handler.release();
	}

	SendIRCMessage(who_message, IRCMessageSender::LOW_PRIORITY, rep_han);
	return OpStatus::OK;
}


OP_STATUS IRCProtocol::HandlePresenceWHOReply(const OpStringC& nick,
	const OpStringC& status)
{
	// Get the contact we're receiving a reply for.
	IRCContacts::IRCContact* contact = 0;
	if (OpStatus::IsError(m_contacts.Contact(nick, contact)))
	{
		m_owner.OnPresenceOffline(nick);
		return OpStatus::ERR;
	}

	// Notify about the status.
	if (status.Find("G") != KNotFound)
	{
		if (!contact->IsAway())
		{
			// Send WHOIS to figure out the away message.
			IRCMessage whois_message;
			RETURN_IF_ERROR(whois_message.SetCommand(UNI_L("WHOIS")));
			RETURN_IF_ERROR(whois_message.AddParameter(nick));

			PresenceWhoisReplyHandler* reply_handler = OP_NEW(PresenceWhoisReplyHandler, (*this));
			SendIRCMessage(whois_message, IRCMessageSender::LOW_PRIORITY, reply_handler);
		}
	}
	else if (contact->IsAway())
	{
		contact->SetAwayMessage(UNI_L(""));
		m_owner.OnPresenceUpdate(nick, contact->AwayMessage());
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::HandlePresenceWHOISReply(const OpStringC& nick,
	const OpStringC& away_message)
{
	// Get the contact we're receiving a reply for.
	IRCContacts::IRCContact* contact = 0;
	if (OpStatus::IsError(m_contacts.Contact(nick, contact)))
	{
		m_owner.OnPresenceOffline(nick);
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(contact->SetAwayMessage(away_message));
	m_owner.OnPresenceUpdate(nick, away_message);

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::CheckForOfflineContacts()
{
	// Notify offline presence for all offline contacts.
	OpVector<OpStringC> offline_nicknames;
	RETURN_IF_ERROR(m_contacts.OfflineContacts(offline_nicknames));

	for (UINT32 index = 0, count = offline_nicknames.GetCount();
		index < count; ++index)
	{
		m_owner.OnPresenceOffline(*offline_nicknames.Get(index));
	}

	return OpStatus::OK;
}


void IRCProtocol::ProcessModeChanges(const IRCMessage &message, BOOL is_initial)
{
	unsigned int parameter_start_index = 0;
	if (message.IsNumericReply())
	{
		// First parameter of a numeric reply is the target, and that should
		// be us.
		parameter_start_index = 1;
	}

	if ((message.ParameterCount() - parameter_start_index) > 1)
	{
		OpString sender;
		OpString channel;
		OpString modes;
		OpString parameter;

		message.MessageSender(sender);
		channel.Set(message.Parameter(parameter_start_index + 0));
		modes.Set(message.Parameter(parameter_start_index + 1));

		// Ignore non channel modes for the moment.
		if (!IsChannel(channel))
			return;

		OpString room;
		StripChannelPrefix(channel, room);

		// Parse the mode changes and signal them one by one.
		int cur_pos = 0;
		const INT32 length = modes.Length();
		int mode_param_count = 2 + parameter_start_index;

		enum Sign { NoMode, ModePlus, ModeMinus } cur_sign;
		cur_sign = NoMode;

		for (; cur_pos < length; ++cur_pos)
		{
			if (modes[cur_pos] == '+')
				cur_sign = ModePlus;
			else if (modes[cur_pos] == '-')
				cur_sign = ModeMinus;
			else
			{
				OP_ASSERT(cur_sign != NoMode);

				const uni_char mode = modes[cur_pos];
				OpString mode_parameter;

				// Check if the mode needs a parameter.
				if (m_mode_info->RequiresParameter(mode, cur_sign == ModePlus))
				{
					const size_t index = mode_param_count;
					if (message.ParameterCount() > index)
					{
						parameter.Set(message.Parameter(index));
						++mode_param_count;
					}
				}

				BOOL is_user_mode = FALSE;

				const uni_char mode_string[2] = { mode, 0 };
				if (m_allowed_user_mode_changes.Find(mode_string) != KNotFound)
					is_user_mode = TRUE;

				OpStringC empty;
				ChatInfo chat_info(room,channel);

				m_owner.OnChannelModeChanged(chat_info,
					is_initial ? empty : sender, mode,
					cur_sign == ModePlus, parameter, is_user_mode,
					ModeIsOpOrBetter(mode_string),
					ModeIsVoiceOrBetter(mode_string));
			}
		}
	}
}


void IRCProtocol::ProcessServerSupportMessage(const IRCMessage &message)
{
	const OpStringC& parameters = message.Parameters();

	// Check for CHANMODES.
	int find_pos = parameters.Find(UNI_L("CHANMODES="));
	if (find_pos != KNotFound)
	{
		OpString chanmodes;
		chanmodes.Set(parameters);

		chanmodes.Delete(0, find_pos + 10);
		find_pos = chanmodes.Find(UNI_L(" "));
		if (find_pos != KNotFound)
			chanmodes.Delete(find_pos);

		// Tokenize what's left of the string; separate on comma.
		StringTokenizer mode_tokenizer(chanmodes, UNI_L(","));

		for (int chanmode_type = 0; chanmode_type < 4 && mode_tokenizer.HasMoreTokens(); ++chanmode_type)
		{
			OpString cur_modes;
			mode_tokenizer.NextToken(cur_modes);

			const int length = cur_modes.Length();
			for (int index = 0; index < length; ++index)
			{
				const uni_char cur_char = cur_modes[index];
				if (!m_mode_info->HasMode(cur_char))
				{
					switch (chanmode_type)
					{
						case 0 : // Mode that adds or removes a nick or address to a list. Always has a parameter.
						case 1 : // Mode that changes a setting and always has a parameter.
							m_mode_info->AddModeRequiringParameter(cur_char);
							break;
						case 2 : // Mode that changes a setting and only has a parameter when set.
							m_mode_info->AddModeRequiringParameterOnlyWhenSet(cur_char);
							break;
						case 3 : // Mode that changes a setting and never has a parameter.
							m_mode_info->AddModeNotRequiringParameter(cur_char);
							break;
						default :
							assert(0);
							break;
					}
				}
			}
		}
	}

	// Check for CHANTYPES.
	find_pos = parameters.Find(UNI_L("CHANTYPES="));
	if (find_pos != KNotFound)
	{
		OpString chantypes;
		chantypes.Set(parameters);

		chantypes.Delete(0, find_pos + 10);
		find_pos = chantypes.Find(UNI_L(" "));
		if (find_pos != KNotFound)
			chantypes.Delete(find_pos);

		m_allowed_channel_prefixes.Set(chantypes);
	}

	// Check for PREFIX.
	find_pos = parameters.Find(UNI_L("PREFIX="));
	if (find_pos != KNotFound)
	{
		OpString prefix;
		prefix.Set(parameters);

		prefix.Delete(0, find_pos + 7);
		find_pos = prefix.Find(UNI_L(" "));
		if (find_pos != KNotFound)
			prefix.Delete(find_pos);

		// Now we will typically have a string like this:
		// (ov)@+
		// o and v are the mode names, and @ and + are what's displayed
		// in whois, namesreply and such.
		if (prefix[0] == '(')
		{
			const int paranthese_end = prefix.Find(UNI_L(")"));
			if (paranthese_end != KNotFound)
			{
				OpString modes;
				modes.Set(prefix.SubString(1).CStr(), paranthese_end - 1);

				// Loop the modes and add them as modes requiring a parameter.
				const int length = modes.Length();
				for (int index = 0; index < length; ++index)
				{
					const uni_char mode = modes[index];

					if (!m_mode_info->HasMode(mode))
						m_mode_info->AddModeRequiringParameter(mode);
				}

				// Store the valid user mode changes.
				m_allowed_user_mode_changes.Set(modes);

				// Store the valid user prefixes.
				m_allowed_user_prefixes.Set(prefix.SubString(paranthese_end + 1));
			}
		}
	}
}


UINT IRCProtocol::SendIRCMessage(const IRCMessage &message,
	IRCMessageSender::Priority priority, IRCIncomingHandler* handler,
	BOOL notify_owner, UINT delay_secs)
{
	// Get charset and convert to one (or several) raw messages.
	OpString8 charset;
	m_owner.GetCurrentCharset(charset);

	OpAutoVector<OpString8> raw_messages;
	OpStatus::Ignore(message.RawMessages(raw_messages, charset));

	// Process each raw message.
	const UINT32 message_count = raw_messages.GetCount();
	for (UINT32 index = 0; index < message_count; ++index)
	{
		OpString8 *cur_message = raw_messages.Get(index);
		OpStatus::Ignore(m_message_sender.Send(message, *cur_message, priority, index == 0 ? handler : 0, delay_secs));

		// Check if we should notify the GUI.
		if (notify_owner && message.Command() == "JOIN")
		{
			IRCMessage current_irc_message;
			RETURN_IF_ERROR(current_irc_message.FromRawLine(*cur_message, charset, m_servername));
			if (current_irc_message.ParameterCount() >= 1)
			{
				const OpStringC& target = current_irc_message.Parameter(0);
				if (IsChannel(target))
				{
					OpString stripped_room;
					OpString password;
					StripChannelPrefix(target, stripped_room);

					ChatInfo chat_info(stripped_room, target);
					OpString empty;
					if (current_irc_message.ParameterCount() >= 2)
					{
						password.Set(current_irc_message.Parameter(1));
						m_owner.OnJoinedChannelPassword(chat_info, password);
					}
					else
					{
						m_owner.OnJoinedChannel(chat_info, empty);
					}
				}
			}
		}
		else if (notify_owner && (message.Command() == "PRIVMSG" || message.Command() == "NOTICE"))
		{
			IRCMessage current_irc_message;
			RETURN_IF_ERROR(current_irc_message.FromRawLine(*cur_message, charset, m_servername));

			const BOOL is_privmsg = (current_irc_message.Command() == "PRIVMSG");
			BOOL is_action = FALSE;

			if (current_irc_message.ParameterCount() >= 2)
			{
				const OpStringC& target = current_irc_message.Parameter(0);

				OpString text;
				text.Set(current_irc_message.Parameter(1));

				if (current_irc_message.IsCTCPMessage())
				{
					CTCPInformation ctcp_info;
					current_irc_message.CTCPInfo(ctcp_info);

					text.Set(ctcp_info.Parameters());

					if (ctcp_info.Type() == "ACTION")
						is_action = TRUE;
					else
						break;
				}

				if (IsChannel(target))
				{
					OpString stripped_room;
					StripChannelPrefix(target, stripped_room);

					ChatInfo chat_info(stripped_room, target);

					OpStringC empty;
					if (is_privmsg && is_action)
						m_owner.OnCTCPChannelAction(chat_info, text, empty);
					else
						m_owner.OnChannelMessage(chat_info, text, empty);
				}
				else
				{
					ChatInfo chat_info(target);

					if (is_privmsg && is_action)
						m_owner.OnCTCPPrivateAction(chat_info, text, TRUE);
					else
						m_owner.OnPrivateMessage(chat_info, text, TRUE);
				}
			}
		}
	}

	if (handler != 0)
		handler->SetRawMessageCount(message_count);

	return message_count;
}


BOOL IRCProtocol::IsChannel(const OpStringC &channel) const
{
	return (channel.FindFirstOf(m_allowed_channel_prefixes) == 0);
}


void IRCProtocol::StripChannelPrefix(const OpStringC &channel, OpString &out) const
{
	out.Set(channel);

	// As of yet, only strip #.
	if (out.HasContent() && out[0] == '#')
		out.Delete(0, 1);
}


void IRCProtocol::PrefixChannel(const OpStringC &channel, OpString &out) const
{
	out.Set(channel);

	if (channel.IsEmpty())
		return;

	// Since we only strip the # we must not check on it before deciding
	// to prefix or not.
	OpString prefixes;
	prefixes.Set(m_allowed_channel_prefixes);

	const int hash_pos = prefixes.Find(UNI_L("#"));
	if (hash_pos != KNotFound)
		prefixes.Delete(hash_pos, 1);

	// Only prefix the channel if it does not have a channel prefix in front.
	const int first_prefix_pos = out.FindFirstOf(prefixes);
	if (first_prefix_pos == KNotFound || first_prefix_pos > 0)
		out.Insert(0, "#");
}


void IRCProtocol::StripNicknamePrefix(const OpStringC& nick, OpString &out) const
{
	out.Set(nick);

	if (out.FindFirstOf(m_allowed_user_prefixes) == 0)
		out.Delete(0, 1);
}


void IRCProtocol::NicknamePrefix(const OpStringC& nick, OpString& prefix) const
{
	prefix.Set(nick);

	if (prefix.FindFirstOf(m_allowed_user_prefixes) == 0)
		prefix.Delete(1);
	else
		prefix.Empty();
}


OP_STATUS IRCProtocol::CreateBanAddress(OpString& ban_address,
	const OpStringC& user, const OpStringC& hostmask) const
{
	RETURN_IF_ERROR(ban_address.Set("*!*"));
	RETURN_IF_ERROR(ban_address.Append(user));
	RETURN_IF_ERROR(ban_address.Append("@"));

	if (ban_address.Length() > 3 && ban_address[3] == '~')
		ban_address.Delete(3, 1);

	// Find the number of dots in the hostmask, to determine what we should
	// ban from it. If it's a dotted ip address, we want to ban 000.000.000.*,
	// and if it's a hostmasked ip, we want to ban either *.foo.bar.com or
	// *.bar.com.
	BOOL is_ipv4 = FALSE;
	RETURN_IF_ERROR(OpMisc::IsIPV4Address(is_ipv4, hostmask));

	BOOL is_ipv6 = FALSE;
	RETURN_IF_ERROR(OpMisc::IsIPV6Address(is_ipv6, hostmask));

	if (is_ipv4 || is_ipv6)
	{
		// Get the first three numbers.
		int find_pos = StringUtils::FindLastOf(hostmask, UNI_L(":."));
		if (find_pos != KNotFound)
		{
			OpString sub_ip;
			RETURN_IF_ERROR(sub_ip.Set(hostmask));

			sub_ip.Delete(find_pos + 1);

			RETURN_IF_ERROR(ban_address.Append(sub_ip));
			RETURN_IF_ERROR(ban_address.Append("*"));
		}
		else
		{
			// Just use the whole ip.
			RETURN_IF_ERROR(ban_address.Append(hostmask));
		}
	}
	else
	{
		// Does this hostmask have two dots in it? If so, we want to ban on
		// the form *.bar.com.
		const int dot_count = StringUtils::SubstringCount(hostmask, UNI_L("."));
		if (dot_count == 2)
		{
			// Extract everything after the first dot.
			const int find_pos = hostmask.Find(UNI_L("."));
			if (find_pos == KNotFound)
				return OpStatus::ERR;

			OpString sub_hostmask;
			RETURN_IF_ERROR(sub_hostmask.Set(hostmask));

			sub_hostmask.Delete(0, find_pos);

			RETURN_IF_ERROR(ban_address.Append("*"));
			RETURN_IF_ERROR(ban_address.Append(sub_hostmask));
		}
		else if (dot_count > 2)
		{
			// Here, the hostmask may be like '81-86-33-95.dsl.pipex.com',
			// and we want to ban *.dsl.pipex.com.
			const int find_pos = StringUtils::FindNthOf(dot_count - 3, hostmask, UNI_L("."));
			if (find_pos == KNotFound)
				return OpStatus::ERR;

			OpString sub_hostmask;
			RETURN_IF_ERROR(sub_hostmask.Set(hostmask));

			sub_hostmask.Delete(0, find_pos);

			RETURN_IF_ERROR(ban_address.Append("*"));
			RETURN_IF_ERROR(ban_address.Append(sub_hostmask));
		}
		else
		{
			// Just use the whole hostmask.
			RETURN_IF_ERROR(ban_address.Append(hostmask));
		}
	}

	return OpStatus::OK;
}


OP_STATUS IRCProtocol::RespondToCTCPRequest(const IRCMessage& message,
	const OpStringC& command, BOOL& requires_reply, BOOL& handled)
{
	OP_ASSERT(message.IsCTCPMessage());
	OP_ASSERT(message.Command() == "PRIVMSG");

	OpStringC destination(message.ParameterCount() >= 1 ? message.Parameter(0).CStr() : NULL);

	OpString sender;
	message.MessageSender(sender);

	const BOOL destination_is_channel = IsChannel(destination);

	OpString room;
	if (destination_is_channel)
		StripChannelPrefix(destination, room);

	requires_reply = FALSE;
	handled = FALSE;

	CTCPInformation ctcp_info;
	RETURN_IF_ERROR(message.CTCPInfo(ctcp_info));

	if (ctcp_info.Type() == "ACTION" && ctcp_info.ParameterCount() > 0)
	{
		const OpStringC& action = ctcp_info.Parameters();
		if (destination_is_channel)
		{
			ChatInfo chat_info(room, destination);
			m_owner.OnCTCPChannelAction(chat_info, action, sender);
		}
		else
		{
			ChatInfo chat_info(sender);
			m_owner.OnCTCPPrivateAction(chat_info, action, FALSE);
		}

		handled = TRUE;
	}
	else if (ctcp_info.Type() == "PING" && message.ParameterCount() >= 2)
	{
		IRCMessage ping_message;
		ping_message.SetIsCTCP();
		ping_message.SetCommand(UNI_L("NOTICE"));
		ping_message.AddParameter(sender);
		ping_message.AddParameter(message.Parameter(1));

		SendIRCMessage(ping_message, IRCMessageSender::NORMAL_PRIORITY, 0);

		requires_reply = TRUE;
		handled = TRUE;
	}
	else if (ctcp_info.Type() == "VERSION")
	{
		// Get the current version.
		OpString user_agent_reply;
		user_agent_reply.Set(UNI_L("VERSION "));

		{
			char user_agent[256];
			g_uaManager->GetUserAgentStr(user_agent, 256, NULL, NULL, UA_Opera);

			user_agent_reply.Append(user_agent);
		}

		IRCMessage notice_reply;
		notice_reply.SetIsCTCP();
		notice_reply.SetCommand(UNI_L("NOTICE"));
		notice_reply.AddParameter(sender);
		notice_reply.AddParameter(user_agent_reply);

		SendIRCMessage(notice_reply, IRCMessageSender::NORMAL_PRIORITY, 0);

		requires_reply = TRUE;
		handled = TRUE;
	}
	else if (ctcp_info.Type() == "TIME")
	{
		// Reply with the current client time.
		OpString time_string;
		FormatTime(time_string, UNI_L("%c"), m_owner.GetCurrentTime());

		if (time_string.HasContent())
		{
			time_string.Insert(0, "TIME "); // Reply should start with 'TIME'.

			IRCMessage time_reply;
			time_reply.SetIsCTCP();
			time_reply.SetCommand(UNI_L("NOTICE"));
			time_reply.AddParameter(sender);
			time_reply.AddParameter(time_string);

			SendIRCMessage(time_reply, IRCMessageSender::NORMAL_PRIORITY, 0);
			requires_reply = TRUE;
		}

		handled = TRUE;
	}
	else if (ctcp_info.Type() == "DCC" && ctcp_info.ParameterCount() > 0)
	{
		const OpStringC& dcc_type = ctcp_info.Parameter(0);
		if (dcc_type.CompareI("SEND") == 0)
		{
			if (ctcp_info.ParameterCount() < 5)
				return OpStatus::ERR;

			UINT id = 0;
			if (ctcp_info.ParameterCount() > 5)
				id = uni_atoi(ctcp_info.Parameter(5).CStr());

			// Create a receiver.
			DCCFileReceiver *dcc_recv = OP_NEW(DCCFileReceiver, (*this));
			if (!dcc_recv)
				return OpStatus::ERR_NO_MEMORY;

			OpAutoPtr<DCCFileReceiver> receiver(dcc_recv);

			const OpStringC& filename = ctcp_info.Parameter(1);
			const UINT32 file_size = uni_atoi(ctcp_info.Parameter(4).CStr());

			RETURN_IF_ERROR(dcc_recv->InitCommon(sender, filename, file_size, id));

			const UINT port = uni_atoi(ctcp_info.Parameter(3).CStr());

			// If the port is zero we should initialize a
			// receiver that is a server.
			if (port != 0)
			{
				RETURN_IF_ERROR(dcc_recv->InitClient(ctcp_info.Parameter(2), port));

				// Remove any old receivers on this port, before adding a new one.
				DCCFileReceiver* old_receiver = 0;
				OpStatus::Ignore(m_dcc_sessions.FileReceiver(port, 0, old_receiver));

				if (old_receiver != 0)
				{
					m_owner.OnFileReceiveFailed(*old_receiver);
					m_dcc_sessions.RemoveFileReceiver(old_receiver);
				}

				RETURN_IF_ERROR(m_dcc_sessions.AddActiveFileReceiver(dcc_recv));
			}
			else
			{
				// Initialize the receiver.
				UINT start_port = 0, end_port = 0;
				m_owner.GetTransferPortRange(start_port, end_port);

				RETURN_IF_ERROR(dcc_recv->InitServer(start_port, end_port));

				// Add to internal collection.
				RETURN_IF_ERROR(m_dcc_sessions.AddPassiveFileReceiver(dcc_recv));
			}

			// Notify the owner (to ask if we want to receive).
			m_owner.OnDCCSendRequest(sender, filename, file_size,
				port, dcc_recv->Id());

			receiver.release();
			handled = TRUE;
		}
		else if (dcc_type.CompareI("RESUME") == 0)
		{
			RETURN_IF_ERROR(DCCHandleResumeRequest(ctcp_info));
			handled = TRUE;
		}

		requires_reply = TRUE;
	}

	return OpStatus::OK;
}


BOOL IRCProtocol::ModeIsOpOrBetter(const OpStringC& mode) const
{
	// We consider a half-op to be an operator.
	const int op_pos = StringUtils::FindLastOf(m_allowed_user_mode_changes, UNI_L("oh"));
	if (op_pos == KNotFound)
		return FALSE;

	return (m_allowed_user_mode_changes.Find(mode) <= op_pos);
}


BOOL IRCProtocol::ModeIsVoiceOrBetter(const OpStringC& mode) const
{
	const int voice_pos = m_allowed_user_mode_changes.Find("v");
	if (voice_pos == KNotFound)
		return FALSE;

	return (m_allowed_user_mode_changes.Find(mode) <= voice_pos);
}


BOOL IRCProtocol::PrefixIsOpOrBetter(const OpStringC& prefix) const
{
	// We consider a half-op to be an operator.
	const int op_pos = StringUtils::FindLastOf(m_allowed_user_mode_changes, UNI_L("oh"));
	if (op_pos == KNotFound)
		return FALSE;

	return (m_allowed_user_prefixes.Find(prefix) <= op_pos);
}


BOOL IRCProtocol::PrefixIsVoiceOrBetter(const OpStringC& prefix) const
{
	const int voice_pos = m_allowed_user_mode_changes.Find("v");
	if (voice_pos == KNotFound)
		return FALSE;

	return (m_allowed_user_prefixes.Find(prefix) <= voice_pos);
}

#endif // IRC_SUPPORT
