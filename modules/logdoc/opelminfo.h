/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/** @file opelminfo.cpp
 *
 * Header and implementation of the element info used in the link panel
 *
 * @author Stig Halvorsen
 *
 */

#ifndef OP_ELM_INFO_H
#define OP_ELM_INFO_H

class URL;

#include "modules/util/opstring.h"
#include "modules/logdoc/link_type.h"

class OpElementInfo
{
public:

	enum EIType {
		UNKNOWN,
		CLINK,
		A,
		LINK,
		GO
	};

private:
	EIType				m_type;
	LinkType
						m_kind;
	URL*				m_url;
	const uni_char*		m_title;
	const uni_char*		m_alt;
	const uni_char*		m_rel;
	const uni_char*		m_rev;
	OpString			m_text;

	INT32				m_font_size;
	INT32				m_font_weight;
	INT32				m_font_color;

public:

	OpElementInfo(EIType type, LinkType kind = LINK_TYPE_OTHER) :
		m_type(type), m_kind(kind), m_url(NULL), m_title(NULL), m_alt(NULL),
		m_font_size(0), m_font_weight(0), m_font_color(0) {}

	EIType	GetType() { return m_type; }
	void	SetType(EIType type) { m_type = type; }

	LinkType		GetKind() { return m_kind; }

	const URL*		GetURL() { return m_url; }
	void			SetURL(URL* url) { m_url = url; }
	const uni_char*	GetTitle() { return m_title; }
	void			SetTitle(const uni_char *title) { m_title = title; }
	const uni_char*	GetAlt() { return m_alt; }
	void			SetAlt(const uni_char *alt) { m_alt = alt; }
	const uni_char*	GetRel() { return m_rel; }
	void			SetRel(const uni_char *rel) { m_rel = rel; }
	const uni_char*	GetRev() { return m_rev; }
	void			SetRev(const uni_char* rev) { m_rev = rev; }

	const uni_char*	GetText() { return m_text.CStr(); }
	OP_STATUS		SetText(const uni_char *txt) { return m_text.Set(txt); }
	OP_STATUS		AppendText(const uni_char *txt) { return m_text.Append(txt); }

	INT32	GetFontSize() { return m_font_size; }
	void	SetFontSize(INT32 size) { m_font_size = size; }
	INT32	GetFontWeight() { return m_font_weight; }
	void	SetFontWeight(INT32 weight) { m_font_weight = weight; }
	INT32	GetTextColor() { return m_font_color; }
	void	SetTextColor(INT32 color) { m_font_color = color; }

	// Include the first image thingy?

	const URL*	GetImageURL() { return NULL; }
	const uni_char*	GetImageTitle() { return NULL; }
	const uni_char*	GetImageAlt() { return NULL; }
};

#endif // OP_ELM_INFO_H
