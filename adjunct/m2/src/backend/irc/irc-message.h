/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef IRC_MESSAGE_H
#define IRC_MESSAGE_H

#include "modules/util/opstring.h"
# include "modules/util/adt/opvector.h"

//********************************************************************
//
//	IRCReply
//
//********************************************************************

class IRCReply
{
public:
	enum
	{
		// Client -> Server, 001 - 099.
		RPL_WELCOME				= 1,
		RPL_YOURHOST			= 2,
		RPL_CREATED				= 3,
		RPL_MYINFO				= 4,
		RPL_ISUPPORT			= 5,

		// Command replies, 200 - 399.
		RPL_TRACELINK			= 200,
		RPL_TRACECONNECTING		= 201,
		RPL_TRACEHANDSHAKE		= 202,
		RPL_TRACEUNKNOWN		= 203,
		RPL_TRACEOPERATOR		= 204,
		RPL_TRACEUSER			= 205,
		RPL_TRACESERVER			= 206,
		RPL_TRACESERVICE		= 207,
		RPL_TRACENEWTYPE		= 208,
		RPL_TRACECLASS			= 209,
		RPL_STATSLINKINFO		= 211,
		RPL_STATSCOMMANDS		= 212,
		RPL_ENDOFSTATS			= 219,
		RPL_UMODEIS				= 221,
		RPL_SERVLIST			= 234,
		RPL_SERVLISTEND			= 235,
		RPL_STATSUPTIME			= 242,
		RPL_STATSOLINE			= 243,
		RPL_LUSERCLIENT			= 251,
		RPL_LUSEROP				= 252,
		RPL_LUSERUNKNOWN		= 253,
		RPL_LUSERCHANNELS		= 254,
		RPL_LUSERME				= 255,
		RPL_ADMINME				= 256,
		RPL_ADMINLOC1			= 257,
		RPL_ADMINLOC2			= 258,
		RPL_ADMINEMAIL			= 259,
		RPL_TRACELOG			= 261,
		RPL_TRACEEND			= 262,
		RPL_TRYAGAIN			= 263,

		RPL_AWAY				= 301,
		RPL_USERHOST			= 302,
		RPL_ISON				= 303,
		RPL_UNAWAY				= 305,
		RPL_NOWAWAY				= 306,
		RPL_WHOISUSER			= 311,
		RPL_WHOISSERVER			= 312,
		RPL_WHOISOPERATOR		= 313,
		RPL_WHOWASUSER			= 314,
		RPL_ENDOFWHO			= 315,
		RPL_WHOISIDLE			= 317,
		RPL_ENDOFWHOIS			= 318,
		RPL_WHOISCHANNELS		= 319,
		RPL_LISTSTART			= 321,
		RPL_LIST				= 322,
		RPL_LISTEND				= 323,
		RPL_CHANNELMODEIS		= 324,
		RPL_UNIQOPIS			= 325,
		RPL_LOGGEDINAS			= 330,
		RPL_NOTOPIC				= 331,
		RPL_TOPIC				= 332,
		RPL_LISTUSAGE			= 334,
		RPL_INVITING			= 341,
		RPL_SUMMONING			= 342,
		RPL_INVITELIST			= 346,
		RPL_ENDOFINVITELIST		= 347,
		RPL_EXCEPTLIST			= 348,
		RPL_ENDOFEXCEPTLIST		= 349,
		RPL_VERSION				= 351,
		RPL_WHOREPLY			= 352,
		RPL_NAMREPLY			= 353,
		RPL_LINKS				= 364,
		RPL_ENDOFLINKS			= 365,
		RPL_ENDOFNAMES			= 366,
		RPL_BANLIST				= 367,
		RPL_ENDOFBANLIST		= 368,
		RPL_ENDOFWHOWAS			= 369,
		RPL_INFO				= 371,
		RPL_MOTD				= 372,
		RPL_ENDOFINFO			= 374,
		RPL_MOTDSTART			= 375,
		RPL_ENDOFMOTD			= 376,
		RPL_YOUREOPER			= 381,
		RPL_REHASHING			= 382,
		RPL_YOURESERVICE		= 383,
		RPL_TIME				= 391,
		RPL_USERSSTART			= 392,
		RPL_USERS				= 393,
		RPL_ENDOFUSERS			= 394,
		RPL_NOUSERS				= 395,

		// Error replies, 400 - 599.
		ERR_NOSUCHNICK			= 401,
		ERR_NOSUCHSERVER		= 402,
		ERR_NOSUCHCHANNEL		= 403,
		ERR_CANNOTSENDTOCHAN	= 404,	
		ERR_TOOMANYCHANNELS		= 405,
		ERR_WASNOSUCHNICK		= 406,
		ERR_TOOMANYTARGETS		= 407,
		ERR_NOSUCHSERVICE		= 408,
		ERR_NOORIGIN			= 409,
		ERR_NORECIPIENT			= 411,
		ERR_NOTEXTTOSEND		= 412,
		ERR_NOTOPLEVEL			= 413,
		ERR_WILDTOPLEVEL		= 414,
		ERR_BADMASK				= 415,
		ERR_UNKNOWNCOMMAND		= 421,
		ERR_NOMOTD				= 422,
		ERR_NOADMININFO			= 423,
		ERR_FILEERROR			= 424,
		ERR_NONICKNAMEGIVEN		= 431,
		ERR_ERRONEUSNICKNAME	= 432,
		ERR_NICKNAMEINUSE		= 433,
		ERR_NICKCOLLISION		= 436,
		ERR_UNAVAILRESOURCE		= 437,
		ERR_USERNOTINCHANNEL	= 441,
		ERR_NOTONCHANNEL		= 442,
		ERR_USERONCHANNEL		= 443,
		ERR_NOLOGIN				= 444,
		ERR_SUMMONDISABLED		= 445,
		ERR_USERSDISABLED		= 446,
		ERR_NOTREGISTERED		= 451,
		ERR_NEEDMOREPARAMS		= 461,
		ERR_ALREADYREGISTRED	= 462,
		ERR_NOPERMFORHOST		= 463,
		ERR_PASSWDMISMATCH		= 464,
		ERR_YOUREBANNEDCREEP	= 465,
		ERR_YOUWILLBEBANNED		= 466,
		ERR_KEYSET				= 467,
		ERR_CHANNELISFULL		= 471,
		ERR_UNKNOWNMODE			= 472,
		ERR_INVITEONLYCHAN		= 473,
		ERR_BANNEDFROMCHAN		= 474,
		ERR_BADCHANNELKEY		= 475,
		ERR_BADCHANMASK			= 476,
		ERR_NOCHANMODES			= 477,
		ERR_BANLISTFULL			= 478,
		ERR_NOPRIVILEGES		= 481,
		ERR_CHANOPRIVSNEEDED	= 482,
		ERR_CANTKILLSERVER		= 483,
		ERR_RESTRICTED			= 484,
		ERR_UNIQOPPRIVSNEEDED	= 485,
		ERR_NOOPERHOST			= 491,
		ERR_UMODEUNKNOWNFLAG	= 501,
		ERR_USERSDONTMATCH		= 502,

		// Non-standard replies, 600 ->.
		RPL_SECURECONNECTION	= 617
	};

	static BOOL IsError(UINT reply) { return reply > 400; }
};


//********************************************************************
//
//	CTCPInformation
//
//********************************************************************

class CTCPInformation
{
public:
	// Construction / destruction.
	CTCPInformation() { }
	~CTCPInformation() { }

	OP_STATUS Init(const OpStringC& ctcp_message);

	// Methods.
	const OpStringC& Type() const { return m_type; }

	UINT ParameterCount() const { return m_parameters.GetCount(); }
	const OpStringC& Parameter(UINT index) const;
	const OpStringC& Parameters() const { return m_parameters_string; }

private:
	// No copy or assignment.
	CTCPInformation(const CTCPInformation& ctcp_info);
	CTCPInformation& operator=(const CTCPInformation& ctcp_info);

	// Members.
	OpString m_type;

	OpAutoVector<OpString> m_parameters;
	OpString m_parameters_string;
};


//********************************************************************
//
//	IRCMessage
//
//********************************************************************

class IRCMessage
{
public:
	// Constructors.
	IRCMessage() : m_is_ctcp(FALSE) { }
	IRCMessage(IRCMessage const &other);

	/*! Construct the IRC message from a raw server line. */
	OP_STATUS FromRawLine(const OpStringC& raw_line);
	OP_STATUS FromRawLine(const OpStringC8& raw_line, const OpStringC8& charset, const OpStringC8& server);

	/*! Retrieve the user nickname or the servername from the prefix. */
	OP_STATUS MessageSender(OpString &sender) const;
	/*! Retrieve the user-id and host from the prefix. */
	OP_STATUS UserInformation(OpString& user_id, OpString& host) const;

	/*! Retrieve the CTCP request and the parameters, if this is a ctcp message. */
	OP_STATUS CTCPInfo(CTCPInformation& ctcp_info) const;

	/*! Retrieve the numeric value. */
	UINT NumericReplyValue() const;

	/*! Figure out whether sender is a server or not. */
	BOOL SenderIsServer() const;
	/*! Figure out whether this is a numeric reply or not. */
	BOOL IsNumericReply() const;
	/*! Figure out whether this is a CTCP message or not. */
	BOOL IsCTCPMessage() const { return m_is_ctcp; }

	// Accessors.
	const OpStringC& Prefix() const { return m_prefix; }
	const OpStringC& Command() const { return m_command; }
	UINT ParameterCount() const { return m_parameter_collection.GetCount(); }
	const OpStringC& Parameter(UINT Index) const;
	const OpStringC& Parameters() const { return m_parameters; }

	// Set functions.
	OP_STATUS SetPrefix(const OpStringC& prefix);
	OP_STATUS SetCommand(const OpStringC& command);
	void SetIsCTCP(BOOL is_ctcp = TRUE) { m_is_ctcp = is_ctcp; }
	OP_STATUS AddParameter(const OpStringC& parameter);

	/*! Convert to raw irc messages from the containing members. This function will
		automatically split a message into several raw messages if it's too long to
		fit. */
	OP_STATUS RawMessages(OpAutoVector<OpString8> &raw_messages, const OpStringC8& charset = OpStringC8()) const;

private:
	// No assignment.
	IRCMessage &operator=(IRCMessage const &rhs);

	// Methods.
	OP_STATUS AddCTCPMarkerIfNeeded(OpString8 &raw_message) const;

	OP_STATUS AddRawMessage(OpAutoVector<OpString8> &raw_messages, OpString8 *raw_message) const;
	OP_STATUS CreateNewRawMessage(OpString8 **new_message, const OpStringC8& initial_value) const;

	BOOL AddWordToRawMessage(OpString8 &raw_message, OpString8 &preferred_charset,
		const OpStringC& word, UINT max_bytes_for_word, UINT &characters_added) const;
	int FindFirstWord(const OpStringC& find_in, OpString &word) const;
	OP_STATUS Parse(const OpStringC& raw_line, const OpStringC8& charset, const OpStringC8& server);

	OP_STATUS ConvertToChar16(const OpStringC8& input, const OpStringC8& charset, const OpStringC8& server, OpString& output) const;

	// Static members.
	static const UINT m_max_byte_length;
	static const char m_ctcp_marker;

	// Members.
	OpString m_prefix;
	OpString m_command;
	OpAutoVector<OpString> m_parameter_collection;
	OpString m_parameters;
	BOOL m_is_ctcp;
};

#endif
