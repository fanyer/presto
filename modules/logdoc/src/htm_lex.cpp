/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
*/

#include "core/pch.h"

#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/stdlib/include/double_format.h"

#define DEFINE_COLORS
#include "modules/logdoc/html_col.h"
#include "modules/url/url2.h"

#include "modules/logdoc/xml_ev.h"
#include "modules/logdoc/src/xml.h"
#include "modules/xmlutils/xmlutils.h"

#ifdef _WML_SUPPORT_
# include "modules/logdoc/wml.h"
#endif // _WML_SUPPORT_

#ifdef SVG_SUPPORT
# include "modules/svg/svg.h"
#endif // SVG_SUPPORT

#ifdef USE_UNICODE_SEGMENTATION
# include "modules/unicode/unicode_segmenter.h"
#endif

OP_STATUS LogdocXmlName::SetName(const XMLCompleteNameN& name)
{
	return m_name.Set(name);
}

OP_STATUS LogdocXmlName::SetName(const uni_char *qname, unsigned int qname_len, BOOL is_xml)
{
	if (!is_xml)
	{
		XMLCompleteNameN tmp_name(NULL, 0, NULL, 0, qname, qname_len);
		return m_name.Set(tmp_name);
	}

	XMLCompleteNameN tmp_name(qname, qname_len);
	return m_name.Set(tmp_name);
}

OP_STATUS LogdocXmlName::SetName(LogdocXmlName *src)
{
	return m_name.Set(src->GetName());
}

/*virtual*/ OP_STATUS
LogdocXmlName::CreateCopy(ComplexAttr **copy_to)
{
	*copy_to = OP_NEW(LogdocXmlName, ());
	if (*copy_to)
	{
		OP_STATUS oom = ((LogdocXmlName*)*copy_to)->SetName(m_name);
		if (OpStatus::IsMemoryError(oom))
		{
			OP_DELETE(*copy_to);
			*copy_to = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		return OpStatus::OK;
	}

	return OpStatus::ERR_NO_MEMORY;
}

/*virtual*/ OP_STATUS
LogdocXmlName::ToString(TempBuffer *buffer)
{
	const uni_char* prefix = m_name.GetPrefix();
	if (prefix)
	{
		RETURN_IF_MEMORY_ERROR(buffer->Append(prefix));
		RETURN_IF_MEMORY_ERROR(buffer->Append(':'));
	}

	return buffer->Append(m_name.GetLocalPart());
}

HTM_Lex::~HTM_Lex()
{
	OP_DELETEA(m_attr_array);
}

void
HTM_Lex::InitL()
{
	m_attr_array = OP_NEWA_L(HtmlAttrEntry, HtmlAttrEntriesMax); // ~36*100 = ~3.5KB
}

void HTM_Lex::FindAttrValue(int attr, HtmlAttrEntry* html_attrs, uni_char* &val_start, UINT &val_len)
{
	int i = 0;
	while (html_attrs[i].attr != ATTR_NULL)
	{
		HtmlAttrEntry* hae = &html_attrs[i++];
		if (hae->attr == attr)
		{
			val_start = (uni_char*)hae->value;
			val_len = hae->value_len;
			return;
		}
	}
	val_start = 0;
	val_len = 0;
}

void HTM_Lex::IntToHex(int val, uni_char &c1, uni_char &c2)
{
    int tmp_val = val >> 4;
    if( tmp_val < 10 )
        c1 = '0' + tmp_val;
    else
        c1 = 'a' + tmp_val - 10;

    tmp_val = val & 0x0F;
    if( tmp_val < 10 )
        c2 = '0' + tmp_val;
    else
        c2 = 'a' + tmp_val - 10;
}

/*static*/
Markup::AttrType HTM_Lex::GetAttr(const uni_char* str, int len, BOOL case_sensitive)
{
	if (len == 0)
		return Markup::HA_NULL;

	Markup::AttrType type;
	type = g_html5_name_mapper->GetAttrTypeFromName(str, case_sensitive, Markup::HTML);
	return type == Markup::HA_NULL ? Markup::HA_XML : type;
}

/*static*/
int HTM_Lex::GetXMLEventAttr(const uni_char* str, int len)
{
	// Note: if/when we no longer support namespaces in HTML, case_sensitive will
	// always be TRUE, and this extra code can be removed.

	switch (len)
	{
	case 5:
		if (uni_strncmp(str, "event", 5) == 0)
			return XML_EV_EVENT;
		else if (uni_strncmp(str, "phase", 5) == 0)
			return XML_EV_PHASE;
		break;

	case 6:
		if (uni_strncmp(str, "target", 6) == 0)
			return XML_EV_TARGET;
		break;

	case 7:
		if (uni_strncmp(str, "handler", 7) == 0)
			return XML_EV_HANDLER;
		break;

	case 8:
		if (uni_strncmp(str, "observer", 8) == 0)
			return XML_EV_OBSERVER;
		break;

	case 9:
		if (uni_strncmp(str, "propagate", 9) == 0)
			return XML_EV_PROPAGATE;
		break;

	case 13:
		if (uni_strncmp(str, "defaultAction", 13) == 0)
			return XML_EV_DEFAULTACTION;
		break;
	}

    return XML_EV_UNKNOWN;
}

COLORREF HTM_Lex::GetColValByName(const uni_char* str, int len, int &idx)
{
	if (len <= COLOR_MAX_SIZE)
	{
		int end_idx = COLOR_idx[len+1];
		for (idx=COLOR_idx[len]; idx<end_idx; idx++)
			if (uni_strni_eq_lower(str, _COLOR_+COLOR_name[idx], len) != 0)
				return COLOR_val[idx];
	}
	return USE_DEFAULT_COLOR;
}

COLORREF HTM_Lex::GetColIdxByName(const uni_char* str, int len)
{
	if (len <= COLOR_MAX_SIZE)
	{
		int end_idx = COLOR_idx[len+1];
		for (long idx=COLOR_idx[len]; idx<end_idx; idx++)
			if (uni_strni_eq_lower(str, _COLOR_+COLOR_name[idx], len) != 0)
				return idx | CSS_COLOR_KEYWORD_TYPE_x_color;
	}
	return USE_DEFAULT_COLOR;
}

COLORREF HTM_Lex::GetColValByIndex(COLORREF idx)
{
	if (COLOR_is_indexed(idx)) // named color
	{
		if ((idx & CSS_COLOR_KEYWORD_TYPE_ui_color) == CSS_COLOR_KEYWORD_TYPE_ui_color) // UI color
			return g_op_ui_info->GetUICSSColor(idx & CSS_COLOR_KEYWORD_TYPE_index);
		else // X11 color
			return COLOR_val[idx & CSS_COLOR_KEYWORD_TYPE_index];
	}

	return idx;
}

const uni_char* HTM_Lex::GetColNameByIndex(int idx)
{
	return (idx < COLOR_SIZE) ? _COLOR_+COLOR_name[idx] : NULL;
}

void HTM_Lex::GetRGBStringFromVal(COLORREF color, uni_char *buffer, BOOL hash_notation /*= FALSE*/)
{
	int red = OP_GET_R_VALUE(color);
	int green = OP_GET_G_VALUE(color);
	int blue = OP_GET_B_VALUE(color);
	int alpha = OP_GET_A_VALUE(color);

	if (alpha != 255)
	{
		double alpha_double = static_cast<double>(alpha/255.0);
		char buf[32]; // ARRAY OK 2011-11-07 sof
		OpDoubleFormat::ToPrecision(buf, alpha_double, 4);
		// Strip trailing zeroes in the decimal part
		char* period = op_strrchr(buf, '.');
		char* last_meaningful = period-1;
		char* c = period+1;
		while (*c)
		{
			if (*c == 'e' || *c == 'E')
			{
				op_memmove(last_meaningful+1, c, op_strlen(c));
				break;
			}
			else if (*c != '0')
				last_meaningful = c;
			c++;
		}
		int len = last_meaningful - buf + 1;
		uni_char alpha_str[32]; // ARRAY OK 2009-09-22 bratell
		make_doublebyte_in_buffer(buf, len, alpha_str, 32);

		uni_sprintf(buffer, UNI_L("rgba(%d, %d, %d, %s)"), red, green, blue, alpha_str);
	}
	else if (hash_notation)
	{
		int i = 0;

	    buffer[i++] = '#';

		IntToHex(red, buffer[i], buffer[i + 1]);
		IntToHex(green, buffer[i + 2], buffer[i + 3]);
		IntToHex(blue, buffer[i + 4], buffer[i + 5]);
	    buffer[i + 6] = '\0';
	}
	else
	{
		uni_sprintf(buffer, UNI_L("rgb(%d, %d, %d)"), red, green, blue);
	}
}


Markup::Type HTM_Lex::GetElementType(const uni_char *tok, int tok_len, NS_Type ns_type, BOOL case_sensitive)
{
	switch(ns_type)
	{
#ifdef XML_EVENTS_SUPPORT
	case NS_EVENT:
		return static_cast<Markup::Type>(uni_strncmp(UNI_L("listener"), tok, tok_len) == 0 ? XML_EV_LISTENER : XML_EV_UNKNOWN);
#endif // XML_EVENTS_SUPPORT

#ifdef SVG_SUPPORT
	case NS_SVG:
		return g_html5_name_mapper->GetTypeFromName(tok, tok_len, case_sensitive, Markup::SVG);
#endif // SVG_SUPPORT
#ifdef MATHML_ELEMENT_CODES
	case NS_MATHML:
		return g_html5_name_mapper->GetTypeFromName(tok, tok_len, case_sensitive, Markup::MATH);
#endif // MATHML_ELEMENT_CODES

	case NS_HTML:
#ifdef _WML_SUPPORT_
	case NS_WML:
#endif // _WML_SUPPORT_
	default:
		return g_html5_name_mapper->GetTypeFromName(tok, tok_len, case_sensitive, Markup::HTML);
	}
}

/*static*/
Markup::AttrType HTM_Lex::GetAttrType(const uni_char *tok, int tok_len, NS_Type ns_type, BOOL case_sensitive)
{
	if (tok_len == 0)
		return ATTR_NULL;

	switch(ns_type)
	{
#ifdef XML_EVENTS_SUPPORT
	case NS_EVENT:
		return static_cast<Markup::AttrType>(HTM_Lex::GetXMLEventAttr(tok, tok_len));
#endif // XML_EVENTS_SUPPORT
#ifdef XLINK_SUPPORT
	case NS_XLINK:
		return XLink_Lex::GetAttrType(tok, tok_len);
#endif // XLINK_SUPPORT
	case NS_XML:
		return XML_Lex::GetAttrType(tok, tok_len);
#ifdef SVG_SUPPORT
	case NS_SVG:
		return g_html5_name_mapper->GetAttrTypeFromName(tok, tok_len, case_sensitive, Markup::SVG);
#endif // SVG_SUPPORT
#ifdef MATHML_ELEMENT_CODES
	case NS_MATHML:
		return g_html5_name_mapper->GetAttrTypeFromName(tok, tok_len, case_sensitive, Markup::MATH);
#endif // MATHML_ELEMENT_CODES

	case NS_HTML:
#ifdef _WML_SUPPORT_
	case NS_WML:
#endif // _WML_SUPPORT_
	default:
		return g_html5_name_mapper->GetAttrTypeFromName(tok, tok_len, case_sensitive, Markup::HTML);
	}
}

/*static*/
Markup::AttrType HTM_Lex::GetAttrType(const uni_char *tok, NS_Type ns_type, BOOL case_sensitive)
{
	if (ns_type == NS_SVG)
		return g_html5_name_mapper->GetAttrTypeFromName(tok, case_sensitive, Markup::SVG);
	else if (ns_type == NS_MATHML)
		return g_html5_name_mapper->GetAttrTypeFromName(tok, case_sensitive, Markup::MATH);
	else
		return g_html5_name_mapper->GetAttrTypeFromName(tok, case_sensitive, Markup::HTML);
}

Markup::Type HTM_Lex::GetElementType(const uni_char *tok, NS_Type ns_type, BOOL case_sensitive)
{
	if (ns_type == NS_SVG)
		return g_html5_name_mapper->GetTypeFromName(tok, case_sensitive, Markup::SVG);
	else if (ns_type == NS_MATHML)
		return g_html5_name_mapper->GetTypeFromName(tok, case_sensitive, Markup::MATH);
	else
		return g_html5_name_mapper->GetTypeFromName(tok, case_sensitive, Markup::HTML);
}

// *buf_start == buf_end means that tag not finished.
// When tag is finished parsed, **buf_start == '>'
// Return TRUE if attribute found.

//
int HTM_Lex::GetAttrValue(uni_char** buf_start, uni_char* buf_end, HtmlAttrEntry* ha_list,
						  BOOL strict_html, HLDocProfile *hld_profile, BOOL can_grow,
						  NS_Type elm_ns, unsigned level, BOOL resolve_ns)
{
	register uni_char* buf_p = *buf_start;

	while (buf_p != buf_end && IsHTML5Space(*buf_p))
		buf_p++;

    ha_list->ns_idx = NS_IDX_HTML;
	ha_list->attr = ATTR_NULL;
	ha_list->is_id = FALSE;
	ha_list->is_specified = TRUE;
	ha_list->is_special = FALSE;
	uni_char* attr_start = buf_p;

	// Read past the attribute name
	// skip until either >, =, / or space (start of attr value, or end of tag/buffer):
	if (buf_p != buf_end && *buf_p != '>' && *buf_p != '/')
	{
		do
		{
			buf_p++;
		}
		while (buf_p != buf_end && *buf_p != '>' && !IsHTML5Space(*buf_p) && *buf_p != '=' && *buf_p != '/');
	}

	if (buf_p == buf_end && can_grow)
	{
		return HTM_Lex::ATTR_RESULT_NEED_GROW;
	}
	else if (attr_start != buf_end && *attr_start == '/')
	{
		OP_ASSERT(attr_start == buf_p);
		buf_p++; // Consume the slash
		*buf_start = buf_p;
#if 0
		// According to the HTML5 spec we should, if the next char is '>', "set the self-closing flag of the current tag token"
		// We don't have such a tag right now.Instead we will return here to parse '> and will then return
		// ATTR_RESULT_DONE
		if (buf_p == buf_end)
		{
			buf_start--; // Unconsume the / so that we can detect it again
			return can_grow ? HTM_Lex::ATTR_RESULT_NEED_GROW : HTM_Lex::ATTR_RESULT_DONE;
		}

		if (buf_p == '>')
		{
			// Set self-closing flag
			return HTM_Lex::ATTR_RESULT_DONE;
		}
#endif // 0
		return HTM_Lex::ATTR_RESULT_FOUND;
	}

    ha_list->name = attr_start;
    ha_list->name_len = buf_p - attr_start;

	// Scan for the equal sign or next attribute or end of tag since whitespace is allowed here
	while (buf_p != buf_end && IsHTML5Space(*buf_p))
		buf_p++;

	if (buf_p == buf_end && can_grow)
		return HTM_Lex::ATTR_RESULT_NEED_GROW;

	if (buf_p != buf_end && *buf_p == '=')
	{
		// parse value
		while (++buf_p != buf_end && IsHTML5Space(*buf_p)){}

		if (buf_p == buf_end && can_grow)
			return HTM_Lex::ATTR_RESULT_NEED_GROW;

		if (buf_p != buf_end)
		{
			uni_char quote_c = 0;
			if (*buf_p == '"' || *buf_p == '\'')
				quote_c = *buf_p++;

			ha_list->value = buf_p;

			if (strict_html)
			{
				while (buf_p != buf_end &&
						((!quote_c && *buf_p != '>' && !IsHTML5Space(*buf_p)) ||
						 (quote_c && *buf_p != quote_c)))
						 buf_p++;
			}
			else
			{
				while (buf_p != buf_end && *buf_p != '>' &&
						((!quote_c && !IsHTML5Space(*buf_p)) ||
						 (quote_c && *buf_p != quote_c)))
						 buf_p++;
			}

			if (buf_p == buf_end && can_grow) // a html-tag does always end with '>', tag not finished !
				return HTM_Lex::ATTR_RESULT_NEED_GROW;

			ha_list->value_len = buf_p - ha_list->value;
			if (quote_c && buf_p != buf_end && quote_c == *buf_p)
				buf_p++;
		}
	}
	else // attribute has no value
	{
		ha_list->value = 0;
		ha_list->value_len = 0;
	}

#ifdef USE_HTML_PARSER_FOR_XML
	if (hld_profile->IsXml())
	{
		XMLCompleteNameN attr_name(attr_start, ha_list->name_len);
		XMLNamespaceDeclaration::Reference& current_ns = hld_profile->GetCurrentNsDecl();

		if (resolve_ns)
		{
			OP_STATUS oom = XMLNamespaceDeclaration::ProcessAttribute(current_ns, attr_name, ha_list->value, ha_list->value_len, level);
			if (OpStatus::IsMemoryError(oom))
				hld_profile->SetIsOutOfMemory(TRUE);
		}

		XMLNamespaceDeclaration::ResolveName(current_ns, attr_name, TRUE);

		NS_Type attr_ns;
		if (attr_name.GetPrefixLength() == 0 && !attr_name.IsXMLNamespaces())
		{
			ha_list->ns_idx = NS_IDX_DEFAULT;
			attr_ns = elm_ns;
		}
		else
		{
			ha_list->ns_idx = attr_name.GetNsIndex();
			attr_ns = attr_name.GetNsType();
		}

		ha_list->attr = HTM_Lex::GetAttrType(attr_name.GetLocalPart(), attr_name.GetLocalPartLength(), attr_ns);
	}
	else
#endif // USE_HTML_PARSER_FOR_XML
		ha_list->attr = HTM_Lex::GetAttrType(ha_list->name, ha_list->name_len, NS_HTML);


	*buf_start = buf_p;
	return (ha_list->attr != ATTR_NULL || buf_p != buf_end && *buf_p != '>') ? HTM_Lex::ATTR_RESULT_FOUND : HTM_Lex::ATTR_RESULT_DONE;
}

const uni_char*	HTM_Lex::GetElementString(Markup::Type tag, int ns_idx, BOOL html_uppercase)
{
	if (Markup::HasNameEntry(tag))
	{
		if (ns_idx == NS_IDX_SVG)
			return g_html5_name_mapper->GetNameFromType(static_cast<Markup::Type>(tag), Markup::SVG, FALSE);
		else if (ns_idx == NS_IDX_MATHML)
			return g_html5_name_mapper->GetNameFromType(static_cast<Markup::Type>(tag), Markup::MATH, FALSE);
		else
			return g_html5_name_mapper->GetNameFromType(static_cast<Markup::Type>(tag), Markup::HTML, html_uppercase);
	}

	return NULL;
}

/*static*/ const uni_char*	HTM_Lex::GetAttributeString(Markup::AttrType attr, NS_Type ns_type)
{
	if (Markup::HasAttrNameEntry(attr))
	{
		if (ns_type == NS_SVG)
			return g_html5_name_mapper->GetAttrNameFromType(static_cast<Markup::AttrType>(attr), Markup::SVG, FALSE);
		else if (ns_type == NS_MATHML)
			return g_html5_name_mapper->GetAttrNameFromType(static_cast<Markup::AttrType>(attr), Markup::MATH, FALSE);
		else
			return g_html5_name_mapper->GetAttrNameFromType(static_cast<Markup::AttrType>(attr), Markup::HTML, FALSE);
	}

	return NULL;
}

const uni_char* HTM_Lex::GetTagString(Markup::Type tag)
{
	if (Markup::HasNameEntry(tag))
		return g_html5_name_mapper->GetNameFromType(static_cast<Markup::Type>(tag), Markup::HTML, FALSE);
	return NULL;
}

Markup::Type HTM_Lex::GetTag(const uni_char* str, int len, BOOL case_sensitive/*= FALSE*/)
{
	return g_html5_name_mapper->GetTypeFromName(str, len, case_sensitive, Markup::HTML);
}

#include "modules/logdoc/data/entities_len.h"
#include "modules/unicode/unicode_stringiterator.h"

/*static*/ BOOL
HTM_Lex::ScanBackIfEscapeChar(const uni_char* buf_start, uni_char** buf_end, BOOL allowed_to_read_next_char)
{
	// *buf_end is the first character after the buffer
	int i = 0;
	uni_char* buf_p = *buf_end;
	if (buf_p != buf_start)
	{
		buf_p--;
		while (buf_p != buf_start && *buf_p != ';' && *buf_p != ' ' && *buf_p != '&' && i < AMP_ESC_MAX_SIZE)
		{
			buf_p--;
			i++;
		}
	}

#ifdef USE_UNICODE_SEGMENTATION
	BOOL ret_val = FALSE;

	if (*buf_p != '&')
	{
		// Reset
		buf_p = *buf_end;
	}
	else
	{
		*buf_end = buf_p;
		allowed_to_read_next_char = TRUE;
		ret_val = TRUE;
	}

	// Now unscan one character to make sure the last character in the buffer
	// is complete (we do not know if the following buffer will contain a
	// mark that attaches to the last character of this buffer).
	if (buf_p > buf_start)
	{
		UnicodeStringIterator iter(buf_start, KAll,
			*buf_end - buf_start + (allowed_to_read_next_char ? 1 : 0));

		UnicodePoint last;

		if (allowed_to_read_next_char)
		{
			iter.Previous();
			last = iter.At();
		}
		else
			last = 0;

		while (!iter.IsAtBeginning())
		{
			iter.Previous();
			UnicodePoint up = iter.At();

			BOOL must_move_back;

			// Move back on stand-alone surrogates.
			if (Unicode::IsSurrogate(up))
			{
				must_move_back = TRUE;
			}
			// If we don't have a second character or only half of a surrogate
			// we try to make a decision based on one character.
			else if (!last || Unicode::IsSurrogate(last))
			{
				must_move_back = UnicodeSegmenter::MightBeFirstHalfOfGraphemeCluster(up);
			}
			else
			{
				must_move_back = !UnicodeSegmenter::IsGraphemeClusterBoundary(up, last);
			}

			if (!must_move_back)
			{
				// There is a break between the current and the previous character
				iter.Next();
				*buf_end = const_cast<uni_char*>(buf_start + iter.Index());

				// FIXME: Handle case where last character is an entity reference; we need
				// to decode it and check if it is a combining mark.
				return TRUE;
			}
			last = up;
		}

		// Ooops. There are no complete characters in this buffer. Just return
		// one character and hope for the best
		iter.Next();
		*buf_end = const_cast<uni_char *>(buf_start + iter.Index());
	}


	return ret_val;
#else
	if (*buf_p == '&')
	{
		*buf_end = buf_p;
		return TRUE;
	}
	else
		return FALSE;
#endif
}

class TempNsStackElm : public Link
{
public:
	const uni_char*	m_prefix;
	size_t			m_prefix_len;
	const uni_char*	m_ns;
	size_t			m_ns_len;
	int				m_level;

	TempNsStackElm(const uni_char *prefix, size_t prefix_len, const uni_char *ns, size_t ns_len, int level)
		: m_prefix(prefix), m_prefix_len(prefix_len), m_ns(ns), m_ns_len(ns_len), m_level(level) {};
};

#define CHECK_BUFP(statement) {statement;} if (buf_p >= buf_end) return can_grow ? HTM_LEX_MUST_GROW_BUF : HTM_LEX_END_OF_BUF;

int HTM_Lex::GetContentById2(const uni_char* id, uni_char *buf_start, int buf_len, uni_char*& content, int &content_len, int &token_ns_idx, BOOL can_grow)
{
	if (!id)
		return HTM_LEX_END_OF_BUF;

	int level = 0;
	int found_depth = -1;
	uni_char *found_start = NULL;
	int id_len = uni_strlen(id);
	uni_char *buf_p = buf_start;
	uni_char *buf_end = buf_start + buf_len;
	AutoDeleteHead ns_stack;

	while (buf_p < buf_end && level >= found_depth)
	{
		if (*buf_p == '<')
		{
			BOOL end_tag = FALSE;
			uni_char *tag_start = buf_p++;

			if (*buf_p == '/')
			{
				end_tag = TRUE;
				buf_p++;
			}

			if (*buf_p == '!')
			{
				buf_p++;

				if (buf_end - buf_p > 2 && *buf_p == '-' && buf_p[1] == '-')
				{
					buf_p += 2;
					while (buf_end - buf_p > 2 && *buf_p != '-' && buf_p[1] != '-') buf_p++;
				}
				else if (buf_end > buf_p && *buf_p == '[')
				{
					while (buf_end - buf_p > 2 && *buf_p != ']' && buf_p[1] != ']') buf_p++;
				}

				if (buf_end - buf_p <= 2)
					return can_grow ? HTM_LEX_MUST_GROW_BUF : HTM_LEX_END_OF_BUF;
			}
			else if (*buf_p != '?')
			{
				if (!end_tag)
					level++;

				if (found_start) // don't wanna check anything just fast forward to next
				{
					CHECK_BUFP( while (buf_p < buf_end && *buf_p != '>') buf_p++ );

					if (*buf_p == '>' && *(buf_p - 1) == '/')
						end_tag = TRUE;
				}
				else
				{
					CHECK_BUFP( while (buf_p < buf_end && *buf_p != '>' && !IsHTML5Space(*buf_p)) buf_p++ ); // tagname

					while (buf_p < buf_end && *buf_p != '>') // loop through the attributes
					{
						CHECK_BUFP( while (buf_p < buf_end && IsHTML5Space(*buf_p)) buf_p++ ); // skip spaces

						uni_char *attr_start = buf_p;
						CHECK_BUFP( while (buf_p < buf_end && *buf_p != '=' && *buf_p != '>' && !IsHTML5Space(*buf_p)) buf_p++ );

						size_t attr_len = buf_p - attr_start;

						BOOL is_id = (buf_p - attr_start == 2 && uni_strni_eq(attr_start, "ID", 2));
						BOOL is_xmlns = (attr_len >= 5 && uni_strni_eq(attr_start, "XMLNS", 5));

						CHECK_BUFP( while (buf_p < buf_end && IsHTML5Space(*buf_p)) buf_p++ ); // skip spaces before '='

						if (*buf_p == '=') // the attibute has a value - good
						{
							buf_p++;
							CHECK_BUFP( while (buf_p < buf_end && IsHTML5Space(*buf_p)) buf_p++ ); // skip spaces after '='

							uni_char quote_char = 0;
							if (*buf_p == '\'' || *buf_p == '"')
							{
								CHECK_BUFP( quote_char = *buf_p++ );
							}

							uni_char *attr_val = buf_p;

							CHECK_BUFP( while (buf_p < buf_end && ((quote_char && *buf_p != quote_char)
								|| (!quote_char && *buf_p != '>' && *buf_p != '/' && !IsHTML5Space(*buf_p)))) buf_p++ );

							if (is_id)
							{
								if (!found_start && id_len == buf_p - attr_val && uni_strncmp(attr_val, id, id_len) == 0)
								{
									found_start = tag_start; // we have found the correct element
									found_depth = level;
								}
							}
							else if (is_xmlns)
							{
								if (attr_start[5] == ':')
								{
									// add ns with prefix
									TempNsStackElm *tnse = OP_NEW(TempNsStackElm, (attr_start + 6, attr_len - 6, attr_val, buf_p - attr_val, level));
									if (!tnse)
										return OpStatus::ERR_NO_MEMORY;

									TempNsStackElm *top = (TempNsStackElm*)ns_stack.Last();
									if (top && top->m_level == level)
										tnse->Precede(top);
									else
										tnse->Into(&ns_stack);
								}
								else
								{
									// add default ns
									TempNsStackElm *tnse = OP_NEW(TempNsStackElm, (NULL, 0, attr_val, buf_p - attr_val, level));
									if (!tnse)
										return OpStatus::ERR_NO_MEMORY;
									tnse->Into(&ns_stack);
								}
							}

							if (quote_char && *buf_p == quote_char)
								buf_p++;
							else if (*buf_p == '/')
								end_tag = TRUE;
						}
					} // attribute loop
				}

				if (end_tag)
				{

					TempNsStackElm *top = (TempNsStackElm*)ns_stack.Last();
					while (top && top->m_level == level)
					{
						// pop namespaces
						top->Out();
						OP_DELETE(top);
						top = (TempNsStackElm*)ns_stack.Last();
					}

					level--;
				}
			}

			if (!found_start)
			{
				CHECK_BUFP( while (buf_p < buf_end && *buf_p != '>') buf_p++ );

				content = buf_p;
			}
		}

		buf_p++;
	}

	if (found_start)
	{
		if (level >= found_depth)
			return can_grow ? HTM_LEX_MUST_GROW_BUF : HTM_LEX_END_OF_BUF;

		content = found_start;
		content_len = buf_p - found_start;

		const uni_char *tag_start = buf_p = found_start + 1;
		while (buf_p < buf_end && !IsHTML5Space(*buf_p) && *buf_p != '>')
			buf_p++;

		return HTM_Lex::GetElementType(tag_start, buf_p - tag_start, NS_HTML, FALSE);
	}

	return can_grow ? HTM_LEX_MUST_GROW_BUF : HTM_LEX_END_OF_BUF;
}
