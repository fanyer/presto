/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined MHTML_ARCHIVE_SAVE_SUPPORT || defined LOGDOC_INLINE_FINDER_SUPPORT

#include "modules/logdoc/inline_finder.h"

#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/html_att.h"
#include "modules/logdoc/savewithinline.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/link.h"
#include "modules/logdoc/html5parser.h" // for HTML5Parser::IsHTML5WhiteSpace()
#include "modules/logdoc/src/html5/html5tokenizer.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/style/css_save.h"
#include "modules/url/url2.h"

/* static */ OP_STATUS
InlineFinder::Make(InlineFinder*& finder, URL& url, LinkHandler* callback, BOOL only_safe_links)
{
	RETURN_OOM_IF_NULL(finder = OP_NEW(InlineFinder, (url, callback, only_safe_links)));

	TRAPD(status, finder->m_token.InitializeL());
	if (OpStatus::IsMemoryError(status))
	{
		OP_DELETE(finder);
		finder = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	finder->m_dummy_profile = finder->m_dummy_logdoc.GetHLDocProfile();
	finder->m_dummy_profile->SetURL(&finder->m_url);

	return OpStatus::OK;
}

InlineFinder::InlineFinder(URL url, LinkHandler* callback, BOOL only_safe_links)
	: m_tokenizer(NULL, TRUE),
	  m_dummy_logdoc(NULL),
	  m_url(url),
	  m_base_url(url),
	  m_callback(callback),
	  m_only_safe_links(only_safe_links),
	  m_doctype_found(FALSE),
	  m_in_style(FALSE)
{
}

InlineFinder::~InlineFinder()
{
}

static void
ParseAttributeForLinksL(const uni_char* attribute, HTML5TokenWrapper& token, Markup::Type type, URL& base_url, BOOL look_for_multiple_urls, BOOL only_safe_links, HLDocProfile* hld_profile, InlineFinder::LinkHandler* callback)
{
	uni_char* attr_str = token.GetUnescapedAttributeValueL(attribute);  ANCHOR_ARRAY(uni_char, attr_str);
	if (!attr_str)
		return;

	const uni_char* link = attr_str;

	do
	{
		unsigned link_len = 0;
		if (look_for_multiple_urls)
		{
			while (link[link_len] && !HTML5Parser::IsHTML5WhiteSpace(link[link_len]))  // find first whitespace
				link_len++;

			if (link_len == 0)  // only whitespace remaining in attribute
				return;
		}
		else
			link_len = uni_strlen(link);

		OpStackAutoPtr<URL> inline_url(CreateUrlFromString(link, link_len, &base_url,
			hld_profile, type == Markup::HTE_IMG, type == Markup::HTE_LINK || type == Markup::HTE_PROCINST));

		if (!inline_url.get())
			LEAVE(OpStatus::ERR_NO_MEMORY);

		if (!inline_url->IsEmpty() && inline_url->Type() != URL_DATA)
#ifndef NO_SAVE_SUPPORT
			if (!only_safe_links || SafeTypeToSave(type, *inline_url))
#endif // NO_SAVE_SUPPORT
				callback->LinkFoundL(*inline_url, type, &token);

		if (!look_for_multiple_urls)
			return;

		// find start of next link by skipping whitespace
		while (HTML5Parser::IsHTML5WhiteSpace(link[link_len]))
			link_len++;
		link += link_len;
	} while (*link);
}

/* static */ void
InlineFinder::ParseLoadedURLForInlinesL(URL& url, LinkHandler* callback, Window* window, BOOL only_safe_links/*=FALSE*/)
{
	if (url.Status(TRUE) != URL_LOADED || !callback)
		LEAVE(OpStatus::ERR);

	InlineFinder* finder;
	LEAVE_IF_ERROR(InlineFinder::Make(finder, url, callback, only_safe_links));
	ANCHOR_PTR(InlineFinder, finder);

	// Get ready to start parsing
	URL_DataDescriptor* url_data_desc = url.GetDescriptor(NULL, TRUE, FALSE, TRUE, window);

	if (!url_data_desc)
		LEAVE(OpStatus::ERR);

	ANCHOR_PTR(URL_DataDescriptor, url_data_desc);

	BOOL more;

	url_data_desc->RetrieveDataL(more);

	const uni_char* data_buf = reinterpret_cast<const uni_char*>(url_data_desc->GetBuffer());
	unsigned long data_len = url_data_desc->GetBufSize();

	// loop through the document, process links and add mime parts as we go along
	// loops through one data_buf at a time (reading more into data_buf at the end)
	while (data_buf && UNICODE_DOWNSIZE(data_len) > 0)
	{
		unsigned uni_len = UNICODE_DOWNSIZE(data_len);

		finder->AppendDataL(data_buf, uni_len, !more);

		TRAPD(status, finder->ParseForInlinesL());
		if (OpStatus::IsError(status))
			LEAVE(status);

		OP_ASSERT(status == HTML5ParserStatus::NEED_MORE_DATA || status == OpStatus::OK);

		url_data_desc->ConsumeData(uni_len * sizeof(uni_char));
		url_data_desc->RetrieveDataL(more);
		data_buf = (uni_char*)url_data_desc->GetBuffer();
		data_len = url_data_desc->GetBufSize();
	}
}

void InlineFinder::AppendDataL(const uni_char* buffer, unsigned length, BOOL end_of_data)
{
	m_tokenizer.AddDataL(buffer, length, end_of_data, FALSE, FALSE);
}

#ifdef SPECULATIVE_PARSER
OP_STATUS InlineFinder::RestoreTokenizerState(HTML5ParserState* state, unsigned buffer_stream_position)
{
	m_token.ResetWrapper();
	RETURN_IF_LEAVE(m_tokenizer.AddDataL(UNI_L(""), 0, FALSE, TRUE, TRUE));
	return m_tokenizer.RestoreTokenizerState(state, buffer_stream_position);
}
#endif // SPECULATIVE_PARSER

OP_STATUS InlineFinder::SetDoctype(const uni_char *name, const uni_char *pub, const uni_char *sys)
{
	RETURN_IF_ERROR(m_dummy_logdoc.SetDoctype(name, pub, sys));
	m_doctype_found = TRUE;
	return OpStatus::OK;
}

void InlineFinder::ParseForInlinesL()
{
	while (m_token.GetType() != HTML5Token::ENDOFFILE)
	{
		m_tokenizer.GetNextTokenL(m_token);

		switch (m_token.GetType())
		{
		case HTML5Token::DOCTYPE:
			if (!m_doctype_found)
				LEAVE_IF_ERROR(SetDoctype(m_token.GetNameStr(), m_token.GetPublicIdentifier(), m_token.GetSystemIdentifier()));
			break;

		case HTML5Token::START_TAG:
			{
				Markup::Type type = g_html5_name_mapper->GetTypeFromTokenBuffer(m_token.GetName());

#if defined STYLE_EXTRACT_URLS
				// check for style attribute, and parse its content if it exists
				uni_char* style_attr = m_token.GetUnescapedAttributeValueL(UNI_L("style"));
				if (style_attr)
				{
					ANCHOR_ARRAY(uni_char, style_attr);
					DataSrc style_src; ANCHOR(DataSrc, style_src);
					LEAVE_IF_ERROR(style_src.AddSrc(style_attr, uni_strlen(style_attr), m_url));

					AutoDeleteHead css_links; ANCHOR(AutoDeleteHead, css_links);
					ExtractURLsL(m_url, &style_src, &css_links);  // let style module collect links
					if (!css_links.Empty())
						m_callback->LinkListFoundL(css_links);
				}
#endif // STYLE_EXTRACT_URLS

				switch (type)
				{
				case Markup::HTE_SCRIPT:
					ParseAttributeForLinksL(UNI_L("src"), m_token, type, m_base_url, FALSE, m_only_safe_links, m_dummy_profile, m_callback);
					if (m_url.ContentType() != URL_XML_CONTENT || !m_token.IsSelfClosing())
						m_tokenizer.SetState(HTML5Tokenizer::SCRIPT_DATA);
					break;

				case Markup::HTE_STYLE:
					m_in_style = TRUE;
					// fall-through
				case Markup::HTE_NOSCRIPT:
				case Markup::HTE_NOEMBED:
					m_tokenizer.SetState(HTML5Tokenizer::RAWTEXT);
					break;

				case Markup::HTE_BASE:
					{
						uni_char* base_str = m_token.GetUnescapedAttributeValueL(UNI_L("href"));  ANCHOR_ARRAY(uni_char, base_str);
						if (base_str)
							m_base_url = g_url_api->GetURL(m_base_url, base_str);
					}
					break;

				case Markup::HTE_NOFRAMES:
					if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled))
						m_tokenizer.SetState(HTML5Tokenizer::RAWTEXT);
					break;

				case Markup::HTE_BODY:
				case Markup::HTE_TABLE:
				case Markup::HTE_TR:
				case Markup::HTE_TH:
				case Markup::HTE_TD:
					ParseAttributeForLinksL(UNI_L("background"), m_token, type, m_base_url, FALSE, m_only_safe_links, m_dummy_profile, m_callback);
					break;

				case Markup::HTE_LINK:
					{
						uni_char* rel; unsigned rel_len;
						BOOL must_delete = m_token.GetAttributeValueL(UNI_L("rel"), rel, rel_len);

						if (rel)
						{
							uni_char tmp = rel[rel_len];
							rel[rel_len] = '\0';
							unsigned kinds = LinkElement::MatchKind(rel);
							if (must_delete)
								OP_DELETEA(rel);
							else
								rel[rel_len] = tmp;

							if (!(kinds & (LINK_TYPE_STYLESHEET | LINK_TYPE_ICON)))
								break;

							ParseAttributeForLinksL(UNI_L("href"), m_token, type, m_base_url, FALSE, m_only_safe_links, m_dummy_profile, m_callback);
						}
						break;
					}
				case Markup::HTE_OBJECT:
				case Markup::HTE_APPLET:
					{
						// Need to parse URLs relative to codebase, if it exists
						URL codebase = m_base_url; ANCHOR(URL, codebase);
						uni_char* codebase_str = m_token.GetUnescapedAttributeValueL(UNI_L("codebase"));
						if (codebase_str)
						{
							codebase = g_url_api->GetURL(codebase_str);
							OP_DELETEA(codebase_str);
						}

						ParseAttributeForLinksL(UNI_L("archive"), m_token, type, codebase, TRUE, m_only_safe_links, m_dummy_profile, m_callback);
						if (type == Markup::HTE_OBJECT)
							ParseAttributeForLinksL(UNI_L("data"), m_token, type, codebase, FALSE, m_only_safe_links, m_dummy_profile, m_callback);
						else
							ParseAttributeForLinksL(UNI_L("src"), m_token, type, codebase, FALSE, m_only_safe_links, m_dummy_profile, m_callback);
						break;
					}
				case Markup::HTE_FRAME:
				case Markup::HTE_IFRAME:
					if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::FramesEnabled))
						break;
					// fallthrough
				default:
					ParseAttributeForLinksL(UNI_L("src"), m_token, type, m_base_url, FALSE, m_only_safe_links, m_dummy_profile, m_callback);
					break;
				}
			}
			break;

		case HTML5Token::END_TAG:
			if (m_in_style)
			{
#ifdef STYLE_EXTRACT_URLS
				AutoDeleteHead css_links; ANCHOR(AutoDeleteHead, css_links);
				ExtractURLsL(m_url, &m_style_src, &css_links);  // let style module collect links
				if (!css_links.Empty())
					m_callback->LinkListFoundL(css_links);
				m_style_src.DeleteAll();
#endif // STYLE_EXTRACT_URLS
				m_in_style = FALSE;
			}
			break;

		case HTML5Token::CHARACTER:
			if (m_in_style)
			{
#ifdef STYLE_EXTRACT_URLS
				HTML5StringBuffer::ContentIterator style_content(m_token.GetData());
				ANCHOR(HTML5StringBuffer::ContentIterator, style_content);

				const uni_char* data; unsigned length;
				while ((data = style_content.GetNext(length)) != NULL)
					LEAVE_IF_ERROR(m_style_src.AddSrc(data, length, m_url));
#endif // STYLE_EXTRACT_URLS
			}
			break;

		case HTML5Token::PROCESSING_INSTRUCTION:
			if (m_token.GetName()->IsEqualTo(UNI_L("xml-stylesheet"), 14))
				ParseAttributeForLinksL(UNI_L("href"), m_token, Markup::HTE_PROCINST, m_base_url, FALSE, m_only_safe_links, m_dummy_profile, m_callback);
			break;

		default:
			break;
		}
	}
}


#endif // defined MHTML_ARCHIVE_SAVE_SUPPORT || defined API_LOGDOC_INLINE_FINDER
