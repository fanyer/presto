/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
*/

#include "core/pch.h"

#include "modules/style/src/css_selector.h"
#include "modules/probetools/probepoints.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/layout/box/box.h"
#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/doc/html_doc.h"
#include "modules/style/src/css_pseudo_stack.h"
#include "modules/style/css_matchcontext.h"
#ifdef MEDIA_HTML_SUPPORT
# include "modules/media/mediatrack.h"
#endif // MEDIA_HTML_SUPPORT

/*
	nmstart 	[_a-z]|{nonascii}|{escape}
	nonascii	[^\0-\177]
	unicode 	\\[0-9a-f]{1,6}(\r\n|[ \n\r\t\f])?
	escape 	{unicode}|\\[^\n\r\f0-9a-f]
	nmchar 	[_a-z0-9-]|{nonascii}|{escape}
*/

enum NmAccept {
	NmDash = 0,
	NmStart = 1,
	NmChar = 2
};

static inline BOOL NeedEscape(uni_char c, NmAccept accept)
{
	return !(c >= 'A' && c <= 'Z' ||
			 c >= 'a' && c <= 'z' ||
			 c == '_' ||
			 c >= 0x80 ||
			 accept == NmChar && c >= '0' && c <= '9' ||
			 accept != NmStart && c == '-');
}

static inline uni_char HexDigitToChar(uni_char digit)
{
	return digit + ((digit < 10) ? '0' : ('a' - 10));
}

OP_STATUS EscapeIdent(const uni_char* ident, TempBuffer* buf)
{
	const uni_char* tmp = ident;

	NmAccept accept(NmDash);

	while (tmp)
	{
		const uni_char* next_escape = tmp;

		while (*next_escape)
		{
			if (NeedEscape(*next_escape, accept))
				break;
			else
			{
				if (accept == NmDash && *next_escape == '-')
					accept = NmStart;
				else
					accept = NmChar;
			}

			next_escape++;
		}

		RETURN_IF_ERROR(buf->Append(tmp, *next_escape ? next_escape - tmp : (size_t)-1));

		uni_char escape_char = *next_escape++;

		if (escape_char)
		{
			tmp = next_escape;
			OP_ASSERT(escape_char < 128);
			uni_char escape[5] = { '\\' };  /* ARRAY OK 2010-03-19 rune */
			int i = 1;
			if (escape_char < 0x20 || escape_char >= '0' && escape_char <= '9')
			{
				if (escape_char > 0xf)
					escape[i++] = HexDigitToChar(escape_char >> 4);

				escape[i++] = HexDigitToChar(escape_char & 0xf);
				escape[i++] = ' ';
			}
			else
				escape[i++] = escape_char;

			escape[i] = 0;
			RETURN_IF_ERROR(buf->Append(escape));
		}
		else
			tmp = NULL;
	}
	return OpStatus::OK;
}

CSS_SelectorAttribute::CSS_SelectorAttribute(unsigned short typ, unsigned short att, uni_char* val, int ns_index)
	: m_attr(att), m_type(typ), m_string(val), m_ns_idx(ns_index)
{
}

CSS_SelectorAttribute::CSS_SelectorAttribute(unsigned short typ, uni_char* name_and_val, int ns_index)
	: m_attr(Markup::HA_XML), m_type(typ), m_string(name_and_val), m_ns_idx(ns_index)
{
}

CSS_SelectorAttribute::CSS_SelectorAttribute(unsigned short typ, unsigned short att)
	: m_attr(att), m_type(typ), m_string(0), m_ns_idx(NS_IDX_DEFAULT)
{
}

CSS_SelectorAttribute::CSS_SelectorAttribute(unsigned short type, ReferencedHTMLClass* ref)
	: m_attr(1), m_type(type), m_class(ref)
{
}

CSS_SelectorAttribute::CSS_SelectorAttribute(unsigned short type, int a, int b)
	: m_attr(0), m_type(type), m_string(0)
{
	if (a > AB_COEFF_MAX)
		a = AB_COEFF_MAX;
	else if (a < AB_COEFF_MIN)
		a = AB_COEFF_MIN;
	if (b > AB_COEFF_MAX)
		b = AB_COEFF_MAX;
	else if (b < AB_COEFF_MIN)
		b = AB_COEFF_MIN;

	m_nth_expr = MAKE_AB_COEFF(a, b);
}

CSS_SelectorAttribute::~CSS_SelectorAttribute()
{
	if (GetType() == CSS_SEL_ATTR_TYPE_CLASS && IsClassRef())
		m_class->UnRef();
	else
		OP_DELETEA(m_string);
}

OP_STATUS CSS_SelectorAttribute::GetSelectorText(TempBuffer* buf)
{
	if (IsNegated())
		RETURN_IF_ERROR(buf->Append(":not("));

	unsigned short type = GetType();
	if (type == CSS_SEL_ATTR_TYPE_ID)
	{
		RETURN_IF_ERROR(buf->Append("#"));
		RETURN_IF_ERROR(EscapeIdent(GetId(), buf));
	}
	else if (type == CSS_SEL_ATTR_TYPE_CLASS)
	{
		RETURN_IF_ERROR(buf->Append("."));
		RETURN_IF_ERROR(EscapeIdent(GetClass(), buf));
	}
	else if (type == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS || type == CSS_SEL_ATTR_TYPE_PSEUDO_PAGE)
	{
		OpString pseudo_str;

		const char* pseudo_pfx = (type == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS && IsPseudoElement(GetPseudoClass())) ? "::" : ":";
		RETURN_IF_ERROR(buf->Append(pseudo_pfx));

		const char* pseudo_cstr;
		if (type == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS)
			pseudo_cstr = CSS_GetPseudoClassString(GetPseudoClass());
		else
			pseudo_cstr = CSS_GetPseudoPageString(GetPseudoClass());
		RETURN_IF_ERROR(pseudo_str.Set(pseudo_cstr));
		pseudo_str.MakeLower();
		RETURN_IF_ERROR(buf->Append(pseudo_str.CStr()));
		if (type == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS)
		{
			if (GetPseudoClass() == PSEUDO_CLASS_LANG)
			{
				RETURN_IF_ERROR(buf->Append('('));
				RETURN_IF_ERROR(EscapeIdent(GetValue(), buf));
				RETURN_IF_ERROR(buf->Append(')'));
			}
#ifdef MEDIA_HTML_SUPPORT
			else if (GetPseudoClass() == PSEUDO_CLASS_CUE && m_string)
			{
				RETURN_IF_ERROR(buf->Append('('));
				RETURN_IF_ERROR(buf->Append(m_string));
				RETURN_IF_ERROR(buf->Append(')'));
			}
#endif // MEDIA_HTML_SUPPORT
		}
	}
	else if (type == CSS_SEL_ATTR_TYPE_ELM)
	{
		// Must always be negated.
		OP_ASSERT(IsNegated());

		int ns_idx = GetNsIdx();
		if (ns_idx >= 0)
		{
			NS_Element* ns_elm = g_ns_manager->GetElementAt(ns_idx);
			if (ns_elm->GetPrefix())
			{
				RETURN_IF_ERROR(EscapeIdent(ns_elm->GetPrefix(), buf));
				RETURN_IF_ERROR(buf->Append("|"));
			}
		}
		Markup::Type elm_type = (Markup::Type)GetAttr();
		if (elm_type == Markup::HTE_ANY)
		{
			RETURN_IF_ERROR(buf->Append("*"));
		}
		else if (elm_type == Markup::HTE_UNKNOWN)
		{
			RETURN_IF_ERROR(EscapeIdent(GetValue(), buf));
		}
		else
		{
			RETURN_IF_ERROR(buf->Append(HTM_Lex::GetElementString(elm_type, GetNsIdx() < 0 ? NS_IDX_HTML : GetNsIdx(), FALSE)));
		}
	}
	else if (type >= CSS_SEL_ATTR_TYPE_NTH_CHILD && type <= CSS_SEL_ATTR_TYPE_NTH_LAST_OF_TYPE)
	{
		RETURN_IF_ERROR(buf->Append(":"));
		const char* string = NULL;
		switch (type)
		{
		case CSS_SEL_ATTR_TYPE_NTH_CHILD:
			string = "nth-child";
			break;
		case CSS_SEL_ATTR_TYPE_NTH_OF_TYPE:
			string = "nth-of-type";
			break;
		case CSS_SEL_ATTR_TYPE_NTH_LAST_CHILD:
			string = "nth-last-child";
			break;
		case CSS_SEL_ATTR_TYPE_NTH_LAST_OF_TYPE:
			string = "nth-last-of-type";
			break;
		}
		RETURN_IF_ERROR(buf->Append(string));

		// Append linear function of n.
		int a, b;
		char n_func[64]; /* ARRAY OK 2009-02-12 rune */
		n_func[0] = '(';
		char* str_start = &n_func[1];
		GET_AB_COEFF(m_nth_expr, a, b);
		if (a != 0)
		{
			if (a == -1)
				*str_start++ = '-';
			else if (a != 1)
			{
				op_itoa(a, str_start, 10);
				while (*str_start)
					str_start++;
			}
			*str_start++ = 'n';
		}
		if (b || a == 0)
		{
			if (b > 0 && a != 0)
				*str_start++ = '+';
			op_itoa(b, str_start, 10);
			while (*str_start)
				str_start++;
		}
		*str_start++ = ')';
		*str_start++ = '\0';

		RETURN_IF_ERROR(buf->Append(n_func));
	}
	else // Attribute selector
	{
		RETURN_IF_ERROR(buf->Append("["));
		int ns_idx = GetNsIdx();
		if (ns_idx >= 0)
		{
			NS_Element* ns_elm = g_ns_manager->GetElementAt(ns_idx);
			const uni_char* prefix = ns_elm->GetPrefix();
			if (prefix && *prefix != 0)
			{
				RETURN_IF_ERROR(EscapeIdent(ns_elm->GetPrefix(), buf));
				RETURN_IF_ERROR(buf->Append("|"));
			}
		}

		const uni_char* attr_name = NULL;
		const uni_char* attr_val = GetValue();
		if (GetAttr() == Markup::HA_XML)
		{
			attr_name = attr_val;
			if (type != CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED)
				attr_val += uni_strlen(attr_name) + 1;
		}
		else
			attr_name = HTM_Lex::GetAttrString(static_cast<Markup::AttrType>(GetAttr()));

		RETURN_IF_ERROR(EscapeIdent(attr_name, buf));
		if (type != CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED)
		{
			if (type == CSS_SEL_ATTR_TYPE_HTMLATTR_EQUAL)
				RETURN_IF_ERROR(buf->Append("="));
			else if (type == CSS_SEL_ATTR_TYPE_HTMLATTR_INCLUDES)
				RETURN_IF_ERROR(buf->Append("~="));
			else if (type == CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH)
				RETURN_IF_ERROR(buf->Append("|="));
			else if (type == CSS_SEL_ATTR_TYPE_HTMLATTR_PREFIXMATCH)
				RETURN_IF_ERROR(buf->Append("^="));
			else if (type == CSS_SEL_ATTR_TYPE_HTMLATTR_SUFFIXMATCH)
				RETURN_IF_ERROR(buf->Append("$="));
			else if (type == CSS_SEL_ATTR_TYPE_HTMLATTR_SUBSTRMATCH)
				RETURN_IF_ERROR(buf->Append("*="));

			RETURN_IF_ERROR(CSS::FormatQuotedString(buf, attr_val));
		}
		RETURN_IF_ERROR(buf->Append("]"));
	}
	if (IsNegated())
		RETURN_IF_ERROR(buf->Append(")"));

	return OpStatus::OK;
}

#ifdef MEDIA_HTML_SUPPORT
CSS_SelectorAttribute* CSS_SelectorAttribute::Clone()
{
	CSS_SelectorAttribute* clone = NULL;
	uni_char* text = NULL;
	unsigned short type = GetType();
	switch (type)
	{
	case CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED:
		if (GetAttr() == ATTR_XML)
		{
			text = UniSetNewStr(GetValue());
			if (!text)
				return NULL;

			clone = OP_NEW(CSS_SelectorAttribute, (type, text, GetNsIdx()));
		}
		else
		{
			clone = OP_NEW(CSS_SelectorAttribute, (type, GetAttr(), NULL, GetNsIdx()));
		}
		break;

	case CSS_SEL_ATTR_TYPE_HTMLATTR_EQUAL:
	case CSS_SEL_ATTR_TYPE_HTMLATTR_INCLUDES:
	case CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH:
	case CSS_SEL_ATTR_TYPE_HTMLATTR_PREFIXMATCH:
	case CSS_SEL_ATTR_TYPE_HTMLATTR_SUFFIXMATCH:
	case CSS_SEL_ATTR_TYPE_HTMLATTR_SUBSTRMATCH:
		{
			text = UniSetNewStr(GetValue());
			if (!text)
				return NULL;

			if (GetAttr() == ATTR_XML)
				clone = OP_NEW(CSS_SelectorAttribute, (type, text, GetNsIdx()));
			else
				clone = OP_NEW(CSS_SelectorAttribute, (type, GetAttr(), text, GetNsIdx()));
		}
		break;

	case CSS_SEL_ATTR_TYPE_PSEUDO_PAGE:
		{
			text = UniSetNewStr(GetValue());
			if (!text)
				return NULL;

			clone = OP_NEW(CSS_SelectorAttribute, (type, text));
		}
		break;

	case CSS_SEL_ATTR_TYPE_PSEUDO_CLASS:
		{
			if (GetPseudoClass() == PSEUDO_CLASS_LANG)
			{
				text = UniSetNewStr(GetValue());
				if (!text)
					return NULL;
			}

			clone = OP_NEW(CSS_SelectorAttribute, (type, GetPseudoClass(), text));
		}
		break;

	case CSS_SEL_ATTR_TYPE_CLASS:
		if (IsClassRef())
		{
			ReferencedHTMLClass* class_ref = GetClassRef();
			clone = OP_NEW(CSS_SelectorAttribute, (type, class_ref));

			if (clone)
				class_ref->Ref();
		}
		else
		{
			text = UniSetNewStr(GetValue());
			if (!text)
				return NULL;

			clone = OP_NEW(CSS_SelectorAttribute, (type, 0, text));
		}
		break;

	case CSS_SEL_ATTR_TYPE_ID:
		{
			text = UniSetNewStr(GetValue());
			if (!text)
				return NULL;

			clone = OP_NEW(CSS_SelectorAttribute, (type, 0, text));
		}
		break;

	case CSS_SEL_ATTR_TYPE_ELM:
		{
			if (GetValue())
			{
				text = UniSetNewStr(GetValue());
				if (!text)
					return NULL;
			}

			clone = OP_NEW(CSS_SelectorAttribute, (type, GetAttr(), text, GetNsIdx()));
		}
		break;

	case CSS_SEL_ATTR_TYPE_NTH_CHILD:
	case CSS_SEL_ATTR_TYPE_NTH_OF_TYPE:
	case CSS_SEL_ATTR_TYPE_NTH_LAST_CHILD:
	case CSS_SEL_ATTR_TYPE_NTH_LAST_OF_TYPE:
		clone = OP_NEW(CSS_SelectorAttribute, (type, GetAttr()));
		break;

	default:
		OP_ASSERT(!"Unhandled selector attribute");
	}

	if (clone)
	{
		if (IsNegated())
			clone->SetNegated();
	}
	else
	{
		OP_DELETEA(text);
	}
	return clone;
}
#endif

CSS_SimpleSelector::CSS_SimpleSelector(int el) :
	m_packed_init(0),
	m_elm_name(0),
	m_ns_idx(NS_IDX_ANY_NAMESPACE)
{
	m_packed.elm = el;
	if (m_ns_idx >= 0)
		g_ns_manager->IncNsRefCount(m_ns_idx);
}

CSS_SimpleSelector::~CSS_SimpleSelector()
{
	m_sel_attr_list.Clear();
	if (m_packed.universal == NOT_UNIVERSAL)
		OP_DELETEA(m_elm_name);
	if (m_ns_idx >= 0)
		g_ns_manager->DecNsRefCount(m_ns_idx);
}

void CSS_SimpleSelector::SetNameSpaceIdx(int val)
{
	if (m_ns_idx >= 0)
		g_ns_manager->DecNsRefCount(m_ns_idx);

	if (val >= 0)
		g_ns_manager->IncNsRefCount(val);

	m_ns_idx = val;
}

void CSS_SimpleSelector::AddAttribute(CSS_SelectorAttribute* new_attr)
{
	new_attr->Into(&m_sel_attr_list);
}

void CSS_SimpleSelector::SetElmName(uni_char* val)
{
	m_elm_name = val;
}

#define SEL_COUNT_MASK (UINT_MAX >> 1)
#define SEL_COUNT_NEGATED (UINT_MAX & ~SEL_COUNT_MASK)

void CSS_SimpleSelector::CountIdsAndAttrs(unsigned short& id_count,
										  unsigned short& attr_count,
										  unsigned short& elm_count,
										  unsigned int& has_class,
										  unsigned int& has_id,
										  BOOL& has_fullscreen)

{
	unsigned short i=0, a=0, e=0;
	BOOL has_neg_elm = FALSE;

	if (GetElm() != Markup::HTE_ANY)
		e++;

	CSS_SelectorAttribute* sel_attr = GetFirstAttr();
	while (sel_attr)
	{
		if (sel_attr->GetType() == CSS_SEL_ATTR_TYPE_ID)
		{
			i++;
			if (sel_attr->IsNegated())
				has_id |= SEL_COUNT_NEGATED;
			else
				has_id++;
		}
		else if (sel_attr->GetType() != CSS_SEL_ATTR_TYPE_ELM)
		{
			a++;
			if (sel_attr->GetType() == CSS_SEL_ATTR_TYPE_CLASS)
			{
				if (sel_attr->IsNegated())
					has_class |= SEL_COUNT_NEGATED;
				else
					has_class++;
			}
			else
				if (sel_attr->GetType() == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS &&
					(sel_attr->GetPseudoClass() == PSEUDO_CLASS_FULLSCREEN ||
					 sel_attr->GetPseudoClass() == PSEUDO_CLASS_FULLSCREEN_ANCESTOR))
					has_fullscreen = TRUE;
		}
		else
		{
			if (sel_attr->GetAttr() != Markup::HTE_ANY)
				e++;
			if (sel_attr->IsNegated())
				has_neg_elm = TRUE;
		}

		sel_attr = sel_attr->Suc();
	}

	if (e == 0 && m_ns_idx == NS_IDX_ANY_NAMESPACE && !has_neg_elm)
	{
		if (has_class == 1 && a == 1 && i == 0)
		{
			CSS_SelectorAttribute* attr = GetFirstAttr();

			if (attr->IsClassRef())
				m_universal_class_ref = attr->GetClassRef();
			else
			{
				m_universal_class = attr->GetClass();
				SetUniversalIsString();
			}

			SetUniversal(UNIVERSAL_CLASS);
		}
		else if (has_id == 1 && i == 1 && a == 0)
		{
			SetUniversal(UNIVERSAL_ID);
			m_universal_id = GetFirstAttr()->GetId();
		}
		else if (a == 0 && i == 0)
			SetUniversal(UNIVERSAL);
	}

	if (i) id_count += i;
	if (a) attr_count += a;
	if (e) elm_count += e;
}

BOOL CSS_SimpleSelector::MatchType(CSS_MatchContextElm* context_elm, BOOL is_xml) const
{
	OP_PROBE4(OP_PROBE_CSS_SIMPLESELECTOR_MATCHTYPE);

	// Match namespace
	if (GetNameSpaceIdx() != NS_IDX_ANY_NAMESPACE)
	{
		int element_ns_idx = context_elm->GetNsIdx();
		if (GetNameSpaceIdx() != NS_IDX_NOT_FOUND)
		{
			if (!g_ns_manager->CompareNsType(GetNameSpaceIdx(), element_ns_idx))
				return FALSE;
			if (g_ns_manager->GetNsTypeAt(element_ns_idx) == NS_USER)
			{
				if (uni_stricmp(g_ns_manager->GetElementAt(element_ns_idx)->GetUri(), g_ns_manager->GetElementAt(GetNameSpaceIdx())->GetUri()) != 0)
					return FALSE;
			}
		}
		else
			return FALSE;
	}

	Markup::Type selector_element_type = GetElm();

	// this matches any element
	if (selector_element_type == Markup::HTE_ANY)
		return TRUE;

	int element_type = context_elm->Type();

	// At this point, we know that we either have a selector namespace of NS_IDX_ANY_NAMESPACE,
	// or that the namespace of the selector and the element matches.
	if (element_type != Markup::HTE_UNKNOWN && selector_element_type != Markup::HTE_UNKNOWN
		&& (GetNameSpaceIdx() >= NS_IDX_DEFAULT || context_elm->GetNsIdx() == NS_IDX_HTML || g_ns_manager->GetNsTypeAt(context_elm->GetNsIdx()) == NS_HTML))
		return selector_element_type == element_type;

	const uni_char* selector_tag_name = GetElmName();

	if (!selector_tag_name)
		selector_tag_name = HTM_Lex::GetElementString(selector_element_type, GetNameSpaceIdx() < 0 ? NS_IDX_HTML : GetNameSpaceIdx(), FALSE);

	const uni_char* tag_name = context_elm->HtmlElement()->GetTagName();

	return tag_name && selector_tag_name && ((is_xml && uni_str_eq(tag_name, selector_tag_name)) || (!is_xml && uni_stricmp(tag_name, selector_tag_name) == 0));
}

static BOOL IsAttrCaseSensitive(int attr)
{
	switch (attr)
	{
	case Markup::HA_DIR:
	case Markup::HA_REL:
	case Markup::HA_REV:
	case Markup::HA_AXIS:
	case Markup::HA_FACE:
	case Markup::HA_LANG:
	case Markup::HA_LINK:
	case Markup::HA_TEXT:
	case Markup::HA_TYPE:
	case Markup::HA_ALIGN:
	case Markup::HA_ALINK:
	case Markup::HA_CLEAR:
	case Markup::HA_COLOR:
	case Markup::HA_DEFER:
	case Markup::HA_FRAME:
	case Markup::HA_MEDIA:
	case Markup::HA_RULES:
	case Markup::HA_SCOPE:
	case Markup::HA_SHAPE:
	case Markup::HA_VLINK:
	case Markup::HA_ACCEPT:
	case Markup::HA_METHOD:
	case Markup::HA_NOHREF:
	case Markup::HA_NOWRAP:
	case Markup::HA_TARGET:
	case Markup::HA_VALIGN:
	case Markup::HA_BGCOLOR:
	case Markup::HA_CHARSET:
	case Markup::HA_CHECKED:
	case Markup::HA_COMPACT:
	case Markup::HA_DECLARE:
	case Markup::HA_ENCTYPE:
	case Markup::HA_NOSHADE:
	case Markup::HA_CODETYPE:
	case Markup::HA_DISABLED:
	case Markup::HA_HREFLANG:
	case Markup::HA_LANGUAGE:
	case Markup::HA_MULTIPLE:
	case Markup::HA_NORESIZE:
	case Markup::HA_READONLY:
	case Markup::HA_SELECTED:
	case Markup::HA_DIRECTION:
	case Markup::HA_SCROLLING:
	case Markup::HA_VALUETYPE:
	case Markup::HA_HTTP_EQUIV:
	case Markup::HA_ACCEPT_CHARSET:
		return FALSE;

	default:
		return TRUE;
	}
}

BOOL CSS_SimpleSelector::MatchAttrs(CSS_MatchContext* context, CSS_MatchContextElm* context_elm, int& failed_pseudo) const
{
	OP_PROBE4(OP_PROBE_CSS_SIMPLESELECTOR_MATCHATTRS);

	int local_failed_pseudo = 0;
	int push_bits = 0;
	int quirk_push_bits = 0;

	BOOL failed = FALSE;
	BOOL has_id_class_or_attr = FALSE;

	HTML_Element* he = context_elm->HtmlElement();

	CSS_SelectorAttribute* sel_attr = GetFirstAttr();
	while (sel_attr)
	{
		unsigned short sel_attr_type = sel_attr->GetType();
		BOOL neg = sel_attr->IsNegated();

		switch (sel_attr_type)
		{
		case CSS_SEL_ATTR_TYPE_ID:
			{
				OP_PROBE4(OP_PROBE_CSS_MATCHATTRS_ID);

				has_id_class_or_attr = TRUE;

				const uni_char* elm_id = context_elm->GetIdAttribute();

				// Case sensitive id selectors in strict mode (according to spec.)
				// Case insensitive in quirks mode (like IE).
				// rune@opera.com 2002-12-03
				if ((!elm_id || !sel_attr->GetId()
					|| (context->StrictMode() && uni_strcmp(elm_id, sel_attr->GetId()) != 0)
					|| (!context->StrictMode() && uni_stricmp(elm_id, sel_attr->GetId()) != 0)) != neg)
					failed = TRUE;
			}
			break;

		case CSS_SEL_ATTR_TYPE_CLASS:
			{
				has_id_class_or_attr = TRUE;
				BOOL match = FALSE;

				const ClassAttribute* class_attr = context_elm->GetClassAttribute();
				if (class_attr)
				{
					if (sel_attr->IsClassRef())
						match = class_attr->MatchClass(sel_attr->GetClassRef());
					else
						match = class_attr->MatchClass(sel_attr->GetClass(), context->StrictMode());
				}

				if (match == neg)
					failed = TRUE;
			}
			break;

		case CSS_SEL_ATTR_TYPE_HTMLATTR_DEFINED:
			{
				has_id_class_or_attr = TRUE;

				if (sel_attr->GetAttr() == Markup::HA_XML)
				{
					int sel_ns_idx = sel_attr->GetNsIdx();
					if ((-1 == he->FindAttrIndex(Markup::HA_XML, sel_attr->GetValue(), sel_ns_idx, context->IsXml(), TRUE)) != neg)
						failed = TRUE;
				}
				else
				{
					unsigned short elm_attr = sel_attr->GetAttr();
					if (he->HasAttr(elm_attr, sel_attr->GetNsIdx()) == neg)
						failed = TRUE;
				}
			}
			break;

		case CSS_SEL_ATTR_TYPE_HTMLATTR_EQUAL:
		case CSS_SEL_ATTR_TYPE_HTMLATTR_INCLUDES:
		case CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH:
		case CSS_SEL_ATTR_TYPE_HTMLATTR_PREFIXMATCH:
		case CSS_SEL_ATTR_TYPE_HTMLATTR_SUFFIXMATCH:
		case CSS_SEL_ATTR_TYPE_HTMLATTR_SUBSTRMATCH:
			{
				has_id_class_or_attr = TRUE;

				const uni_char* he_attr_val = NULL;
				const uni_char* attr_val = sel_attr->GetValue();
				BOOL case_sensitive = context->IsXml();
				BOOL attr_failed = TRUE;

				if (!attr_val || !*attr_val && !(sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_EQUAL || sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH))
					goto attr_match_failed;

				if (sel_attr->GetAttr() == Markup::HA_XML)
				{
					int sel_ns_idx = sel_attr->GetNsIdx();
					if (sel_ns_idx == NS_IDX_DEFAULT && !context->IsXml())
					{
						case_sensitive = IsAttrCaseSensitive(HTM_Lex::GetAttrType(attr_val, NS_HTML, FALSE));
					}
					int idx = he->FindAttrIndex(Markup::HA_XML, attr_val, sel_ns_idx, context->IsXml(), TRUE);
					if (idx != -1)
					{
						g_styleTempBuffer->Clear();
						he_attr_val = he->GetAttrValue(attr_val, he->GetAttrNs(idx), Markup::HTE_ANY, g_styleTempBuffer);
					}
					attr_val += uni_strlen(attr_val) + 1;
				}
				else
				{
					unsigned short elm_attr = sel_attr->GetAttr();
					case_sensitive = IsAttrCaseSensitive(elm_attr);
					if (elm_attr == Markup::HA_ID)
						he_attr_val = context_elm->GetIdAttribute();
					else
					{
						if (sel_attr->GetNsIdx() == NS_IDX_ANY_NAMESPACE)
						{
							const uni_char* attr_name = HTM_Lex::GetAttrString(static_cast<Markup::AttrType>(elm_attr));
							he_attr_val = he->GetAttrValue(attr_name, NS_IDX_ANY_NAMESPACE, Markup::HTE_ANY, g_styleTempBuffer);
						}
						else
							he_attr_val = he->GetAttrValue(elm_attr, sel_attr->GetNsIdx(), Markup::HTE_ANY, FALSE, g_styleTempBuffer);
					}
				}

				if (!he_attr_val)
					goto attr_match_failed;

				if (sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_EQUAL)
				{
					if (case_sensitive)
					{
						if (uni_strcmp(attr_val, he_attr_val) != 0)
							goto attr_match_failed;
					}
					else if (uni_stricmp(attr_val, he_attr_val) != 0)
						goto attr_match_failed;
				}
				else if (sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_PREFIXMATCH ||
						 sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_SUFFIXMATCH ||
						 sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH)
				{
					size_t attr_len = uni_strlen(attr_val);
					if (sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_SUFFIXMATCH)
					{
						size_t he_attr_len = uni_strlen(he_attr_val);
						if (he_attr_len < attr_len)
							goto attr_match_failed;
						he_attr_val += he_attr_len - attr_len;
					}

					if (case_sensitive)
					{
						if (uni_strncmp(he_attr_val, attr_val, attr_len) != 0)
							goto attr_match_failed;
					}
					else if (uni_strnicmp(he_attr_val, attr_val, attr_len) != 0)
						goto attr_match_failed;

					if (sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_DASHMATCH && uni_strlen(he_attr_val) > attr_len && he_attr_val[attr_len] != '-')
						goto attr_match_failed;
				}
				else if (sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_SUBSTRMATCH)
				{
					if (case_sensitive)
					{
						if (uni_strstr(he_attr_val, attr_val) == NULL)
							goto attr_match_failed;
					}
					else if (uni_stristr(he_attr_val, attr_val) == NULL)
						goto attr_match_failed;
				}
				else // CSS_SEL_ATTR_TYPE_HTMLATTR_INCLUDES
				{
					OP_ASSERT(sel_attr_type == CSS_SEL_ATTR_TYPE_HTMLATTR_INCLUDES);
					const uni_char* sep = UNI_L(" \x9\xa\xc\xd");
					uni_char* buf = (uni_char*) g_memory_manager->GetTempBuf2();
					if (uni_strlen(he_attr_val) < UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2Len()))
						uni_strcpy(buf, he_attr_val);
					else
					{
						uni_strncpy(buf, he_attr_val, UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2Len()-1));
						buf[UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2Len()-1)] = '\0';
					}

					uni_char* val = uni_strtok(buf, sep);
					if (!val)
						goto attr_match_failed;

					while (val)
					{
						if (case_sensitive)
						{
							if (uni_strcmp(val, attr_val) == 0)
								break;
						}
						else
							if (uni_stricmp(val, attr_val) == 0)
								break;
						val = uni_strtok(NULL, sep);
					}
					if (!val)
						goto attr_match_failed;
				}
				attr_failed = FALSE;
attr_match_failed:
				if (attr_failed != neg)
					failed = TRUE;
			}
			break;

		case CSS_SEL_ATTR_TYPE_PSEUDO_CLASS:
			{
				unsigned short pseudo_class = sel_attr->GetPseudoClass();

				switch (pseudo_class)
				{
				case PSEUDO_CLASS_VISITED:
				case PSEUDO_CLASS_LINK:
					{
						context_elm->CacheLinkState(context->Document());

						if (!context_elm->IsLink())
						{
							if (!neg)
								failed = TRUE;
						}
						else
						{
							BOOL match_visited = (context_elm->IsVisited() != neg);
							if (pseudo_class == PSEUDO_CLASS_VISITED)
							{
								push_bits |= CSS_PSEUDO_CLASS_VISITED;
								if (!match_visited)
								{
									local_failed_pseudo |= CSS_PSEUDO_CLASS_VISITED;
								}
							}
							else
							{
								push_bits |= CSS_PSEUDO_CLASS_LINK;
								if (match_visited)
								{
									local_failed_pseudo |= CSS_PSEUDO_CLASS_LINK;
								}
							}
						}
					}
					break;

				case PSEUDO_CLASS_ACTIVE:
					{
						context_elm->CacheDynamicState();
						BOOL active_cand = context->StrictMode() || GetElm() != Markup::HTE_ANY;
						if (!active_cand)
						{
							context_elm->CacheLinkState(context->Document());
							active_cand = context_elm->IsLink();
						}
						if (active_cand)
							push_bits |= CSS_PSEUDO_CLASS_ACTIVE;
						else
							quirk_push_bits |= CSS_PSEUDO_CLASS_ACTIVE;

						if (neg == context_elm->IsActivated())
							local_failed_pseudo |= CSS_PSEUDO_CLASS_ACTIVE;
					}
					break;

				case PSEUDO_CLASS_HOVER:
					{
						context_elm->CacheDynamicState();
						BOOL hover_cand = context->StrictMode() || GetElm() != Markup::HTE_ANY;
						if (!hover_cand)
						{
							context_elm->CacheLinkState(context->Document());
							hover_cand = context_elm->IsLink();
						}
						if (hover_cand)
							push_bits |= CSS_PSEUDO_CLASS_HOVER;
						else
							quirk_push_bits |= CSS_PSEUDO_CLASS_HOVER;

						BOOL is_hovered = context_elm->IsHovered();
#ifdef DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS
						if (is_hovered)
							context_elm->SetIsHoveredWithMatch();
#endif // DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS
						if (neg == is_hovered)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_HOVER;
					}
					break;

				case PSEUDO_CLASS_FOCUS:
					context_elm->CacheDynamicState();
					push_bits |= CSS_PSEUDO_CLASS_FOCUS;
					{
						BOOL is_focused = context_elm->IsFocused();
#ifdef DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS
						if (is_focused)
							context_elm->SetIsFocusedWithMatch();
#endif // DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS
						if (neg == is_focused)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_FOCUS;
					}
					break;

				case PSEUDO_CLASS__O_PREFOCUS:
					if (he->IsFocusable(context->Document()))
						push_bits |= CSS_PSEUDO_CLASS__O_PREFOCUS;
					{
						BOOL pre_focused = he->IsPreFocused();
#if defined DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS && defined SELECT_TO_FOCUS_INPUT
						if (pre_focused)
							context_elm->SetIsPreFocusedWithMatch();
#endif // DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS && SELECT_TO_FOCUS_INPUT
						if (neg == pre_focused)
							local_failed_pseudo |= CSS_PSEUDO_CLASS__O_PREFOCUS;
					}
					break;

				case PSEUDO_CLASS_FIRST_LINE:
					if (context->PseudoElm() != ELM_PSEUDO_FIRST_LINE)
						local_failed_pseudo |= CSS_PSEUDO_CLASS_FIRST_LINE;
					break;

				case PSEUDO_CLASS_FIRST_LETTER:
					if (context->PseudoElm() != ELM_PSEUDO_FIRST_LETTER)
						local_failed_pseudo |= CSS_PSEUDO_CLASS_FIRST_LETTER;
					break;

				case PSEUDO_CLASS_BEFORE:
					if (context->PseudoElm() != ELM_PSEUDO_BEFORE)
						local_failed_pseudo |= CSS_PSEUDO_CLASS_BEFORE;
					break;

				case PSEUDO_CLASS_AFTER:
					if (context->PseudoElm() != ELM_PSEUDO_AFTER)
						local_failed_pseudo |= CSS_PSEUDO_CLASS_AFTER;
					break;

#ifdef MEDIA_HTML_SUPPORT
				case PSEUDO_CLASS_CUE:
					if ((context_elm->Type() == Markup::HTE_VIDEO && context_elm->GetNsType() == NS_HTML) == neg)
						failed = TRUE;
					break;

				case PSEUDO_CLASS_PAST:
					if ((context_elm->IsCueElement() &&
						 MediaCueDisplayState::GetTimeState(he) == CUE_TIMESTATE_PAST) == neg)
						failed = TRUE;
					break;

				case PSEUDO_CLASS_FUTURE:
					if ((context_elm->IsCueElement() &&
						 MediaCueDisplayState::GetTimeState(he) == CUE_TIMESTATE_FUTURE) == neg)
						failed = TRUE;
					break;
#endif // MEDIA_HTML_SUPPORT

				case PSEUDO_CLASS_ROOT:
					if ((he == context->Document()->GetLogicalDocument()->GetDocRoot()) == neg)
						failed = TRUE;
					break;

				case PSEUDO_CLASS_EMPTY:
					{
						if (context->MarkPseudoBits())
							he->SetUpdatePseudoElm();

						if (!context->Document()->IsParsed() && !he->GetEndTagFound())
							failed = TRUE;
						else
						{
							HTML_Element* child = he->FirstChildActualStyle();

							while (child && (!Markup::IsRealElement(child->Type()) && (child->Type() != Markup::HTE_TEXTGROUP && child->Type() != Markup::HTE_TEXT || child->GetTextContentLength() == 0)))
								child = child->SucActualStyle();

							if ((child == NULL) == neg)
								failed = TRUE;
						}
					}
					break;

				case PSEUDO_CLASS_ONLY_CHILD:
					{
						CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
						if (parent)
						{
							if (context->MarkPseudoBits())
								parent->HtmlElement()->SetUpdatePseudoElm();

							if (!context->Document()->IsParsed() && !parent->HtmlElement()->GetEndTagFound())
								failed = TRUE;
							else
							{
								HTML_Element* child = parent->HtmlElement()->FirstChildActualStyle();

								while (child && (!Markup::IsRealElement(child->Type()) || child == he))
									child = child->SucActualStyle();

								if ((child == NULL) == neg)
									failed = TRUE;
							}
						}
						else if (!neg)
							failed = TRUE;
					}
					break;

				case PSEUDO_CLASS_ONLY_OF_TYPE:
					{
						CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
						if (parent)
						{
							if (context->MarkPseudoBits())
								parent->HtmlElement()->SetUpdatePseudoElm();

							if (!context->Document()->IsParsed() && !parent->HtmlElement()->GetEndTagFound())
								failed = TRUE;
							else
							{
								HTML_Element* child = parent->HtmlElement()->FirstChildActualStyle();
								Markup::Type elm_type = context_elm->Type();
								NS_Type ns_type = context_elm->GetNsType();
								const uni_char* elm_name = elm_type == Markup::HTE_UNKNOWN ? he->GetTagName() : NULL;

								while (child && (child == he || !MatchElmType(child, elm_type, ns_type, elm_name, context->IsXml())))
									child = child->SucActualStyle();

								if ((child == NULL) == neg)
									failed = TRUE;
							}
						}
						else if (!neg)
							failed = TRUE;
					}
					break;

				case PSEUDO_CLASS_TARGET:
					{
						HTML_Document* doc = context->Document()->GetHtmlDocument();
						if ((doc && he == doc->GetURLTargetElement()) == neg)
							failed = TRUE;
					}
					break;

				case PSEUDO_CLASS_FIRST_CHILD:
					{
						CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
						if (parent)
						{
							HTML_Element* child = he->PredActualStyle();

							while (child && !Markup::IsRealElement(child->Type()))
								child = child->PredActualStyle();

							if ((child == NULL) == neg)
								failed = TRUE;
							context->Document()->GetHLDocProfile()->GetCSSCollection()->SetMatchFirstChild();
						}
						else if (!neg)
							failed = TRUE;
					}
					break;

				case PSEUDO_CLASS_FIRST_OF_TYPE:
					{
						CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
						if (parent)
						{
							if (context->MarkPseudoBits())
								parent->HtmlElement()->SetUpdatePseudoElm();

							HTML_Element* child = parent->HtmlElement()->FirstChildActualStyle();
							Markup::Type elm_type = context_elm->Type();
							NS_Type ns_type = context_elm->GetNsType();
							const uni_char* elm_name = elm_type == Markup::HTE_UNKNOWN ? he->GetTagName() : NULL;

							while (child && !MatchElmType(child, elm_type, ns_type, elm_name, context->IsXml()))
								child = child->SucActualStyle();

							if ((child == he) == neg)
								failed = TRUE;
						}
						else if (!neg)
							failed = TRUE;
					}
					break;

				case PSEUDO_CLASS_LAST_CHILD:
					{
						CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
						if (parent)
						{
							if (context->MarkPseudoBits())
								parent->HtmlElement()->SetUpdatePseudoElm();

							if (!context->Document()->IsParsed() && !parent->HtmlElement()->GetEndTagFound())
								failed = TRUE;
							else
							{
								HTML_Element* child = parent->HtmlElement()->LastChildActualStyle();

								while (child && !Markup::IsRealElement(child->Type()))
									child = child->PredActualStyle();

								if ((child == he) == neg)
									failed = TRUE;
							}
						}
						else if (!neg)
							failed = TRUE;
					}
					break;

				case PSEUDO_CLASS_LAST_OF_TYPE:
					{
						CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
						if (parent)
						{
							if (context->MarkPseudoBits())
								parent->HtmlElement()->SetUpdatePseudoElm();

							if (!context->Document()->IsParsed() && !parent->HtmlElement()->GetEndTagFound())
								failed = TRUE;
							else
							{
								HTML_Element* child = parent->HtmlElement()->LastChildActualStyle();
								Markup::Type elm_type = context_elm->Type();
								NS_Type ns_type = context_elm->GetNsType();
								const uni_char* elm_name = elm_type == Markup::HTE_UNKNOWN ? he->GetTagName() : NULL;

								while (child && !MatchElmType(child, elm_type, ns_type, elm_name, context->IsXml()))
									child = child->PredActualStyle();

								if ((child == he) == neg)
									failed = TRUE;
							}
						}
						else if (!neg)
							failed = TRUE;
					}
					break;

				case PSEUDO_CLASS_LANG:
					{
						const uni_char* cur_lang = he->GetCurrentLanguage();
						const uni_char* sel_lang = sel_attr->GetValue();
						if (cur_lang && sel_lang)
						{
							const uni_char* match = uni_stristr(cur_lang, sel_lang);
							if (match == cur_lang)
							{
								uni_char next_char = *(cur_lang+uni_strlen(sel_lang));
								if (neg == (next_char == '\0' || next_char == '-'))
									failed = TRUE;
							}
							else if (!neg)
								failed = TRUE;
						}
						else if (!neg)
							failed = TRUE;
					}
					break;

				case PSEUDO_CLASS_SELECTION:
					local_failed_pseudo |= CSS_PSEUDO_CLASS_SELECTION;
					break;

				case PSEUDO_CLASS_VALID:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_VALID;
						if (((CSS_PSEUDO_CLASS_VALID & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_VALID;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_CHECKED:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_CHECKED;
						if (((CSS_PSEUDO_CLASS_CHECKED & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_CHECKED;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_DEFAULT:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_DEFAULT;
						if (((CSS_PSEUDO_CLASS_DEFAULT & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_DEFAULT;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_INVALID:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_INVALID;
						if (((CSS_PSEUDO_CLASS_INVALID & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_INVALID;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_IN_RANGE:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_IN_RANGE;
						if (((CSS_PSEUDO_CLASS_IN_RANGE & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_IN_RANGE;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_OPTIONAL:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_OPTIONAL;
						if (((CSS_PSEUDO_CLASS_OPTIONAL & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_OPTIONAL;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_REQUIRED:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_REQUIRED;
						if (((CSS_PSEUDO_CLASS_REQUIRED & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_REQUIRED;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_READ_ONLY:
					push_bits |= CSS_PSEUDO_CLASS_READ_ONLY;
					if (he->IsFormElement())
					{
						if (((CSS_PSEUDO_CLASS_READ_ONLY & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_READ_ONLY;
					}
					else if (neg)
						local_failed_pseudo |= CSS_PSEUDO_CLASS_READ_ONLY;
					break;
				case PSEUDO_CLASS_READ_WRITE:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_READ_WRITE;
						if (((CSS_PSEUDO_CLASS_READ_WRITE & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_READ_WRITE;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_OUT_OF_RANGE:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_OUT_OF_RANGE;
						if (((CSS_PSEUDO_CLASS_OUT_OF_RANGE & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_OUT_OF_RANGE;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_INDETERMINATE:
					if (he->IsFormElement())
					{
						push_bits |= CSS_PSEUDO_CLASS_INDETERMINATE;
						if (((CSS_PSEUDO_CLASS_INDETERMINATE & he->GetFormValue()->GetMarkedPseudoClasses()) != 0) == neg)
							local_failed_pseudo |= CSS_PSEUDO_CLASS_INDETERMINATE;
					}
					else
						failed = !neg;
					break;
				case PSEUDO_CLASS_ENABLED:
				case PSEUDO_CLASS_DISABLED:
					{
						BOOL is_disabled;
						if (he->IsFormElement())
						{
							is_disabled = (he->GetFormValue()->GetMarkedPseudoClasses() & CSS_PSEUDO_CLASS_DISABLED) != 0;
						}
						else if (he->IsMatchingType(Markup::HTE_FIELDSET, NS_HTML))
						{
							is_disabled = he->IsDisabled(context->Document());
						}
						else if (he->IsMatchingType(Markup::HTE_OPTGROUP, NS_HTML) || he->IsMatchingType(Markup::HTE_OPTION, NS_HTML))
						{
							// Loop up to the closest select, look at its cached flag and add the
							// disabled attributes on options and optgroups to it.
							is_disabled = FALSE;
							HTML_Element* elm = he;
							while (elm)
							{
								if (elm->IsMatchingType(Markup::HTE_SELECT, NS_HTML))
								{
									is_disabled = (elm->GetFormValue()->GetMarkedPseudoClasses() & CSS_PSEUDO_CLASS_DISABLED) != 0;
									break;
								}
								if ((elm->IsMatchingType(Markup::HTE_OPTGROUP, NS_HTML) ||
									elm->IsMatchingType(Markup::HTE_OPTION, NS_HTML)) && elm->GetDisabled())
								{
									is_disabled = TRUE;
									break;
								}
								elm = elm->Parent();
							}
						}
						else
						{
							failed = !neg;
							break;
						}
						if (pseudo_class == PSEUDO_CLASS_ENABLED)
						{
							push_bits |= CSS_PSEUDO_CLASS_ENABLED;
							if (is_disabled != neg)
								local_failed_pseudo |= CSS_PSEUDO_CLASS_ENABLED;
						}
						else
						{
							push_bits |= CSS_PSEUDO_CLASS_DISABLED;
							if (is_disabled == neg)
								local_failed_pseudo |= CSS_PSEUDO_CLASS_DISABLED;
						}
					}
					break;
				case PSEUDO_CLASS_FULLSCREEN:
					if (context_elm->IsFullscreenElement() == neg)
						failed = TRUE;
					break;
				case PSEUDO_CLASS_FULLSCREEN_ANCESTOR:
					if (context_elm->IsFullscreenAncestor() == neg)
						failed = TRUE;
					break;

				default:
					OP_ASSERT(!"Unknown pseudo class!");
					break;
				}
			}
			break;

		case CSS_SEL_ATTR_TYPE_ELM:
			{
				int element_type = context_elm->Type();
				int element_ns_idx = context_elm->GetNsIdx();
				Markup::Type selector_element_type = static_cast<Markup::Type>(sel_attr->GetAttr());
				int selector_ns_idx = sel_attr->GetNsIdx();

				// Match namespace
				if (selector_ns_idx == NS_IDX_ANY_NAMESPACE ||
					selector_ns_idx != NS_IDX_NOT_FOUND && g_ns_manager->CompareNsType(selector_ns_idx, element_ns_idx) &&
					(g_ns_manager->GetNsTypeAt(element_ns_idx) != NS_USER ||
					 uni_stricmp(g_ns_manager->GetElementAt(element_ns_idx)->GetUri(), g_ns_manager->GetElementAt(selector_ns_idx)->GetUri()) == 0))
				{
					// this matches any element
					if (selector_element_type == Markup::HTE_ANY)
					{
						failed = TRUE;
						break;
					}

					// At this point, we know that we either have a selector namespace of NS_IDX_ANY_NAMESPACE,
					// or that the namespace of the selector and the element matches.
					if (element_type != Markup::HTE_UNKNOWN && selector_element_type != Markup::HTE_UNKNOWN
						&& (selector_ns_idx >= NS_IDX_DEFAULT || element_ns_idx == NS_IDX_HTML || g_ns_manager->GetNsTypeAt(element_ns_idx) == NS_HTML))
					{
						if (selector_element_type == element_type)
							failed = TRUE;
					}
					else
					{
						const uni_char* selector_tag_name = sel_attr->GetValue();

						if (!selector_tag_name)
							selector_tag_name = HTM_Lex::GetElementString(selector_element_type, selector_ns_idx < 0 ? NS_IDX_HTML : selector_ns_idx, FALSE);

						const uni_char* tag_name = he->GetTagName();
						BOOL case_sensitive = context->IsXml();
						if (tag_name && selector_tag_name && ((case_sensitive && uni_str_eq(tag_name, selector_tag_name)) || (!case_sensitive && uni_stricmp(tag_name, selector_tag_name) == 0)))
							failed = TRUE;
					}
				}
			}
			break;

		case CSS_SEL_ATTR_TYPE_NTH_CHILD:
			{
				OP_PROBE4(OP_PROBE_CSS_MATCHATTRS_NTH_CHILD);

				CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
				if (parent)
				{
					if (context->MarkPseudoBits())
						parent->HtmlElement()->SetUpdatePseudoElm();

					unsigned int count = context->GetNthCount(context_elm, CSS_NthCache::NTH_CHILD);
					if (sel_attr->MatchNFunc(count) == neg)
						failed = TRUE;
				}
				else if (!neg)
					failed = TRUE;
			}
			break;

		case CSS_SEL_ATTR_TYPE_NTH_OF_TYPE:
			{
				CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
				if (parent)
				{
					if (context->MarkPseudoBits())
						parent->HtmlElement()->SetUpdatePseudoElm();

					unsigned int count = context->GetNthCount(context_elm, CSS_NthCache::NTH_OF_TYPE, context->IsXml());
					if (sel_attr->MatchNFunc(count) == neg)
						failed = TRUE;
				}
				else if (!neg)
					failed = TRUE;
			}
			break;

		case CSS_SEL_ATTR_TYPE_NTH_LAST_CHILD:
			{
				CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
				if (parent)
				{
					if (context->MarkPseudoBits())
						parent->HtmlElement()->SetUpdatePseudoElm();

					if (!context->Document()->IsParsed() && !parent->HtmlElement()->GetEndTagFound())
						failed = TRUE;
					else
					{
						unsigned int count = context->GetNthCount(context_elm, CSS_NthCache::NTH_LAST_CHILD);
						if (sel_attr->MatchNFunc(count) == neg)
							failed = TRUE;
					}
				}
				else if (!neg)
					failed = TRUE;
			}
			break;

		case CSS_SEL_ATTR_TYPE_NTH_LAST_OF_TYPE:
			{
				CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
				if (parent)
				{
					if (context->MarkPseudoBits())
						parent->HtmlElement()->SetUpdatePseudoElm();

					if (!context->Document()->IsParsed() && !parent->HtmlElement()->GetEndTagFound())
						failed = TRUE;
					else
					{
						unsigned int count = context->GetNthCount(context_elm, CSS_NthCache::NTH_LAST_OF_TYPE, context->IsXml());
						if (sel_attr->MatchNFunc(count) == neg)
							failed = TRUE;
					}
				}
				else if (!neg)
					failed = TRUE;
			}
			break;

		default:
			OP_ASSERT(!"Unknown selector attribute type!");
			break;
		}

		if (local_failed_pseudo && !context->MarkPseudoBits())
			failed = TRUE;

		if (failed)
			break;

		sel_attr = sel_attr->Suc();
	}

	if (sel_attr == NULL)
	{
		if (context->MarkPseudoBits())
		{
			if (!context->StrictMode() && quirk_push_bits && has_id_class_or_attr)
				push_bits |= quirk_push_bits;

			if (push_bits)
				g_css_pseudo_stack->Push(he, push_bits);
		}

		failed_pseudo = local_failed_pseudo;

		// If we don't mark pseudo bits, we either break on
		// local_failed_pseudo (with sel_attr != NULL) or
		// we have a true match (failed_pseudo == 0).
		OP_ASSERT(context->MarkPseudoBits() || !failed_pseudo);
	}
	else
		failed_pseudo = 0;

	return (sel_attr == NULL && !failed_pseudo);
}

OP_STATUS CSS_SimpleSelector::GetSelectorText(const CSS* stylesheet, TempBuffer* buf)
{
	BOOL ns_txt = FALSE;
	int ns_idx = GetNameSpaceIdx();
	if (ns_idx >= 0)
	{
		NS_Element* ns_elm = g_ns_manager->GetElementAt(ns_idx);
		if (ns_elm->GetPrefix())
		{
			RETURN_IF_ERROR(buf->Append(ns_elm->GetPrefix()));
			RETURN_IF_ERROR(buf->Append("|"));
			ns_txt = TRUE;
		}
	}
	else if (stylesheet->GetDefaultNameSpaceIdx() != NS_IDX_NOT_FOUND && ns_idx == NS_IDX_ANY_NAMESPACE)
	{
		RETURN_IF_ERROR(buf->Append("*|"));
		ns_txt = TRUE;
	}

	Markup::Type elm_type = GetElm();
	if (elm_type == Markup::HTE_ANY && (!GetFirstAttr() || ns_txt))
	{
		RETURN_IF_ERROR(buf->Append("*"));
	}
	else if (elm_type == Markup::HTE_UNKNOWN)
	{
		RETURN_IF_ERROR(EscapeIdent(GetElmName(), buf));
	}
	else
	{
		RETURN_IF_ERROR(buf->Append(HTM_Lex::GetElementString(elm_type, GetNameSpaceIdx() < 0 ? NS_IDX_HTML : GetNameSpaceIdx(), FALSE)));
	}

	CSS_SelectorAttribute* sel_attr = GetFirstAttr();
	while (sel_attr)
	{
		RETURN_IF_ERROR(sel_attr->GetSelectorText(buf));
		sel_attr = sel_attr->Suc();
	}

	return OpStatus::OK;
}

#ifdef MEDIA_HTML_SUPPORT
CSS_SimpleSelector* CSS_SimpleSelector::Clone()
{
	CSS_SimpleSelector* clone = OP_NEW(CSS_SimpleSelector, (m_packed.elm));
	if (!clone)
		return NULL;

	clone->m_packed_init = m_packed_init;
	clone->SetNameSpaceIdx(m_ns_idx);

	// Clone selector attributes
	CSS_SelectorAttribute* sel_attr = GetFirstAttr();
	while (sel_attr)
	{
		CSS_SelectorAttribute* sel_attr_clone = sel_attr->Clone();
		if (!sel_attr_clone)
		{
			OP_DELETE(clone);
			return NULL;
		}

		clone->AddAttribute(sel_attr_clone);

		sel_attr = sel_attr->Suc();
	}

	unsigned short id_count, attr_count, elm_count;
	unsigned int has_class, has_id;
	BOOL has_fullscreen;
	clone->CountIdsAndAttrs(id_count, attr_count, elm_count, has_class, has_id, has_fullscreen);

	return clone;
}

void CSS_Selector::Prepend(Head& simple_sels)
{
	CSS_SimpleSelector* first_simple_sel = static_cast<CSS_SimpleSelector*>(simple_sels.First());
	OP_ASSERT(first_simple_sel);
	first_simple_sel->SetCombinator(CSS_COMBINATOR_DESCENDANT);

	m_simple_selectors.Append(&simple_sels);
}

void CSS_Selector::PrependSimpleSelector(CSS_SimpleSelector* sel)
{
	CSS_SimpleSelector* first_simple_sel = FirstSelector();
	OP_ASSERT(first_simple_sel);
	first_simple_sel->SetCombinator(CSS_COMBINATOR_DESCENDANT);

	sel->IntoStart(&m_simple_selectors);
}

BOOL CSS_Selector::HasPastOrFuturePseudo()
{
	if (CSS_SimpleSelector* sel = FirstSelector())
		if (CSS_SelectorAttribute* sel_attr = sel->GetFirstAttr())
			if (sel_attr->GetType() == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS)
			{
				unsigned short pseudo_class = sel_attr->GetPseudoClass();
				return pseudo_class == PSEUDO_CLASS_PAST || pseudo_class == PSEUDO_CLASS_FUTURE;
			}

	return FALSE;
}

#endif // MEDIA_HTML_SUPPORT

void CSS_Selector::AddSimpleSelector(CSS_SimpleSelector* simple_sel, unsigned short combinator)
{
	simple_sel->SetCombinator(combinator);
	simple_sel->Into(&m_simple_selectors);
}

void CSS_Selector::CalculateSpecificity()
{
	unsigned short a=0, b=0, c=0;
	CSS_SimpleSelector* sel = FirstSelector();

	// Must be at least one simple selector.
	OP_ASSERT(sel);

	// Cache pseudo element type
	CSS_SelectorAttribute* last_attr = sel->GetLastAttr();
	if (last_attr && last_attr->GetType() == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS)
	{
		unsigned short pseudo_class = last_attr->GetPseudoClass();
		if (pseudo_class == PSEUDO_CLASS_BEFORE)
			SetPseudoElm(ELM_PSEUDO_BEFORE);
		else if (pseudo_class == PSEUDO_CLASS_AFTER)
			SetPseudoElm(ELM_PSEUDO_AFTER);
		else if (pseudo_class == PSEUDO_CLASS_FIRST_LINE)
			SetPseudoElm(ELM_PSEUDO_FIRST_LINE);
		else if (pseudo_class == PSEUDO_CLASS_FIRST_LETTER)
			SetPseudoElm(ELM_PSEUDO_FIRST_LETTER);
	}

	while (sel)
	{
		unsigned int has_class = 0, has_id = 0;
		BOOL has_fullscreen = FALSE;

		sel->CountIdsAndAttrs(a, b, c, has_class, has_id, has_fullscreen);

		if (!sel->Pred())
		{
			if (has_id & SEL_COUNT_MASK)
			{
				SetHasIdInTarget();
				if (has_id == 1)
					SetHasSingleIdInTarget();
			}
			if (has_class & SEL_COUNT_MASK)
			{
				SetHasClassInTarget();
				if (has_class == 1)
					SetHasSingleClassInTarget();
			}
		}
		else
			if (has_fullscreen)
				SetHasComplexFullscreen();

		sel = sel->Suc();
	}
	// Truncate b and c so that they don't overflow into a and b.
	if (c > 24) c = 24;
	if (b > 24) b = 24;
	m_packed.specificity = a*625 + b*25 + c;
}

int CSS_Selector::GetMaxSuccessiveAdjacent()
{
	// maximum number of successive adjacent combinators in a selector
	int adj_suc_max = 0;
	int adj_suc = 0;

	CSS_SimpleSelector* sel = FirstSelector();

	while (sel)
	{
		if (sel->GetCombinator() == CSS_COMBINATOR_ADJACENT)
		{
			if (adj_suc < INT_MAX)
				adj_suc += 1;
		}
		else if (sel->GetCombinator() == CSS_COMBINATOR_ADJACENT_INDIRECT)
		{
			adj_suc = INT_MAX;
			break;
		}
		else if (adj_suc > 0)
		{
			if (adj_suc > adj_suc_max)
				adj_suc_max = adj_suc;
			adj_suc = 0;
		}
		sel = sel->Suc();
	}

	if (adj_suc > adj_suc_max)
		adj_suc_max = adj_suc;

	return adj_suc_max;
}

BOOL CSS_Selector::Match(CSS_MatchContext* context,
						 CSS_MatchContextElm* context_elm,
						 CSS_SimpleSelector* simple_sel,
						 BOOL& match_to_top,
						 BOOL& match_to_left) const
{
	OP_PROBE4(OP_PROBE_CSS_SELECTOR_MATCH2);

	int failed_pseudo = 0;
	int tmp_failed_pseudo = 0;
	int pseudo_pos = g_css_pseudo_stack->GetPos();

	// iterate through the selector chain
	while (simple_sel && context_elm)
	{
		switch (simple_sel->GetCombinator())
		{
		// Two elements that share the same parent
		case CSS_COMBINATOR_ADJACENT:
			{
				HTML_Element* sibling = context_elm->HtmlElement()->PredActualStyle();

				while (sibling && !Markup::IsRealElement(sibling->Type()))
					sibling = sibling->PredActualStyle();

				// no sibling
				if (sibling == NULL)
				{
					g_css_pseudo_stack->Backtrack(pseudo_pos);
					match_to_left = TRUE;
					return FALSE;
				}

				context_elm = context->GetSiblingElm(context_elm, sibling);
				BOOL match = simple_sel->Match(context, context_elm, tmp_failed_pseudo);
				if (match || tmp_failed_pseudo)
				{
					failed_pseudo |= tmp_failed_pseudo;
					break;
				}

				g_css_pseudo_stack->Backtrack(pseudo_pos);
				return FALSE;
			}

		case CSS_COMBINATOR_ADJACENT_INDIRECT:
			{
				BOOL tight_match = TRUE;

				for (HTML_Element* candidate = context_elm->HtmlElement()->PredActualStyle(); candidate; candidate = candidate->PredActualStyle())
				{
					while (candidate && !Markup::IsRealElement(candidate->Type()))
						candidate = candidate->PredActualStyle();

					if (candidate == NULL)
					{
						g_css_pseudo_stack->Backtrack(pseudo_pos);
						match_to_left = TRUE;
						return FALSE;
					}

					context_elm = context->GetSiblingElm(context_elm, candidate);

					BOOL match = simple_sel->Match(context, context_elm, tmp_failed_pseudo);
					if (!match && (!tmp_failed_pseudo || simple_sel->Suc() == NULL))
					{
						if (tmp_failed_pseudo)
							g_css_pseudo_stack->Commit();
						continue;
					}

					if (simple_sel->Suc() == NULL)
					{
						g_css_pseudo_stack->Commit();
						return (failed_pseudo | tmp_failed_pseudo) == 0;
					}

					BOOL local_to_left = FALSE;
					// try a recursive match for next selector
					if (Match(context, context_elm, simple_sel->Suc(), match_to_top, local_to_left))
					{
						if (tmp_failed_pseudo)
							continue;

						return failed_pseudo == 0;
					}

					if (match_to_top)
						break;

					if (tight_match)
					{
						if (local_to_left)
							break;
						else
							tight_match = FALSE;
					}
				}
				if (tight_match)
					match_to_left = TRUE;
				g_css_pseudo_stack->Backtrack(pseudo_pos);
				return FALSE;
			}

		// Two elements where the first is a direct child of the second
		case CSS_COMBINATOR_CHILD:
			{
				CSS_MatchContextElm* parent = context->GetParentElm(context_elm);
				if (!parent)
				{
					/* We must have a parent, since all valid rules apply to
					   children, descendants or siblings. */
					g_css_pseudo_stack->Backtrack(pseudo_pos);
					match_to_top = TRUE;
					return FALSE;
				}

				// the selector_element must match the parent
				BOOL match = simple_sel->Match(context, parent, tmp_failed_pseudo);
				if (match || tmp_failed_pseudo)
				{
					failed_pseudo |= tmp_failed_pseudo;
					// the parent is the subject of the next selector
					context_elm = parent;
					break;
				}

				g_css_pseudo_stack->Backtrack(pseudo_pos);
				return FALSE;
			}

		// Two elements where the first is a descendant of the second
		case CSS_COMBINATOR_DESCENDANT:
			{
				/* We need to recurse to find all possible matches :-/
				   This can be expensive for a long chain... */

				BOOL tight_match = TRUE;

				// try to find any parent which matches, stop when found
				for (CSS_MatchContextElm* candidate = context->GetParentElm(context_elm); candidate; candidate = context->GetParentElm(candidate))
				{
					BOOL match = simple_sel->Match(context, candidate, tmp_failed_pseudo);
					if (!match && (!tmp_failed_pseudo || simple_sel->Suc() == NULL))
					{
						if (tmp_failed_pseudo)
							g_css_pseudo_stack->Commit();

						// this parent doesn't match, continue with next
						continue;
					}

					// the rule is: CANDIDATE ELEMENT
					// the tree is: CANDIDATE - ... - ELEMENT

					// reached end of selectors
					if (simple_sel->Suc() == NULL)
					{
						g_css_pseudo_stack->Commit();
						return (failed_pseudo | tmp_failed_pseudo) == 0;
					}

					BOOL local_to_top = FALSE;
					BOOL local_to_left = FALSE;
					// try a recursive match for next selector
					if (Match(context, candidate, simple_sel->Suc(), local_to_top, local_to_left))
					{
						if (tmp_failed_pseudo)
							continue;

						return failed_pseudo == 0;
					}

					if (tight_match)
					{
						if (local_to_top)
							break;
						else
							tight_match = FALSE;
					}
				}
				if (tight_match)
					match_to_top = TRUE;
				g_css_pseudo_stack->Backtrack(pseudo_pos);
				return FALSE;
			}

		default:
			// should not happen
			OP_ASSERT(FALSE);
			g_css_pseudo_stack->Backtrack(pseudo_pos);
			return FALSE;
		}

		// consume this selector
		simple_sel = simple_sel->Suc();
	}

	// we have processed all selectors without failing to match, success!

	if (simple_sel == NULL)
	{
		g_css_pseudo_stack->Commit();

		if (!failed_pseudo)
			return TRUE;
	}
	else
		g_css_pseudo_stack->Backtrack(pseudo_pos);

	match_to_top = TRUE;
	return FALSE;
}

CSS_Selector::MatchResult
CSS_Selector::Match(CSS_MatchContext* context, CSS_MatchContextElm* context_elm) const
{
	OP_PROBE4(OP_PROBE_CSS_SELECTOR_MATCH1);

	if (context->PseudoElm() && context->PseudoElm() != GetPseudoElm())
		return NO_MATCH;

	CSS_SimpleSelector* simple_sel = FirstSelector();

	// we must have at least one selector
	OP_ASSERT(simple_sel);

	int failed_pseudo = 0;

	BOOL match = simple_sel->Match(context, context_elm, failed_pseudo);

	// For ::before and ::after, we already know that a pseudo-element will
	// not be generated from this rule if it doesn't have a content property.
	// However, if this selector matching query comes from the debugger,
	// we tentatively match the rule anyway to let the debugger override
	// normal matching behavior.
	if (CSS_PSEUDO_NEEDS_CONTENT(failed_pseudo) && !HasContentProperty()
#ifdef STYLE_GETMATCHINGRULES_API
		&& !context->Listener()
#endif // STYLE_GETMATCHINGRULES_API
		)
		failed_pseudo &= ~CSS_PSEUDO_CLASS_GROUP_BEFORE_AFTER;

	if (match || failed_pseudo)
	{
		match = TRUE;
		simple_sel = simple_sel->Suc();

		if (simple_sel != NULL)
		{
			BOOL match_to_top = FALSE;
			BOOL match_to_left = FALSE;
			match = Match(context, context_elm, simple_sel, match_to_top, match_to_left);

			OP_ASSERT(g_css_pseudo_stack->GetPos() == 0 || !match);
			if (!match)
				g_css_pseudo_stack->Backtrack(0);
		}
		else
			g_css_pseudo_stack->Commit();
	}
	else
		g_css_pseudo_stack->Backtrack(0);

	if (match && failed_pseudo && failed_pseudo == (failed_pseudo & CSS_PSEUDO_CLASS_GROUP_ELEMENTS))
		g_css_pseudo_stack->AddHasPseudo(failed_pseudo);

	OP_ASSERT(g_css_pseudo_stack->GetPos() == 0);

	if (match)
	{
		if (failed_pseudo == 0)
			return MATCH;
#ifdef STYLE_GETMATCHINGRULES_API
		else if (context->Listener())
			return context->Listener()->MatchPseudo(failed_pseudo) ? MATCH : NO_MATCH;
#endif // STYLE_GETMATCHINGRULES_API
		else if (failed_pseudo == CSS_PSEUDO_CLASS_SELECTION)
			return MATCH_SELECTION;
	}

	return NO_MATCH;
}

OP_STATUS CSS_Selector::GetSelectorText(const CSS* stylesheet, TempBuffer* buf)
{
	CSS_SimpleSelector* simple_sel = LastSelector();
	while (simple_sel)
	{
		RETURN_IF_ERROR(simple_sel->GetSelectorText(stylesheet, buf));

		CSS_SimpleSelector* next_simple_sel = simple_sel->Pred();
		if (next_simple_sel)
		{
			// Skip a potential tail of simple selectors that
			// shouldn't be visible in the serialization.
			if (!next_simple_sel->IsSerializable())
				break;

			const char* comb_str;
			switch (simple_sel->GetCombinator())
			{
			case CSS_COMBINATOR_CHILD:
				comb_str = " > ";
				break;
			case CSS_COMBINATOR_ADJACENT:
				comb_str = " + ";
				break;
			case CSS_COMBINATOR_ADJACENT_INDIRECT:
				comb_str = " ~ ";
				break;
			default:
				OP_ASSERT(!"Unknown combinator.");
				// fall through
			case CSS_COMBINATOR_DESCENDANT:
				comb_str = " ";
				break;
			}
			RETURN_IF_ERROR(buf->Append(comb_str));
		}

		simple_sel = next_simple_sel;
	}
	return OpStatus::OK;
}
