/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _SVG_H_
#define _SVG_H_

#ifdef SVG_SUPPORT

#include "modules/logdoc/html5namemapper.h"

//-----------------------------------------------------------
// the SVG lexer
//-----------------------------------------------------------

class SVG_Lex
{
private:
public:
    SVG_Lex() {}
    ~SVG_Lex() {}

    static Markup::Type 	GetElementType(const uni_char *tag, int tag_len)
	{
		return g_html5_name_mapper->GetTypeFromName(tag, tag_len, TRUE, Markup::SVG);
	}
    static Markup::AttrType    	GetAttrType(const uni_char *attr, int len = -1)
	{
		if (len < 0)
			return g_html5_name_mapper->GetAttrTypeFromName(attr, TRUE, Markup::SVG);
		else
			return g_html5_name_mapper->GetAttrTypeFromName(attr, len, TRUE, Markup::SVG);
	}

	static const uni_char*	GetElementString(Markup::Type elem)
	{
		return g_html5_name_mapper->GetNameFromType(elem, Markup::SVG, FALSE);
	}
	static const uni_char*	GetAttrString(Markup::AttrType attr)
	{
		return g_html5_name_mapper->GetAttrNameFromType(attr, Markup::SVG, FALSE);
	}
};

#endif // SVG_SUPPORT

#ifdef XLINK_SUPPORT
//-----------------------------------------------------------
// the XLink lexer
//-----------------------------------------------------------

class XLink_Lex
{
private:
    XLink_Lex(); // Not implemented. Only static functions.
    ~XLink_Lex(); // Not implemented. Only static functions.
public:
    static Markup::AttrType   GetAttrType(const uni_char *attr, int len = -1)
	{
		if (len < 0)
			return g_html5_name_mapper->GetAttrTypeFromName(attr, TRUE, Markup::XLINK);
		else
			return g_html5_name_mapper->GetAttrTypeFromName(attr, len, TRUE, Markup::XLINK);
	}
	static const uni_char*	GetAttrString(Markup::AttrType attr)
	{
		return g_html5_name_mapper->GetAttrNameFromType(attr, Markup::XLINK, FALSE);
	}
};

#endif // XLINK_SUPPORT
#endif // _SVG_H
