/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include "modules/logdoc/logdocenum.h"

#include "modules/hardcore/mem/mem_man.h"

class HtmlAttrEntry;
class HLDocProfile;

#define NEW_HTML_Attributes(n)  OP_NEWA(AttrItem, n)
#define NEW_PrivateAttrs(x) (PrivateAttrs::Create x )
#define NEW_CoordsAttr(x)   (CoordsAttr::Create x )

#define DELETE_HTML_Attributes(a)   OP_DELETEA((AttrItem*) a)
#define DELETE_PrivateAttrs(a)  OP_DELETE(a)
#define DELETE_CoordsAttr(a)    OP_DELETE(a)

class AttrItem
{
private:
	union {
		struct {
			unsigned int attr:12;
			unsigned int ns:8;
			unsigned int special:1;
			unsigned int type:4;
			unsigned int need_free:1;
			unsigned int specified:1;
			unsigned int id:1;
			unsigned int event:1;
			// used 29
		} packed;
		unsigned int init;
	} m_info;
	void*			m_value;

public:

			AttrItem();
			~AttrItem();

	Markup::AttrType
				GetAttr() { return static_cast<Markup::AttrType>(m_info.packed.attr); }
	int			GetNsIdx() { return m_info.packed.ns; }

	ItemType	GetType() { return (ItemType)m_info.packed.type; }

	BOOL	NeedFree() { return m_info.packed.need_free; }
	void	SetNeedFree(BOOL need_free) { m_info.packed.need_free = need_free; }
	BOOL	IsSpecified() { return m_info.packed.specified; }
	BOOL	IsSpecial() { return m_info.packed.special; }
	BOOL	IsId() { return m_info.packed.id; }
	BOOL	IsEvent() { return m_info.packed.event; }

	void*	GetValue() { return m_value; }

	void	Replace(short attr, ItemType item_type, void* value, int ns_idx, BOOL need_free, BOOL is_special, BOOL is_id, BOOL is_specified, BOOL is_event);
	void	Set(short attr, ItemType item_type, void* value, int ns_idx, BOOL need_free, BOOL is_special, BOOL is_id, BOOL is_specified, BOOL is_event);

	void	Clean();
	static void	Clean(ItemType item_type, void* value);
};

class PrivateAttrs
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

private:

	int				m_used_len;
	int				m_length;
	uni_char**		m_names;
	uni_char**		m_values;

					PrivateAttrs();
	OP_STATUS		SetName(HTML_ElementType type,
						HtmlAttrEntry* hae,
						int idx
#ifdef _APPLET_2_EMBED_
						, BOOL &has_codebase
#endif // _APPLET_2_EMBED_
					);
	OP_STATUS		SetValue(HLDocProfile* hld_profile,
						HTML_ElementType type,
						HtmlAttrEntry* hae,
						int idx);
public:

	~PrivateAttrs();
	static PrivateAttrs *Create(int len);

	int				GetLength() { return m_used_len; }
	uni_char**		GetNames() { return m_names; }
	uni_char**		GetValues() { return m_values; }

	OP_STATUS		ProcessAttributes(HLDocProfile* hld_profile, HTML_ElementType type, HtmlAttrEntry *ha_list);
#ifdef _APPLET_2_EMBED_
	OP_STATUS		AddAttribute(HLDocProfile* hld_profile, HTML_ElementType type, HtmlAttrEntry &ha_entry, BOOL &has_codebase);
#else // _APPLET_2_EMBED_
	OP_STATUS		AddAttribute(HLDocProfile* hld_profile, HTML_ElementType type, HtmlAttrEntry &ha_entry);
#endif // _APPLET_2_EMBED_
	OP_STATUS		SetAttribute(HLDocProfile* hld_profile, HTML_ElementType type, HtmlAttrEntry &ha_entry);

	PrivateAttrs*	GetCopy(int new_len);
};


class CoordsAttr
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

private:

	int				m_length;
	int*			m_coords;
	OpString m_original_string;

					CoordsAttr(int len) : m_length(len), m_coords(NULL) { }
public:
	static CoordsAttr *Create(int len);
					~CoordsAttr()
					{
						REPORT_MEMMAN_DEC(m_length * sizeof(int));
						OP_DELETEA(m_coords);
					}

	OP_STATUS		CreateCopy(CoordsAttr** new_copy);
	OP_STATUS		SetOriginalString(const uni_char* string, unsigned len) { return m_original_string.Set(string, len); }
	int				GetLength() { return m_length; }
	int*			GetCoords() { return m_coords; }
	const uni_char*	GetOriginalString() { return m_original_string.CStr(); }
};

class UrlAndStringAttr
{
private:
	UrlAndStringAttr() {}
	~UrlAndStringAttr() {}

	URL*		m_resolved_url;
	unsigned	m_string_length;
	uni_char    m_string[1]; /* ARRAY OK 2011-04-21 jl */

public:
	/**
	 * @param[in] copy_string If TRUE the string |url| will be copied. If FALSE it will be used
	 * directly and owned by the UrlAndString object. REPORT_MEMMAN_INC will be done for the string
	 * either way.
	 */
	static OP_STATUS	Create(const uni_char *url, unsigned url_length, UrlAndStringAttr*& url_attr);

	static void Destroy(UrlAndStringAttr *url_attr);

	uni_char*	GetString() { return m_string; }
	unsigned	GetStringLength() { return m_string_length; }

	URL*		GetResolvedUrl() { return m_resolved_url; }
	OP_STATUS	SetResolvedUrl(URL &url);
	OP_STATUS	SetResolvedUrl(URL *url);
};

#endif // ATTRIBUTE_H
