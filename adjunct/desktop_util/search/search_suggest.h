/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Petter Nilsen
 */

#ifndef _SEARCH_SUGGEST_H_INCLUDED
#define _SEARCH_SUGGEST_H_INCLUDED

#include "modules/cache/url_dd.h"
#include "modules/url/url2.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/ecmascript/json_listener.h"
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"

class OpSearchSuggestParser;

class SearchSuggestEntry
{
public:
	SearchSuggestEntry(const uni_char *data, UINT32 num_queries)
	{
		m_data.Set(data);
		m_num_queries = num_queries;
	}
	virtual ~SearchSuggestEntry() {}

	virtual const uni_char*	GetData() { return m_data.CStr(); }
	virtual UINT32		GetNumQueries() { return m_num_queries; }

private:
	OpString	m_data;				// the query data
	UINT32		m_num_queries;		// rating of this data entry, if available. Not currently used
};

class SearchSuggest : public MessageObject
{
public:
	class Listener
	{
	public:
		virtual ~Listener() {}

		virtual void OnQueryStarted() = 0;
		virtual void OnQueryComplete(const OpStringC& search_id, OpVector<SearchSuggestEntry>& entries) = 0;
		virtual void OnQueryError(const OpStringC& search_id, OP_STATUS error) = 0;
	};

	SearchSuggest(SearchSuggest::Listener *listener);
	virtual		~SearchSuggest();

	// start a search
	OP_STATUS	Connect(OpString& search_term, const OpStringC& provider, SearchTemplate::SearchSource source);

	// abort any suggest call that might be in progress
	void		Abort();

	// used by the callee to check if a suggest api is available for this search id (maps to the unique id used in the search.ini)
	BOOL		HasSuggest(const OpStringC& search_id);

	// Returns TRUE if a search is in progress
	BOOL		IsBusy() const { return m_busy; }

//	void		OnTimeout(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	void		HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	URL_DataDescriptor *			m_url_dd;
	URL								m_host_url;
	BOOL							m_new_data;
	BOOL							m_busy;
	MessageHandler *				m_mh;
	SearchSuggest::Listener *		m_listener;
	OpSearchSuggestParser	*		m_parser;
	OpString						m_active_provider;
};

// base class for the suggest result parsing
class OpSearchSuggestParser
{
	friend class SearchSuggest;

public:
	OpSearchSuggestParser(SearchTemplate::SearchSuggestProtocol protocol, const OpStringC& provider)
		: m_suggest_protocol(protocol) 
	{
		m_provider.Set(provider.CStr());
	};
	virtual		~OpSearchSuggestParser() {};

	// factory method
	static OP_STATUS Construct(OpSearchSuggestParser **parser, const OpStringC& provider);

	virtual OP_STATUS Init() = 0;
	virtual OP_STATUS Parse(const uni_char *data, const UINT32 length) = 0;

	virtual OP_STATUS CreateSearchURL(OpString& url, OpString& search_term, SearchTemplate::SearchSource source)
	{
		SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(m_provider);
		if(!search || !search->IsSuggestEnabled())
		{
			return OpStatus::ERR_NOT_SUPPORTED;
		}
		RETURN_IF_ERROR(search->MakeSuggestQuery(search_term, url, source));

		return OpStatus::OK;
	}
	// return the search provider UUID for this parser
	OpStringC GetSearchProvider() { return m_provider; }

	// return the suggest protocol in use
	SearchTemplate::SearchSuggestProtocol GetProtocol() { return m_suggest_protocol; }

protected:
	OpVector<SearchSuggestEntry>	m_suggest_entries;	// the parsed entries

	SearchTemplate::SearchSuggestProtocol	m_suggest_protocol;	// protocol to use
	OpString								m_provider;			// provider currently in use
};

#ifdef SEARCH_SUGGEST_GOOGLE

// Parser for the Google search suggestions
class GoogleSearchSuggestParser : public OpSearchSuggestParser
{
public:
	GoogleSearchSuggestParser();
	virtual		~GoogleSearchSuggestParser();

	virtual OP_STATUS Init() { return OpStatus::OK;	}
	virtual OP_STATUS Parse(const uni_char *data, const UINT32 length);

	virtual OP_STATUS CreateSearchURL(OpString& url, OpString& search_term);

private:
	XMLFragment						m_xml_fragment;
};
#endif // SEARCH_SUGGEST_GOOGLE

// Parser for the JSON search suggestions, see http://en.wikipedia.org/w/api.php

class JSONSearchSuggestParser : public OpSearchSuggestParser, public JSONListener
{
public:
	JSONSearchSuggestParser(SearchTemplate::SearchSuggestProtocol protocol, const OpStringC& provider);
	virtual		~JSONSearchSuggestParser();

	// OpSearchSuggestParser
	virtual OP_STATUS Init() { return OpStatus::OK;	}
	virtual OP_STATUS Parse(const uni_char *data, const UINT32 length);

	// JSONListener
	virtual OP_STATUS EnterArray();
	virtual OP_STATUS LeaveArray();
	virtual OP_STATUS EnterObject();
	virtual OP_STATUS LeaveObject();
	virtual OP_STATUS AttributeName(const OpString&);
	virtual OP_STATUS String(const OpString&);
	virtual OP_STATUS Number(double num);
	virtual OP_STATUS Bool(BOOL val);
	virtual OP_STATUS Null();

protected:
	UINT32		m_array_level;	// nested level of the JSON level
};

#endif // _SEARCH_SUGGEST_H_INCLUDED
