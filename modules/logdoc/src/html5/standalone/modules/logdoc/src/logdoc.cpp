/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/src/html5/standalone/standalone.h"

#include "modules/logdoc/logdoc.h"

#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/src/html5/h5node.h"
#include "modules/logdoc/markup.h"
#include "modules/logdoc/src/html5/html5tokenizer.h"

LogicalDocument::~LogicalDocument()
{
	OP_DELETE(m_parser);
	if (m_root->Clean())
		m_root->Free();

	OP_DELETEA(m_doctype_name);
	OP_DELETEA(m_doctype_public_id);
	OP_DELETEA(m_doctype_system_id);
}

OP_PARSING_STATUS LogicalDocument::Parse(Markup::Type context_elm_type, const uni_char *buffer, unsigned buffer_length, BOOL end_of_data)
{
	if (!m_root)
	{
		m_root = OP_NEW(H5Element, (Markup::HTE_DOC_ROOT, Markup::HTML));
		if (!m_root)
			return OpStatus::ERR_NO_MEMORY;
	}

	H5Element *context_elm = NULL;
	if (context_elm_type != Markup::HTE_DOC_ROOT)
	{
		context_elm = OP_NEW(H5Element, (context_elm_type, Markup::HTML));
		if (!context_elm)
			return OpStatus::ERR_NO_MEMORY;
	}

	if (!m_parser)
	{
		m_parser = OP_NEW(HTML5Parser, (this));
		if (!m_parser)
		{
			OP_DELETE(m_root);
			return OpStatus::ERR_NO_MEMORY;
		}

#ifdef HTML5_STANDALONE
		if (m_tokenize_only)
			m_parser->SetOutputTokenizerResults(TRUE);
#endif // HTML5_STANDALONE
	}

	OP_STATUS oom_status = OpStatus::OK;
	TRAP(oom_status, m_parser->AppendDataL(buffer, buffer_length, end_of_data));
	if (OpStatus::IsError(oom_status))
		return oom_status;

	TRAP(oom_status, m_parser->ParseL(m_root, context_elm));

	OP_DELETE(context_elm);

	if (oom_status == HTML5ParserStatus::NEED_MORE_DATA)
		return ParsingStatus::NEED_MORE_DATA;
	else if (oom_status == HTML5ParserStatus::EXECUTE_SCRIPT)
		return ParsingStatus::EXECUTE_SCRIPT;

	return oom_status;
}

OP_PARSING_STATUS LogicalDocument::ParseFragment(H5Element *context_elm, const uni_char *buffer, unsigned buffer_length)
{
	HTML5Parser fragment_parser(this);

	H5Element root(Markup::HTE_DOC_ROOT, Markup::HTML);

#ifdef HTML5_STANDALONE
	if (m_tokenize_only)
		fragment_parser.SetOutputTokenizerResults(TRUE);
#endif // HTML5_STANDALONE

	OP_STATUS oom_status = OpStatus::OK;
	TRAP(oom_status, m_parser->AppendDataL(buffer, buffer_length, TRUE, TRUE));
	if (OpStatus::IsError(oom_status))
		return oom_status;

	TRAP(oom_status, m_parser->ParseL(&root, context_elm));

	if (oom_status == HTML5ParserStatus::NEED_MORE_DATA)
		return ParsingStatus::NEED_MORE_DATA;
	else if (oom_status == HTML5ParserStatus::EXECUTE_SCRIPT)
		return ParsingStatus::EXECUTE_SCRIPT;

	return oom_status;
}

OP_STATUS LogicalDocument::AddParsingData(const uni_char* data, unsigned data_length)
{
	OP_ASSERT(m_root && m_parser);
	
	RETURN_IF_LEAVE(m_parser->AppendDataL(data, data_length, FALSE));

	return OpStatus::OK;
}

OP_PARSING_STATUS LogicalDocument::ContinueParsing()
{
	OP_ASSERT(m_root && m_parser);

	TRAPD(oom_status, m_parser->ContinueParsingL());

	if (oom_status == HTML5ParserStatus::NEED_MORE_DATA)
		return ParsingStatus::NEED_MORE_DATA;
	else if (oom_status == HTML5ParserStatus::EXECUTE_SCRIPT)
		return ParsingStatus::EXECUTE_SCRIPT;

	return oom_status;
}

OP_STATUS LogicalDocument::SetDoctype(const uni_char *name, const uni_char *pub_id, const uni_char *sys_id)
{
	RETURN_IF_MEMORY_ERROR(UniSetStrN(m_doctype_name, name, uni_strlen(name)));
	if (pub_id)
		RETURN_IF_MEMORY_ERROR(UniSetStrN(m_doctype_public_id, pub_id, uni_strlen(pub_id)));
	if (sys_id)
		RETURN_IF_MEMORY_ERROR(UniSetStrN(m_doctype_system_id, sys_id, uni_strlen(sys_id)));

	return OpStatus::OK;
}

#include "modules/logdoc/src/dtdstrings.inc"

void LogicalDocument::CheckQuirksMode()
{
	m_strict_mode = TRUE;

	if (!m_doctype_name || !uni_stri_eq(m_doctype_name, "HTML"))
		m_strict_mode = FALSE;

	size_t pub_len = m_doctype_public_id ? uni_strlen(m_doctype_public_id) : 0;
	size_t sys_len = m_doctype_system_id ? uni_strlen(m_doctype_system_id) : 0;

	if (pub_len > 0)
	{
		if (uni_stri_eq_lower(m_doctype_public_id, "html"))
			m_strict_mode = FALSE;
		else
		{
			if (pub_len > gDTDlenmin)
			{
				int index = (pub_len > gDTDlenmax) ? gDTDlengths[gDTDlenmax - gDTDlenmin + 1]
				: gDTDlengths[pub_len - gDTDlenmin];

				while (index >= 0)
				{
					const char *check_str = gDTDStrings[index];
					size_t check_len = op_strlen(check_str);

					if (check_len > pub_len)
						break;
					else if (uni_strni_eq(check_str, m_doctype_public_id, check_len))
					{
						int check_tok = gDTDtokens[index];
						if (((check_tok & DTD_EXACT_MATCH) && check_len == pub_len)
							|| !(check_tok & DTD_SYSTEM_IDENT))
						{
							if ((check_tok & DTD_TRIGGERS_LIMITED_QUIRKS)
								|| ((check_tok & DTD_NEED_SYSTEM_ID)
								&& m_doctype_system_id != NULL))
							{
								m_quirks_line_height_mode = TRUE;
							}
							else
								m_strict_mode = FALSE;
						}

						break;
					}

					index--;
				}
			}
		}
	}

	if (sys_len > 0)
	{
		if (sys_len > gDTDlenmin)
		{
			int index = (sys_len > gDTDlenmax) ? gDTDlengths[gDTDlenmax - gDTDlenmin]
			: gDTDlengths[sys_len - gDTDlenmin];

			while (index)
			{
				const char *check_str = gDTDStrings[index];
				size_t check_len = op_strlen(check_str);

				if (check_len > sys_len)
					break;
				else if (uni_strni_eq(check_str, m_doctype_system_id, check_len))
				{
					int check_tok = gDTDtokens[index];
					if ((check_tok & DTD_SYSTEM_IDENT)
						&& (!(check_tok & DTD_EXACT_MATCH)
						|| check_len == sys_len))
					{
						m_strict_mode = FALSE;

						if (check_tok & DTD_TRIGGERS_LIMITED_QUIRKS)
							m_quirks_line_height_mode = TRUE;
					}

					break;
				}

				index--;
			}
		}
	}
}
