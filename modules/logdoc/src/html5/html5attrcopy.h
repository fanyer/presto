/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5ATTRCOPY_H
#define HTML5ATTRCOPY_H

#include "modules/logdoc/markup.h"
#include "modules/logdoc/src/html5/html5tokenbuffer.h"

class HTML5Attr;

/**
 * HTML5AttrCopy is used to hold a copy of the attributes found at tokenization
 * when storing the tokens for elements that may have to be regenerated from the
 * original token data, for instance during adoption.
 * In standalone mode it is also used for storing attributes in the nodes in
 * the logical tree.
 */
class HTML5AttrCopy
{
public:
#ifdef HTML5_STANDALONE
	friend class H5Element;
#endif // HTML5_STANDALONE

	HTML5AttrCopy() : m_name(0), m_value(NULL), m_ns(Markup::HTML) {}
	~HTML5AttrCopy();

	void		CopyL(HTML5Attr *src);
	void		CopyL(HTML5AttrCopy *src);

	HTML5TokenBuffer*
				GetName() { return &m_name; }
	uni_char*	GetValue() const { return m_value; }

	void		SetNameL(const uni_char* name, unsigned length);
	void		SetNameL(HTML5TokenBuffer *name);
	void		SetValueL(const uni_char* value, unsigned length);

	Markup::Ns	GetNs() const { return m_ns; }
	void		SetNs(Markup::Ns ns) { m_ns = ns; }

	BOOL		IsEqual(HTML5Attr *attr);

private:
	HTML5TokenBuffer	m_name;
	uni_char*			m_value;
	Markup::Ns			m_ns;
};

#endif // HTML5ATTRCOPY_H
