// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "adjunct/m2/src/engine/chatinfo.h"
#include "adjunct/m2/src/engine/engine.h"
#include "irc-message.h"
#include "irc-module.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/regexp/include/regexp_api.h"
#include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/util/authenticate.h"

#include "modules/prefsfile/prefsfile.h"

#include <assert.h>

//********************************************************************
//
//	IRCCommandInterpreter
//
//********************************************************************

static const char g_commands[] =
	// Format: alias, irc-command, is-ctcp, parameters separated by comma.
	"msg, PRIVMSG, 0, %1, %2\r\n"
	"j, JOIN, 0, %#1, %2\r\n"
	"join, JOIN, 0, %#1, %2\r\n"
	"op, MODE, 0, %room, +o, %1\r\n"
	"deop, MODE, 0, %room, -o, %1\r\n"
	"t, TOPIC, 0, %room, %1\r\n"
	"topic, TOPIC, 0, %#1, %2\r\n"
	"me, PRIVMSG, 1, %room, ACTION %%1\r\n"
	"notice, NOTICE, 0, %1, %2\r\n"
	"quit, QUIT, 0, %1\r\n"
	"ctcp, PRIVMSG, 1, %1, %2\r\n"
	"away, AWAY, 0, %1\r\n"
	"w, WHOIS, 0, %%1, %2\r\n"
	"k, KICK, 0, %room, %1, %2\r\n"
	"kick, KICK, 0, %#1, %2, %3\r\n"
	"ping, PRIVMSG, 1, %1, PING %time\r\n"
	"part, PART, 0, %#1, %2\r\n"
	"invite, INVITE, 0, %1, %room\r\n"
	"slap, PRIVMSG, 1, %room, ACTION slaps a large trout around a bit with %%1\r\n";


class IRCCommandInterpreter
{
public:
	// Constructor / destructor.
	IRCCommandInterpreter() { }
	~IRCCommandInterpreter() { }

	OP_STATUS Init(const OpStringC& valid_channel_prefixes);

	// Methods.
	enum ReturnCode { OK, NOT_ENOUGH_PARAMETERS, FAILED };
	ReturnCode Interpret(const OpStringC& line, const OpStringC& target, const OpStringC& my_nick, IRCMessage& interpreted) const;

private:
	// Internal class.
	class CommandObj
	{
	public:
		// Constructor.
		CommandObj() : m_highest_variable_count(0) { }
		OP_STATUS Init(const OpStringC& line);

		// Methods.
		const OpStringC& Alias() const { return m_alias; }
		const OpStringC& IRCCommand() const { return m_irc_command; }
		BOOL IsCTCP() const { return m_is_ctcp; }

		UINT32 ParameterCount() const { return m_parameters.GetCount(); }
		const OpStringC& Parameter(UINT32 index) const;

		UINT VariableCount() const { return m_highest_variable_count; }

	private:
		// Methods.
		UINT HighestVariableIndex(const OpStringC& parameter) const;

		// Members.
		OpString m_alias;
		OpString m_irc_command;
		BOOL m_is_ctcp;
		OpAutoVector<OpString> m_parameters;

		UINT m_highest_variable_count;
	};

	// Members.
	mutable OpAutoStringHashTable<CommandObj> m_commands;
	OpString m_valid_channel_prefixes;
};


OP_STATUS IRCCommandInterpreter::Init(const OpStringC& valid_channel_prefixes)
{
	OpString work_string;
	RETURN_IF_ERROR(work_string.Set(g_commands));

	RETURN_IF_ERROR(m_valid_channel_prefixes.Set(valid_channel_prefixes));

	int newline_pos = work_string.Find("\r\n");
	while (newline_pos != KNotFound)
	{
 		OpString line;
 		line.Set(work_string.CStr(), newline_pos);

		CommandObj *comobj = OP_NEW(CommandObj, ());
		if (!comobj)
 			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<CommandObj> command_obj(comobj);

		if (OpStatus::IsSuccess(comobj->Init(line)))
		{
			if (!m_commands.Contains(comobj->Alias().CStr()))
			{
				RETURN_IF_ERROR(m_commands.Add(comobj->Alias().CStr(), comobj));
				command_obj.release();
			}
 		}

 		work_string.Delete(0, newline_pos + 2);
 		newline_pos = work_string.Find("\r\n");
 	}

	return OpStatus::OK;
}


IRCCommandInterpreter::ReturnCode IRCCommandInterpreter::Interpret(const OpStringC& line,
	const OpStringC& target, const OpStringC& my_nick, IRCMessage& interpreted) const
{
	StringTokenizer main_tokenizer(line);

	OpString alias;
	if (OpStatus::IsError(main_tokenizer.NextToken(alias)))
		return FAILED;

	if (m_commands.Contains(alias.CStr()))
	{
		CommandObj* command_obj = 0;
		m_commands.GetData(alias.CStr(), &command_obj);
		OP_ASSERT(command_obj != 0);

		interpreted.SetCommand(command_obj->IRCCommand());
		interpreted.SetIsCTCP(command_obj->IsCTCP());

		OpString remaining_string;
		main_tokenizer.RemainingString(remaining_string);

		// Loop and add parameters, while substituting them.
		for (UINT index = 0; index < command_obj->ParameterCount(); ++index)
		{
			StringTokenizer tokenizer(remaining_string);

			OpString parameter;
			parameter.Set(command_obj->Parameter(index));

			const UINT variable_count = command_obj->VariableCount();
			for (UINT count = 0; count < variable_count; ++count)
			{
				// Break out if the parameter does not contain any %.
				if (parameter.Find(UNI_L("%")) == KNotFound)
					break;

				OpString token;

				if (count + 1 == variable_count)
					tokenizer.RemainingString(token);
				else
					tokenizer.NextToken(token);

				OpString variable_string;

				variable_string.AppendFormat(UNI_L("%%%%%u"), count + 1);

				if (parameter.Find(variable_string) != KNotFound)
				{
					// We have a required variable.
					if (token.HasContent())
						StringUtils::Replace(parameter, variable_string, token);
					else
						return NOT_ENOUGH_PARAMETERS;
				}

				// Replace %digit.
				variable_string.Empty();
				variable_string.AppendFormat(UNI_L("%%%u"), count + 1);

				StringUtils::Replace(parameter, variable_string, token);

				// Replace %#digit.
				variable_string.Empty();
				variable_string.AppendFormat(UNI_L("%%#%u"), count + 1);

				const int prefix_pos = token.FindFirstOf(m_valid_channel_prefixes);

				if (token.Length() > 0 && prefix_pos != 0)
					token.Insert(0, "#");
				else if ((token.Length() == 1) && (prefix_pos == 0))
					token.Empty();

				StringUtils::Replace(parameter, variable_string, token);

				// Replace %room and %me.
				StringUtils::Replace(parameter, UNI_L("%room"), target);
				StringUtils::Replace(parameter, UNI_L("%me"), my_nick);

				// Replace %time with the current timestamp.
				if (parameter.Find("%time") != KNotFound)
				{
					const time_t current_time = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();
					OpString time_string;
					time_string.AppendFormat(UNI_L("%u"), current_time);

					StringUtils::Replace(parameter, UNI_L("%time"), time_string);
				}
			}

			if (parameter.HasContent())
			{
				if (OpStatus::IsError(interpreted.AddParameter(parameter)))
					return FAILED;
			}
		}
	}
	else if ((alias.CompareI("raw")) == 0 || alias.CompareI("quote") == 0)
	{
		OpString raw_line;
		main_tokenizer.RemainingString(raw_line);

		if (OpStatus::IsError(interpreted.FromRawLine(raw_line)))
			return FAILED;
	}
	else
	{
		if (OpStatus::IsError(interpreted.FromRawLine(line)))
			return FAILED;
	}

	return OK;
}


OP_STATUS IRCCommandInterpreter::CommandObj::Init(const OpStringC& line)
{
	StringTokenizer tokenizer(line, UNI_L(","));

	RETURN_IF_ERROR(tokenizer.NextToken(m_alias));
	m_alias.Strip();
	m_alias.MakeLower();

	RETURN_IF_ERROR(tokenizer.NextToken(m_irc_command));
	m_irc_command.Strip();

	{
		OpString is_ctcp;
		m_is_ctcp = FALSE;

		if (OpStatus::IsSuccess(tokenizer.NextToken(is_ctcp)))
		{
			is_ctcp.Strip();
			if (is_ctcp.Compare("0") != 0)
				m_is_ctcp = TRUE;
		}
	}

	while (tokenizer.HasMoreTokens())
	{
		OpString *param = OP_NEW(OpString, ());
		if (!param)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpString> parameter(param);

		RETURN_IF_ERROR(tokenizer.NextToken(*param));
		param->Strip();

		RETURN_IF_ERROR(m_parameters.Add(param));
		parameter.release();

		const UINT highest_index = HighestVariableIndex(*param);
		if (highest_index > m_highest_variable_count)
			m_highest_variable_count = highest_index;
	}

	return OpStatus::OK;
}


const OpStringC& IRCCommandInterpreter::CommandObj::Parameter(UINT32 index) const
{
	OP_ASSERT(ParameterCount() > index);
	return *m_parameters.Get(index);
}


UINT IRCCommandInterpreter::CommandObj::HighestVariableIndex(const OpStringC& parameter) const
{
	OpRegExp* reg_exp;

	OpREFlags flags;
	flags.multi_line = FALSE;
	flags.case_insensitive = TRUE;
	flags.ignore_whitespace = FALSE;

	// Match %digit, %%digit or %#digit.
	RETURN_VALUE_IF_ERROR(OpRegExp::CreateRegExp(&reg_exp, UNI_L("%%?#?\\d"), &flags), 0);

	int matches = 0;
	OpREMatchLoc* match_locations = 0;
	OpAutoPtr<OpRegExp> reg_exp_ptr(reg_exp);

	RETURN_VALUE_IF_ERROR(reg_exp->Match(parameter.CStr(), &matches, &match_locations), 0);

	if (!matches)
		return 0;

	UINT highest = 0;

	// Loop through the matches and find the highest digit.
	for (int index = 0; index < matches; ++index)
	{
		OpString match;
		match.Set(parameter.CStr()+match_locations[index].matchloc, match_locations[index].matchlen);

		UINT digit;

		if (match.Find("%%") != KNotFound)
			digit = StringUtils::NumberFromString(match, 2);
		else if (match.Find("%#") != KNotFound)
			digit = StringUtils::NumberFromString(match, 2);
		else
			digit = StringUtils::NumberFromString(match, 1);

		if (digit > highest)
			highest = digit;
	}

	OP_DELETEA(match_locations);
	return highest;
}


//********************************************************************
//
//	IRCBackend
//
//********************************************************************

IRCBackend::IRCBackend(MessageDatabase& database)
  : ProtocolBackend(database),
	m_protocol(0),
	m_command_interpreter(0),
	m_should_list_rooms(FALSE),
	m_chat_status(AccountTypes::OFFLINE),
	m_server_connect_index(0),
	m_connect_count(0),
	m_start_dcc_pool(1024),
	m_end_dcc_pool(1040),
	m_max_ctcp_requests(3), // Max 3 ctcp requests...
	m_max_ctcp_request_interval(10), // ... in 10 seconds.
	m_can_accept_incoming_connections(TRUE),
	m_open_private_chats_in_background(TRUE)
{
}


IRCBackend::~IRCBackend()
{
	Disconnect();
	WriteSettings();

	if (g_hotlist_manager != 0)
		g_hotlist_manager->GetContactsModel()->RemoveModelListener(this);

	g_sleep_manager->RemoveSleepListener(this);
	g_main_message_handler->UnsetCallBacks(this);
}


const UINT IRCBackend::m_presence_interval = 10000;


OP_STATUS IRCBackend::Init(Account* account)
{
	if (!account)
		return OpStatus::ERR_NULL_POINTER; //account_id is not a pointer, but 0 is an illegal account_id and should return an error

	m_account = account;

	// Initialize messages
	g_main_message_handler->SetCallBack(this, MSG_M2_CONNECT, (MH_PARAM_1)this);

	// Init command interpreter.
	IRCCommandInterpreter *comm_int = OP_NEW(IRCCommandInterpreter, ());
	m_command_interpreter = comm_int;
	if (!comm_int)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(comm_int->Init(UNI_L("&#+!")));

	// Load settings.
	OpStatus::Ignore(ReadSettings());

	// Add ourseleves as a listener to the hotlist manager.
	if (g_hotlist_manager != 0)
		g_hotlist_manager->GetContactsModel()->AddModelListener(this);

	RETURN_IF_ERROR(g_sleep_manager->AddSleepListener(this));

	return OpStatus::OK;
}

OP_STATUS IRCBackend::SettingsChanged(BOOL startup)
{
	return OpStatus::OK;
}

OP_STATUS IRCBackend::JoinChatRoom(const OpStringC& room_name, const OpStringC& password, BOOL no_lookup)
{
	BOOL save_config = FALSE;
	OpString password_to_use;
	RETURN_IF_ERROR(password_to_use.Set(password));

	OpString room_copy;
	RETURN_IF_ERROR(room_copy.Set(room_name));

	AddSubscribedFolder(room_copy, password_to_use);

	Room* room = NULL;

	// Only search for the password if asked to
	if (!no_lookup)
		room = m_rooms.Get(FindSubscribedFolder(room_copy));

	if (room)
	{
		if (password_to_use.IsEmpty())
		{
			password_to_use.Set(room->m_password.CStr());
		}
		else
		{
			room->m_password.Set(password_to_use.CStr());
		}
	}
	else
	{
		// password is set, save it to the "room"
		room = m_rooms.Get(FindSubscribedFolder(room_copy));
		if(room)
		{
			if(room->m_password.Compare(password_to_use))
			{
				// password has changed, save the config
				save_config = TRUE;
			}
			room->m_password.Set(password_to_use.CStr());
		}
	}
	if(save_config)
	{
		RETURN_IF_ERROR(WriteSettings());
	}
	if (IsConnected())
	{
		return m_protocol->JoinChatRoom(room_name, password_to_use);
	}
	else
	{
		Room* room = OP_NEW(Room, ());
		if (room)
		{
			room->m_room_name.Set(room_name);
			room->m_password.Set(password_to_use);
			m_connecting_rooms.Add(room);
		}
		return Connect();
	}
}

OP_STATUS IRCBackend::LeaveChatRoom(const ChatInfo& chat_info)
{
	if (!IsConnected())
	{
		// remove any matching room from joining list

		INT32 count = m_connecting_rooms.GetCount();

		for (INT32 i = 0; i < count; i++)
		{
			Room* room = m_connecting_rooms.Get(i);

			if (room->m_room_name.CompareI(chat_info.ChatName()) == 0)
			{
				m_connecting_rooms.Delete(i);
				break;
			}

		}

		return OpStatus::OK;
	}

	return m_protocol->LeaveChatRoom(chat_info);
}

OP_STATUS IRCBackend::SendChatMessage(EngineTypes::ChatMessageType type,
	const OpStringC& message, const ChatInfo& chat_info,
	const OpStringC& chatter)
{
	OP_STATUS status = OpStatus::OK;

	if (!IsConnected())
		status = OpStatus::ERR;
	else if (type == EngineTypes::RAW_COMMAND)
		status = InterpretAndSend(message, chat_info, chatter, TRUE);
	else
		status = m_protocol->SendChatMessage(type, message, chat_info, chatter);

	return status;
}


OP_STATUS IRCBackend::SetChatProperty(const ChatInfo& chat_info, const OpStringC& chatter, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value)
{
	if (!IsConnected())
		return OpStatus::ERR;

	return m_protocol->SetChatProperty(chat_info, chatter, property, property_string, property_value);
}


OP_STATUS IRCBackend::SendFile(const OpStringC& chatter, const OpStringC& filename)
{
	if (!IsConnected())
		return OpStatus::ERR;

	return m_protocol->DccSend(chatter, filename, m_can_accept_incoming_connections);
}


BOOL IRCBackend::IsChatStatusAvailable(AccountTypes::ChatStatus chat_status)
{
	BOOL available = FALSE;

	switch (chat_status)
	{
		case AccountTypes::ONLINE :
		case AccountTypes::BUSY :
		case AccountTypes::BE_RIGHT_BACK :
		case AccountTypes::AWAY :
		case AccountTypes::ON_THE_PHONE :
		case AccountTypes::OUT_TO_LUNCH :
		case AccountTypes::OFFLINE :
			available = TRUE;
			break;
	}

	return available;
}

OP_STATUS IRCBackend::SetChatStatus(AccountTypes::ChatStatus chat_status)
{
	if (chat_status == AccountTypes::OFFLINE)
		OpStatus::Ignore(Disconnect());
	else if (m_chat_status == AccountTypes::OFFLINE)
		OpStatus::Ignore(Connect());

	UpdateChatStatus(chat_status);
	return OpStatus::OK;
}

AccountTypes::ChatStatus IRCBackend::GetChatStatus(BOOL& is_connecting)
{
	if (m_protocol.get() == 0)
		is_connecting = FALSE;
	else
		is_connecting = (m_protocol->State() == IRCProtocol::CONNECTING);

	return m_chat_status;
}

OP_STATUS IRCBackend::Connect()
{
	if (!HasRetrievedPassword())
		return RetrievePassword();

	if (m_protocol.get() != 0)
		return OpStatus::ERR;

	if (m_chat_status == AccountTypes::OFFLINE)
		UpdateChatStatus(AccountTypes::ONLINE);

	// Fetch the next servername we should try to connect to.
	OpString8 servername;

	// Servername may, for IRC, contain several servers.
	{
		OpAutoVector<OpString> servernames;
		GetAccountPtr()->GetIncomingServernames(servernames);

		if (servernames.GetCount() == 0)
			return OpStatus::ERR;

		if (m_server_connect_index >= UINT(servernames.GetCount()))
			m_server_connect_index = 0;

		OpString* selected_server = servernames.Get(m_server_connect_index);
		if (selected_server == 0)
			return OpStatus::ERR;

		servername.Set(selected_server->CStr());
	}

	UINT16 port;
	RETURN_IF_ERROR(GetPort(port));

	OpAuthenticate info;
	RETURN_IF_ERROR(GetLoginInfo(info));

	m_protocol = OP_NEW(IRCProtocol, (*this));
	if (m_protocol.get() == 0)
		return OpStatus::ERR_NO_MEMORY;

	OpString real_name;
	OpString user;

	RETURN_IF_ERROR(m_account->GetRealname(real_name));
	RETURN_IF_ERROR(m_account->GetEmail(user));

	// we only want the first part of the email as username..
	const int end = user.FindFirstOf(UNI_L("@ "));
	if (end != KNotFound)
		user.Delete(end);

	if (user.IsEmpty())
		user.Set("opera");

	const BOOL use_ssl = GetAccountPtr()->GetUseSecureConnectionIn();
	if (m_protocol->Init(servername, port, info.GetPassword(), m_account->GetIncomingUsername(), user, real_name, use_ssl) != OpStatus::OK)
	{
		m_protocol = 0;
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}


OP_STATUS IRCBackend::Disconnect()
{
	if (m_protocol.get() == 0)
		return OpStatus::OK;

	RETURN_IF_ERROR(m_protocol->Cancel(0));
	Cleanup();

	return OpStatus::OK;
}


void IRCBackend::OnSleep()
{
	Disconnect();
}


void IRCBackend::OnWakeUp()
{
}


void IRCBackend::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_M2_CONNECT)
		Connect();
}


UINT32 IRCBackend::GetSubscribedFolderCount()
{
	return m_rooms.GetCount();
}


OP_STATUS IRCBackend::GetSubscribedFolderName(UINT32 index, OpString& name)
{
	Room* room = m_rooms.Get(index);
	return name.Set(room->m_room_name.CStr());
}


OP_STATUS IRCBackend::AddSubscribedFolder(OpString& name, OpString& password)
{
	if (FindSubscribedFolder(name) != -1)
		return OpStatus::OK;

	Room* room = OP_NEW(Room, ());
	if (room)
	{
		RETURN_IF_ERROR(room->m_room_name.Set(name));
		RETURN_IF_ERROR(room->m_password.Set(password));
		return m_rooms.Add(room);
	}
	return OpStatus::ERR_NO_MEMORY;
}


OP_STATUS IRCBackend::RemoveSubscribedFolder(UINT32 index)
{
	m_rooms.Delete(index);
	return OpStatus::OK;
}


OP_STATUS IRCBackend::RemoveSubscribedFolder(const OpStringC& name)
{
	return RemoveSubscribedFolder(FindSubscribedFolder(name));
}


INT32 IRCBackend::FindSubscribedFolder(const OpStringC& name)
{
	for (UINT32 i = 0; i < m_rooms.GetCount(); i++)
	{
		if (m_rooms.Get(i)->m_room_name.Compare(name) == 0)
		{
			return i;
		}
	}

	return -1;
}


void IRCBackend::GetAllFolders()
{
	if (m_protocol.get() == 0)
	{
		Connect();
		m_should_list_rooms = TRUE;
	}
	else
		m_protocol->ListChatRooms();
}


void IRCBackend::StopFolderLoading()
{
}


void IRCBackend::OnConnectionLost(BOOL retry)
{
	if (m_chat_status == AccountTypes::OFFLINE)
	{
		OpAutoVector<OpString> servernames;
		GetAccountPtr()->GetIncomingServernames(servernames);

		if (servernames.GetCount() == 0)
			return;

		// Try next server
		m_server_connect_index = (m_server_connect_index + 1) % servernames.GetCount();
	}
	else
	{
		UpdateChatStatus(AccountTypes::OFFLINE);
	}

	// Try connecting again
	if (retry && m_connect_count < m_max_connect_retries)
	{
		INT32 count = m_connecting_rooms.GetCount();

		for (INT32 i = 0; i < count; i++)
		{
			Room* room = m_connecting_rooms.Get(i);

			ChatInfo chat_info(room->m_room_name, NULL);

			GetAccountPtr()->OnChatReconnecting(chat_info);
		}

		Cleanup(TRUE);
		m_connect_count++;
		g_main_message_handler->PostMessage(MSG_M2_CONNECT, (MH_PARAM_1)this, 0, m_connection_delay);
	}
	else
	{
		Cleanup(FALSE);
	}
}


void IRCBackend::OnRestartRequested()
{
	// Disconnect
	if (m_protocol.get())
		m_protocol->Cancel(0, FALSE);

	// Cleanup, but get ready to resume
	Cleanup(TRUE);

	g_main_message_handler->PostMessage(MSG_M2_CONNECT, (MH_PARAM_1)this, 0, m_connection_delay);
}


void IRCBackend::OnProtocolReady()
{
	// Reset connect count
	m_connect_count = 0;

	// Join the rooms that the user has requested to join.
	INT32 count = m_connecting_rooms.GetCount();
	for (INT32 i = 0; i < count; i++)
	{
		Room* room = m_connecting_rooms.Get(i);
		m_protocol->JoinChatRoom(room->m_room_name, room->m_password);
	}

//	m_connecting_rooms.DeleteAll();

	// List rooms if we need to.
	if (m_should_list_rooms)
	{
		m_protocol->ListChatRooms();
		m_should_list_rooms = FALSE;
	}

	// Get the list of commands that the user wished to execute upon
	// connection, and send them.
	OpAutoVector<OpString> commands;
	GetPerformWhenConnected(commands);

	UINT32 command_count = commands.GetCount();
	for (UINT32 index = 0; index < command_count; ++index)
	{
		OpString *command = commands.Get(index);
		OP_ASSERT(command != 0);

		// In case the command begins with a slash, we must remove it.
		if (command->Length() > 0 && command->CStr()[0] == '/')
			command->Delete(0, 1);

		ChatInfo chat_info;
		InterpretAndSend(*command, chat_info, OpStringC(), FALSE);
	}
}


void IRCBackend::OnInitialPresence(const OpStringC& nick)
{
	// Notify GUI.
	OpStringC empty;
	ChatInfo chat_info;

	GetAccountPtr()->OnChatPropertyChanged(chat_info, nick, empty,
		EngineTypes::CHATTER_PRESENCE, empty, AccountTypes::ONLINE);
}


void IRCBackend::OnPresenceUpdate(const OpStringC& nick, const OpStringC& away_reason)
{
	AccountTypes::ChatStatus chat_status = AccountTypes::ONLINE;

	// Try to see if the away_reason resembles any of the predefined opera ones.
	if (away_reason.HasContent())
	{
		// Compare text to the different away types.
		if (away_reason.FindI(UNI_L("busy")) != KNotFound)
			chat_status = AccountTypes::BUSY;

		else if (away_reason.FindI(UNI_L("be right back")) != KNotFound)
			chat_status = AccountTypes::BE_RIGHT_BACK;

		else if (away_reason.FindI(UNI_L("brb")) != KNotFound)
			chat_status = AccountTypes::BE_RIGHT_BACK;

		else if (away_reason.FindI(UNI_L("phone")) != KNotFound)
			chat_status = AccountTypes::ON_THE_PHONE;

		else if (away_reason.FindI(UNI_L("lunch")) != KNotFound)
			chat_status = AccountTypes::OUT_TO_LUNCH;

		else
			chat_status = AccountTypes::AWAY;
	}

	// Notify GUI.
	OpStringC empty;
	ChatInfo chat_info;

	GetAccountPtr()->OnChatPropertyChanged(chat_info, nick, empty,
		EngineTypes::CHATTER_PRESENCE, empty, chat_status);
}


void IRCBackend::OnPresenceOffline(const OpStringC& nick)
{
	// Notify the GUI.
	OpStringC empty;
	ChatInfo chat_info;

	GetAccountPtr()->OnChatPropertyChanged(chat_info, nick, empty,
		EngineTypes::CHATTER_PRESENCE, empty, AccountTypes::OFFLINE);
}


void IRCBackend::OnIdentdConnectionEstablished(const OpStringC &connection_to)
{
	OpString8 connection_to8;
	connection_to8.Set(connection_to.CStr());

	Log("Identd connection established:", connection_to8);
}


void IRCBackend::OnIdentdIdentConfirmed(const OpStringC8 &request, const OpStringC8 &response)
{
	Log("Identd request:", request);
	Log("Identd response:", response);
}


void IRCBackend::OnRawMessageReceived(const OpStringC8& message)
{
	Log("Received:", message);
}


void IRCBackend::OnRawMessageSent(const OpStringC8& message)
{
	Log("Sent:", message);
}


void IRCBackend::OnNicknameInUse(const OpStringC& nick)
{
	GetAccountPtr()->OnChangeNickRequired(nick);
}


void IRCBackend::OnChannelPasswordRequired(const ChatInfo& chat_room)
{
	OpString room;
	RETURN_VOID_IF_ERROR(room.Set(chat_room.ChatName()));
	if (FindSubscribedFolder(room) != -1) // Make sure that the error message is for a channel that are joined
		GetAccountPtr()->OnRoomPasswordRequired(room);
}


void IRCBackend::OnCTCPPrivateAction(const ChatInfo& chat_info,
	const OpStringC& action, BOOL self_action)
{
	EngineTypes::ChatMessageType chat_message_type = EngineTypes::NO_MESSAGE;

	if (self_action)
		chat_message_type = EngineTypes::PRIVATE_SELF_ACTION_MESSAGE;
	else
		chat_message_type = EngineTypes::PRIVATE_ACTION_MESSAGE;

	GetAccountPtr()->OnChatMessageReceived(chat_message_type, action,
		chat_info, self_action ? OpStringC() : chat_info.ChatName(), TRUE);
}


void IRCBackend::OnCTCPChannelAction(const ChatInfo& chat_info,
	const OpStringC& action, const OpStringC& from)
{
	EngineTypes::ChatMessageType chat_message_type = EngineTypes::NO_MESSAGE;

	if (from.HasContent())
		chat_message_type = EngineTypes::ROOM_ACTION_MESSAGE;
	else
		chat_message_type = EngineTypes::ROOM_SELF_ACTION_MESSAGE;

	GetAccountPtr()->OnChatMessageReceived(chat_message_type, action,
		chat_info, (chat_message_type == EngineTypes::ROOM_ACTION_MESSAGE) ?
		from : OpStringC(), FALSE);
}


void IRCBackend::OnDCCSendRequest(const OpStringC& sender,
	const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id)
{
	GetAccountPtr()->OnFileReceiveRequest(sender, filename, file_size, port_number, id);
}


void IRCBackend::OnServerMessage(const OpStringC& message)
{
	OpStringC empty;
	ChatInfo chat_info;
	GetAccountPtr()->OnChatMessageReceived(EngineTypes::SERVER_MESSAGE,
		message, chat_info, empty, FALSE);
}


void IRCBackend::OnServerInformation(const OpStringC& information)
{
	OpString server_name;
	OpStatus::Ignore(m_protocol->ServerName(server_name));

	GetAccountPtr()->OnChatServerInformation(server_name, information);
}


void IRCBackend::OnPrivateMessage(const ChatInfo& chat_info,
	const OpStringC& message, BOOL self_message)
{
	EngineTypes::ChatMessageType chat_message_type = EngineTypes::NO_MESSAGE;

	if (self_message)
		chat_message_type = EngineTypes::PRIVATE_SELF_MESSAGE;
	else
		chat_message_type = EngineTypes::PRIVATE_MESSAGE;

	GetAccountPtr()->OnChatMessageReceived(chat_message_type, message,
		chat_info, self_message ? OpStringC() : chat_info.ChatName(), TRUE);
}


void IRCBackend::OnChannelMessage(const ChatInfo& chat_info,
	const OpStringC& message, const OpStringC& from)
{
	EngineTypes::ChatMessageType chat_message_type = EngineTypes::NO_MESSAGE;

	if (from.HasContent())
		chat_message_type = EngineTypes::ROOM_MESSAGE;
	else
		chat_message_type = EngineTypes::ROOM_SELF_MESSAGE;

	GetAccountPtr()->OnChatMessageReceived(chat_message_type, message,
		chat_info, (chat_message_type == EngineTypes::ROOM_MESSAGE) ?
		from : OpStringC(), FALSE);
}


void IRCBackend::OnJoinedChannel(const ChatInfo& chat_room, const OpStringC &nick)
{
	if (nick.IsEmpty())
		GetAccountPtr()->OnChatRoomJoined(chat_room);
	else
	{
		const OpString prefix;
		GetAccountPtr()->OnChatterJoined(chat_room, nick, FALSE, FALSE, prefix, FALSE);
	}
}

void IRCBackend::OnJoinedChannelPassword(const ChatInfo& chat_room, const OpStringC &password)
{
	GetAccountPtr()->OnChatRoomJoined(chat_room);
	OpString room_name;
	RETURN_VOID_IF_ERROR(room_name.Set(chat_room.ChatName()));
	if (password.HasContent())
	{
		Room* room = m_rooms.Get(FindSubscribedFolder(room_name));;
		if (room) // Make sure that the error message is for a channel that are joined
		{
			room->m_password.Set(password);
			WriteSettings();
		}
	}
}

void IRCBackend::OnLeftChannel(const ChatInfo& chat_room, const OpStringC &nick, const OpStringC& leave_reason)
{
	OpStringC empty;

	if (nick.IsEmpty())
		GetAccountPtr()->OnChatRoomLeft(chat_room, empty, empty);
	else
		GetAccountPtr()->OnChatterLeft(chat_room, nick, leave_reason, empty);
}


void IRCBackend::OnQuit(const OpStringC& nick, const OpStringC& quit_message)
{
	OpStringC empty;
	ChatInfo chat_info;
	GetAccountPtr()->OnChatterLeft(chat_info, nick, quit_message, empty);
}


void IRCBackend::OnKickedFromChannel(const ChatInfo& chat_room, const OpStringC& nick, const OpStringC& kicker, const OpStringC &kick_reason)
{
	if (nick.IsEmpty())
		GetAccountPtr()->OnChatRoomLeft(chat_room, kicker, kick_reason);
	else
		GetAccountPtr()->OnChatterLeft(chat_room, nick, kick_reason, kicker);
}


void IRCBackend::OnTopicChanged(const ChatInfo& chat_room, const OpStringC& nick, const OpStringC& topic)
{
	OpStringC empty;
	GetAccountPtr()->OnChatPropertyChanged(chat_room, empty, nick,
	EngineTypes::ROOM_TOPIC, topic, 0);
}


void IRCBackend::OnNickChanged(const OpStringC& old_nick, const OpStringC& new_nick)
{
	OpStringC empty;
	ChatInfo chat_info;
	GetAccountPtr()->OnChatPropertyChanged(chat_info, old_nick, empty,
		EngineTypes::CHATTER_NICK, new_nick, 0);
}


void IRCBackend::OnWhoisReply(const OpStringC& nick, const OpStringC& user_id, const OpStringC& host,
	const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message,
	const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& rooms)
{
	GetAccountPtr()->OnWhoisReply(nick, user_id, host, real_name, server,
		server_info, away_message, logged_in_as, is_irc_operator,
		seconds_idle, signed_on, rooms);
}


void IRCBackend::OnInvite(const OpStringC& nick, const ChatInfo& chat_room)
{
	GetAccountPtr()->OnInvite(nick, chat_room);
}


void IRCBackend::OnNickAway(const OpStringC& nick, const OpStringC& away_reason)
{
	OpString formatted;
	formatted.AppendFormat(UNI_L("%s is away (%s)"), nick.CStr(), away_reason.CStr());
	OnServerMessage(formatted);
}


void IRCBackend::OnChannelModeChanged(const ChatInfo& chat_room,
	const OpStringC& nick, uni_char mode, BOOL set,
	const OpStringC& parameter, BOOL is_user_mode,
	BOOL is_operator_or_better, BOOL is_voice_or_better)
{
	OpStringC empty;

	if (mode == 'o' && parameter.HasContent())
	{
		GetAccountPtr()->OnChatPropertyChanged(chat_room, parameter, nick,
			EngineTypes::CHATTER_OPERATOR, empty, set ? 1 : 0);
	}
	else if (mode == 'v' && parameter.HasContent())
	{
		GetAccountPtr()->OnChatPropertyChanged(chat_room, parameter, nick,
			EngineTypes::CHATTER_VOICED, empty, set ? 1 : 0);
	}
	else if (mode == 'm')
	{
		GetAccountPtr()->OnChatPropertyChanged(chat_room, empty, nick,
			EngineTypes::ROOM_MODERATED, empty, set ? 1 : 0);
	}
	else if (mode == 'l')
	{
		GetAccountPtr()->OnChatPropertyChanged(chat_room, empty, nick,
			EngineTypes::ROOM_CHATTER_LIMIT, parameter, set ? 1 : 0);
	}
	else if (mode == 't')
	{
		GetAccountPtr()->OnChatPropertyChanged(chat_room, empty, nick,
			EngineTypes::ROOM_TOPIC_PROTECTION, empty, set ? 1 : 0);
	}
	else if (mode == 'k' && parameter.HasContent())
	{
		GetAccountPtr()->OnChatPropertyChanged(chat_room, empty, nick,
			EngineTypes::ROOM_PASSWORD, parameter, set ? 1 : 0);
	}
	else if (mode == 's')
	{
		GetAccountPtr()->OnChatPropertyChanged(chat_room, empty, nick,
			EngineTypes::ROOM_SECRET, empty, set ? 1 : 0);
	}
	else
	{
		OpString mode_string;

		if (parameter.HasContent())
			mode_string.AppendFormat(UNI_L("%c %s"), mode, parameter.CStr());
		else
			mode_string.AppendFormat(UNI_L("%c"), mode);

		if (set)
			mode_string.Insert(0, UNI_L("+"));
		else
			mode_string.Insert(0, UNI_L("-"));

		EngineTypes::ChatProperty chat_property;
		if (is_user_mode)
		{
			if (is_operator_or_better)
				chat_property = EngineTypes::CHATTER_UNKNOWN_MODE_OPERATOR;
			else if (is_voice_or_better)
				chat_property = EngineTypes::CHATTER_UNKNOWN_MODE_VOICED;
			else
				chat_property = EngineTypes::CHATTER_UNKNOWN_MODE;
		}
		else
			chat_property = EngineTypes::ROOM_UNKNOWN_MODE;

		GetAccountPtr()->OnChatPropertyChanged(chat_room, parameter, nick,
			chat_property, mode_string, set);
	}
}


void IRCBackend::OnChannelListReply(const ChatInfo& chat_room, UINT visible_users, const OpStringC& topic)
{
	OpString channel_string;
	OpString topic_string;

	channel_string.Set(chat_room.ChatName());
	topic_string.Set(topic);

	GetAccountPtr()->OnFolderAdded(topic_string, channel_string, FALSE, visible_users);
}


void IRCBackend::OnChannelListDone()
{
	GetAccountPtr()->OnFolderLoadingCompleted();
}


void IRCBackend::OnChannelUserReply(const ChatInfo& chat_room, const OpStringC& nick, BOOL is_operator, BOOL has_voice, const OpStringC& prefix)
{
	GetAccountPtr()->OnChatterJoined(chat_room, nick, is_operator, has_voice,
		OpStringC(), TRUE);
}


void IRCBackend::OnChannelUserReplyDone(const ChatInfo& chat_room)
{
	OpStringC empty;
	GetAccountPtr()->OnChatPropertyChanged(chat_room, empty, empty,
		EngineTypes::ROOM_STATUS, empty, 0);
}


void IRCBackend::OnFileSendInitializing(ChatFileTransfer& chat_transfer)
{
	MessageEngine::GetInstance()->GetChatFileTransferManager()->FileSendInitializing(chat_transfer);
}


void IRCBackend::OnFileSendBegin(const ChatFileTransfer& chat_transfer,
	BOOL& transfer_failed)
{
	transfer_failed = OpStatus::IsError(
		MessageEngine::GetInstance()->GetChatFileTransferManager()->FileSendBegin(chat_transfer));
}


void IRCBackend::OnFileSendProgress(const ChatFileTransfer& chat_transfer,
	UINT32 bytes_sent, BOOL &transfer_failed)
{
	transfer_failed = OpStatus::IsError(
		MessageEngine::GetInstance()->GetChatFileTransferManager()->FileSendProgress(chat_transfer, bytes_sent));
}


void IRCBackend::OnFileSendCompleted(const ChatFileTransfer& chat_transfer)
{
	MessageEngine::GetInstance()->GetChatFileTransferManager()->FileSendCompleted(chat_transfer);
}


void IRCBackend::OnFileSendFailed(const ChatFileTransfer& chat_transfer)
{
	MessageEngine::GetInstance()->GetChatFileTransferManager()->FileSendFailed(chat_transfer);
}


void IRCBackend::OnFileReceiveInitializing(ChatFileTransfer& chat_transfer,	ChatFileTransferManager::FileReceiveInitListener * listener)
{
	MessageEngine::GetInstance()->GetChatFileTransferManager()->FileReceiveInitializing(chat_transfer, listener);
}


void IRCBackend::OnFileReceiveBegin(const ChatFileTransfer& chat_transfer,
	BOOL& transfer_failed)
{
	transfer_failed = OpStatus::IsError(
		MessageEngine::GetInstance()->GetChatFileTransferManager()->FileReceiveBegin(chat_transfer));
}


void IRCBackend::OnFileReceiveProgress(const ChatFileTransfer& chat_transfer,
	unsigned char* received, UINT32 bytes_received, BOOL &transfer_failed)
{
	transfer_failed = OpStatus::IsError(
		MessageEngine::GetInstance()->GetChatFileTransferManager()->FileReceiveProgress(chat_transfer, received, bytes_received));
}


void IRCBackend::OnFileReceiveCompleted(const ChatFileTransfer& chat_transfer)
{
	MessageEngine::GetInstance()->GetChatFileTransferManager()->FileReceiveCompleted(chat_transfer);
}


void IRCBackend::OnFileReceiveFailed(const ChatFileTransfer& chat_transfer)
{
	MessageEngine::GetInstance()->GetChatFileTransferManager()->FileReceiveFailed(chat_transfer);
}


void IRCBackend::GetCurrentCharset(OpString8 &charset)
{
	GetAccountPtr()->GetCharset(charset);
}


void IRCBackend::GetTransferPortRange(UINT &start_port, UINT &end_port)
{
	start_port = GetAccountPtr()->GetStartOfDccPool();
	end_port = GetAccountPtr()->GetEndOfDccPool();
}


void IRCBackend::GetCTCPLimitations(UINT& max_ctcp_requests, UINT& max_ctcp_request_interval)
{
	max_ctcp_requests = m_max_ctcp_requests;
	max_ctcp_request_interval = m_max_ctcp_request_interval;
}


time_t IRCBackend::GetCurrentTime()
{
	const time_t current_time = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();
	return current_time;
}


void IRCBackend::OnHotlistItemAdded(HotlistModelItem* item)
{
	if( item->IsContact() )
	{
		if (m_protocol.get() != 0 && !item->GetIsInsideTrashFolder())
			m_protocol->IRCContactAdded(item->GetID(), item->GetShortName());
	}
}


void IRCBackend::OnHotlistItemChanged(HotlistModelItem* item, BOOL move_as_child, UINT32 changed_flag)
{
	if( item->IsContact() )
	{
		if (m_protocol.get() != 0)
		{
			if (item->GetIsInsideTrashFolder())
				m_protocol->IRCContactRemoved(item->GetID(), item->GetShortName());
			else
				m_protocol->IRCContactChanged(item->GetID(), item->GetShortName());
		}
	}
}


void IRCBackend::OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child)
{
	if( item->IsContact() )
	{
		if (m_protocol.get() != 0)
			m_protocol->IRCContactRemoved(item->GetID(), item->GetShortName());
	}
}


OP_STATUS IRCBackend::ReadSettings()
{
	PrefsFile* prefs_file = 0;

	OpString file_path;
	OpStatus::Ignore(m_account->GetIncomingOptionsFile(file_path));
	if (file_path.HasContent())
		prefs_file = MessageEngine::GetInstance()->GetGlueFactory()->CreatePrefsFile(file_path);

	if (prefs_file != 0)
	{
		// Read the room list.
		OpString line;
		uni_char buf[32];

		const int room_count = prefs_file->ReadIntL(UNI_L("Account"), UNI_L("Room count"), 0);
		for (int room = 0; room < room_count; ++room)
		{
			uni_sprintf(buf, UNI_L("Name %i"), room);
			TRAPD(err, prefs_file->ReadStringL(UNI_L("Rooms"), buf, line, UNI_L("")));

			OpLineParser parser(line.CStr());

			OpString room_name;
			OpString password;

			parser.GetNextToken(room_name);
			parser.GetNextToken(password);

			if (room_name.HasContent())
				OpStatus::Ignore(AddSubscribedFolder(room_name, password));
		}

		// Read the backend specific settings.
		TRAPD(err, m_start_dcc_pool = UINT(prefs_file->ReadIntL(UNI_L("Settings"), UNI_L("DCC Portpool start"), 1024)); 
				m_end_dcc_pool = UINT(prefs_file->ReadIntL(UNI_L("Settings"), UNI_L("DCC Portpool end"), 1040)); 
				m_max_ctcp_requests = UINT(prefs_file->ReadIntL(UNI_L("Settings"), UNI_L("Max CTCP requests"), 3)); 
				m_max_ctcp_request_interval = UINT(prefs_file->ReadIntL(UNI_L("Settings"), UNI_L("Max CTCP request interval"), 10)); 
				m_can_accept_incoming_connections = BOOL(prefs_file->ReadIntL(UNI_L("Settings"), UNI_L("Can accept incoming connections"), TRUE)); 
				m_open_private_chats_in_background = BOOL(prefs_file->ReadIntL(UNI_L("Settings"), UNI_L("Open private chat windows in background"), TRUE)); 
				prefs_file->ReadStringL(UNI_L("Settings"), UNI_L("Perform when connected"), m_perform_when_connected, UNI_L("")));

		MessageEngine::GetInstance()->GetGlueFactory()->DeletePrefsFile(prefs_file);
	}

	return OpStatus::OK;
}


OP_STATUS IRCBackend::WriteSettings()
{
	PrefsFile* prefs_file = 0;

	OpString file_path;
	m_account->GetIncomingOptionsFile(file_path);

	if (file_path.HasContent())
		prefs_file = MessageEngine::GetInstance()->GetGlueFactory()->CreatePrefsFile(file_path);

	if (prefs_file != 0)
	{
		// Save the room list.
		uni_char buf[32];
		TRAPD(err, prefs_file->DeleteSectionL("Rooms"));

		prefs_file->WriteIntL("Account", "Room count", m_rooms.GetCount());

		for (UINT32 room_index = 0, room_count = m_rooms.GetCount();
			room_index < room_count; ++room_index)
		{
			uni_sprintf(buf, UNI_L("Name %i"), room_index);
			OpLineString line;

			Room* room = m_rooms.Get(room_index);
			if (room != 0)
			{
				line.WriteToken(room->m_room_name.CStr());
				if (room->m_password.HasContent())
					line.WriteToken(room->m_password.CStr());

				prefs_file->WriteStringL(UNI_L("Rooms"), buf, line.GetString());
			}
		}

		// Save the backend specific settings.
		TRAP(err, prefs_file->DeleteSectionL(UNI_L("Settings")); 
				prefs_file->WriteIntL(UNI_L("Settings"), UNI_L("DCC Portpool start"), m_start_dcc_pool); 
				prefs_file->WriteIntL(UNI_L("Settings"), UNI_L("DCC Portpool end"), m_end_dcc_pool); 
				prefs_file->WriteIntL(UNI_L("Settings"), UNI_L("Max CTCP requests"), m_max_ctcp_requests); 
				prefs_file->WriteIntL(UNI_L("Settings"), UNI_L("Max CTCP request interval"), m_max_ctcp_request_interval); 
				prefs_file->WriteIntL(UNI_L("Settings"), UNI_L("Can accept incoming connections"), m_can_accept_incoming_connections); 
				prefs_file->WriteIntL(UNI_L("Settings"), UNI_L("Open private chat windows in background"), m_open_private_chats_in_background); 
				prefs_file->WriteStringL(UNI_L("Settings"), UNI_L("Perform when connected"), m_perform_when_connected); 
				prefs_file->CommitL());
		MessageEngine::GetInstance()->GetGlueFactory()->DeletePrefsFile(prefs_file);
	}

	return OpStatus::OK;
}


void IRCBackend::Cleanup(BOOL resumable)
{
	if (!resumable)
	{
		// Notify GUI that we has left all rooms and that channel listing has
		// completed (this may have been notified allready, but shouldn't hurt
		// to send it twice).
		OpStringC empty;
		ChatInfo chat_info;
		GetAccountPtr()->OnChatRoomLeft(chat_info, empty, empty);
		GetAccountPtr()->OnFolderLoadingCompleted();

		// Remove status information on connecting rooms, we don't have to resume
		m_connecting_rooms.DeleteAll();
		m_should_list_rooms = FALSE;
	}

	// Reset other variables
	m_protocol.reset();
	m_presence_timer = 0;
	m_chat_status = AccountTypes::OFFLINE;
}


void IRCBackend::ChatStatusAsText(AccountTypes::ChatStatus status, OpString &out)
{
	out.Empty();

	switch (status)
	{
		case AccountTypes::ONLINE :
			break;
		case AccountTypes::BUSY :
			out.Set("Busy");
			break;
		case AccountTypes::BE_RIGHT_BACK :
			out.Set("Be right back");
			break;
		case AccountTypes::AWAY :
			out.Set("Away");
			break;
		case AccountTypes::ON_THE_PHONE :
			out.Set("On the phone");
			break;
		case AccountTypes::OUT_TO_LUNCH :
			out.Set("Out to lunch");
			break;
		case AccountTypes::OFFLINE :
			break;
	}
}


BOOL IRCBackend::IsConnected()
{
	if (m_protocol.get() == 0)
		return FALSE;

	return m_protocol->State() == IRCProtocol::ONLINE;
}


OP_STATUS IRCBackend::InterpretAndSend(const OpStringC& raw_command,
	const ChatInfo& chat_info, const OpStringC& chatter, BOOL notify_owner)
{
	IRCMessage interpreted;
	IRCCommandInterpreter::ReturnCode return_code;

	const OpStringC& channel = chat_info.StringData();

	if (channel.HasContent())
		return_code = m_command_interpreter->Interpret(raw_command, channel, m_protocol->Nick(), interpreted);
	else
		return_code = m_command_interpreter->Interpret(raw_command, chatter, m_protocol->Nick(), interpreted);

	switch (return_code)
	{
		case IRCCommandInterpreter::OK :
		{
			RETURN_IF_ERROR(m_protocol->SendUserMessage(interpreted, notify_owner));
			break;
		}
		case IRCCommandInterpreter::FAILED :
		case IRCCommandInterpreter::NOT_ENOUGH_PARAMETERS :
			break;
		default :
			OP_ASSERT(0);
			break;
	}

	return OpStatus::OK;
}


void IRCBackend::UpdateChatStatus(AccountTypes::ChatStatus new_status)
{
	if (new_status != m_chat_status)
	{
		BOOL change_protocol_state = TRUE;

		if ((m_chat_status == AccountTypes::OFFLINE) && (new_status == AccountTypes::ONLINE))
			change_protocol_state = FALSE;
		else if (new_status == AccountTypes::OFFLINE)
			change_protocol_state = FALSE;

		if (change_protocol_state)
		{
			OpString away_message;
			ChatStatusAsText(new_status, away_message);

			m_protocol->SetAway(away_message);
		}

		m_chat_status = new_status;

		MessageEngine::GetInstance()->OnChatStatusChanged(GetAccountPtr()->GetAccountId());
	}
}

OP_STATUS IRCBackend::AcceptReceiveRequest(UINT port_number, UINT id)
{
	if ( m_protocol.get() )
		return m_protocol->AcceptDCCRequest(port_number, id);
	else
		return OpStatus::ERR;
}


OP_STATUS IRCBackend::GetPerformWhenConnected(OpString& commands) const
{
	RETURN_IF_ERROR(commands.Set(m_perform_when_connected));

	// When called from account, we want to use a newline as the separator.
	RETURN_IF_ERROR(StringUtils::Replace(commands, UNI_L(";"), UNI_L("\n")));
	return OpStatus::OK;
}


OP_STATUS IRCBackend::SetPerformWhenConnected(const OpStringC& commands)
{
	// The commands may be separated on several lines, and we can't have that
	// in the settings file. Thus we replace the newline by a semicolon (not
	// very likely that people will need that character in a command).
	RETURN_IF_ERROR(m_perform_when_connected.Set(commands));

	// The input control might end lines with \n or with \r\n. Better to be
	// safe than sorry.
	RETURN_IF_ERROR(StringUtils::Replace(m_perform_when_connected, UNI_L("\r\n"), UNI_L(";")));
	RETURN_IF_ERROR(StringUtils::Replace(m_perform_when_connected, UNI_L("\n"), UNI_L(";")));

	return OpStatus::OK;
}


OP_STATUS IRCBackend::GetPerformWhenConnected(OpAutoVector<OpString> &commands) const
{
	StringTokenizer tokenizer(m_perform_when_connected, UNI_L(";"));
	while (tokenizer.HasMoreTokens())
	{
		OpString *token = OP_NEW(OpString, ());
		if (!token)
			return OpStatus::ERR_NO_MEMORY;

		OpStatus::Ignore(tokenizer.NextToken(*token));
		if (OpStatus::IsError(commands.Add(token)))
			OP_DELETE(token);
	}

	return OpStatus::OK;
}

#endif // IRC_SUPPORT
