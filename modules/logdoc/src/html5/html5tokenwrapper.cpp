/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/probetools/probepoints.h"

#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/html_att.h"

#include "modules/logdoc/src/html5/html5tokenwrapper.h"
#include "modules/logdoc/src/html5/html5attrcopy.h"

#include "modules/logdoc/src/htmlattrentry.h"
#ifndef HTML5_STANDALONE
#include "modules/logdoc/htm_lex.h" // for LogdocXmlName
#endif // HTML5_STANDALONE

/*virtual*/ HTML5TokenWrapper::~HTML5TokenWrapper()
{
	OP_DELETEA(m_attr_copies);
#ifndef HTML5_STANDALONE
	OP_DELETEA(m_attr_entries);
	OP_DELETE(m_tag_name);
#endif // HTML5_STANDALONE
}

void HTML5TokenWrapper::CreateAttributesCopyL(const HTML5TokenWrapper &src_token)
{
	OP_ASSERT(!m_attr_copies);  // we already have a copy array!

	BOOL src_has_copy = src_token.m_attr_copies != NULL;  // in case src is the same as this, check what the value is before we allocate a new array

	m_attr_copies_length = src_token.GetAttrCount();

	if (m_attr_copies_length)
	{
		m_attr_copies = OP_NEWA_L(HTML5AttrCopy, m_attr_copies_length);

		if (src_has_copy)
		{
			for (unsigned attr_i = 0; attr_i < m_attr_copies_length; attr_i++)
				m_attr_copies[attr_i].CopyL(&src_token.m_attr_copies[attr_i]);
		}
		else
		{
			for (unsigned attr_i = 0; attr_i < m_attr_copies_length; attr_i++)
				m_attr_copies[attr_i].CopyL(src_token.GetAttribute(attr_i));
		}
	}
	else
		m_attr_copies = NULL;
}

void HTML5TokenWrapper::CopyL(HTML5TokenWrapper &src_token)
{
	HTML5Token::TokenType type = src_token.GetType();
	OP_ASSERT(type == HTML5Token::START_TAG); // only used for formating elements

	HTML5TokenBuffer *name = src_token.GetName();

	InitializeL(0, 0);
	Reset(type);
	SetNameL(name);

	SetLineNum(src_token.GetLineNum());
	SetLinePos(src_token.GetLinePos());

	CreateAttributesCopyL(src_token);

	m_elm_type = src_token.m_elm_type;
	m_ns = src_token.m_ns;
	m_skipped_spaces = src_token.m_skipped_spaces;
}

void HTML5TokenWrapper::ResetWrapper()
{
	m_elm_type = Markup::HTE_UNKNOWN;
	m_ns = Markup::HTML;
	m_skipped_spaces = 0;

	if (m_attr_copies)
		OP_DELETEA(m_attr_copies);
	m_attr_copies = NULL;
	m_attr_copies_length = 0;
}

BOOL HTML5TokenWrapper::HasAttribute(const uni_char *name, unsigned &attr_idx)
{
	unsigned name_length = uni_strlen(name);
	if (m_attr_copies)
	{
		unsigned attr_count = m_attr_copies_length;
		for (unsigned i = 0; i < attr_count; i++)
		{
			HTML5AttrCopy *attr = &m_attr_copies[i];
			if (attr->GetName()->IsEqualTo(name, name_length))
			{
				attr_idx = i;
				return TRUE;
			}
		}
	}
	else
	{
		unsigned attr_count = GetAttrCount();
		for (unsigned i = 0; i < attr_count; i++)
		{
			HTML5Attr *attr = GetAttribute(i);
			if (attr->GetName()->IsEqualTo(name, name_length))
			{
				attr_idx = i;
				return TRUE;
			}
		}
	}

	return FALSE;
}

void HTML5TokenWrapper::SetAttributeValueL(unsigned attr_idx, const uni_char* value)
{
	if (!m_attr_copies)  // need to have a copy, don't want to change the original
		CreateAttributesCopyL(*this);

	if (attr_idx >= m_attr_copies_length)
	{
		OP_ASSERT(!"Don't try to set an attribute which doesn't already exist");
		return;
	}

	m_attr_copies[attr_idx].SetValueL(value, uni_strlen(value));
}

BOOL HTML5TokenWrapper::AttributeHasValue(unsigned attr_idx, const uni_char *value)
{
	if (m_attr_copies)
	{
		OP_ASSERT(m_attr_copies_length > attr_idx);
		return uni_stri_eq(value, m_attr_copies[attr_idx].GetValue());
	}
	else
	{
		OP_ASSERT(GetAttrCount() > attr_idx);
		if (HTML5Attr *attr = GetAttribute(attr_idx))
			return attr->GetValue().Matches(value, FALSE);
	}

	return FALSE;
}

BOOL HTML5TokenWrapper::RemoveAttributeL(const uni_char *name)
{
	// only implemented for m_attr_copies - need to make copies if we actually remove otherwise
	OP_ASSERT(m_attr_copies || HTML5Token::GetAttrCount() == 0);
	if (!m_attr_copies)
		return FALSE;

	unsigned attr_count = m_attr_copies_length;
	unsigned name_length = uni_strlen(name);
	for (unsigned i = 0; i < attr_count; i++)
	{
		HTML5AttrCopy *attr = &m_attr_copies[i];
		if (attr->GetName()->IsEqualTo(name, name_length))  // this is the one to remove
		{
			// order doesn't matter for attributes, so copy the last attribute into
			// this position, and remove decrease array length
			if (i < attr_count - 1)
			{
				HTML5TokenBuffer *name_buffer = m_attr_copies[attr_count - 1].GetName();
				attr->SetNameL(name_buffer);

				uni_char *attr_value = m_attr_copies[attr_count - 1].GetValue();
				attr->SetValueL(attr_value, attr_value ? uni_strlen(attr_value) : 0);
			}
			m_attr_copies_length--;

			return TRUE;
		}
	}

	return FALSE;
}

unsigned HTML5TokenWrapper::GetAttrCount() const
{
	if (m_attr_copies)
		return m_attr_copies_length;
	else
		return HTML5Token::GetAttrCount();
}

BOOL HTML5TokenWrapper::GetAttributeL(unsigned index, HTML5TokenBuffer *&name, uni_char *&value, unsigned &value_len)
{
	OP_PROBE7_L(OP_PROBE_HTML5TOKENWRAPPER_GETATTRIBUTEL);

	if (m_attr_copies)
	{
		OP_ASSERT(index < m_attr_copies_length);
		name = m_attr_copies[index].GetName();
		value = m_attr_copies[index].GetValue();
		value_len = value ? uni_strlen(value) : 0;
		return FALSE;
	}
	else
	{
		HTML5Attr *attr = GetAttribute(index);
		name = attr->GetName();
		return attr->GetValue().GetBufferL(value, value_len, FALSE);
	}
}

BOOL HTML5TokenWrapper::GetAttributeValueL(const uni_char *name, uni_char *&value, unsigned &value_len, unsigned* attr_idx_param)
{
	value = NULL;
	unsigned attr_index;
	if (HasAttribute(name, attr_index))
	{
		if (attr_idx_param)
			*attr_idx_param = attr_index;

		HTML5TokenBuffer *dummy_name = NULL;
		return GetAttributeL(attr_index, dummy_name, value, value_len);
	}

	return FALSE;
}

#ifndef HTML5_STANDALONE
uni_char* HTML5TokenWrapper::GetUnescapedAttributeValueL(const uni_char* attribute, unsigned* attr_idx)
{
	uni_char* value;
	unsigned value_len;
	BOOL must_delete = GetAttributeValueL(attribute, value, value_len, attr_idx);

	if (value)
	{
		uni_char* link_str;

		if (!must_delete)  // we don't own buffer, make a copy that we do own, so that we can replace characters
			link_str = UniSetNewStrN(value, value_len);
		else
			link_str = value;

		ReplaceEscapes(link_str, FALSE, FALSE, FALSE);

		return link_str;
	}

	return NULL;
}

/**
 * Adjusts casing of attribute names for elements in foreign content.
 * @param[out] hae The attribute entry to change if needed.
 */
static void AdjustForeignAttr(HtmlAttrEntry *hae)
{
	const uni_char *attr_name = hae->name;
	if (*attr_name == 'x')
	{
		const uni_char *prefix_end = uni_strchr(attr_name, ':');
		if (prefix_end)
		{
			int attr_ns_idx = Markup::HTML;
			if (uni_strncmp(attr_name, UNI_L("xlink"), prefix_end - attr_name) == 0)
				attr_ns_idx = NS_IDX_XLINK;
			else if (uni_strncmp(attr_name, UNI_L("xml"), prefix_end - attr_name) == 0)
				attr_ns_idx = NS_IDX_XML;
			else if (uni_strncmp(attr_name, UNI_L("xmlns"), prefix_end - attr_name) == 0)
				attr_ns_idx = NS_IDX_XMLNS;

			prefix_end++; // drop the ':'

			BOOL recognized = FALSE;
			if (attr_ns_idx == NS_IDX_XLINK)
			{
				recognized = uni_str_eq(prefix_end, UNI_L("actuate"))
					|| uni_str_eq(prefix_end, UNI_L("arcrole"))
					|| uni_str_eq(prefix_end, UNI_L("href"))
					|| uni_str_eq(prefix_end, UNI_L("role"))
					|| uni_str_eq(prefix_end, UNI_L("show"))
					|| uni_str_eq(prefix_end, UNI_L("title"))
					|| uni_str_eq(prefix_end, UNI_L("type"));
			}
			else if (attr_ns_idx == NS_IDX_XML)
			{
				recognized = uni_str_eq(prefix_end, UNI_L("base"))
					|| uni_str_eq(prefix_end, UNI_L("lang"))
					|| uni_str_eq(prefix_end, UNI_L("space"));
			}
			else if (attr_ns_idx == NS_IDX_XMLNS)
				recognized = uni_str_eq(prefix_end, UNI_L("xlink"));

			if (recognized)
			{
				hae->name = prefix_end;
				hae->ns_idx = attr_ns_idx;
			}
		}
		else if (uni_str_eq(attr_name, UNI_L("xmlns")))
			hae->ns_idx = NS_IDX_XMLNS;
	}
}

HtmlAttrEntry* HTML5TokenWrapper::GetOrCreateAttrEntries(unsigned &token_attr_count, unsigned &extra_attr_count, LogicalDocument *logdoc)
{
	token_attr_count = GetAttrCount();
	// limit the number of attributes to copy because we only have
	// so many bits to store the attribute array size in the element.
	if (token_attr_count > HTML5TokenWrapper::kMaxAttributeCount)
		token_attr_count = HTML5TokenWrapper::kMaxAttributeCount;
	extra_attr_count = token_attr_count;

	if (m_elm_type == Markup::HTE_COMMENT)
		extra_attr_count += 1; // for content
	else if (m_elm_type == Markup::HTE_DOCTYPE)
		extra_attr_count += 3; // for name, pubid and sysid
	else if (m_elm_type == Markup::HTE_UNKNOWN)
		extra_attr_count += 1; // for tag name

	unsigned needed_attr_array_size = extra_attr_count + 1;
	if (needed_attr_array_size > m_attr_entries_length)
	{
		// +1 to make some room for the trailing ATTR_NULL
		HtmlAttrEntry *new_entries = OP_NEWA(HtmlAttrEntry, needed_attr_array_size);
		if (!new_entries)
			return NULL;

		OP_DELETEA(m_attr_entries);
		m_attr_entries = new_entries;
		m_attr_entries_length = needed_attr_array_size;
	}

	HtmlAttrEntry *hae = m_attr_entries;
	for (unsigned i = 0; i < token_attr_count; i++, hae++)
	{
		uni_char *attr_val;
		HTML5TokenBuffer *name_buf = NULL;

		hae->ns_idx = NS_IDX_DEFAULT;
		hae->is_specified = TRUE;
		hae->is_special = FALSE;
		hae->delete_after_use = GetAttributeL(i, name_buf, attr_val, hae->value_len);
		hae->name = name_buf->GetBuffer();
		hae->name_len = name_buf->Length();
		hae->value = attr_val;

		// substitute foreign attributes in foreign elements
		if (m_ns == Markup::HTML)
			hae->attr = g_html5_name_mapper->GetAttrTypeFromTokenBuffer(name_buf);
		else
		{
			AdjustForeignAttr(hae);
			hae->attr = g_html5_name_mapper->GetAttrTypeFromName(hae->name, FALSE, m_ns);
		}

		hae->is_id = hae->attr == Markup::HA_ID;
	}

	if (m_elm_type == Markup::HTE_COMMENT)
	{
		uni_char *data;
		hae->ns_idx = NS_IDX_DEFAULT;
		hae->is_specified = FALSE;
		hae->is_special = FALSE;
		hae->is_id = FALSE;
		hae->attr = Markup::HA_CONTENT;
		hae->delete_after_use = GetData().GetBufferL(data, hae->value_len, TRUE);
		hae->value = data;
		hae++;
	}
	else if (m_elm_type == Markup::HTE_DOCTYPE)
	{
		hae->ns_idx = NS_IDX_DEFAULT;
		hae->attr = Markup::HA_NAME;
		hae->is_specified = FALSE;
		hae->is_special = FALSE;
		hae->delete_after_use = FALSE;
		hae->is_id = FALSE;
		hae->value = GetNameStr();
		hae->value_len = GetNameLength();

		hae++;
		hae->ns_idx = SpecialNs::NS_LOGDOC;
		hae->attr = ATTR_PUB_ID;
		hae->is_specified = FALSE;
		hae->is_special = TRUE;
		hae->delete_after_use = FALSE;
		hae->is_id = FALSE;
		if (HasPublicIdentifier())
			GetPublicIdentifier(hae->value, hae->value_len);
		else
		{
			hae->value = NULL;
			hae->value_len = 0;
		}

		hae++;
		hae->ns_idx = SpecialNs::NS_LOGDOC;
		hae->attr = ATTR_SYS_ID;
		hae->is_specified = FALSE;
		hae->is_special = TRUE;
		hae->delete_after_use = FALSE;
		hae->is_id = FALSE;
		if (HasSystemIdentifier())
		{
			GetSystemIdentifier(hae->value, hae->value_len);
			if (!hae->value)
			{
				hae->value = UNI_L("");
				hae->value_len = 0;
			}
		}
		else
		{
			hae->value = NULL;
			hae->value_len = 0;
		}

		logdoc->SetDoctype((hae - 2)->value, (hae - 1)->value, hae->value);
		hae++;
	}
	else if (m_elm_type == Markup::HTE_UNKNOWN)
	{
		hae->attr = ATTR_XML_NAME;
		hae->ns_idx = SpecialNs::NS_LOGDOC;
		hae->is_id = FALSE;
		hae->is_special = TRUE;
		hae->is_specified = FALSE;
		hae->delete_after_use = FALSE;
		hae->name = NULL;
		hae->name_len = 0;
		if (!m_tag_name)
		{
			m_tag_name = OP_NEW(LogdocXmlName, ());
			if (m_tag_name == NULL)
				return NULL;
		}

		m_tag_name->SetName(GetNameStr(), GetNameLength(), FALSE);
		hae->value = reinterpret_cast<uni_char*>(m_tag_name);
		hae->value_len = 0;
		hae++;
	}

	hae->attr = ATTR_NULL;

	return m_attr_entries;
}

void HTML5TokenWrapper::DeleteAllocatedAttrEntries()
{
	HtmlAttrEntry *hae = m_attr_entries;
	while (hae->attr != ATTR_NULL)
	{
		if (hae->delete_after_use)
			OP_DELETEA(const_cast<uni_char*>(hae->value));
		hae++;
	}
}
#endif // HTML5_STANDALONE

HTML5AttrCopy* HTML5TokenWrapper::GetLocalAttrCopy(unsigned index)
{
	return &m_attr_copies[index];
}

BOOL HTML5TokenWrapper::IsEqual(HTML5TokenWrapper &token)
{
	// token should always be a non-copied token
	// and this should always be a copied one
	OP_ASSERT(!token.m_attr_copies);

	if (m_elm_type != token.GetElementType()
		|| m_ns != token.GetElementNs())
		return FALSE;

	unsigned attr_count = token.GetAttrCount();
	if (attr_count != m_attr_copies_length)
		return FALSE;

	for (unsigned attr_i = 0; attr_i < m_attr_copies_length; attr_i++)
	{
		if (!m_attr_copies[attr_i].IsEqual(token.GetAttribute(attr_i)))
			return FALSE;
	}

	return TRUE;
}
