/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef WBXML_SUPPORT

#include "modules/logdoc/wbxml.h"
#include "modules/encodings/utility/charsetnames.h" // MIBname
#include "modules/encodings/decoders/inputconverter.h" // inputconverter

#include "modules/util/htmlify.h"
#include "modules/encodings/utility/opstring-encodings.h"

#define WXSTRLEN(str) ((sizeof(str)/sizeof(*str))-1)

#if defined(SI_WBXML_SUPPORT) || defined(EMN_WBXML_SUPPORT)
static void DecodeBcd(uni_char *&dest, const char *&src)
{
	if (src)
	{
		UINT8 c = *(UINT8 *)src ++;
		*dest++ = '0' + (c >> 4);
		*dest++ = '0' + (c & 0x0f);
	}
	else
	{
		*dest ++ = '0';
		*dest ++ = '0';
	}
}

static const uni_char *DecodeDateTime(WBXML_Parser *parser , char **buf)
{
	// Decode %DateTime

	parser->EnsureTmpBufLenL(21);
	uni_char *tmpbuf = parser->GetTmpBuf();
	int len = **buf;
	(*buf) ++;
	OP_ASSERT(len <= 7);

	uni_char *ptr = tmpbuf;
	const char *fmt = "%%-%-%T%:%:%Z";
	while (*fmt)
	{
		char token = *fmt++;
		if (token != '%')
		{
			*ptr++ = token;
			continue;
		}

		uni_char ch1 = '0';
		uni_char ch2 = '0';

		if (len-- > 0)
		{
			UINT8 val = *(UINT8 *)(*buf);
			(*buf) ++;
			ch1 += val >> 4;
			ch2 += val & 0x0f;
		}
		*ptr++ = ch1;
		*ptr++ = ch2;
	}
	*ptr ++ = '\000';

	/* FIXME: SHOULD validate that the result string follows the syntax
	   specified in WAP-167 5.2.1 */

	return tmpbuf;
}
#endif // SI_WBXML_SUPPORT || EMN_WBXML_SUPPORT

#ifdef WML_WBXML_SUPPORT
# include "modules/logdoc/src/wbxml/wml_wbxml.cpp" // WBXML specific content handler
#endif // WML_WBXML_SUPPORT

#ifdef SI_WBXML_SUPPORT
# include "modules/logdoc/src/wbxml/si_wbxml.cpp" // WBXML specific content handler for Service Indication
#endif // SI_WBXML_SUPPORT

#ifdef SL_WBXML_SUPPORT
# include "modules/logdoc/src/wbxml/sl_wbxml.cpp" // WBXML specific content handler for Service Loading
#endif // SL_WBXML_SUPPORT

#ifdef PROV_WBXML_SUPPORT
# include "modules/logdoc/src/wbxml/prov_wbxml.cpp" // WBXML specific content handler for Provisioning
#endif // PROV_WBXML_SUPPORT

#ifdef DRMREL_WBXML_SUPPORT
# include "modules/doc/wbxml/drmrel_wbxml.cpp" // WBXML specific content handler for DRM Rights Expression Language
#endif // DRMREL_WBXML_SUPPORT

#ifdef CO_WBXML_SUPPORT
# include "modules/logdoc/src/wbxml/co_wbxml.cpp" // WBXML specific content handler for Cache Operation
#endif // CO_WBXML_SUPPORT

#ifdef EMN_WBXML_SUPPORT
# include "modules/logdoc/src/wbxml/emn_wbxml.cpp" // WBXML specific content handler for E-Mail Notification
#endif // EMN_WBXML_SUPPORT


class WXToken
{
public:
	// GLOBAL TOKENS
	enum
	{
		SWITCH_PAGE	= 0x00,
		END			= 0x01,
		ENTITY		= 0x02,
		STR_I		= 0x03,
		LITERAL		= 0x04,
		EXT_I_0		= 0x40,
		EXT_I_1		= 0x41,
		EXT_I_2		= 0x42,
		PI			= 0x43,
		LITERAL_C	= 0x44,
		EXT_T_0		= 0x80,
		EXT_T_1		= 0x81,
		EXT_T_2		= 0x82,
		STR_T		= 0x83,
		LITERAL_A	= 0x84,
		EXT_0		= 0xC0,
		EXT_1		= 0xC1,
		EXT_2		= 0xC2,
		WXOPAQUE	= 0xC3,
		LITERAL_AC	= 0xC4
	};
};

//
// WBXML_TagElm implementation
//

void
WBXML_TagElm::SetTagNameL(const uni_char *tag)
{
	m_tag = OP_NEWA_L(uni_char, uni_strlen(tag) + 1);
	uni_strcpy(m_tag, tag);
}

//
// WBXML_Parser implementation
//

enum
{
#ifdef WML_WBXML_SUPPORT
	WBXML_INDEX_WML,
#endif // WML_WBXML_SUPPORT
#ifdef SI_WBXML_SUPPORT
	WBXML_INDEX_SI,
#endif // SI_WBXML_SUPPORT
#ifdef SL_WBXML_SUPPORT
	WBXML_INDEX_SL,
#endif // SL_WBXML_SUPPORT
#ifdef PROV_WBXML_SUPPORT
	WBXML_INDEX_PROV,
#endif // PROV_WBXML_SUPPORT
#ifdef DRMREL_WBXML_SUPPORT
	WBXML_INDEX_DRMREL,
#endif  // DRMREL_WBXML_SUPPORT
#ifdef CO_WBXML_SUPPORT
	WBXML_INDEX_CO,
#endif // CO_WBXML_SUPPORT
#ifdef EMN_WBXML_SUPPORT
	WBXML_INDEX_EMN,
#endif // EMN_WBXML_SUPPORT
	WBXML_INDEX_LAST
};

WBXML_Parser::WBXML_Parser()
  : m_header_parsed(FALSE), m_more(FALSE), m_code_page(0), m_version(0), 
	m_doc_type(WBXML_INDEX_LAST), m_content_type(WBXML_Parser::WX_TYPE_UNKNOWN),
	m_strtbl_len(0), m_strtbl(NULL),
	m_in_buf_end(NULL), m_in_buf_committed(0), m_tmp_buf(NULL), m_tmp_buf_len(0),
	m_out_buf(NULL), m_out_buf_idx(0), m_out_buf_len(0), m_out_buf_committed(0),
	m_overflow_buf(NULL), m_overflow_buf_len(0), m_overflow_buf_idx(0),
	m_char_conv(NULL), m_tag_stack(NULL), m_tag_stack_committed(NULL),
	m_attr_parsing_indicator(NULL),
	m_content_handlers(NULL)
{
}

WBXML_Parser::~WBXML_Parser()
{
	OP_DELETE(m_char_conv);

	// if the attr indicator is not on the stack delete it
	if (m_tag_stack != m_attr_parsing_indicator)
		OP_DELETE(m_attr_parsing_indicator);

	// delete the stack
	while (m_tag_stack)
	{
		WBXML_StackElm *tmp_elm = m_tag_stack->GetNext();
		OP_DELETE(m_tag_stack);
		m_tag_stack = tmp_elm;
	}

	for (int i = 0; i < WBXML_INDEX_LAST; i++)
		OP_DELETE(m_content_handlers[i]);

	OP_DELETEA(m_content_handlers);

	OP_DELETEA(m_tmp_buf);
	OP_DELETEA(m_strtbl);
	OP_DELETEA(m_overflow_buf);
}

OP_WXP_STATUS
WBXML_Parser::Init()
{
	if (WBXML_INDEX_LAST > 0)
	{
		m_content_handlers = OP_NEWA(WBXML_ContentHandler *, WBXML_INDEX_LAST);

		if (!m_content_handlers)
			return OpStatus::ERR_NO_MEMORY;

		for (int i = 0; i < WBXML_INDEX_LAST; i++)
			m_content_handlers[i] = NULL;

#ifdef WML_WBXML_SUPPORT
		m_content_handlers[WBXML_INDEX_WML] = OP_NEW(WML_WBXML_ContentHandler, (this));
		if (!m_content_handlers[WBXML_INDEX_WML])
			return OpStatus::ERR_NO_MEMORY;
		m_content_handlers[WBXML_INDEX_WML]->Init();//OOM: No check needed, always returns OK
#endif // WML_WBXML_SUPPORT

#ifdef SI_WBXML_SUPPORT
		m_content_handlers[WBXML_INDEX_SI] = OP_NEW(ServiceInd_WBXML_ContentHandler, (this));
		if (!m_content_handlers[WBXML_INDEX_SI])
			return OpStatus::ERR_NO_MEMORY;
		m_content_handlers[WBXML_INDEX_SI]->Init();//OOM: No check needed, always returns OK
#endif // SI_WBXML_SUPPORT

#ifdef SL_WBXML_SUPPORT
		m_content_handlers[WBXML_INDEX_SL] = OP_NEW(ServiceLoad_WBXML_ContentHandler, (this));
		if (!m_content_handlers[WBXML_INDEX_SL])
			return OpStatus::ERR_NO_MEMORY;
		m_content_handlers[WBXML_INDEX_SL]->Init();//OOM: No check needed, always returns OK
#endif // SL_WBXML_SUPPORT

#ifdef PROV_WBXML_SUPPORT
		m_content_handlers[WBXML_INDEX_PROV] = OP_NEW(Provisioning_WBXML_ContentHandler, (this));
		if (!m_content_handlers[WBXML_INDEX_PROV])
			return OpStatus::ERR_NO_MEMORY;
		m_content_handlers[WBXML_INDEX_PROV]->Init();//OOM: No check needed, always returns OK
#endif // PROV_WBXML_SUPPORT

#ifdef DRMREL_WBXML_SUPPORT
		m_content_handlers[WBXML_INDEX_DRMREL] = OP_NEW(RightsExpressionLanguage_WBXML_ContentHandler, (this));
		if (!m_content_handlers[WBXML_INDEX_DRMREL])
			return OpStatus::ERR_NO_MEMORY;
		m_content_handlers[WBXML_INDEX_DRMREL]->Init();//OOM: No check needed, always returns OK
#endif // DRMREL_WBXML_SUPPORT

#ifdef CO_WBXML_SUPPORT
		m_content_handlers[WBXML_INDEX_CO] = OP_NEW(CacheOperation_WBXML_ContentHandler, (this));
		if (!m_content_handlers[WBXML_INDEX_CO])
			return OpStatus::ERR_NO_MEMORY;
		m_content_handlers[WBXML_INDEX_CO]->Init();//OOM: No check needed, always returns OK
#endif // CO_WBXML_SUPPORT

#ifdef EMN_WBXML_SUPPORT
		m_content_handlers[WBXML_INDEX_EMN] = OP_NEW(EmailNotification_WBXML_ContentHandler, (this));
		if (!m_content_handlers[WBXML_INDEX_EMN])
			return OpStatus::ERR_NO_MEMORY;
		m_content_handlers[WBXML_INDEX_EMN]->Init();//OOM: No check needed, always returns OK
#endif // EMN_WBXML_SUPPORT
	}

	m_attr_parsing_indicator = OP_NEW(WBXML_AttrElm, ());
	if (!m_attr_parsing_indicator)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

UINT32
WBXML_Parser::SetDocType(UINT32 type)
{
	switch (type)
	{
#ifdef WML_WBXML_SUPPORT
	case 0x02:
	case 0x04:
	case 0x09:
	case 0x0A:
		{
			m_doc_type = WBXML_INDEX_WML;
			m_content_type = WX_TYPE_WML;
		}
		break;
#endif // WML_WBXML_SUPPORT

#ifdef SI_WBXML_SUPPORT
	case 0x05:
		{
			m_doc_type = WBXML_INDEX_SI;
			m_content_type = WX_TYPE_SI;
		}
		break;
#endif // SI_WBXML_SUPPORT

#ifdef SL_WBXML_SUPPORT
	case 0x06:
		{
			m_doc_type = WBXML_INDEX_SL;
			m_content_type = WX_TYPE_SL;
		}
		break;
#endif // SL_WBXML_SUPPORT

#ifdef PROV_WBXML_SUPPORT
	case 0x0b:
		{
			m_doc_type = WBXML_INDEX_PROV;
			m_content_type = WX_TYPE_PROV;
		}
		break;
#endif // PROV_WBXML_SUPPORT

#ifdef DRMREL_WBXML_SUPPORT
	case 0x0e:
		{
			m_doc_type = WBXML_INDEX_DRMREL;
			m_content_type = WX_TYPE_DRMREL;
		}
		break;
#endif // DRMREL_WBXML_SUPPORT

#ifdef CO_WBXML_SUPPORT
	case 0x07:
		{
			m_doc_type = WBXML_INDEX_CO;
			m_content_type = WX_TYPE_CO;
		}
		break;
#endif // CO_WBXML_SUPPORT

#ifdef EMN_WBXML_SUPPORT
		/* Bug in OMA-Push-EMN-v1_0-20020830-C: This specification says
		   (well... only in the example WBXML data in appendix B) that the
		   public identifier for EMN is 0x0f, but both
		   http://www.openmobilealliance.org/tech/omna/omna-wbxml-public-docid.htm
		   and the corrected OMA-Push-EMN-v1_0-20040427-C say 0x0d.
		   Note that the WBXML specification WAP-192-WBXML-20010725-a says that
		   0x0d means "-//WAPFORUM//DTD CHANNEL 1.2//EN  (Channel 1.2)". Oh
		   well, let's assume that 0x0d is correct for EMN.
		*/
	case 0x0d:
		{
			m_doc_type = WBXML_INDEX_EMN;
			m_content_type = WX_TYPE_EMN;
		}
		break;
#endif // EMN_WBXML_SUPPORT

	default:
		{
			m_doc_type = WBXML_INDEX_LAST;
			m_content_type = WX_TYPE_UNKNOWN;
	}
		break;
	}

	return m_doc_type;
}

void
WBXML_Parser::EnsureTmpBufLenL(int len)
{
	if (m_tmp_buf_len < len + 1)
	{
		int new_buf_len = m_tmp_buf_len;
		while (new_buf_len < len + 1)
			new_buf_len += 64;

		uni_char* new_buf = OP_NEWA_L(uni_char, new_buf_len);
		OP_DELETEA(m_tmp_buf);
		m_tmp_buf = new_buf;
		m_tmp_buf_len = new_buf_len;
	}
}

UINT32
WBXML_Parser::GetNextTokenL(const char **buf, BOOL is_mb_value /*= FALSE*/)
{
	const char *buf_p = *buf;
	if (buf_p == m_in_buf_end)
		LEAVE(m_more ? WXParseStatus::NEED_GROW : WXParseStatus::UNEXPECTED_END);

	UINT32 ret_val = is_mb_value ? (UINT32)(*buf_p & 0x7f) : (UINT32)(*buf_p & 0xff);

	if (is_mb_value && (*buf_p & 0x80))
	{
		while (buf_p < m_in_buf_end && (*buf_p & 0x80))
		{
			ret_val <<= 7;
			buf_p++;
			ret_val |= (UINT32)(*buf_p & 0x7f);
		}

		if (buf_p == m_in_buf_end)
			LEAVE(m_more ? WXParseStatus::NEED_GROW : WXParseStatus::UNEXPECTED_END);
	}

	*buf = buf_p + 1;
	return ret_val;
}

void
WBXML_Parser::PassNextStringL(const char **buf)
{
	const char *buf_p = *buf;
	while (buf_p < m_in_buf_end && *buf_p)
		buf_p++;

	if (buf_p == m_in_buf_end)
		LEAVE(m_more ? WXParseStatus::NEED_GROW : WXParseStatus::UNEXPECTED_END);

	*buf = buf_p + 1;
}

void WBXML_Parser::EnqueueL(const uni_char *str, int len, const char* commit_pos, BOOL need_quoting/* = TRUE*/, BOOL may_clear_overflow_buffer/* = TRUE*/)
{
	if (!str || !len)
		return;

	unsigned int cpy_len = len;
	const uni_char *cpy_str = str;
	uni_char *new_str = NULL;

	if (need_quoting)
	{
		new_str = XMLify_string(str, len, TRUE, FALSE);
		if (len > 0 && ! new_str)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		cpy_len = uni_strlen(new_str);
		cpy_str = new_str;
	}

	UINT32 dollars = 0;
#ifdef WML_WBXML_SUPPORT
	if (need_quoting && m_doc_type == WBXML_INDEX_WML)
	{
		for (unsigned int i = 0; i < cpy_len; i++)
			if (cpy_str[i] == '$')
				dollars++;
	}
#endif // WML_WBXML_SUPPORT

	if (dollars > 0)
	{
		for (unsigned int i = 0; i < cpy_len; i++)
		{
			if (cpy_str[i] == '$')
			{
				// quote a dollar with two dollars in WML
				for (UINT32 j = 0; j < 2; j++)
				{
					if (m_out_buf_idx < m_out_buf_len)
					{
						m_out_buf[m_out_buf_idx++] = '$';
					}
					else
					{
						GrowOverflowBufferIfNeededL(m_overflow_buf_idx);
						m_overflow_buf[m_overflow_buf_idx++] = '$';
					}
				}

				dollars--;

				// exit early if we have no more $ to quote
				if (!dollars)
				{
					i++; // to advance one step in the string
					unsigned end_idx = m_out_buf_idx + cpy_len - i;
					if (m_out_buf_idx < m_out_buf_len)
					{
						unsigned tmp_length = MIN(m_out_buf_len - m_out_buf_idx, cpy_len - i);
						uni_strncpy(m_out_buf + m_out_buf_idx, cpy_str + i, tmp_length);
						m_out_buf_idx += tmp_length;
						i += tmp_length;
					}

					if (end_idx > m_out_buf_idx) //Remaining is overflow. Store in separate buffer
					{
						GrowOverflowBufferIfNeededL(m_overflow_buf_idx+(end_idx-m_out_buf_idx));
						uni_strncpy(m_overflow_buf+m_overflow_buf_idx, cpy_str + i, end_idx-m_out_buf_idx);
						m_overflow_buf_idx += end_idx-m_out_buf_idx;
					}
					break;
				}
			}
			else
			{
				if (m_out_buf_idx < m_out_buf_len)
				{
					m_out_buf[m_out_buf_idx++] = cpy_str[i];
				}
				else
				{
					GrowOverflowBufferIfNeededL(m_overflow_buf_idx);
					m_overflow_buf[m_overflow_buf_idx++] = cpy_str[i];
				}
			}
		}
	}
	else
	{
		unsigned end_idx = m_out_buf_idx + cpy_len;
		if (m_out_buf_idx < m_out_buf_len)
		{
			unsigned tmp_length = MIN(m_out_buf_len - m_out_buf_idx, cpy_len);
			uni_strncpy(m_out_buf + m_out_buf_idx, cpy_str, tmp_length);
			m_out_buf_idx += tmp_length;
			cpy_str += tmp_length;
		}

		if (end_idx > m_out_buf_idx) //Remaining is overflow. Store in separate buffer
		{
			GrowOverflowBufferIfNeededL(m_overflow_buf_idx+(end_idx-m_out_buf_idx));
			uni_strncpy(m_overflow_buf+m_overflow_buf_idx, cpy_str, end_idx-m_out_buf_idx);
			m_overflow_buf_idx += end_idx-m_out_buf_idx;
		}
	}

	OP_DELETEA(new_str); // HTMLify_string creates a new buffer

	if (commit_pos) Commit2Buffer(const_cast<char*>(commit_pos));

	if (m_overflow_buf && m_overflow_buf_idx>0)
	{
		// If we not commit we should clear the overflow buffer. The
		// exception is when called from PopTagAndEnqueueL where we
		// should not commit but keep the buffer.
		if (!commit_pos && may_clear_overflow_buffer)
		{
			OP_DELETEA(m_overflow_buf);
			m_overflow_buf = NULL;
			m_overflow_buf_len = 0;
			m_overflow_buf_idx = 0;
		}
		LEAVE(WXParseStatus::BUFFER_FULL);
	}
}

void
WBXML_Parser::ConvertAndEnqueueStrL(const char *str, int len, const char* commit_pos)
{
	OpString enq_str;
	ANCHOR(OpString, enq_str);

	SetFromEncodingL(&enq_str, m_char_conv, str, len);
	
	EnqueueL(enq_str.CStr(), enq_str.Length(), commit_pos);
}

const char*
WBXML_Parser::LookupStr(UINT32 index)
{
	if (m_strtbl && index < m_strtbl_len)
		return &m_strtbl[index];

	return NULL;
}

void
WBXML_Parser::LookupAndEnqueueL(UINT32 index, const char* commit_pos)
{
	if (m_strtbl && index < m_strtbl_len)
	{
		char *str_p = &m_strtbl[index];
		char *strtbl_end = m_strtbl + m_strtbl_len;

		while (*str_p && str_p < strtbl_end)
			str_p++;

		OpString tmp_str;
		ANCHOR(OpString, tmp_str);
		SetFromEncodingL(&tmp_str, m_char_conv, &m_strtbl[index], str_p - &m_strtbl[index]);

		EnqueueL(tmp_str.CStr(), tmp_str.Length(), commit_pos);
	}
}

void
WBXML_Parser::ParseHeaderL(const char **buf)
{
	// WBXML version
	m_version = GetNextTokenL(buf);
	if (m_version > 0x0f)
		LEAVE(WXParseStatus::WRONG_VERSION);

	// DOCTYPE
	BOOL fetch_id_from_strtbl = FALSE;
	UINT32 pub_id = GetNextTokenL(buf, TRUE);
	if (pub_id == 0)
	{
		pub_id = GetNextTokenL(buf, TRUE);
		fetch_id_from_strtbl = TRUE;
	}

	// CHARSET
	OpString encoding;
	ANCHOR(OpString, encoding);
	encoding.SetL("UTF-8");
	UINT32 charset = GetNextTokenL(buf, TRUE);
	if (!m_char_conv)
	{
		if (charset > 0)
		{
			const char *charset_name =
				g_charsetManager->GetCharsetNameFromMIBenum((int)charset);
			if (charset_name)
			{
				// Ignore status value, since we try again below
				OpStatus::Ignore(InputConverter::CreateCharConverter(charset, &m_char_conv));
				encoding.SetFromUTF8L(charset_name);
			}
		}

		if (!m_char_conv)
		{
			LEAVE_IF_ERROR(InputConverter::CreateCharConverter(106, &m_char_conv));
		}
	}

	// STRINGTABLE
	m_strtbl_len = GetNextTokenL(buf, TRUE);
	if (m_strtbl_len > 0 && !m_strtbl)
	{
		if (*buf + m_strtbl_len > m_in_buf_end)
			LEAVE(m_more ? WXParseStatus::NEED_GROW : WXParseStatus::UNEXPECTED_END);

		m_strtbl = OP_NEWA_L(char, m_strtbl_len);
		op_memcpy(m_strtbl, *buf, m_strtbl_len); // sizeof(char) is per definition 1

		*buf += m_strtbl_len;
	}

	// DOCTYPE FROM STRINGTABLE
	OpString o_pub_id;
	ANCHOR(OpString, o_pub_id);
	if (fetch_id_from_strtbl)
	{
		const char *pub_id_str = LookupStr(pub_id);
		SetFromEncodingL(&o_pub_id, m_char_conv, pub_id_str, op_strlen(pub_id_str));
	}
	else
	{
		switch (pub_id)
		{
#ifdef SI_WBXML_SUPPORT
		case 0x5:
			o_pub_id.SetL("si PUBLIC \"-//WAPFORUM//DTD SI 1.0//EN\" \"http://www.wapforum.org/DTD/si.dtd\"");
			break;
#endif // SI_WBXML_SUPPORT
#ifdef SL_WBXML_SUPPORT
		case 0x6:
			o_pub_id.SetL("sl PUBLIC \"-//WAPFORUM//DTD SL 1.0//EN\" \"http://www.wapforum.org/DTD/sl.dtd\"");
			break;
#endif // SL_WBXML_SUPPORT
#ifdef PROV_WBXML_SUPPORT
		case 0xb:
			o_pub_id.SetL("wap-provisioningdoc PUBLIC \"-//WAPFORUM//DTD PROV 1.0//EN\" \"http://www.wapforum.org/DTD/prov.dtd\"");
			break;
#endif // PROV_WBXML_SUPPORT
#ifdef DRMREL_WBXML_SUPPORT
		case 0xe:
			o_pub_id.SetL("o-ex:rights PUBLIC \"-//OMA//DTD DRMREL 1.0//EN\" \"http://www.openmobilealliance.org/DTD/drmrel10.dtd\"");
			break;
#endif // DRMREL_WBXML_SUPPORT
#ifdef CO_WBXML_SUPPORT
		case 0x7:
			o_pub_id.Set("co PUBLIC \"-//WAPFORUM//DTD CO 1.0//EN\" \"http://www.wapforum.org/DTD/co_1.0.dtd\"");
			break;
#endif // CO_WBXML_SUPPORT
#ifdef EMN_WBXML_SUPPORT
		case 0xd:
			o_pub_id.Set("emn PUBLIC \"-//WAPFORUM//DTD EMN 1.0//EN\" \"http://www.wapforum.org/DTD/emn.dtd\">");
			break;
#endif // EMN_WBXML_SUPPORT
		case 0x0: // just to silence computer that there is nothing but default cases :)
		default:
			o_pub_id.SetL("wml PUBLIC \"-//WAPFORUM//DTD WML 1.3//EN\" \"http://www.wapforum.org/DTD/wml13.dtd\"");
			break;
		}
	}

	m_doc_type = SetDocType(pub_id);

	// Write the stuff to the buffer
	EnqueueL(UNI_L("<?xml version=\"1.0\" encoding=\"utf-16\"?>\n"), WXSTRLEN("<?xml version=\"1.0\" encoding=\"utf-16\"?>\n"), *buf, FALSE);
	EnqueueL(UNI_L("<!DOCTYPE "), WXSTRLEN("<!DOCTYPE "), *buf, FALSE);
	EnqueueL(o_pub_id.CStr(), o_pub_id.Length(), *buf, FALSE);
	EnqueueL(UNI_L(">\n\n"), WXSTRLEN(">\n\n"), *buf, FALSE);

	m_header_parsed = TRUE;
}

void
WBXML_Parser::ParseTreeL(const char **buf)
{
	UINT32 tok;
	const char *buf_p = *buf;

	while (buf_p != m_in_buf_end)
	{
		if (IsInAttrState())
			tok = m_attr_parsing_indicator->GetToken();
		else
			tok = GetNextTokenL(&buf_p);

		const char *token_start = buf_p;

		switch (tok)
		{
		case WXToken::END:
			PopTagAndEnqueueL(buf_p);
			break;

		case WXToken::SWITCH_PAGE:
			m_code_page = GetNextTokenL(&buf_p);
			break;

		case WXToken::ENTITY:
			{
				UINT32 entity = GetNextTokenL(&buf_p, TRUE);

				EnsureTmpBufLenL(WXSTRLEN("&#;") + sizeof(UINT32) * 3 + 1);
				int len = uni_sprintf(m_tmp_buf, UNI_L("&#%d;"), entity);

				EnqueueL(m_tmp_buf, len, buf_p, FALSE);
			}
			break;

		case WXToken::STR_I:
			{
				PassNextStringL(&buf_p);
				ConvertAndEnqueueStrL(token_start, buf_p - token_start - 1, buf_p);
			}
			break;

		case WXToken::LITERAL:
		case WXToken::LITERAL_C:
		case WXToken::LITERAL_A:
		case WXToken::LITERAL_AC:
			{
				if (!IsInAttrState())
				{
					UINT32 idx = GetNextTokenL(&buf_p, TRUE);
					const char *tag = LookupStr(idx);

					OpString tmp_str;
					ANCHOR(OpString, tmp_str);
					if (tag)
						SetFromEncodingL(&tmp_str, m_char_conv, tag, op_strlen(tag));
					else
					{
						EnsureTmpBufLenL(WXSTRLEN("tag") + 3 + 1);
						uni_sprintf(m_tmp_buf, UNI_L("tag%X"), tok & 0x3f);
						tmp_str.SetL(m_tmp_buf);
					}

					EnsureTmpBufLenL(tmp_str.Length() + 2);

					int len = uni_sprintf(m_tmp_buf, UNI_L("%s"), tmp_str.CStr());
					EnqueueL(UNI_L("<"), WXSTRLEN("<"), buf_p, FALSE);
					EnqueueL(m_tmp_buf, len, buf_p, TRUE);
				}

				if (tok & 0x80) // has attributes
				{
					if (!IsInAttrState())
						PushAttrState(tok);

					if (!m_attr_parsing_indicator->GetEndFound())
						ParseAttrsL(&buf_p);

					m_attr_parsing_indicator->SetEndFound(FALSE);

					PopAttrState();
				}

				if (tok & 0x40)
				{
					PushTagL(m_tmp_buf, TRUE);
					EnqueueL(UNI_L(">"), WXSTRLEN(">"), buf_p, FALSE);
				}
				else
				{
					EnqueueL(UNI_L(" />"), WXSTRLEN(" />"), buf_p, FALSE);
					Commit2Buffer(buf_p);
				}
			}
			break;

		case WXToken::EXT_I_0:
		case WXToken::EXT_I_1:
		case WXToken::EXT_I_2:
		case WXToken::EXT_T_0:
		case WXToken::EXT_T_1:
		case WXToken::EXT_T_2:
		case WXToken::EXT_0:
		case WXToken::EXT_1:
		case WXToken::EXT_2:
			AppSpecialExtL(tok, &buf_p);
			break;

		case WXToken::PI:
			{
				if (!IsInAttrState())
				{
					EnqueueL(UNI_L("<?"), WXSTRLEN("<?"), NULL, FALSE);
					PushAttrState(tok);
				}

				if (!m_attr_parsing_indicator->GetEndFound())
					ParseAttrsL(&buf_p);

				m_attr_parsing_indicator->SetEndFound(FALSE);

				PopAttrState();

				EnqueueL(UNI_L("?>"), WXSTRLEN("?>"), buf_p, FALSE);
			}
			break;

		case WXToken::STR_T:
			{
				UINT32 idx = GetNextTokenL(&buf_p, TRUE);
				LookupAndEnqueueL(idx, buf_p);
			}
			break;

		case WXToken::WXOPAQUE:
			{
				if (m_doc_type != WBXML_INDEX_LAST)
				{
					const uni_char *val = m_content_handlers[m_doc_type]->OpaqueTextNodeHandlerL(&buf_p);
					if (val)
						EnqueueL(val, uni_strlen(val), buf_p, FALSE);
				}
			}
			break;

		default:
			{
				if (!IsInAttrState())
					AppSpecialTagL(tok, NULL);

				if (tok & 0x80)
				{
					if (!IsInAttrState())
						PushAttrState(tok);

					if (!m_attr_parsing_indicator->GetEndFound())
						ParseAttrsL(&buf_p);

					m_attr_parsing_indicator->SetEndFound(FALSE);

					PopAttrState();
				}

				if (tok & 0x40)
					EnqueueL(UNI_L(">"), WXSTRLEN(">"), buf_p, FALSE);
				else
					EnqueueL(UNI_L(" />"), WXSTRLEN(" />"), buf_p, FALSE);
			}
			break;
		}
	}
}

void
WBXML_Parser::ParseAttrsL(const char **buf)
{
	const char *buf_p = *buf;

	while (buf_p != m_in_buf_end)
	{
		UINT32 tok = GetNextTokenL(&buf_p);

		switch (tok)
		{
		case WXToken::SWITCH_PAGE:
			m_code_page = GetNextTokenL(&buf_p);
			break;

		case WXToken::END:
			m_attr_parsing_indicator->SetEndFound(TRUE);
			*buf = buf_p;
			if (m_attr_parsing_indicator->GetStartFound())
				EnqueueL(UNI_L("\""), WXSTRLEN("\""), buf_p, FALSE);
			return;

		case WXToken::LITERAL:
			{
				UINT32 idx = GetNextTokenL(&buf_p, TRUE);

				if (m_attr_parsing_indicator->GetStartFound()) // end the last attribute
					EnqueueL(UNI_L("\" "), WXSTRLEN("\" "), NULL, FALSE);
				else
					EnqueueL(UNI_L(" "), WXSTRLEN(" "), NULL, FALSE);

				LookupAndEnqueueL(idx, NULL);

				m_attr_parsing_indicator->SetStartFound(TRUE);
				EnqueueL(UNI_L("=\""), WXSTRLEN("=\""), buf_p, FALSE);
			}
			break;

		case WXToken::ENTITY:
			{
				UINT32 entity = GetNextTokenL(&buf_p, TRUE);

				EnsureTmpBufLenL(WXSTRLEN("&#;") + sizeof(UINT32) * 3 + 1);
				int len = uni_sprintf(m_tmp_buf, UNI_L("&#%d;"), entity);

				EnqueueL(m_tmp_buf, len, buf_p, FALSE);
			}
			break;

		case WXToken::EXT_I_0:
		case WXToken::EXT_I_1:
		case WXToken::EXT_I_2:
		case WXToken::EXT_T_0:
		case WXToken::EXT_T_1:
		case WXToken::EXT_T_2:
		case WXToken::EXT_0:
		case WXToken::EXT_1:
		case WXToken::EXT_2:
			{
				if (!m_attr_parsing_indicator->GetStartFound())
					LEAVE(WXParseStatus::NOT_WELL_FORMED);

				AppSpecialExtL(tok, &buf_p);
			}
			break;

		case WXToken::WXOPAQUE:
			{
				if (!m_attr_parsing_indicator->GetStartFound())
					LEAVE(WXParseStatus::NOT_WELL_FORMED);

				if (m_doc_type != WBXML_INDEX_LAST)
				{
					const uni_char *ext =
						m_content_handlers[m_doc_type]->OpaqueAttrHandlerL(&buf_p);

					if (ext)
						EnqueueL(ext, uni_strlen(ext), buf_p, FALSE);
				}
			}
			break;

		case WXToken::STR_I:
			{
				if (!m_attr_parsing_indicator->GetStartFound())
					LEAVE(WXParseStatus::NOT_WELL_FORMED);

				const char *str = buf_p;
				PassNextStringL(&buf_p);
				ConvertAndEnqueueStrL(str, buf_p - str - 1, buf_p);
			}
			break;

		case WXToken::STR_T:
			{
				if (!m_attr_parsing_indicator->GetStartFound())
					LEAVE(WXParseStatus::NOT_WELL_FORMED);

				UINT32 idx = GetNextTokenL(&buf_p, TRUE);
				LookupAndEnqueueL(idx, buf_p);
			}
			break;

		default:
			if (tok < 0x80)
			{
				BOOL start_found = m_attr_parsing_indicator->GetStartFound();
				m_attr_parsing_indicator->SetStartFound(TRUE);
				AppSpecialAttrStartL(tok, start_found, buf_p);
			}
			else
			{
				if (!m_attr_parsing_indicator->GetStartFound())
					LEAVE(WXParseStatus::NOT_WELL_FORMED);

				AppSpecialAttrValueL(tok, buf_p);
			}
			break;
		}
	}

	LEAVE(m_more ? WXParseStatus::NEED_GROW : WXParseStatus::UNEXPECTED_END);
}

void
WBXML_Parser::AppSpecialTagL(UINT32 tok, const char* commit_pos)
{
	const uni_char *tag = NULL;

	if (m_doc_type != WBXML_INDEX_LAST)
		tag = m_content_handlers[m_doc_type]->TagHandler(tok & 0x3f);

	int len = 0;
	if (tag)
	{
		EnsureTmpBufLenL(uni_strlen(tag) + 2);
		len = uni_sprintf(m_tmp_buf, UNI_L("<%s"), tag);
	}
	else
	{
		EnsureTmpBufLenL(WXSTRLEN("<tag") + 3);
		len = uni_sprintf(m_tmp_buf, UNI_L("<tag%X"), tok & 0x3f);
	}

	if (tok & 0x40)
		PushTagL(m_tmp_buf + 1, FALSE);

	EnqueueL(m_tmp_buf, len, commit_pos, FALSE);
}

void
WBXML_Parser::AppSpecialAttrStartL(UINT32 tok, BOOL already_in_attr, const char* commit_pos)
{
	const uni_char *attr = NULL;

	if (m_doc_type != WBXML_INDEX_LAST)
		attr = m_content_handlers[m_doc_type]->AttrStartHandler(tok);

	int len = 0;
	if (attr)
	{
		if (already_in_attr) // this should end the last attribute
		{
			EnsureTmpBufLenL(uni_strlen(attr) + WXSTRLEN("\" ") + 1);
			len = uni_sprintf(m_tmp_buf, UNI_L("\" %s"), attr);
		}
		else
		{
			EnsureTmpBufLenL(uni_strlen(attr) + WXSTRLEN(" ") + 1);
			len = uni_sprintf(m_tmp_buf, UNI_L(" %s"), attr);
		}
	}
	else
	{
		if (already_in_attr) // this should end the last attribute
		{
			EnsureTmpBufLenL(WXSTRLEN("\" attrNN=\"") + 1);
			len = uni_sprintf(m_tmp_buf, UNI_L("\" attr%X=\""), tok);
		}
		else
		{
			EnsureTmpBufLenL(WXSTRLEN(" attrNN=\"") + 1);
			len = uni_sprintf(m_tmp_buf, UNI_L(" attr%X=\""), tok);
		}
	}

	EnqueueL(m_tmp_buf, len, commit_pos, FALSE);
}

void
WBXML_Parser::AppSpecialAttrValueL(UINT32 tok, const char* commit_pos)
{
	const uni_char *val = NULL;

	if (m_doc_type != WBXML_INDEX_LAST)
		val = m_content_handlers[m_doc_type]->AttrValueHandler(tok);

	if (val)
		EnqueueL(val, uni_strlen(val), commit_pos, FALSE);
}

void
WBXML_Parser::AppSpecialExtL(UINT32 tok, const char **buf)
{
	const uni_char *ext = NULL;

	if (m_doc_type != WBXML_INDEX_LAST)
		ext = m_content_handlers[m_doc_type]->ExtHandlerL(tok, buf);

	if (ext)
		EnqueueL(ext, uni_strlen(ext), *buf, FALSE);
}

void WBXML_Parser::PushTagL(const uni_char *tag, BOOL need_quoting)
{
	OpStackAutoPtr<WBXML_TagElm> new_elm(OP_NEW_L(WBXML_TagElm, (need_quoting)));

	new_elm->SetTagNameL(tag);

	if (m_tag_stack)
		new_elm->Precede(m_tag_stack);

	m_tag_stack = new_elm.release();
}

void
WBXML_Parser::PopTagAndEnqueueL(const char* commit_pos)
{
	if (!m_tag_stack)
	{
		LEAVE(WXParseStatus::NOT_WELL_FORMED);
	}

	WBXML_TagElm *top_elm = (WBXML_TagElm*) m_tag_stack;
	const uni_char *tag = top_elm->GetTagName();

	uni_char *tmp = OP_NEWA_L(uni_char, WXSTRLEN("</") + uni_strlen(tag) + WXSTRLEN(">") + 1);

	uni_sprintf(tmp, UNI_L("</%s>"), tag);

	OP_WXP_STATUS p_stat = OpStatus::OK;
	TRAP(p_stat, EnqueueL(tmp, uni_strlen(tmp), 0, FALSE, FALSE));
	OP_DELETEA(tmp);

	if (OpStatus::IsError(p_stat) && p_stat != WXParseStatus::BUFFER_FULL)
		LEAVE(p_stat);

	m_tag_stack = m_tag_stack->GetNext();
	OP_DELETE(top_elm);

	Commit2Buffer(const_cast<char*>(commit_pos));

	if (p_stat == WXParseStatus::BUFFER_FULL) LEAVE(p_stat);
}

void
WBXML_Parser::PushAttrState(UINT32 tok)
{
	m_attr_parsing_indicator->Precede(m_tag_stack);

	m_tag_stack_committed = m_tag_stack = m_attr_parsing_indicator;
	m_attr_parsing_indicator->SetToken(tok);
	m_attr_parsing_indicator->SetStartFound(FALSE);
}

void
WBXML_Parser::PopAttrState()
{
	OP_ASSERT(m_tag_stack == m_attr_parsing_indicator);
	m_tag_stack_committed = m_tag_stack = m_tag_stack->GetNext();
}

void
WBXML_Parser::GrowOverflowBufferIfNeededL(int wanted_index)
{
	OP_ASSERT(m_overflow_buf_len == 0 || m_overflow_buf != 0);
	if (wanted_index < m_overflow_buf_len) //Buffer is large enough. Do nothing
		return;

	int new_length = MAX(256, static_cast<int>(wanted_index*1.2)+1); //Grow by 20% (and 1, just to make sure we grow even for small values..)(and allocate at least 256 uni_chars)
	uni_char* new_buffer = OP_NEWA(uni_char, new_length);
	if (!new_buffer)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	if (m_overflow_buf && m_overflow_buf_idx>0) //Restore any previous content
	{
		op_memcpy(new_buffer, m_overflow_buf, UNICODE_SIZE(m_overflow_buf_idx));
	}

	OP_DELETEA(m_overflow_buf);
	m_overflow_buf = new_buffer;
	m_overflow_buf_len = new_length;
}

OP_WXP_STATUS
WBXML_Parser::Parse(char *out_buf, int &out_buf_len, const char **buf, const char *buf_end, BOOL more_buffers)
{
	if (!buf || !*buf || *buf == buf_end && !(m_overflow_buf && m_overflow_buf_idx > 0))
	{
		out_buf_len = 0;
		return OpStatus::OK;
	}

	//Paste in overflowed data first
	int overflow_append_length = 0;
	if (m_overflow_buf && m_overflow_buf_idx>0)
	{	
		int out_buf_len_tmp = out_buf_len;
		overflow_append_length = MIN(out_buf_len_tmp, UNICODE_SIZE(m_overflow_buf_idx));
		int overflow_append_uni_length = UNICODE_DOWNSIZE(overflow_append_length);
		op_memcpy(out_buf, m_overflow_buf, overflow_append_length);
		out_buf += overflow_append_length;
		out_buf_len -= overflow_append_length;
		if (m_overflow_buf_idx > overflow_append_uni_length)
		{
			op_memmove(m_overflow_buf, m_overflow_buf + overflow_append_uni_length, 
				UNICODE_SIZE(m_overflow_buf_idx - overflow_append_uni_length));
		}
		m_overflow_buf_idx -= overflow_append_uni_length;
		if (m_overflow_buf_idx == 0)
		{
			OP_DELETEA(m_overflow_buf);
			m_overflow_buf = NULL;
			m_overflow_buf_len = 0;
		}
		else
		{
			more_buffers = TRUE;
		}
	}

	const char *orig_buf = *buf;
	m_out_buf = (uni_char*)out_buf;
	m_out_buf_len = UNICODE_DOWNSIZE(out_buf_len);
	m_more = more_buffers;
	m_in_buf_end = buf_end;

	if (!m_tmp_buf)
	{
		m_tmp_buf_len = 64;
		m_tmp_buf = OP_NEWA(uni_char, m_tmp_buf_len);

		if (!m_tmp_buf)
			return OpStatus::ERR_NO_MEMORY;
	}

	// put this here before the TRAPs to avoid 'clobbered' warning
	out_buf_len = overflow_append_length;

	OP_WXP_STATUS p_stat;
	// Parse the header of the WBXML document
	if (!m_header_parsed)
	{
		TRAP(p_stat, ParseHeaderL(buf));

		if (OpStatus::IsError(p_stat) || p_stat == WXParseStatus::NEED_GROW)
		{
			// don't return anything until entire header is parsed
			*buf = orig_buf;
			out_buf_len = 0;
			return p_stat;
		}
	}
	else
	{
		m_in_buf_committed = *buf;
		m_out_buf_committed = 0;
		m_out_buf_idx = 0;
	}

	// Parse the body of the WBXML document
	TRAP(p_stat, ParseTreeL(buf));

	while (m_tag_stack && m_tag_stack != m_tag_stack_committed)
	{
		WBXML_StackElm *tmp_tag = m_tag_stack->GetNext();
		OP_DELETE(m_tag_stack);
		m_tag_stack = tmp_tag;
	}

	out_buf_len += UNICODE_SIZE(m_out_buf_committed);
	*buf = m_in_buf_committed;

	return p_stat;
}

BOOL
WBXML_Parser::IsWml()
{
#ifdef WML_WBXML_SUPPORT
	return m_doc_type == WBXML_INDEX_WML;
#else // WML_WBXML_SUPPORT
	return FALSE;
#endif // WML_WBXML_SUPPORT
}

#endif // WBXML_SUPPORT
