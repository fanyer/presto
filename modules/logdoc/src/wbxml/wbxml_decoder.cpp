/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#include "core/pch.h"

#ifdef WBXML_SUPPORT

#include "modules/logdoc/wbxml_decoder.h"
#include "modules/url/url2.h"
#include "modules/logdoc/wbxml.h"

WBXML_Decoder::WBXML_Decoder()
	: Data_Decoder()
	, m_parser(NULL)
	, m_src_buf(NULL)
	, m_src_buf_pos(0)
	, m_src_buf_len(0)
{
}

WBXML_Decoder::~WBXML_Decoder()
{
	OP_DELETE(m_parser);
	OP_DELETEA(m_src_buf);
}

unsigned int
#ifdef OOM_SAFE_API
WBXML_Decoder::ReadDataL(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more)
#else
WBXML_Decoder::ReadData(char *buf, unsigned int blen, URL_DataDescriptor *desc, BOOL &read_storage, BOOL &more)
#endif //OOM_SAFE_API
{
	Data_Decoder *source = (Data_Decoder *) Suc();
	unsigned long read_data = 0;
	const char *src_buf = buf;
	int new_len = 0;
	int ret_len = 0;
	const char *tmp_src_buf = NULL;

	if(source)
	{
		if (!m_src_buf)
		{
			m_src_buf = OP_NEWA_L(char, blen);
			m_src_buf_len = blen;
			m_src_buf_pos = 0;
		}

		if(!source->Finished())
		{
#ifdef OOM_SAFE_API
			read_data = source->ReadDataL(m_src_buf + m_src_buf_pos, m_src_buf_len - m_src_buf_pos, desc, read_storage, more);
#else
			read_data = source->ReadData(m_src_buf + m_src_buf_pos, m_src_buf_len - m_src_buf_pos, desc, read_storage, more);
#endif //OOM_SAFE_API
		}

		src_buf = m_src_buf;
		read_data += m_src_buf_pos; // add the length of the rest from last time
		source_is_finished = source->Finished();
	}
	else if(desc)
	{
#ifdef OOM_SAFE_API
		read_data = desc->RetrieveDataL(more);
#else
		read_data = desc->RetrieveData(more);
#endif //OOM_SAFE_API

		if(more || desc->HasMessageHandler() || desc->FinishedLoading())
			source_is_finished = !more;

		src_buf = read_data ? desc->GetBuffer() : NULL;
		read_storage = TRUE;
	}
	else
		source_is_finished = TRUE;

	if (read_data && src_buf)
	{
		OP_WXP_STATUS parse_status = OpStatus::OK;

		new_len = blen - ret_len;

		if (!m_parser)
		{
			m_parser = OP_NEW(WBXML_Parser, ());
			if (!m_parser || OpStatus::IsMemoryError(m_parser->Init()))
			{
				OP_DELETE(m_parser);
				m_parser = NULL;
#ifdef OOM_SAFE_API
				LEAVE(OpStatus::ERR_NO_MEMORY);
#else
				return 0;
#endif //OOM_SAFE_API
			}
		}

		tmp_src_buf = src_buf;
		WBXML_Parser::Content_t	old_content_type = m_parser->GetContentType();
		parse_status = m_parser->Parse(buf, new_len, &tmp_src_buf, tmp_src_buf + read_data, more);

		if (OpStatus::IsError(parse_status))
		{
			error = TRUE;
			more = FALSE;
#ifdef OOM_SAFE_API
			LEAVE(parse_status);
#else
			return 0;
#endif //OOM_SAFE_API
		}

		UINT rest = read_data - (tmp_src_buf - src_buf);

		if (source)
		{
			// consume used data
			if (rest > 0)
			{
				if (rest == m_src_buf_len && parse_status == WXParseStatus::NEED_GROW)
					m_src_buf_len += blen;

				char *new_buffer = OP_NEWA_L(char, m_src_buf_len);
				op_memcpy(new_buffer, tmp_src_buf, rest);

				OP_DELETEA(m_src_buf);
				m_src_buf = new_buffer;
			}

			m_src_buf_pos = rest;
		}
		else if (desc)
		{
			desc->ConsumeData(read_data - rest);

			if (parse_status == WXParseStatus::NEED_GROW && desc->Grow() == 0)
			{
#ifdef OOM_SAFE_API
				LEAVE(OpStatus::ERR_NO_MEMORY);
#else // OOM_SAFE_API
				return 0;
#endif //OOM_SAFE_API
			}
		}

		ret_len += new_len;

		if (old_content_type != m_parser->GetContentType())
		{
			switch (m_parser->GetContentType())
			{
#ifdef WML_WBXML_SUPPORT
			case WBXML_Parser::WX_TYPE_WML:
				desc->GetURL().SetAttribute(URL::KForceContentType, URL_WML_CONTENT);
				break;
#endif // WML_WBXML_SUPPORT

			default:
				desc->GetURL().SetAttribute(URL::KForceContentType, URL_XML_CONTENT);
				break;
			}
			desc->GetURL().SetAttribute(URL::KMIME_CharSet, "utf-16");
		}
		}

	more = !source_is_finished || (tmp_src_buf && ((unsigned long)(tmp_src_buf - src_buf) != read_data));
	finished = !more;

	return ret_len;
}

	/** Get character set associated with this Data_Decoder.
	  * @return NULL if no character set is associated
	  */
const char*
WBXML_Decoder::GetCharacterSet()
{
	return "utf-16";
}

	/** Get character set detected for this CharacterDecoder.
	  * @return Name of character set, NULL if no character set can be
	  *         determined.
	  */
const char*
WBXML_Decoder::GetDetectedCharacterSet()
{
	return "utf-16";
}
	/** Stop guessing character set for this document.
	  * Character set detection wastes cycles, and avoiding it if possible
	  * is nice.
	  */
void
WBXML_Decoder::StopGuessingCharset() {}

BOOL
WBXML_Decoder::IsWml()
{
	return m_parser && m_parser->IsWml();
}

BOOL
WBXML_Decoder::IsA(const uni_char *type)
{
	return (uni_stricmp(type, UNI_L("WBXML_Decoder")) == 0) || Data_Decoder::IsA(type);
}

#endif // WBXML_SUPPORT
