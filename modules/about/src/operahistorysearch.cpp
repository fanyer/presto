/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Pavel Studeny
*/

#include "core/pch.h"

#ifdef OPERAHISTORYSEARCH_URL

#include "modules/about/operahistorysearch.h"
#include "modules/url/url_rep.h"
#include "modules/search_engine/VisitedSearch.h"
#include "modules/search_engine/WordSegmenter.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/htmlify.h"
#include "modules/pi/OpLocale.h"
#include "modules/formats/uri_escape.h"

// keywords: q query string, p results per page (default 20), s start with result# (default 0)
/*static*/
OP_STATUS OperaHistorySearch::ParseQuery(uni_char **dst, int *rpp, int *sr, const char *src)
{
	// FIXME: Check if Sequence_Splitter can be used.
	const char *amp;
	char *d_buf;
	int q_len;

	*rpp = 20;
	*sr = 0;

	if ((src = op_strchr(src, '?')) != NULL)
		++src;

	if (src == NULL || *src == 0)
	{
		if (*dst == NULL)
		{
			if ((*dst = OP_NEWA(uni_char, 1)) == NULL)
				return OpStatus::ERR_NO_MEMORY;

			**dst = 0;
		}
		return OpStatus::OK;
	}

	*dst = NULL;

	do {
		while ((src[0] != 'q' && src[0] != 'p' && src[0] != 's') || src[1] != '=')
		{
			if ((src = op_strchr(src, '&')) != NULL)
				++src;

			if (src == NULL || *src == 0)
			{
				if (*dst == NULL)
				{
					if ((*dst = OP_NEWA(uni_char, 1)) == NULL)
						return OpStatus::ERR_NO_MEMORY;

					**dst = 0;
				}
				return OpStatus::OK;
			}
		}

		switch (src[0])
		{
		case 'p':
			src += 2;
			*rpp = op_atoi(src);
			break;

		case 's':
			src += 2;
			*sr = op_atoi(src);
			break;

		case 'q':
			src += 2;

			if ((amp = op_strchr(src, '&')) == NULL)
				amp = src + op_strlen(src);

			q_len = amp - src;

			if ((d_buf = OP_NEWA(char, q_len+1)) == NULL)
				return OpStatus::ERR_NO_MEMORY;

			if ((*dst = OP_NEWA(uni_char, q_len+1)) == NULL)
			{
				OP_DELETEA(d_buf);
				return OpStatus::ERR_NO_MEMORY;
			}

			op_memcpy(d_buf, src, q_len);
			UriUnescape::ReplaceChars(d_buf, q_len, UriUnescape::All); // Updates q_len
			d_buf[q_len] = 0;

			from_utf8(*dst, d_buf, (q_len + 1) * sizeof(uni_char));

			OP_DELETEA(d_buf);
			break;
		} // switch
	} while (*src != 0);

	if (*rpp <= 0)
		*rpp = 20;

	if (*sr < 0)
		*sr = 0;

	return OpStatus::OK;
}

OperaHistorySearch *OperaHistorySearch::Create(URL &url)
{
	OperaHistorySearch *ohs;

	if ((ohs = OP_NEW(OperaHistorySearch, (url))) == NULL)
		return NULL;

	if (ohs->Init() != OpStatus::OK)
	{
		OP_DELETE(ohs);
		return NULL;
	}

	return ohs;
}

OperaHistorySearch::~OperaHistorySearch()
{
	OP_DELETEA(m_query);
}

OP_STATUS OperaHistorySearch::Init()
{
	if (m_query != NULL)
	{
		OP_DELETE(m_query);
		m_query = NULL;
	}

	RETURN_IF_ERROR(ParseQuery(&m_query, &m_res_p_page, &m_start_res, m_url.GetRep()->LinkId()));
#ifdef SEARCH_ENGINE_PHRASESEARCH
	return m_highlighter.Init(m_query, PhraseMatcher::AllPhrases);
#else
	return m_highlighter.Init(m_query);
#endif // SEARCH_ENGINE_PHRASESEARCH
}

#define MAX_PAR_LEN 320

OP_STATUS OperaHistorySearch::WriteResult(OpString &html, const VisitedSearch::Result &result, int res_no)
{
	OpString url;
	OpString plaintext;
	OpString title;
	struct tm *rtm;
	uni_char vis_time[32]; /* ARRAY OK 2008-09-11 adame */
	uni_char *htmlified_url, *htmlified_title, *uni_plaintext, *htmlified_plaintext;
	OP_STATUS res;

	RETURN_IF_ERROR(url.SetFromUTF8(result.url));

	htmlified_url = HTMLify_string(url.CStr());
	if (!htmlified_url && url.CStr())
	{
		OP_DELETEA(htmlified_url);
		return OpStatus::ERR_NO_MEMORY;
	}

	htmlified_title = HTMLify_string(result.title);
	if (!htmlified_title && result.title)
	{
		OP_DELETEA(htmlified_url);
		OP_DELETEA(htmlified_title);
		return OpStatus::ERR_NO_MEMORY;
	}

	uni_plaintext = result.GetPlaintext();
	htmlified_plaintext = HTMLify_string(uni_plaintext);
	if (!htmlified_plaintext && uni_plaintext)
	{
		OP_DELETEA(htmlified_url);
		OP_DELETEA(htmlified_title);
		OP_DELETEA(htmlified_plaintext);
		return OpStatus::ERR_NO_MEMORY;
	}

	res = m_highlighter.AppendHighlight(title, result.title != NULL ?  htmlified_title : htmlified_url, MAX_PAR_LEN, UNI_L("<dfn>"), UNI_L("</dfn>"));
	if (OpStatus::IsError(res))
	{
		OP_DELETEA(htmlified_url);
		OP_DELETEA(htmlified_title);
		OP_DELETEA(htmlified_plaintext);
		return res;
	}

	res = m_highlighter.AppendHighlight(plaintext, htmlified_plaintext, MAX_PAR_LEN, UNI_L("<dfn>"), UNI_L("</dfn>"));
	if (OpStatus::IsError(res))
	{
		OP_DELETEA(htmlified_url);
		OP_DELETEA(htmlified_title);
		OP_DELETEA(htmlified_plaintext);
		return res;
	}

	OP_DELETEA(htmlified_title);
	OP_DELETEA(htmlified_plaintext);

	rtm = op_localtime(&(result.visited));
	g_oplocale->op_strftime(vis_time, 32, UNI_L("%x %X"), rtm);

	res = html.AppendFormat(
UNI_L("<li value=\"%i\">\n<h2><a href=\"%s\">%s</a></h2>\n<p>%s</p>\n<cite><ins>%s</ins> \x2014 %s</cite>\n"),
				res_no, htmlified_url, title.CStr(), plaintext.CStr(), vis_time, htmlified_url);

	OP_DELETEA(htmlified_url);

	return res;
}

OP_STATUS OperaHistorySearch::GenerateData()
{
	OpString line, search_style, next_loc, prev_loc, title_loc;
	SearchIterator<VisitedSearch::Result> *it;
	int i;

	m_url.SetAttribute(URL::KUnique, TRUE);

#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_LEAVE(g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleSearchFile, &search_style));
#else
	search_style.Empty();
#endif

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_HISTORY_SEARCH, title_loc));

	uni_char *htmlified_query = NULL;
	if (m_query != NULL && *m_query != 0)
	{
		htmlified_query = HTMLify_string(m_query, uni_strlen(m_query), TRUE);
		if (!htmlified_query)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(line.Append(htmlified_query));
		RETURN_IF_ERROR(line.AppendFormat(UNI_L(" \x2014 %s"), title_loc.CStr()));
	}
	else
	{
		RETURN_IF_ERROR(line.Set(title_loc));
	}

	ANCHOR_ARRAY(uni_char, htmlified_query);

	// Initialize, open HEAD and write TITLE and stylesheet link
	RETURN_IF_ERROR(OpenDocument(line.CStr(), search_style.CStr()));

	// Close HEAD, open BODY and write the top H1
	RETURN_IF_ERROR(OpenBody(Str::S_HISTORY_SEARCH));

	// <form method=\"get\" action=\"\" onsubmit=\"location.href='opera:historysearch?q='+encodeURIComponent(this.q.value);return false;\">
	RETURN_IF_ERROR(line.Set(
		"<form method=\"get\" action=\"opera:historysearch\">\n"
		"<fieldset><legend>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_HISTORY_SEARCH));
	RETURN_IF_ERROR(line.Append("</legend>\n"
		"<input type=\"text\" name=\"q\" value=\""));
	if (m_query != NULL && *m_query != 0)
		RETURN_IF_ERROR(line.Append(htmlified_query));
	RETURN_IF_ERROR(line.Append("\"><input type=\"submit\" value=\""));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_HISTORY_SEARCH));
	RETURN_IF_ERROR(line.Append(
		"\">\n"
		"</fieldset>\n"
		"</form>\n"));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

#ifdef NO_EMPTY_SEARCHES
	if(m_query && *m_query)
	{
#endif // NO_EMPTY_SEARCHES

#ifdef SEARCH_ENGINE_PHRASESEARCH
	it = g_visited_search->Search(m_query != NULL ? m_query : UNI_L(""), VisitedSearch::RankSort, PhraseMatcher::AllPhrases);
#else
	it = g_visited_search->Search(m_query != NULL ? m_query : UNI_L(""));
#endif // SEARCH_ENGINE_PHRASESEARCH

	if (it == NULL)
	{
		RETURN_IF_ERROR(line.Set("<p>"));
		RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_NO_MATCHES_FOUND));
		RETURN_IF_ERROR(line.Append("</p>\n"));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	}
	else {
		OpAutoPtr< SearchIterator<VisitedSearch::Result> > anchor(it);

		line.Empty();
		if (it->Empty())
		{
			RETURN_IF_ERROR(line.Set("<p>"));
			RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_NO_MATCHES_FOUND));
			RETURN_IF_ERROR(line.Append("</p>\n"));
		}
		else {
			RETURN_IF_ERROR(line.AppendFormat(UNI_L("<ol start=\"%i\">\n"), m_start_res + 1));

			i = 0;
			while (i < m_start_res)
			{
				if (!it->Next())
					break;
				++i;
			}

			if (i >= m_start_res)
			{
				i = 0;
				do	{
					RETURN_IF_ERROR(WriteResult(line, it->Get(), m_start_res + i + 1));
				} while (it->Next() && ++i < m_res_p_page);
			}

			RETURN_IF_ERROR(it->Error());

			RETURN_IF_ERROR(line.Append("</ol>\n"));

			RETURN_IF_ERROR(g_languageManager->GetString(Str::M_SITE_NAVIGATION_MENU_NEXT, next_loc));
			if (!next_loc.HasContent())
			{
				RETURN_IF_ERROR(next_loc.Set("&gt;"));
			}

			RETURN_IF_ERROR(g_languageManager->GetString(Str::M_SITE_NAVIGATION_MENU_PREVIOUS, prev_loc));
			if (!prev_loc.HasContent())
			{
				RETURN_IF_ERROR(prev_loc.Set("&lt;"));
			}

			if (!it->End())
			{
				if (m_start_res > 0)
				{

					RETURN_IF_ERROR(line.AppendFormat(UNI_L("<ul><li><a rel=\"prev\" href=\"opera:historysearch?q=%s&amp;p=%i&amp;s=%i\">%s</a></li> <li><a rel=\"next\" href=\"opera:historysearch?q=%s&amp;p=%i&amp;s=%i\">%s</a></li></ul>\n"),
						m_query == NULL || *m_query == 0 ? UNI_L("") : htmlified_query, m_res_p_page, m_start_res - m_res_p_page, prev_loc.CStr(), m_query == NULL || *m_query == 0 ? UNI_L("") : htmlified_query, m_res_p_page, m_start_res + m_res_p_page, next_loc.CStr()));
				}
				else {
					RETURN_IF_ERROR(line.AppendFormat(UNI_L("<ul><li>%s</li> <li><a rel=\"next\" href=\"opera:historysearch?q=%s&amp;p=%i&amp;s=%i\">%s</a></li></ul>\n"),
						prev_loc.CStr(), m_query == NULL || *m_query == 0 ? UNI_L("") : htmlified_query, m_res_p_page, m_start_res + m_res_p_page, next_loc.CStr()));
				}
			}
			else if (m_start_res > 0)
			{
				RETURN_IF_ERROR(line.AppendFormat(UNI_L("<ul><li><a rel=\"prev\" href=\"opera:historysearch?q=%s&amp;p=%i&amp;s=%i\">%s</a></li> <li>%s</li></ul>\n"),
					m_query == NULL || *m_query == 0 ? UNI_L("") : htmlified_query, m_res_p_page, m_start_res - m_res_p_page, prev_loc.CStr(), next_loc.CStr()));
			}
			else {
				RETURN_IF_ERROR(line.AppendFormat(UNI_L("<ul><li>%s</li> <li>%s</li></ul>\n"), prev_loc.CStr(), next_loc.CStr()));
			}
		}

		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	}

#ifdef NO_EMPTY_SEARCHES
	}
#endif // NO_EMPTY_SEARCHES

	// Close BODY and HTML elements and finish off.

	return CloseDocument();
}

#endif  // OPERAHISTORYSEARCH_URL
