//
// Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)
//

#ifndef IMAP_PARSEELEMENTS_H
#define IMAP_PARSEELEMENTS_H

#include "adjunct/m2/src/util/parser.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"

// Element classes
class ImapUnion : public ParseElement
{
public:
	ImapUnion(int val_number) : m_val_number(val_number), m_val_string(NULL) {}
	ImapUnion(const char* val_string, unsigned length) : m_length(length)
	{
		m_val_string = OP_NEWA(char, length + 1);
		if (m_val_string)
			op_strlcpy(m_val_string, val_string, length + 1);
	}
	virtual ~ImapUnion() { OP_DELETEA(m_val_string); }
	void Release()		 { m_val_string = NULL; }

	union
	{
		int m_val_number;
		int m_length;
	};
	char* m_val_string;
};

class ImapVector : public ParseElement, public OpVector<ParseElement>
{
};

class ImapNumberVector : public ParseElement, public OpINT32Vector
{
};

class ImapSortedVector : public ParseElement, public OpINTSortedVector
{
};

class ImapEnvelope : public ParseElement
{
public:
	ImapEnvelope(ImapUnion* date, ImapUnion* subject, ImapVector* from, ImapVector* sender, ImapVector* reply_to, ImapVector* to, ImapVector* cc, ImapVector* bcc, ImapUnion* in_reply_to, ImapUnion* message_id)
		: m_date(date), m_subject(subject), m_from(from), m_sender(sender), m_reply_to(reply_to), m_to(to), m_cc(cc), m_bcc(bcc), m_in_reply_to(in_reply_to), m_message_id(message_id) {}

	ImapUnion* m_date;
	ImapUnion* m_subject;
	ImapVector* m_from;
	ImapVector* m_sender;
	ImapVector* m_reply_to;
	ImapVector* m_to;
	ImapVector* m_cc;
	ImapVector* m_bcc;
	ImapUnion* m_in_reply_to;
	ImapUnion* m_message_id;
};

class ImapAddress : public ParseElement
{
public:
	ImapAddress(ImapUnion* name, ImapUnion* route, ImapUnion* mailbox, ImapUnion* host)
		: m_name(name), m_route(route), m_mailbox(mailbox), m_host(host) { }

	ImapUnion* m_name;
	ImapUnion* m_route;
	ImapUnion* m_mailbox;
	ImapUnion* m_host;
};

class ImapDateTime : public ParseElement
{
public:
	ImapDateTime(int day, int month, int year, int hour, int minute, int second, int timezone)
		: m_day(day), m_month(month), m_year(year), m_hour(hour), m_minute(minute), m_second(second), m_timezone(timezone) { }

	int m_day;
	int m_month;
	int m_year;
	int m_hour;
	int m_minute;
	int m_second;
	int m_timezone;
};

class ImapBody : public ParseElement
{
public:
	ImapBody(int media_type, int media_subtype, ImapVector* body_list)
		: m_media_type(media_type), m_media_subtype(media_subtype), m_multipart(TRUE), m_body_list(body_list) { }
	ImapBody(int media_type, int media_subtype, ImapBody* body = NULL)
		: m_media_type(media_type), m_media_subtype(media_subtype), m_multipart(FALSE), m_body(body) { }

	int m_media_type;
	int m_media_subtype;
	BOOL m_multipart;
	union
	{
		ImapVector* m_body_list;
		ImapBody* m_body;
	};
};

class ImapRawBody : public ParseElement
{

};

class ImapNamespaceDesc : public ParseElement
{
public:
	ImapNamespaceDesc(ImapUnion* prefix, char delimiter) : m_prefix(prefix), m_delimiter(delimiter) { }

	ImapUnion* m_prefix;
	char m_delimiter;
};

namespace ImapSection
{
	enum ImapSectionTypes
	{
		HEADERS,
		COMPLETE,
		PART,
		MIME
	};
};

#endif // IMAP_PARSEELEMENTS_H
