/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/src/textdata.h"

#include "modules/logdoc/logdoc_util.h" // for ReplaceWhitespace && ReplaceEscapes
#include "modules/logdoc/logdoc_constants.h" // for SplitTextLen

TextData::~TextData()
{
	if (m_text)
	{
		REPORT_MEMMAN_DEC((packed.text_len + 1) * sizeof(uni_char));
	}

	OP_DELETEA(m_text);
}

OP_STATUS
TextData::Construct(const uni_char* new_text,
					unsigned int new_text_len,
					BOOL resolve_entities,
					BOOL is_cdata,
					BOOL mark_as_unfinished/* = FALSE*/)
{
	OP_ASSERT(new_text_len <= SplitTextLen || !"Truncating text. Use a HE_TEXTGROUP and suitable high(er) level APIs");
	OP_ASSERT(is_cdata == !!is_cdata);
	OP_ASSERT(mark_as_unfinished == !!mark_as_unfinished);
	OP_ASSERT(!is_cdata || !resolve_entities);

	// mark_as_unfinished means that we have something that looks like the start of 
	// an entity, but we haven't seen everything yet. We will store it as
	// "<resolved entity>\0<original text\0"

	unsigned int use_text_len = MIN(new_text_len, SplitTextLen); // Not needed anymore?
	if (mark_as_unfinished)
	{
		OP_ASSERT(resolve_entities || !"if we don't resolve entities it can't be unfinished");

		m_text = OP_NEWA(uni_char, 2*(use_text_len + 1));
		if (!m_text)
			return OpStatus::ERR_NO_MEMORY;
		op_memcpy(m_text, new_text, use_text_len * sizeof(uni_char));
		m_text[use_text_len] = 0;
		unsigned decoded_text_len = ReplaceEscapes(m_text, use_text_len, FALSE, FALSE, FALSE);
		op_memcpy(m_text+decoded_text_len+1, new_text, use_text_len * sizeof(uni_char));
		m_text[decoded_text_len+1+use_text_len] = '\0';
		use_text_len = decoded_text_len;
	}
	else
	{
		m_text = OP_NEWA(uni_char, use_text_len + 1);
		if (!m_text)
			return OpStatus::ERR_NO_MEMORY;
		op_memcpy(m_text, new_text, use_text_len * sizeof(uni_char));
		m_text[use_text_len] = 0;

		if (resolve_entities)
			use_text_len = ReplaceEscapes(m_text, use_text_len, FALSE, FALSE, FALSE);
	}

	packed.text_len = use_text_len;

	packed.unterminated_entity = mark_as_unfinished;
	packed.cdata = is_cdata;

	REPORT_MEMMAN_INC((use_text_len + 1) * sizeof(uni_char));

    return OpStatus::OK;
}

OP_STATUS
TextData::Construct(TextData *src_txt)
{
	return Construct(src_txt->GetText(), src_txt->GetTextLen(), FALSE, packed.cdata, packed.unterminated_entity);
}


