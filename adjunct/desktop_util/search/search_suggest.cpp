/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Petter Nilsen
 */

#include "core/pch.h"

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/util/tempbuf.h"

#include "modules/url/url2.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/ecmascript/jsonlite/json_lexer.h"
#include "modules/ecmascript/json_serializer.h"
#include "modules/ecmascript/json_parser.h"
#include "modules/encodings/detector/charsetdetector.h"

#include "adjunct/desktop_util/search/search_suggest.h"

SearchSuggest::SearchSuggest(SearchSuggest::Listener *listener) :
	m_url_dd(NULL), m_new_data(FALSE), m_busy(FALSE), m_mh(NULL), m_listener(listener), m_parser(NULL)
{

}

SearchSuggest::~SearchSuggest()
{
	Abort();
	OP_DELETE(m_url_dd);
	if (m_mh)
	{
		m_mh->UnsetCallBacks(this);
		OP_DELETE(m_mh);
	}
	OP_DELETE(m_parser);
}

OP_STATUS SearchSuggest::Connect(OpString& search_term, const OpStringC& provider, SearchTemplate::SearchSource source)
{
	OpString server;

	if(m_active_provider.Compare(provider) || !m_parser)
	{
		// only create a new parser if the provider has changed
		if(m_parser)
		{
			OP_DELETE(m_parser);
			m_parser = NULL;
		}
		RETURN_IF_ERROR(OpSearchSuggestParser::Construct(&m_parser, provider));
		RETURN_IF_ERROR(m_parser->Init());
		RETURN_IF_ERROR(m_active_provider.Set(provider));
	}
	RETURN_IF_ERROR(m_parser->CreateSearchURL(server, search_term, source));

	m_host_url = g_url_api->GetURL(server.CStr());
	if (!m_host_url.IsEmpty())
	{
		// Initiate the download of the document pointed to by the
		// URL, and forget about it.  The HandleCallback function
		// will deal with successful downloads and check whether a
		// user notification is required.

		if (!m_mh)
			m_mh = OP_NEW(MessageHandler, (NULL));
		if (!m_mh)
			return OpStatus::ERR_NO_MEMORY;

		m_mh->SetCallBack(this, MSG_HEADER_LOADED, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_DATA_LOADED, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_MULTIPART_RELOAD, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_LOADING_FAILED, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_INLINE_LOADING, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_LOADING_DELAYED, m_host_url.Id());
		m_mh->SetCallBack(this, MSG_URL_MOVED, m_host_url.Id());

		URL tmp;
		m_host_url.Load(m_mh, tmp, TRUE, FALSE);
		m_busy = TRUE;
	}
	if(m_listener)
	{
		m_listener->OnQueryStarted();
	}
	return OpStatus::OK;
}

void SearchSuggest::Abort()
{
	if(!m_host_url.IsEmpty() && m_host_url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
	{
		m_host_url.StopLoading(m_mh);
		OP_DELETE(m_url_dd);
		m_url_dd = NULL;
		m_busy = FALSE;

		m_mh->UnsetCallBacks(this);
	}
}

void SearchSuggest::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(par1 == (MH_PARAM_1)m_host_url.Id());

	OP_STATUS ret = OpStatus::OK;
	BOOL more = TRUE;

	switch (msg)
	{
	case MSG_URL_DATA_LOADED:
		if (m_new_data)
		{
			m_new_data = FALSE;
		}
		if (m_host_url.GetAttribute(URL::KLoadStatus) != URL_LOADED)
			return;
		else
		{
			TempBuffer data;
			data.SetCachedLengthPolicy(TempBuffer::TRUSTED);
			if (!m_url_dd)
			{
				m_url_dd = m_host_url.GetDescriptor(NULL, TRUE, FALSE, TRUE, NULL, URL_X_JAVASCRIPT, 0, TRUE);	// URL_X_JAVASCRIPT ensures unicode

				if(!m_url_dd)
				{
					more = FALSE;
					ret = OpStatus::ERR_NO_SUCH_RESOURCE;
				}
			}
			while (more)
			{
				TRAPD(err, m_url_dd->RetrieveDataL(more));
				if (OpStatus::IsError(err))
				{
					ret = err;
					break;
				}
				UINT32 length = m_url_dd->GetBufSize();
				const char* buffer = m_url_dd->GetBuffer();

				// undetermined content is not in unicode
				if (OpStatus::IsError(data.Append((uni_char *)buffer, UNICODE_DOWNSIZE(length))))
				{
					ret = OpStatus::ERR_NO_MEMORY;
					break;
				}
				m_url_dd->ConsumeData(UNICODE_SIZE(UNICODE_DOWNSIZE(length)));
			}
			// call parser
			if (m_parser && OpStatus::IsSuccess(ret))
			{
				ret = m_parser->Parse(data.GetStorage(), data.Length());
			}
			OP_DELETE(m_url_dd);
			m_url_dd = NULL;
		}
		break;

	case MSG_HEADER_LOADED:
		m_new_data = TRUE;

	case MSG_MULTIPART_RELOAD:
	case MSG_URL_INLINE_LOADING:
	case MSG_URL_LOADING_DELAYED:
	case MSG_URL_MOVED:
		return;

	case MSG_URL_LOADING_FAILED:
		ret = OpStatus::ERR;
		break;

	default:
		OP_ASSERT(0);
	}

	m_busy = FALSE;

	if (m_listener)
	{
		if(OpStatus::IsSuccess(ret))
		{
			m_listener->OnQueryComplete(m_active_provider, m_parser->m_suggest_entries);
			m_parser->m_suggest_entries.DeleteAll();
		}
		else
		{
			m_listener->OnQueryError(m_active_provider, ret);
			m_parser->m_suggest_entries.DeleteAll();
		}
	}
}

BOOL SearchSuggest::HasSuggest(const OpStringC& search_id)
{
	SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(search_id);
	if(search && search->IsSuggestEnabled())
	{
		return TRUE;
	}
	return FALSE;
}

/*
void SearchSuggest::OnTimeout(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{

}
*/
/*static */
OP_STATUS OpSearchSuggestParser::Construct(OpSearchSuggestParser **parser, const OpStringC& provider)
{
	SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(provider);
	if(!search || !search->IsSuggestEnabled())
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	switch(search->GetSuggestProtocol())
	{
		case SearchTemplate::SUGGEST_JSON:
		{
			*parser = OP_NEW(JSONSearchSuggestParser, (search->GetSuggestProtocol(), provider));

			if (!*parser)
				return OpStatus::ERR_NO_MEMORY;

			return OpStatus::OK;

		}
	}
	return OpStatus::ERR_NOT_SUPPORTED;
}

/*****************************************************************************************
*
* Parser for the results from the search suggest lookup - this one is tailored for 
* JSON suggests such as Yandex, Wikipedia and Bing
*
*****************************************************************************************/

/*
GET /suggest-opera?part=%s&n=10&nav=no HTTP/1.1 
Host: suggest.yandex.ru 

part - user request in utf8 (this is the only required parameter) 
n - maximum suggestions number (optional parameter, default value is 10) 
nav - if 'yes', navigation suggestions will be returned if available
(optional parameter, default value is 'no') 

Navigation suggestions are suggestions which contain url and description. 
There can be either one or none navigation suggestion. 

The response is in JSON format.

/suggest-opera?part=p&n=5 

[ 
"p", 
[ 
"psp", 
"perfect world", 
"philips", 
"playground", 
"promodj.ru" 
] 
]

*/

JSONSearchSuggestParser::JSONSearchSuggestParser(SearchTemplate::SearchSuggestProtocol protocol, const OpStringC& provider) 
			: OpSearchSuggestParser(protocol, provider)
{

}

JSONSearchSuggestParser::~JSONSearchSuggestParser()
{

}

OP_STATUS JSONSearchSuggestParser::Parse(const uni_char *data, const UINT32 length)
{
	JSONParser json_parser(this);

	m_array_level = 0;

	RETURN_IF_ERROR(json_parser.Parse(data, length));

	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::EnterArray()
{
	m_array_level++;

	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::LeaveArray()
{
	m_array_level--;
	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::EnterObject()
{
	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::LeaveObject()
{
	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::AttributeName(const OpString& str)
{
	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::String(const OpString& str)
{
	if(m_array_level == 2)
	{
		SearchSuggestEntry *entry = OP_NEW(SearchSuggestEntry, (str, 0));
		if(!entry)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		RETURN_IF_ERROR(m_suggest_entries.Add(entry));
	}
	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::Number(double num)
{
	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::Bool(BOOL val)
{
	return OpStatus::OK;
}

OP_STATUS JSONSearchSuggestParser::Null()
{
	return OpStatus::OK;
}

#ifdef SEARCH_SUGGEST_GOOGLE

/*****************************************************************************************
*
* Parser for the results from the search suggest lookup - this one is tailored for 
* Google Suggest
*
*****************************************************************************************/

GoogleSearchSuggestParser::GoogleSearchSuggestParser()
{

}

GoogleSearchSuggestParser::~GoogleSearchSuggestParser()
{

}

#define GOOGLE_QUERY_STRING UNI_L("http://google.com/complete/search?output=toolbar&q=%s")

OP_STATUS GoogleSearchSuggestParser::CreateSearchURL(OpString& url, OpString& search_term)
{
	RETURN_IF_ERROR(url.AppendFormat(GOOGLE_QUERY_STRING, search_term.CStr()));

	return OpStatus::OK;
}

/*
<toplevel>
 <CompleteSuggestion>
  <suggestion data="microsoft office"/>
  <num_queries int="229000000"/>
 </CompleteSuggestion>
 <CompleteSuggestion>
  <suggestion data="microsoft update"/>
  <num_queries int="115000000"/>
 </CompleteSuggestion>
</toplevel>
*/
OP_STATUS GoogleSearchSuggestParser::Parse(const uni_char *data, const UINT32 length)
{
	if(OpStatus::IsError(m_xml_fragment.Parse(data, length)))
	{
		return OpStatus::ERR_PARSING_FAILED;
	}
	TempBuffer buffer;
	if(m_xml_fragment.EnterElement(UNI_L("toplevel")))
	{
		while(m_xml_fragment.HasMoreElements())
		{
			if(m_xml_fragment.EnterElement(UNI_L("CompleteSuggestion")))
			{
				const uni_char *data_attr = NULL;
				UINT32 num_queries_attr = 0;

				while(m_xml_fragment.EnterAnyElement())
				{
					if(!uni_stricmp(m_xml_fragment.GetElementName().GetLocalPart(), UNI_L("suggestion")))
					{
						data_attr = m_xml_fragment.GetAttribute(UNI_L("data"));
					}
					else if(!uni_stricmp(m_xml_fragment.GetElementName().GetLocalPart(), UNI_L("num_queries")))
					{
						const uni_char *attr = m_xml_fragment.GetAttribute(UNI_L("int"));
						if(attr)
						{
							num_queries_attr = uni_atoi(attr);
						}
					}
					m_xml_fragment.LeaveElement();
				}
				if(data_attr)
				{
					SearchSuggestEntry *entry = OP_NEW(SearchSuggestEntry, (data_attr, num_queries_attr));
					if(!entry)
					{
						return OpStatus::ERR_NO_MEMORY;
					}
					RETURN_IF_ERROR(m_suggest_entries.Add(entry));

					data_attr = NULL;
					num_queries_attr = 0;
				}
				m_xml_fragment.LeaveElement();
			}
			else
			{
				break;
			}
		}
		m_xml_fragment.LeaveElement();
	}
	return OpStatus::OK;
}

#endif // SEARCH_SUGGEST_GOOGLE

