/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef SPECULATIVE_PARSER

#include "modules/logdoc/src/html5/html5speculativeparser.h"
#include "modules/doc/frm_doc.h"

/* static */ OP_STATUS
HTML5SpeculativeParser::Make(HTML5SpeculativeParser *&parser, URL url, URL base_url, LogicalDocument *logdoc)
{
	HTML5SpeculativeParser *tmp_parser = OP_NEW(HTML5SpeculativeParser, (logdoc, url));
	RETURN_OOM_IF_NULL(tmp_parser);
	OpAutoPtr<HTML5SpeculativeParser> parser_anchor(tmp_parser);

	RETURN_IF_ERROR(InlineFinder::Make(tmp_parser->m_inline_finder, url, tmp_parser, FALSE));
	tmp_parser->m_inline_finder->SetBaseURL(base_url);
	RETURN_IF_ERROR(tmp_parser->m_inline_finder->SetDoctype(logdoc->GetDoctypeName(), logdoc->GetDoctypePubId(), logdoc->GetDoctypeSysId()));

	parser = parser_anchor.release();
	return OpStatus::OK;
}

HTML5SpeculativeParser::HTML5SpeculativeParser(LogicalDocument *logdoc, URL url)
	: m_inline_finder(NULL),
	  m_logdoc(logdoc),
	  m_url(url),
	  m_is_finished(FALSE)
{
}

HTML5SpeculativeParser::~HTML5SpeculativeParser()
{
	OP_DELETE(m_inline_finder);
}

void
HTML5SpeculativeParser::ParseL()
{
	m_inline_finder->ParseForInlinesL();
	m_is_finished = TRUE;
}

void HTML5SpeculativeParser::AppendDataL(const uni_char *buffer, unsigned length, BOOL end_of_data)
{
	m_inline_finder->AppendDataL(buffer, length, end_of_data);
}

/* virtual */ void
HTML5SpeculativeParser::LinkFoundL(URL &link_url, Markup::Type type, HTML5TokenWrapper *token)
{
	if (type != Markup::HTE_SCRIPT && type != Markup::HTE_LINK && type != Markup::HTE_ANY && type != Markup::HTE_IMG)
		return;

	FramesDocument *frm_doc = m_logdoc->GetDocument();
	Window *window = frm_doc->GetWindow();
	if (window->IsURLAlreadyRequested(link_url))
		return;

#ifdef CORS_SUPPORT
	/* With the current URL API, URLs that have CORS checks need to be made
	   unique. This makes it really hard to retrieve the LoadInlineElm for the
	   data loaded speculatively when we create the real element and the data
	   will be loaded again. To avoid wasting resources on loading it twice, we
	   will just skip the pre-loading, for now. */
	if ((type == Markup::HTE_SCRIPT || type == Markup::HTE_IMG) && token)
	{
		unsigned attr_idx = 0;
		if (token->HasAttribute(UNI_L("crossorigin"), attr_idx))
			return;
	}
#endif // CORS_SUPPORT

	int inline_type = GENERIC_INLINE;
	if (type == Markup::HTE_SCRIPT)
		inline_type = SCRIPT_INLINE;
	else if (type == Markup::HTE_LINK)
		inline_type = CSS_INLINE;
	else if (type == Markup::HTE_IMG)
	{
		if (!window->LoadImages() || !window->ShowImages())
			return;
		inline_type = IMAGE_INLINE;
	}

	OpStatus::Ignore(frm_doc->LoadInline(&link_url, m_logdoc->GetRoot(), GENERIC_INLINE, LoadInlineOptions().SpeculativeLoad().ForcePriority(frm_doc->GetInlinePriority(inline_type, &link_url))));
 	OP_NEW_DBG("HTML5SpeculativeParser::LinkFound", "speculative_parser");
	OP_DBG((UNI_L("link_url=%s"), link_url.UniName(PASSWORD_SHOW)));
}

/* virtual */ void
HTML5SpeculativeParser::LinkListFoundL(Head &link_list)
{
	for (URLLink* link = (URLLink*)link_list.First(); link; link = (URLLink*)link->Suc())
		LinkFoundL(link->url, Markup::HTE_ANY, NULL);
}

#endif // SPECULATIVE_PARSER
