/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
**
*/

/* This is the implementation of WAP Service Loading binary XML decoder, as
   specified in WAP-168-ServiceLoad-20010731-a */


CONST_ARRAY(ServiceLoad_WBXML_attr_start_tokens, uni_char*)
	CONST_ENTRY(UNI_L("action=\"execute-low")), // 0x05
	CONST_ENTRY(UNI_L("action=\"execute-high")),
	CONST_ENTRY(UNI_L("action=\"cache")),
	CONST_ENTRY(UNI_L("href=\"")),
	CONST_ENTRY(UNI_L("href=\"http://")),
	CONST_ENTRY(UNI_L("href=\"http://www.")),
	CONST_ENTRY(UNI_L("href=\"https://")),
	CONST_ENTRY(UNI_L("href=\"https://www."))
CONST_END(ServiceLoad_WBXML_attr_start_tokens)

CONST_ARRAY(ServiceLoad_WBXML_attr_value_tokens, uni_char*)
	CONST_ENTRY(UNI_L(".com/")), // 0x85
	CONST_ENTRY(UNI_L(".edu/")),
	CONST_ENTRY(UNI_L(".net/")),
	CONST_ENTRY(UNI_L(".org/"))
CONST_END(ServiceLoad_WBXML_attr_value_tokens)

class ServiceLoad_WBXML_ContentHandler : public WBXML_ContentHandler
{
public:
	ServiceLoad_WBXML_ContentHandler(WBXML_Parser *parser)
		: WBXML_ContentHandler(parser) {}

	OP_WXP_STATUS Init();

	const uni_char *TagHandler(UINT32 tok);
	const uni_char *AttrStartHandler(UINT32 tok);
	const uni_char *AttrValueHandler(UINT32 tok);
};

OP_WXP_STATUS ServiceLoad_WBXML_ContentHandler::Init()
{
	return OpStatus::OK;
}

const uni_char *ServiceLoad_WBXML_ContentHandler::TagHandler(UINT32 tok)
{
	return tok == 5 ? UNI_L("sl") : 0;
}

const uni_char *ServiceLoad_WBXML_ContentHandler::AttrStartHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x0c)
		return 0;

	return ServiceLoad_WBXML_attr_start_tokens[tok - 0x05];
}

const uni_char *ServiceLoad_WBXML_ContentHandler::AttrValueHandler(UINT32 tok)
{
	if (tok < 0x85 || tok > 0x88)
		return 0;

	return ServiceLoad_WBXML_attr_value_tokens[tok - 0x85];
}
