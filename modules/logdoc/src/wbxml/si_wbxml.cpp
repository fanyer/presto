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

/* This is the implementation of WAP Service Indication binary XML decoder, as
   specified in WAP-167-ServiceInd-20010731-a */


CONST_ARRAY(ServiceInd_WBXML_tag_tokens, uni_char*)
	CONST_ENTRY(UNI_L("si")),
	CONST_ENTRY(UNI_L("indication")),
	CONST_ENTRY(UNI_L("info")),
	CONST_ENTRY(UNI_L("item"))
CONST_END(ServiceInd_WBXML_tag_tokens)

CONST_ARRAY(ServiceInd_WBXML_attr_start_tokens, uni_char*)
	CONST_ENTRY(UNI_L("action=\"signal-none")),
	CONST_ENTRY(UNI_L("action=\"signal-low")),
	CONST_ENTRY(UNI_L("action=\"signal-medium")),
	CONST_ENTRY(UNI_L("action=\"signal-high")),
	CONST_ENTRY(UNI_L("action=\"delete")),
	CONST_ENTRY(UNI_L("created=\"")),
	CONST_ENTRY(UNI_L("href=\"")),
	CONST_ENTRY(UNI_L("href=\"http://")),
	CONST_ENTRY(UNI_L("href=\"http://www.")),
	CONST_ENTRY(UNI_L("href=\"https://")),
	CONST_ENTRY(UNI_L("href=\"https://www.")),
	CONST_ENTRY(UNI_L("si-expires=\"")),
	CONST_ENTRY(UNI_L("si-id=\"")),
	CONST_ENTRY(UNI_L("class=\""))
CONST_END(ServiceInd_WBXML_attr_start_tokens)

CONST_ARRAY(ServiceInd_WBXML_attr_value_tokens, uni_char*)
	CONST_ENTRY(UNI_L(".com/")),
	CONST_ENTRY(UNI_L(".edu/")),
	CONST_ENTRY(UNI_L(".net/")),
	CONST_ENTRY(UNI_L(".org/"))
CONST_END(ServiceInd_WBXML_attr_value_tokens)

class ServiceInd_WBXML_ContentHandler : public WBXML_ContentHandler
{
public:
	ServiceInd_WBXML_ContentHandler(WBXML_Parser *parser)
		: WBXML_ContentHandler(parser) {}

	OP_WXP_STATUS Init();

	const uni_char *TagHandler(UINT32 tok);
	const uni_char *AttrStartHandler(UINT32 tok);
	const uni_char *AttrValueHandler(UINT32 tok);
	const uni_char *OpaqueAttrHandlerL(char **buf);
};

OP_WXP_STATUS ServiceInd_WBXML_ContentHandler::Init()
{
	return OpStatus::OK;
}

const uni_char *ServiceInd_WBXML_ContentHandler::TagHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x08)
		return 0;

	return ServiceInd_WBXML_tag_tokens[tok - 0x05];
}

const uni_char *ServiceInd_WBXML_ContentHandler::AttrStartHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x12)
		return 0;

	return ServiceInd_WBXML_attr_start_tokens[tok - 0x05];
}

const uni_char *ServiceInd_WBXML_ContentHandler::AttrValueHandler(UINT32 tok)
{
	if (tok < 0x85 || tok > 0x88)
		return 0;

	return ServiceInd_WBXML_attr_value_tokens[tok - 0x85];
}

const uni_char *ServiceInd_WBXML_ContentHandler::OpaqueAttrHandlerL(char **buf)
{
	return DecodeDateTime(GetParser(), buf);
}
