//
// Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_FLAGS_H
#define IMAP_FLAGS_H

namespace ImapFlags
{
	enum NoFlag
	{
		NONE							= 0
	};

	enum MessageFlag
	{
		FLAG_RECENT						= 1 << 0,
		FLAG_ANSWERED					= 1 << 1,
		FLAG_FLAGGED					= 1 << 2,
		FLAG_DELETED					= 1 << 3,
		FLAG_SEEN						= 1 << 4,
		FLAG_DRAFT						= 1 << 5,
		FLAG_STAR						= 1 << 6,
		FLAG_SPAM						= 1 << 7,
		FLAG_NOT_SPAM					= 1 << 8,
		FLAGS_INCLUDED_IN_FETCH			= 1 << 31
	};

	enum ListFlag
	{
		LFLAG_HASCHILDREN				= 1 << 0,
		LFLAG_HASNOCHILDREN				= 1 << 1,
		LFLAG_MARKED					= 1 << 2,
		LFLAG_NOINFERIORS				= 1 << 3,
		LFLAG_NOSELECT					= 1 << 4,
		LFLAG_UNMARKED					= 1 << 5,
		LFLAG_INBOX						= 1 << 6,
		LFLAG_DRAFTS					= 1 << 7,
		LFLAG_TRASH						= 1 << 8,
		LFLAG_SENT						= 1 << 9,
		LFLAG_SPAM						= 1 << 10, // corresponds to \Junk and \Spam
		LFLAG_FLAGGED					= 1 << 11, // corresponds to \Flagged and \Starred
		LFLAG_ALLMAIL					= 1 << 12, // corresponds to \All and \AllMail
		LFLAG_ARCHIVE					= 1 << 13
	};

	enum XlistFlag 
	{

	};

	enum Capability
	{
		CAP_AUTH_PLAIN					= 1 << 0,
		CAP_AUTH_LOGIN					= 1 << 1,
		CAP_AUTH_CRAMMD5				= 1 << 2,
		CAP_IDLE						= 1 << 3,
		CAP_LITERALPLUS					= 1 << 4,
		CAP_LOGINDISABLED				= 1 << 5,
		CAP_NAMESPACE					= 1 << 6,
		CAP_STARTTLS					= 1 << 7,
		CAP_UIDPLUS						= 1 << 8,
		CAP_UNSELECT					= 1 << 9,
		CAP_CONDSTORE					= 1 << 10,
		CAP_ENABLE						= 1 << 11,
		CAP_QRESYNC						= 1 << 12,
		CAP_COMPRESS_DEFLATE			= 1 << 13,
		CAP_XLIST						= 1 << 14,
		CAP_SPECIAL_USE					= 1 << 15,
		CAP_ID							= 1 << 16
	};

	enum State
	{
		STATE_CONNECTING				= 1 << 0,
		STATE_CONNECTED					= 1 << 1,
		STATE_RECEIVED_CAPS 			= 1 << 2,
		STATE_SECURE					= 1 << 3,
		STATE_AUTHENTICATED				= 1 << 4,
		STATE_COMPRESSED				= 1 << 5,
		STATE_ENABLED_QRESYNC			= 1 << 6,
		STATE_SELECTED					= 1 << 7,
		STATE_UNSELECTED				= 1 << 8,
	};

	enum Options
	{
		OPTION_DISABLE_IDLE 			= 1 << 0,
		OPTION_DISABLE_BODYSTRUCTURE	= 1 << 1,
		OPTION_FORCE_SINGLE_CONNECTION	= 1 << 2,
		OPTION_EXTRA_CONNECTION_FAILS	= 1 << 3,
		OPTION_LOCKED					= 1 << 4,
		OPTION_DISABLE_QRESYNC			= 1 << 5,
		OPTION_DISABLE_UIDPLUS			= 1 << 6,
		OPTION_DISABLE_COMPRESS			= 1 << 7,
		OPTION_DISABLE_SPECIAL_USE		= 1 << 8	/// < disables SPECIAL-USE and XLIST
	};
};

#endif // IMAP_FLAGS_H
