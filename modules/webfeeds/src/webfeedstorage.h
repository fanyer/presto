// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef WEBFEEDSTORAGE_H
#define WEBFEEDSTORAGE_H

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT

# include "modules/xmlutils/xmltokenhandler.h"
# include "modules/xmlutils/xmlparser.h"

#include "modules/util/opfile/opfile.h"
#include "modules/webfeeds/webfeeds_api.h"

class WebFeed;
class WebFeedEntry;
class WebFeedContentElement;
class WebFeedsAPI_impl;

class WriteBuffer
{
public:
	WriteBuffer();
	~WriteBuffer();

   void	InitL(const uni_char *file_name);

	void	AppendOpString(OpStringC *str);
	void	Append(const uni_char *buf);
	void	Append(const char* buf);
	void	Append(const char* buf, int buf_len);
	void	Append(UINT num, const char *format = NULL, int digits = 1);
	void	AppendDate(double date);
	void	AppendBinaryL(const unsigned char* buf, int buf_len);
	void	AppendQuotedL(const uni_char* buf);
	void	AppendQuotedL(const char* buf);

	void	Flush();
	
	UINT	GetBytesWritten() { return m_written + m_used; }

	OpFile&	GetFile() { return m_file; }

private:

	UINT		m_max_len;
	UINT		m_used;
	UINT		m_written;
	char*		m_buffer;
	OpFile		m_file;
};

class WebFeedStorage : public XMLTokenHandler
{
public:
		WebFeedStorage();
		~WebFeedStorage();

	OP_STATUS	LoadFeed(WebFeedsAPI_impl::WebFeedStub* stub, WebFeed *&feed, WebFeedsAPI_impl* api_impl);
	void		SaveFeedL(WebFeed *feed);
	OP_STATUS	LoadStore(WebFeedsAPI_impl* api_impl);
	void		SaveStoreL(WebFeedsAPI_impl* api_impl);

	/// Implementation of the XMLTokenHandler API
	virtual Result	HandleToken(XMLToken &token);

private:
	WebFeed*		m_current_feed;
	WebFeedsAPI_impl::WebFeedStub* m_current_stub;
	BOOL			m_own_stub;
	WebFeedEntry*	m_current_entry;
	WebFeedContentElement*
					m_current_content;
	WebFeedsAPI_impl*	m_api_impl;

	OpString		m_current_prop;
	BOOL			m_in_title;
	BOOL			m_is_feed_file;
	UINT			m_def_update_interval;
	UINT			m_first_free_id;

	OP_STATUS	LocalLoad(const uni_char *file_name);

	OP_STATUS	HandleStartTagToken(const XMLToken& token);
	OP_STATUS	HandleEndTagToken(const XMLToken& token);
	OP_STATUS	HandleTextToken(const XMLToken& token);

	OP_STATUS	AddNewContent();
};

#endif // WEBFEEDS_SAVED_STORE_SUPPORT

#endif // WEBFEEDSTORAGE_H
