// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef ENUMS_H
#define ENUMS_H

namespace AccountTypes
{
	enum AccountType //If you add a new type here, make sure start of NewAccountWizard::OnOk() works with this new type and update Account::GetProtocolName!!!!
	{
		UNDEFINED,
		// mail types must come in order
		_FIRST_MAIL,
		POP,
		IMAP,
		SMTP,
		_LAST_MAIL,
		// news types must come in order
		_FIRST_NEWS,
		NEWS,
		_LAST_NEWS,
		// chat types must come in order
		_FIRST_CHAT,
		IRC,
		MSN,	//Disabled in NewAccountWizard::OnOk
		AOL,	//Disabled in NewAccountWizard::OnOk
		ICQ,	//Disabled in NewAccountWizard::OnOk
		JABBER,	//Disabled in NewAccountWizard::OnOk
		_LAST_CHAT,
		// feed types must come in order
		_FIRST_RSS,
		RSS,	//Disabled in NewAccountWizard::OnOk
		_LAST_RSS,
		// special types come last
		IMPORT,	//Disabled in NewAccountWizard::OnOk
		WEB,
		ARCHIVE,
  		_LAST_ACCOUNT_TYPE
	};

	enum AccountTypeCategory
	{
		TYPE_CATEGORY_MAIL,
		TYPE_CATEGORY_NEWS,
		TYPE_CATEGORY_CHAT
	};

	enum AccountStatus // Sorted in order of 'importance' (i.e. if two states are true, use the one that is last in this list)
	{
		DISCONNECTED,
		CONNECTING,
		AUTHENTICATING,
		CONNECTED,
		CONTACTING,			//:Contacting server... (for any miscellaneous server job)
        EMPTYING_TRASH,
		SENDING_MESSAGES,
		UPDATING_FEEDS,
		CHECKING_FOLDER,	//:Checking folder <foldername> (alt. Checking folder (x/y))
		FETCHING_FOLDERS,	//:Fetching folder list...
		FETCHING_BODY,		//:Fetching message body...
		FETCHING_ATTACHMENT,//:Fetching attachment...
		CREATING_FOLDER,	//:Creating folder...
		DELETING_FOLDER,	//:Deleting folder...
		RENAMING_FOLDER,	//:Renaming folder...
		FOLDER_SUBSCRIPTION,//:Setting folder subscription... (used for both subscribe and unsubscribe)
		DELETING_MESSAGE,	//:Deleting message...
		APPENDING_MESSAGE,	//:Appending message...
		STORING_FLAG,		//:Storing message flag...
		FETCHING_HEADERS,
		FETCHING_MESSAGES,
		FETCHING_GROUPS
	};

	enum TextDirection
	{
		DIR_AUTOMATIC,
		DIR_LEFT_TO_RIGHT,
		DIR_RIGHT_TO_LEFT
	};

	enum ChatStatus
	{
		ONLINE,
		BUSY,
		BE_RIGHT_BACK,
		AWAY,
		ON_THE_PHONE,
		OUT_TO_LUNCH,
		APPEAR_OFFLINE,
		OFFLINE
	};

    enum HeadersDisplay
    {
        TO=0,
        CC=1,
        BCC=2,
		REPLYTO=3,
        SUBJECT=4,
		ACCOUNT=5,
        ATTACHMENTS=6,
		PRIORITY=7,
		ENCODING=8,
		DIRECTION=9,
		NEWSGROUPS=10,
        FOLLOWUPTO=11,
		HEADER_DISPLAY_LAST
    };

	enum FolderPathType
	{
		FOLDER_INBOX,
		FOLDER_SENT,
		FOLDER_TRASH,
		FOLDER_ROOT,
		FOLDER_SPAM,
		FOLDER_DRAFTS,
		FOLDER_FLAGGED,
		FOLDER_NORMAL,
		FOLDER_ALLMAIL
	};

	//Nice documents: <URL:http://www.iana.org/assignments/sasl-mechanisms>, <URL:http://www.sendmail.org/~ca/email/mel/SASL_info.html>
	enum AuthenticationType
	{
		NONE=0, //No authentication required
		PLAINTEXT=1, //NNTP "AUTHINFO USER/AUTHINFO PASS", rfc2980, POP "USER/PASS", rfc1939
		PLAIN=2, //, SMTP "AUTH PLAIN" rfc2595, IMAP "AUTH PLAIN" rfc2595 / <URL:http://www.ietf.org/internet-drafts/draft-ietf-sasl-plain-04.txt> (exp Aug.2004)
		SIMPLE=3, //NNTP "AUTHINFO SIMPLE", rfc2980
		GENERIC=4, //NNTP "AUTHINFO GENERIC", rfc2980
		LOGIN=5, //SMTP "AUTH LOGIN", POP "AUTH LOGIN"
		APOP=6, //POP "APOP", rfc1939
//		RESERVED7=7,
//		RESERVED8=8,
//		RESERVED9=9,
		CRAM_MD5=10, //NNTP "AUTHINFO GENERIC CRAM-MD5", SMTP "AUTH CRAM-MD5", POP "AUTH CRAM-MD5", IMAP "AUTHENTICATE CRAM-MD5", rfc2195
//		RESERVED11=11,
		SHA1=12,
		KERBEROS=13, //NNTP "AUTHINFO GENERIC KERBEROS_V4", SMTP "AUTH KERBEROS_V4", POP "AUTH KERBEROS_V4", IMAP "AUTHENTICATE KERBEROS_V4", rfc1731
//		RESERVED14=14, //Replace with good authentication mechanism
		DIGEST_MD5=15, //NNTP "AUTHINFO GENERIC DIGEST-MD5", SMTP "AUTH DIGEST-MD5", POP "AUTH DIGEST-MD5", IMAP "AUTHENTICATE DIGEST-MD5", rfc2831
		AUTOSELECT=31 //Start from the most secure, working its way down until authentication-method is implemented
	};

	/** If you create a new class that inherits from MboxStore, it should
	  * have its own MboxType defined here. DO NOT change existing types!
	  */
	enum MboxType
	{
		MBOX_NONE = 0,	///< Message has not been saved to any store
		MBOX_MONTHLY,	///< MboxMonthly, stores mails in one big mbox file for every month
		MBOX_PER_MAIL,	///< MboxPerMail, one file per mail
		MBOX_CURSOR,	///< MboxCursor, stores mails in a compressed binary file
		MBOX_TYPE_COUNT	///< For checking number of types, don't remove this!
	};
};

namespace EngineTypes
{
	enum ChatMessageType
	{
		NO_MESSAGE = 0,
		WHOIS_COMMAND,
		IGNORE_COMMAND,
		UNIGNORE_COMMAND,
		OP_COMMAND,
		DEOP_COMMAND,
		VOICE_COMMAND,
		DEVOICE_COMMAND,
		KICK_COMMAND,
		KICK_WITH_REASON_COMMAND,
		BAN_COMMAND,
		RAW_COMMAND,
		ROOM_MESSAGE,
		ROOM_SELF_MESSAGE,
		ROOM_ACTION_MESSAGE,
		ROOM_SELF_ACTION_MESSAGE,
		PRIVATE_MESSAGE,
		PRIVATE_SELF_MESSAGE,
		PRIVATE_ACTION_MESSAGE,
		PRIVATE_SELF_ACTION_MESSAGE,
		SECURE_MESSAGE,
		SECURE_SELF_MESSAGE,
		SERVER_MESSAGE
	};

	enum ChatProperty
	{
		ROOM_STATUS,
		ROOM_TOPIC,
		ROOM_MODERATED,
		ROOM_CHATTER_LIMIT,
		ROOM_TOPIC_PROTECTION,
		ROOM_PASSWORD,
		ROOM_SECRET,
		ROOM_UNKNOWN_MODE,
		CHATTER_NICK,
		CHATTER_OPERATOR,
		CHATTER_VOICED,
		CHATTER_UNKNOWN_MODE_OPERATOR,
		CHATTER_UNKNOWN_MODE_VOICED,
		CHATTER_UNKNOWN_MODE,
		CHATTER_PRESENCE
	};

	enum YesNoQuestionType
	{
		DELETE_DOWNLOADED_MESSAGES_ON_SERVER	///< pop3 "leave mail on server" toggling
	};

	enum ReindexType
	{
		REINDEX_NONE,
		REINDEX_ALL,
		REINDEX_PER_ACCOUNT
	};

	enum ErrorSeverity
	{
		VERBOSE,
		INFORMATION,
		GENERIC_ERROR,
		CRITICAL,
		DONOTSHOW
	};

	enum ImporterId
	{
		NONE,
		OPERA,
		EUDORA,
		NETSCAPE,
		OE,
		OUTLOOK,
		MBOX,
		APPLE,
		THUNDERBIRD,
		OPERA7
	};

};

namespace IndexTypes
{
	enum Type
	{
		CONTACTS_INDEX,	/// Indexes for Contacts in contact list
		SEARCH_INDEX,	/// For Search results
		FOLDER_INDEX,	/// For general purpose folders or memory-only indexes.
		NEWSGROUP_INDEX,/// For Newsgroups
		IMAP_INDEX,		/// For IMAP folders
		NEWSFEED_INDEX,	/// For RSS feeds
		ARCHIVE_INDEX,	/// For Archives
		UNIONGROUP_INDEX,	/// UnionIndexGroup index of all child indexes, used for feed and mailing list Folders
		INTERSECTION_INDEX, /// IntersectionIndexGroup index, used for "search in filters" and POP inbox index
		COMPLEMENT_INDEX	/// ComplementIndexGroup index, used for POP sent index
	};

	/**
	 * List of the most common Index ids and ranges
	 */
	enum Id
	{
		/// New and unread mail
		UNREAD_UI = 1, // 0 == default

		/// All received mail..
		RECEIVED = 2,
		OUTBOX = 3,
		DRAFTS = 4,
		SENT = 5,
		TRASH = 6,
		UNREAD = 7,
		SPAM = 8,
		RECEIVED_LIST = 9,
		CLIPBOARD = 10,
		RECEIVED_NEWS = 11,
		NEW_UI = 12,
		RECEIVED_LISTS = 13,
		HIDDEN = 14,
		PIN_BOARD = 15,

		LAST_IMPORTANT,

		// Attachments
 		FIRST_ATTACHMENT=100,
		DOC_ATTACHMENT=100,
		IMAGE_ATTACHMENT=101,
		AUDIO_ATTACHMENT=102,
		VIDEO_ATTACHMENT=103,
		ZIP_ATTACHMENT=104,
		LAST_ATTACHMENT,

		ID_SIZE			= 100000000,

		/// Ids for contacts, folders, searches.
		FIRST_CONTACT	= 100000000,
		LAST_CONTACT	= 199999999,
		FIRST_FOLDER	= 200000000,
		LAST_FOLDER		= 299999999,
		FIRST_SEARCH	= 300000000,
		LAST_SEARCH		= 399999999,
		FIRST_NEWSGROUP	= 400000000,
		LAST_NEWSGROUP	= 499999999,
		FIRST_LABEL		= 500000000,
		LABEL_IMPORTANT = 500000000,
		LABEL_TODO,
		LABEL_MAIL_BACK,
		LABEL_CALL_BACK,
		LABEL_MEETING,
		LABEL_FUNNY,
		LABEL_VALUABLE,
		LAST_LABEL		= 599999999,
		FIRST_THREAD	= 600000000,
		LAST_THREAD		= 699999999,
		FIRST_IMAP		= 700000000,
		LAST_IMAP		= 799999999,
		FIRST_POP		= 800000000,
		LAST_POP		= 900000000,
		FIRST_ACCOUNT	= 1000000000,
		LAST_ACCOUNT	= 1100000000,
		FIRST_NEWSFEED	= 1200000000,
		LAST_NEWSFEED	= 1300000000,
		FIRST_ARCHIVE	= 1300000000,
		LAST_ARCHIVE	= 1400000000,
		FIRST_UNIONGROUP= 1500000000,
		LAST_UNIONGROUP	= 1600000000,
		LAST_INDEX = LAST_UNIONGROUP,

		// Categories only from here on (max. 32)
		FIRST_CATEGORY		= LAST_INDEX,
		CATEGORY_MY_MAIL	= FIRST_CATEGORY,
		CATEGORY_MAILING_LISTS,
		CATEGORY_ACTIVE_CONTACTS,
		CATEGORY_ACTIVE_THREADS,
		CATEGORY_LABELS,
		CATEGORY_ATTACHMENTS,
		LAST_CATEGORY,
		LAST			= 1700000000,
	};

	enum ModelType
	{
		MODEL_TYPE_FLAT,				// for contacts, this means To and from
		MODEL_TYPE_THREADED,
		MODEL_TYPE_TO,					// only for contacts
		MODEL_TYPE_FROM,				// only for contacts
		MODEL_TYPE_ATTACHMENTS,			// only for attachments
		MODEL_TYPE_THUMBNAILS			// only for attachments
	};

	enum ModelAge
	{
		MODEL_AGE_TODAY,
		MODEL_AGE_WEEK,
		MODEL_AGE_MONTH,
		MODEL_AGE_3_MONTHS,
		MODEL_AGE_YEAR,
		MODEL_AGE_FOREVER
	};

	enum ModelFlags
	{
		MODEL_FLAG_READ = 0,
		MODEL_FLAG_TRASH,
		MODEL_FLAG_SPAM,
		MODEL_FLAG_MAILING_LISTS,
		MODEL_FLAG_NEWSGROUPS,
		MODEL_FLAG_HIDDEN,
		MODEL_FLAG_NEWSFEEDS,
		MODEL_FLAG_SENT,
		MODEL_FLAG_DUPLICATES,
		MODEL_FLAG_LAST,
		MODEL_FLAG_OVERRIDE_DEFAULTS_FOR_THIS_INDEX = 31
	};

	enum ModelSort
	{
		MODEL_SORT_BY_NONE = -1,
		MODEL_SORT_BY_STATUS = 0,
		MODEL_SORT_BY_FROM = 1,
		MODEL_SORT_BY_SUBJECT = 2,
		MODEL_SORT_BY_LABEL = 4,
		MODEL_SORT_BY_SIZE = 6,
		MODEL_SORT_BY_SENT = 7
	};

	enum IndexFlags
	{
		INDEX_FLAGS_VISIBLE,
		INDEX_FLAGS_ASCENDING,
		INDEX_FLAGS_HIDE_FROM_OTHER,
		INDEX_FLAGS_MARK_MATCH_AS_READ,
		INDEX_FLAGS_WATCHED,
		INDEX_FLAGS_IGNORED,
		INDEX_FLAGS_INHERIT_FILTER_DEPRECATED,
		INDEX_FLAGS_VIEW_EXPANDED,
		INDEX_FLAGS_SERCH_NEW_MESSAGE_ONLY,
		INDEX_FLAGS_AUTO_FILTER,
		INDEX_FLAGS_ALLOW_EXTERNAL_EMBEDS
	};

	enum GroupingMethods
	{
		GROUP_BY_NONE = -1,
		GROUP_BY_READ = 0,
		GROUP_BY_FLAG = 1,
		GROUP_BY_DATE = 2,
	};

	inline UINT32 GetAccountIndexId(UINT16 account_id) { return IndexTypes::FIRST_ACCOUNT + account_id; }
	inline UINT32 GetPOPInboxIndexId(UINT16 account_id) { return IndexTypes::FIRST_POP + account_id * 2; }
	inline UINT32 GetPOPSentIndexId(UINT16 account_id) { return IndexTypes::FIRST_POP + (account_id * 2) + 1; }
};

#define INDEX_ID_TYPE(id) (id / IndexTypes::ID_SIZE)

namespace SearchTypes
{
	enum Field
	{
		CACHED_HEADERS	= 0,	///< Essential headers from cache only (faster)
		CACHED_SENDER	= 1,	///< Sender/From header from cache only
		CACHED_SUBJECT	= 2,	///< Subject header from cache only
		HEADERS			= 3,	///< All headers, can be optimised with Imap/News
		ENTIRE_MESSAGE	= 4,	///< All headers and body of message
		FOLDER_PATH		= 5,///< Internal, used for IMAP/Import of folders
		HEADER_FROM		= 6,
		HEADER_TO		= 7,
		HEADER_CC		= 8,
		HEADER_REPLYTO	= 9,
		HEADER_NEWSGROUPS=10,
		BODY			= 11 ///< Body of message, requires local copy
	};

	enum Option
	{
		EXACT_PHRASE	= 0, ///< Default, search for the entered string, case insensitive
		ANY_WORD		= 1, ///< All words, case insensitive
		REGEXP			= 2, ///< Regular expression
		DOESNT_CONTAIN	= 3  ///< Opposite of EXACT_PHRASE
	};

	enum Operator
	{
		OR = 0,
		AND = 1
	};
};

namespace MessageTypes
{
	enum CreateType {
        NEW,
        REPLY,						// first try to use Reply-To, then From
		REPLY_TO_LIST,				// first try List-Post, then X-Mailing-List, then Mailing-List, then like REPLY
		REPLY_TO_SENDER,			// will use the From header
		QUICK_REPLY,				// like reply
        REPLYALL,					// copying CC header, To is set to From, Reply-To and old To, with duplicate deletion
        FOLLOWUP,
        REPLY_AND_FOLLOWUP,
        FORWARD,
        REDIRECT,
		CANCEL_NEWSPOST,
		REOPEN // just reopen existing message
    };
};

namespace MailLayout
{
	enum MailLayoutEnum
	{
		LIST_ON_TOP_MESSAGE_BELOW = 0,
		LIST_ON_LEFT_MESSAGE_ON_RIGHT = 1,
		LIST_ONLY = 2
	};
};

#endif // ENUMS_H
