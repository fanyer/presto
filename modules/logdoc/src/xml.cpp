/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
*/

#include "core/pch.h"

#include "modules/logdoc/src/xml.h"
#include "modules/xmlutils/xmldocumentinfo.h"

CONST_ARRAY(XML_tag_name, uni_char*)
    CONST_ENTRY(UNI_L("id")),
    CONST_ENTRY(UNI_L("lang")),
    CONST_ENTRY(UNI_L("base")),
    CONST_ENTRY(UNI_L("space"))
CONST_END(XML_tag_name)

/*static*/ const uni_char*
XML_Lex::GetAttrString(XML_AttrType attr)
{
	if (attr >= XMLA_ID && attr <= XMLA_SPACE)
		return XML_tag_name[attr - XMLA_ID];

	return NULL;
}

/*static*/ XML_AttrType
XML_Lex::GetAttrType(const uni_char *attr, int attr_len)
{
	switch (attr_len)
	{
	case 2:
		if (uni_strncmp(attr, UNI_L("id"), attr_len) == 0)
			return XMLA_ID;
		break;
	case 4:
		if (uni_strncmp(attr, UNI_L("lang"), attr_len) == 0)
			return XMLA_LANG;
		else if (uni_strncmp(attr, UNI_L("base"), attr_len) == 0)
			return XMLA_BASE;
		break;
	case 5:
		if (uni_strncmp(attr, UNI_L("space"), attr_len) == 0)
			return XMLA_SPACE;
		break;
	}

	return XMLA_UNKNOWN;
}

/* static */ OP_STATUS
XMLDocumentInfoAttr::Make(XMLDocumentInfoAttr *&attr, const XMLDocumentInformation *docinfo)
{
	OP_ASSERT(docinfo->GetDoctypeDeclarationPresent());

	attr = OP_NEW(XMLDocumentInfoAttr, ());

	if (!attr)
		return OpStatus::ERR_NO_MEMORY;

	attr->docinfo = OP_NEW(XMLDocumentInformation, ());

	if (!attr->docinfo || OpStatus::IsMemoryError(attr->docinfo->SetDoctypeDeclaration(docinfo->GetDoctypeName(), docinfo->GetPublicId(), docinfo->GetSystemId(), docinfo->GetInternalSubset())))
	{
		OP_DELETE(attr);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (docinfo->GetDoctype())
		attr->docinfo->SetDoctype(docinfo->GetDoctype());

	return OpStatus::OK;
}

/* virtual */
XMLDocumentInfoAttr::~XMLDocumentInfoAttr()
{
	OP_DELETE(docinfo);
}

/* virtual */ OP_STATUS
XMLDocumentInfoAttr::CreateCopy(ComplexAttr **copy_to)
{
	XMLDocumentInfoAttr *attr;

	RETURN_IF_ERROR(Make(attr, docinfo));

	*copy_to = attr;
	return OpStatus::OK;
}
