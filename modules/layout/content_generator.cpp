/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/layout/content_generator.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc_util.h"

ContentGenerator::Content::Content()
	: is_text(TRUE),
#ifdef SKIN_SUPPORT
	  is_skin(FALSE),
#endif // SKIN_SUPPORT
	  content_buf(NULL),
	  content_buf_size(0)
{}

ContentGenerator::Content::~Content()
{
	OP_DELETEA(content_buf);
	REPORT_MEMMAN_DEC(content_buf_size * sizeof(uni_char));
}

BOOL
ContentGenerator::Content::AddString(const uni_char* string)
{
	// later: use a generic string buffer class here instead
	if (!content_buf)
	{
		content_buf_size = 256;
		content_buf = OP_NEWA(uni_char, content_buf_size);

		if (!content_buf)
		{
			content_buf_size = 0;
			return FALSE;
		}

		REPORT_MEMMAN_INC(content_buf_size * sizeof(uni_char));
		*content_buf = '\0';
	}
	int content_buf_used = uni_strlen(content_buf);
	int free_size = content_buf_size - content_buf_used - 1;
	int string_len = (string) ? uni_strlen(string) : 0;
	if (string_len > free_size)
	{
		int old_content_buf_size = content_buf_size;
		content_buf_size += string_len - free_size + 256;

		uni_char* new_content_buf = OP_NEWA(uni_char, content_buf_size);
		if (!new_content_buf)
		{
			content_buf_size = old_content_buf_size;
			return FALSE;
		}

		REPORT_MEMMAN_INC((content_buf_size - old_content_buf_size) * sizeof(uni_char));
		uni_strcpy(new_content_buf, content_buf);

		OP_DELETEA(content_buf);
		content_buf = new_content_buf;
	}
	if (string)
		uni_strcpy(content_buf+content_buf_used, string);

	return TRUE;
}

void
ContentGenerator::Content::Reset()
{
	if (content_buf)
		content_buf[0] = '\0';

	is_text = TRUE;
}


OP_STATUS
ContentGenerator::Content::CreateElement(HTML_Element*& element,
										 HLDocProfile* hld_profile) const
{
	element = NULL;

	if (!content_buf)
		return OpStatus::OK;

	OP_STATUS stat = OpStatus::OK;

	if (is_text)
	{
		element = HTML_Element::CreateText(content_buf, uni_strlen(content_buf), FALSE, FALSE, FALSE);
		if (!element)
			return OpStatus::ERR_NO_MEMORY;
		return OpStatus::OK;
	}
	else
	{
		element = NEW_HTML_Element();

		if (!element)
			return OpStatus::ERR_NO_MEMORY;

		// create an image element with text data as url
		HtmlAttrEntry ha_list[2];

#ifdef SKIN_SUPPORT
		if (is_skin)
		{
			ha_list[0].attr      = Markup::HA_NULL;
		}
		else
#endif // SKIN_SUPPORT
		{
			ha_list[0].ns_idx    = NS_IDX_DEFAULT;
			ha_list[0].attr      = Markup::HA_SRC;
			ha_list[0].value     = content_buf;
			ha_list[0].value_len = uni_strlen(content_buf);
			ha_list[1].attr      = Markup::HA_NULL;
		}

		stat = element->Construct(hld_profile, NS_IDX_HTML, Markup::HTE_IMG, ha_list);
#ifdef SKIN_SUPPORT
		if (stat == OpStatus::OK && is_skin && content_buf)
		{
			uni_char* new_str = UniSetNewStr(content_buf);
			if (!new_str || element->SetSpecialAttr(Markup::LAYOUTA_SKIN_ELM, ITEM_TYPE_STRING, (void*)new_str, TRUE, SpecialNs::NS_LAYOUT) == -1)
			{
				OP_DELETEA(new_str);
				stat = OpStatus::ERR_NO_MEMORY;
			}
		}
#endif // SKIN_SUPPORT
	}

	if (stat == OpStatus::ERR_NO_MEMORY)
	{
		DELETE_HTML_Element(element);
	}

	return stat;
}

// ---

ContentGenerator::~ContentGenerator()
{
	Reset();
}

void
ContentGenerator::Reset()
{
	content_list.Clear();
}

const ContentGenerator::Content*
ContentGenerator::GetContent()
{
	return (Content*)content_list.First();
}

BOOL
ContentGenerator::AddURL(const uni_char* url)
{
	Content* content = OP_NEW(Content, ());

	if (!content)
		return FALSE;

	if (!content->AddString(url))
	{
		OP_DELETE(content);
		return FALSE;
	}

	content->is_text = FALSE;
	content->Into(&content_list);

	return TRUE;
}

BOOL
ContentGenerator::AddString(const uni_char* string)
{
	Content* last_content = (Content*)content_list.Last();

	if (last_content && last_content->is_text)
	{
		return last_content->AddString(string);
	}
	else
	{
		Content* content = OP_NEW(Content, ());

		if (!content)
			return FALSE;

		if (!content->AddString(string))
		{
			OP_DELETE(content);
			return FALSE;
		}

		content->Into(&content_list);

		return TRUE;
	}
}

BOOL
ContentGenerator::AddLineBreak()
{
	const uni_char force_linebreak[2] = { FORCE_LINE_BREAK_CHAR, 0 };

	return AddString(force_linebreak);
}

#ifdef SKIN_SUPPORT
BOOL
ContentGenerator::AddSkinElement(const uni_char* skin_elm)
{
	Content* content = OP_NEW(Content, ());

	if (!content)
		return FALSE;

	if (!content->AddString(skin_elm))
	{
		OP_DELETE(content);
		return FALSE;
	}

	content->is_text = FALSE;
	content->is_skin = TRUE;
	content->Into(&content_list);

	return TRUE;
}
#endif // SKIN_SUPPORT
