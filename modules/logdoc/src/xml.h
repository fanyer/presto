/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef XML_H
#define XML_H

#include "modules/logdoc/xmlenum.h"
#include "modules/logdoc/complex_attr.h"

class XML_Lex
{
private:
public:
	XML_Lex() {}
	~XML_Lex() {}

	static const uni_char*  GetElementString(XML_ElementType type) { return NULL; }
	static const uni_char*  GetAttrString(XML_AttrType attr);

	static XML_ElementType GetElementType(const uni_char *tag) { return XMLE_UNKNOWN; }
	static XML_ElementType GetElementType(const uni_char *tag, int tag_len) { return XMLE_UNKNOWN; }
	static XML_AttrType    GetAttrType(const uni_char *attr) { return GetAttrType(attr, uni_strlen(attr)); }
	static XML_AttrType    GetAttrType(const uni_char *attr, int attr_len);
};

class XMLDocumentInformation;

class XMLDocumentInfoAttr
  : public ComplexAttr
{
private:
	XMLDocumentInformation *docinfo;

	XMLDocumentInfoAttr()
		: docinfo(NULL)
	{
	}

public:
	static OP_STATUS Make(XMLDocumentInfoAttr *&attr, const XMLDocumentInformation *docinfo);

	virtual ~XMLDocumentInfoAttr();
	virtual OP_STATUS CreateCopy(ComplexAttr **copy_to);

	const XMLDocumentInformation *GetDocumentInfo() { return docinfo; }
};

#endif // XML_H
