/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/src/attribute.h"

#include "modules/logdoc/helistelm.h"
#include "modules/logdoc/datasrcelm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/namespace.h"
#include "modules/logdoc/complex_attr.h"
#include "modules/logdoc/xmlevents.h"
#include "modules/logdoc/htm_lex.h" // for HtmlAttrEntry
#include "modules/logdoc/html_att.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/htm_elm.h" // For ReplaceAttributeEntities
#include "modules/style/css_property_list.h"
#include "modules/util/OpHashTable.h"

class URL;

#define EMBED_JAVA_CODE_STRING UNI_L("java_CODE")
#define EMBED_JAVA_CODEBASE_STRING UNI_L("java_CODEBASE")
#define EMBED_JAVA_DOCBASE_STRING UNI_L("java_DOCBASE")
#define EMBED_JAVA_ARCHIVE_STRING UNI_L("java_ARCHIVE")
#define EMBED_JAVA_OBJECT_STRING UNI_L("java_OBJECT")
#define EMBED_JAVA_TYPE_STRING UNI_L("java_TYPE")

#if defined _APPLET_2_EMBED_
/* If this is defined, some parameters of the <applet> tag
 * are translated to the EMBED_JAVA_whatever versions above.
 * Also, a default codebase parameter will be added, if none
 * is found in the <applet> tag.
 */
//# define _APPLET_2_EMBED_TRANSLATE_STRINGS_
#endif // _APPLET_2_EMBED_


/**
 * AttrItem implementation
 **/

AttrItem::AttrItem() : m_value(NULL)
{
	m_info.init = 0;
	m_info.packed.special = TRUE;
	m_info.packed.type = ITEM_TYPE_BOOL;
	g_ns_manager->GetElementAt(GetNsIdx())->IncRefCountA();
}

AttrItem::~AttrItem()
{
	Clean();
	if (!IsSpecial())
		g_ns_manager->GetElementAt(GetNsIdx())->DecRefCountA();
}

void
AttrItem::Replace(short attr, ItemType item_type, void* value, int ns_idx, BOOL need_free, BOOL is_special, BOOL is_id, BOOL is_specified, BOOL is_event)
{
	Clean();
	Set(attr, item_type, value, ns_idx, need_free, is_special, is_id, is_specified, is_event);
}

void
AttrItem::Set(short attr, ItemType item_type, void* value, int ns_idx, BOOL need_free, BOOL is_special, BOOL is_id, BOOL is_specified, BOOL is_event)
{
	// We must update the refcount for the old and the new ns_idx
	// If the old value was special we don't need to count that namespace down
	// If the new value will be special we don't have to count the namespace up
	if (!IsSpecial())
		g_ns_manager->GetElementAt(GetNsIdx())->DecRefCountA();

	if (!is_special)
		g_ns_manager->GetElementAt(ns_idx)->IncRefCountA();

	m_info.packed.attr = attr;
	m_info.packed.ns = ns_idx;
	m_info.packed.special = is_special;
	m_info.packed.type = item_type;
	m_info.packed.need_free = need_free;
	m_info.packed.specified = is_specified;
	m_info.packed.id = is_id;
	m_info.packed.event = is_event;

	m_value = value;
}

void
AttrItem::Clean()
{
	if (NeedFree())
		Clean(GetType(), m_value);
}

/*static*/ void
AttrItem::Clean(ItemType item_type, void *value)
{
	switch (item_type)
	{
	case ITEM_TYPE_STRING:
		OP_DELETEA((uni_char*) value); // FIXME:REPORT_MEMMAN-KILSMO
		break;

	case ITEM_TYPE_NUM_AND_STRING:
		OP_DELETEA((char*) value); // FIXME:REPORT_MEMMAN-KILSMO
		break;

	case ITEM_TYPE_URL:
		OP_DELETE((URL*) value); // FIXME:REPORT_MEMMAN-KILSMO
		break;

	case ITEM_TYPE_URL_AND_STRING:
		UrlAndStringAttr::Destroy(static_cast<UrlAndStringAttr *>(value));
		break;

	case ITEM_TYPE_PRIVATE_ATTRS:
		DELETE_PrivateAttrs((PrivateAttrs*) value); // FIXME:REPORT_MEMMAN-KILSMO
		break;

	case ITEM_TYPE_COORDS_ATTR:
		DELETE_CoordsAttr((CoordsAttr*) value); // FIXME:REPORT_MEMMAN-KILSMO
		break;

#ifdef XML_EVENTS_SUPPORT
	case ITEM_TYPE_XML_EVENTS_REGISTRATION:
		OP_DELETE((XML_Events_Registration *) value); // FIXME:REPORT_MEMMAN-KILSMO
		break;
#endif
	case ITEM_TYPE_COMPLEX:
		OP_DELETE((ComplexAttr*) value);
		break;

	case ITEM_TYPE_CSS:
		OP_DELETE((CSS*) value);
		break;

#ifdef _DEBUG
	default:
		OP_ASSERT(!"Missing item type");
	case ITEM_TYPE_NUM:
	case ITEM_TYPE_BOOL:
		;
#endif // _DEBUG
	}
}

/**
 * PrivateAttrs implementation
 **/

PrivateAttrs::PrivateAttrs()
	: m_used_len(0)
	, m_length(0)
	, m_names(NULL)
	, m_values(NULL)
{
}

OP_STATUS
PrivateAttrs::SetName(HTML_ElementType type,
					  HtmlAttrEntry* hae,
					  int idx
#ifdef _APPLET_2_EMBED_
					, BOOL &has_codebase
#endif // _APPLET_2_EMBED_
					)
{
#if defined _APPLET_2_EMBED_ && defined _APPLET_2_EMBED_TRANSLATE_STRINGS_
	if (type == HE_APPLET && hae->attr == ATTR_CODE)
		m_names[idx] = UniSetNewStr(EMBED_JAVA_CODE_STRING); // FIXME:REPORT_MEMMAN-KILSMO
	else if (type == HE_APPLET && hae->attr == ATTR_CODEBASE)
	{
		has_codebase = TRUE;
		m_names[idx] = UniSetNewStr(EMBED_JAVA_CODEBASE_STRING); // FIXME:REPORT_MEMMAN-KILSMO
	}
	else if (type == HE_APPLET && hae->attr == ATTR_ARCHIVE)
		m_names[idx] = UniSetNewStr(EMBED_JAVA_ARCHIVE_STRING); // FIXME:REPORT_MEMMAN-KILSMO
	else if (type == HE_APPLET && hae->attr == ATTR_OBJECT)
		m_names[idx] = UniSetNewStr(EMBED_JAVA_OBJECT_STRING); // FIXME:REPORT_MEMMAN-KILSMO
	else if (type == HE_APPLET && hae->attr == ATTR_TYPE)
		m_names[idx] = UniSetNewStr(EMBED_JAVA_TYPE_STRING); // FIXME:REPORT_MEMMAN-KILSMO
	else
#endif
	{
		m_names[idx] = OP_NEWA(uni_char, hae->name_len + 1); // FIXME:REPORT_MEMMAN-KILSMO
		if (m_names[idx])
		{
			uni_strncpy(m_names[idx], hae->name, hae->name_len);
			m_names[idx][hae->name_len] = 0;
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS
PrivateAttrs::SetValue(HLDocProfile* hld_profile,
					   HTML_ElementType type,
					   HtmlAttrEntry* hae,
					   int idx)
{
	if (hae->value && hae->value_len)
	{
		OpString value_obj;
		OP_STATUS status = value_obj.Set(hae->value, hae->value_len);
		// Return value checked below, after we've cleaned up some
		const uni_char* value = value_obj.CStr();
		unsigned value_len = hae->value_len;

		RETURN_IF_ERROR(status); // If the string operation above succeded.

		if (type == HE_APPLET && hae->attr == ATTR_CODE && (value_len >= 5
			&& uni_strni_eq(value + value_len - 5, ".JAVA", 5)))
		{
			m_values[idx] = OP_NEWA(uni_char, value_len + 2); // FIXME:REPORT_MEMMAN-KILSMO
			if (m_values[idx])
			{
				uni_strncpy(m_values[idx], value, value_len);
				uni_strcpy(m_values[idx] + value_len - 5, UNI_L(".class"));
			}
		}
		else if (hae->attr == ATTR_WIDTH || hae->attr == ATTR_HEIGHT)
		{
			m_values[idx] = OP_NEWA(uni_char, 20); // enough space to hold a long // FIXME:REPORT_MEMMAN-KILSMO
			if (m_values[idx])
			{
				unsigned used_val_len = MIN(value_len, (unsigned)20 - 1);
				uni_strncpy(m_values[idx], value, used_val_len);
				(m_values[idx])[used_val_len] = 0;
			}
		}
		else
		{
			if (type == HE_EMBED)
			{
				uni_char* str = SetStringAttr(value, value_len, FALSE);
				m_values[idx] = str;
			}
			else
				m_values[idx] = UniSetNewStrN(value, value_len); // FIXME:REPORT_MEMMAN-KILSMO
		}

		if (!m_values[idx])
			return OpStatus::ERR_NO_MEMORY;
	}
	else
		m_values[idx] = NULL;

	return OpStatus::OK;
}

PrivateAttrs::~PrivateAttrs()
{
	if (m_names && m_values)		// not guaranteed due to possible OOM in PrivateAttrs::Create
	{
		for (int i = 0; i < m_used_len; i++)
		{
			OP_DELETEA(m_names[i]); // FIXME:REPORT_MEMMAN-KILSMO
			OP_DELETEA(m_values[i]); // FIXME:REPORT_MEMMAN-KILSMO
		}
	}
	OP_DELETEA(m_names); // FIXME:REPORT_MEMMAN-KILSMO
	OP_DELETEA(m_values); // FIXME:REPORT_MEMMAN-KILSMO
}

PrivateAttrs*
PrivateAttrs::Create(int len)
{
	PrivateAttrs* pa = OP_NEW(PrivateAttrs, ());
	if (!pa)
		return NULL;

	pa->m_names = OP_NEWA(uni_char*, len + 2); // FIXME:REPORT_MEMMAN-KILSMO
	if (!pa->m_names)
	{
		OP_DELETE(pa);
		return NULL;
	}

	pa->m_values = OP_NEWA(uni_char*, len + 2); // FIXME:REPORT_MEMMAN-KILSMO
	if (!pa->m_values)
	{
		OP_DELETE(pa);
		return NULL;
	}

	pa->m_length = len + 2;

	return pa;
}

OP_STATUS
PrivateAttrs::ProcessAttributes(HLDocProfile* hld_profile, HTML_ElementType type, HtmlAttrEntry* ha_list)
{
#ifdef _APPLET_2_EMBED_
	BOOL has_codebase = FALSE;
#endif // _APPLET_2_EMBED_


	// A set that will contain the name of the added attributes. Used as a workaround
	// to avoid duplicate attributes here though the real fix is to fix the html lexer/parser.
	OpAutoStringHashTable<OpString> stored_attribute_set(hld_profile->IsXml());

	for (int j = 0; ha_list[j].attr != ATTR_NULL; j++)
	{
		if (ha_list[j].name && ha_list[j].name_len)
		{
			OpString* attribute_name = OP_NEW(OpString, ());
			if (!attribute_name)
				return OpStatus::ERR_NO_MEMORY;
			if (OpStatus::IsMemoryError(attribute_name->Set(ha_list[j].name, ha_list[j].name_len)))
			{
				OP_DELETE(attribute_name);
				return OpStatus::ERR_NO_MEMORY;
			}

			BOOL duplicate_attr = stored_attribute_set.Contains(attribute_name->CStr());
			if (duplicate_attr ||
				OpStatus::IsMemoryError(stored_attribute_set.Add(attribute_name->CStr(), attribute_name)))
			{
				OP_DELETE(attribute_name);
				if (!duplicate_attr)
					return OpStatus::ERR_NO_MEMORY;
			}

			if (!duplicate_attr)
			{
#ifdef _APPLET_2_EMBED_
				RETURN_IF_MEMORY_ERROR(AddAttribute(hld_profile, type, ha_list[j], has_codebase));
#else // _APPLET_2_EMBED_
				RETURN_IF_MEMORY_ERROR(AddAttribute(hld_profile, type, ha_list[j]));
#endif // _APPLET_2_EMBED_
			}
		}
	}

#ifdef _APPLET_2_EMBED_
# ifdef _APPLET_2_EMBED_TRANSLATE_STRINGS_
	if (type == HE_APPLET)
	{
		m_names[m_used_len] = UniSetNewStr(EMBED_JAVA_DOCBASE_STRING); // FIXME:REPORT_MEMMAN-KILSMO
		OpString url_str;
		RETURN_IF_MEMORY_ERROR(hld_profile->BaseURL()->GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, url_str, TRUE));
		m_values[m_used_len] = UniSetNewStr(url_str.CStr()); // FIXME:REPORT_MEMMAN-KILSMO

		// OOM check
		if (!m_names[m_used_len] || !m_values[m_used_len])
		{
			OP_DELETEA(m_names[m_used_len]);
			OP_DELETEA(m_values[m_used_len]);
			return OpStatus::ERR_NO_MEMORY;
		}

		m_used_len++;

		if (!has_codebase)
		{
			m_names[m_used_len] = UniSetNewStr(EMBED_JAVA_CODEBASE_STRING); // FIXME:REPORT_MEMMAN-KILSMO
			RETURN_IF_MEMORY_ERROR(g_url_api->GetURL(*hld_profile->BaseURL(), ".").GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, url_str, TRUE));
			m_values[m_used_len] = UniSetNewStr(url_str.CStr()); // FIXME:REPORT_MEMMAN-KILSMO

			// Work-around: Remove the localhost part from file URLs.
			// The Java Plugin does not parse those URLs right.
			const uni_char *url_start = UNI_L("file://localhost");
			if (uni_strni_eq(m_values[m_used_len], url_start, uni_strlen(url_start))) // starts with file://localhost
			{
				uni_char *c = m_values[m_used_len] + uni_strlen(url_start);
				for ( ; *(c-9) = *c ; c++); // 9 == strlen("localhost")
			}

			// OOM check
			if (!m_names[m_used_len] || !m_values[m_used_len])
			{
				OP_DELETEA(m_names[m_used_len]);
				OP_DELETEA(m_values[m_used_len]);
				return OpStatus::ERR_NO_MEMORY;
			}

			m_used_len++;
		}
	}
# endif // TRANSLATE_STRINGS
#endif // APPLET_2_EMBED

	return OpStatus::OK;
}

OP_STATUS
PrivateAttrs::AddAttribute(HLDocProfile* hld_profile,
						   HTML_ElementType type,
						   HtmlAttrEntry &ha_entry
#ifdef _APPLET_2_EMBED_
						   , BOOL &has_codebase
#endif // _APPLET_2_EMBED_
						   )
{
	if (ha_entry.name && ha_entry.name_len)
	{
		if (m_used_len >= m_length)
		{
			// Growing by adding a constant, this will cause O(n^2) behaviour so the number of attributes must be low or the initial size correct
			uni_char **tmp_names = OP_NEWA(uni_char*, m_length + 2);
			uni_char **tmp_values = OP_NEWA(uni_char*, m_length + 2);
			if (!tmp_names || !tmp_values)
				return OpStatus::ERR_NO_MEMORY;

			int i;
			for (i = 0; i < m_length; i++)
			{
				tmp_names[i] = UniSetNewStr(m_names[i]);
				if (!tmp_names[i])
					break;
				OP_DELETEA(m_names[i]);
				m_names[i] = NULL;

				tmp_values[i] = UniSetNewStr(m_values[i]);
				if (m_values[i] && !tmp_values[i])
					break;
				OP_DELETEA(m_values[i]);
				m_values[i] = NULL;
			}

			// oom if not completed, clean up
			if (i < m_length)
			{
				int j = 0;
				// we have deleted the old entries so we need to put them back
				// if we fail to make the new arrays
				while (j < i)
				{
					if (!m_names[j])
						m_names[j] = tmp_names[j];
					if (!m_values[j])
						m_values[j] = tmp_values[j];
					++j;
				}

				OP_DELETEA(tmp_names);
				OP_DELETEA(tmp_values);

				return OpStatus::ERR_NO_MEMORY;
			}

			m_used_len = m_length;
			m_length += 2;

			OP_DELETEA(m_names);
			m_names = tmp_names;

			OP_DELETEA(m_values);
			m_values = tmp_values;
		}

#ifdef _APPLET_2_EMBED_
		RETURN_IF_MEMORY_ERROR(SetName(type, &ha_entry, m_used_len, has_codebase));
#else // _APPLET_2_EMBED_
		RETURN_IF_MEMORY_ERROR(SetName(type, &ha_entry, m_used_len));
#endif // _APPLET_2_EMBED_
		RETURN_IF_MEMORY_ERROR(SetValue(hld_profile, type, &ha_entry, m_used_len));
		m_used_len++;
	}

	return OpStatus::OK;
}

OP_STATUS
PrivateAttrs::SetAttribute(HLDocProfile* hld_profile, HTML_ElementType type, HtmlAttrEntry &ha_entry)
{
	for (int j = 0; j < m_used_len; j++)
	{
		if (uni_strnicmp(m_names[j], ha_entry.name, ha_entry.name_len) == 0 && m_names[j][ha_entry.name_len] == 0)
		{
			OP_DELETEA(m_values[j]);
			RETURN_IF_MEMORY_ERROR(SetValue(hld_profile, type, &ha_entry, j));
			return OpStatus::OK;
		}
	}

#ifdef _APPLET_2_EMBED_
	BOOL has_codebase = FALSE;
	return AddAttribute(hld_profile, type, ha_entry, has_codebase);
#else
	return AddAttribute(hld_profile, type, ha_entry);
#endif // _APPLET_2_EMBED_
}

PrivateAttrs*
PrivateAttrs::GetCopy(int new_len)
{
	int use_len = new_len;
	if (use_len < m_used_len)
		use_len = m_used_len;

	PrivateAttrs* new_pa;
	new_pa = NEW_PrivateAttrs((use_len));

	// OOM check
	if (!new_pa)
		return NULL;

	uni_char** new_names = new_pa->GetNames();
	uni_char** new_values = new_pa->GetValues();

	new_pa->m_used_len = m_used_len;

	for (int i = 0; i < m_used_len; i++)
	{
		if (m_names[i])
		{
			int name_len = uni_strlen(m_names[i]);
			new_names[i] = OP_NEWA(uni_char, name_len + 1); // FIXME:REPORT_MEMMAN-KILSMO

			if (new_names[i] == 0)
			{
				new_pa->m_used_len = i; // Makes the destructor delete just as many names/values as we have inserted.
				OP_DELETE(new_pa);
				return NULL;
			}

			uni_strcpy(new_names[i], m_names[i]);
		}
		else
		{
			new_names[i] = 0;
		}

		if (m_values[i])
		{
			int value_len = uni_strlen(m_values[i]);

			if (new_names[i] && value_len < 20)
				if ((uni_stri_eq(new_names[i], "WIDTH")) ||
					(uni_stri_eq(new_names[i], "HEIGHT")))
					value_len = 20;

			new_values[i] = OP_NEWA(uni_char, value_len + 1); // FIXME:REPORT_MEMMAN-KILSMO

			if (new_values[i] == 0)
			{
				new_pa->m_used_len = i + 1; // Makes the destructor delete just as many names/values as we have inserted.
				OP_DELETE(new_pa);
				return NULL;
			}

			uni_strcpy(new_values[i], m_values[i]);
		}
		else
		{
			new_values[i] = 0;
		}
	}

	return new_pa;
}


/**
 * CoordsAttr implementation
 **/

CoordsAttr*
CoordsAttr::Create(int len)
{
	CoordsAttr *attrs = OP_NEW(CoordsAttr, (len));
	if (!attrs)
		return NULL;

	// The same number will be decreased from memory usage count in the destructor, regardless of OOM.
	REPORT_MEMMAN_INC(len * sizeof(int));

	attrs->m_coords = OP_NEWA(int, len);
	if (!attrs->m_coords)
	{
		OP_DELETE(attrs);
		return NULL;
	}
	return attrs;
}

OP_STATUS CoordsAttr::CreateCopy(CoordsAttr** new_copy)
{
	OP_ASSERT(new_copy);
	CoordsAttr* copy = NEW_CoordsAttr((m_length));
	if (!copy || OpStatus::IsError(copy->m_original_string.Set(m_original_string)))
	{
		OP_DELETE(copy);
		return OpStatus::ERR_NO_MEMORY;
	}
	op_memcpy(copy->m_coords, m_coords, m_length*sizeof(int));
	*new_copy = copy;
	return OpStatus::OK;
}

/*static*/ OP_STATUS
UrlAndStringAttr::Create(const uni_char *url, unsigned url_length, UrlAndStringAttr*& url_attr)
{
	OP_ASSERT(url);

	/* UrlAndStringAttr::m_string is one uni_char long, so space for the
	   terminating null character is included in sizeof(UrlAndStringAttr). */
	size_t allocation_size = sizeof(UrlAndStringAttr) + url_length * sizeof(uni_char);

	url_attr = static_cast<UrlAndStringAttr *>(op_malloc(allocation_size));

	if (!url_attr)
		return OpStatus::ERR_NO_MEMORY;

	REPORT_MEMMAN_INC(allocation_size);

	url_attr->m_resolved_url = NULL;
	url_attr->m_string_length = url_length;

	op_memcpy(url_attr->m_string, url, url_length * sizeof(uni_char));
	url_attr->m_string[url_length] = 0;

	return OpStatus::OK;
}

/* static */ void
UrlAndStringAttr::Destroy(UrlAndStringAttr *url_attr)
{
	size_t allocation_size = sizeof(UrlAndStringAttr) + url_attr->m_string_length * sizeof(uni_char);

	OP_DELETE(url_attr->m_resolved_url);
	op_free(url_attr);

	REPORT_MEMMAN_DEC(allocation_size);
}

OP_STATUS UrlAndStringAttr::SetResolvedUrl(URL &url)
{
	if (m_resolved_url)
		OP_DELETE(m_resolved_url);

	m_resolved_url = OP_NEW(URL, (url));
	if (!m_resolved_url)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS UrlAndStringAttr::SetResolvedUrl(URL *url)
{
	if (m_resolved_url)
		OP_DELETE(m_resolved_url);

	if (url)
	{
		m_resolved_url = OP_NEW(URL, (*url));
		if (!m_resolved_url)
			return OpStatus::ERR_NO_MEMORY;
	}
	else
		m_resolved_url = NULL;

	return OpStatus::OK;
}
