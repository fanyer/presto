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

/* This is the implementation of Cache Operation binary XML
 * decoder, as specified in WAP-175-CacheOp-20010731-a.
 * WBXML MIME type:      application/vnd.wap.coc
 * XML MIME type:        text/vnd.wap.co
 * The specification also notes: These types are not yet registered with the IANA, and are consequently experimental media types.
 */

CONST_ARRAY(CacheOperation_WBXML_tag_tokens, uni_char*)
	CONST_ENTRY(UNI_L("co")), // 0x05
	CONST_ENTRY(UNI_L("invalidate-object")),
	CONST_ENTRY(UNI_L("invalidate-service"))
CONST_END(CacheOperation_WBXML_tag_tokens)

CONST_ARRAY(CacheOperation_WBXML_attr_start_tokens, uni_char*)
	CONST_ENTRY(UNI_L("uri=\"")), // 0x05
	CONST_ENTRY(UNI_L("uri=\"http://")),
	CONST_ENTRY(UNI_L("uri=\"http://www.")),
	CONST_ENTRY(UNI_L("uri=\"https://")),
	CONST_ENTRY(UNI_L("uri=\"https://www."))
CONST_END(CacheOperation_WBXML_attr_start_tokens)

CONST_ARRAY(CacheOperation_WBXML_attr_value_tokens, uni_char*)
	CONST_ENTRY(UNI_L(".com/")), // 0x85
	CONST_ENTRY(UNI_L(".edu/")),
	CONST_ENTRY(UNI_L(".net/")),
	CONST_ENTRY(UNI_L(".org/"))
CONST_END(CacheOperation_WBXML_attr_value_tokens)

class CacheOperation_WBXML_ContentHandler : public WBXML_ContentHandler
{
public:
	CacheOperation_WBXML_ContentHandler(WBXML_Parser *parser)
		: WBXML_ContentHandler(parser) {}

	OP_WXP_STATUS Init();

	const uni_char *TagHandler(UINT32 tok);
	const uni_char *AttrStartHandler(UINT32 tok);
	const uni_char *AttrValueHandler(UINT32 tok);
};

OP_WXP_STATUS CacheOperation_WBXML_ContentHandler::Init()
{
	return OpStatus::OK;
}

const uni_char *CacheOperation_WBXML_ContentHandler::TagHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x07)
		return 0;

	return CacheOperation_WBXML_tag_tokens[tok - 0x05];
}

const uni_char *CacheOperation_WBXML_ContentHandler::AttrStartHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x09)
		return 0;

	return CacheOperation_WBXML_attr_start_tokens[tok - 0x05];
}

const uni_char *CacheOperation_WBXML_ContentHandler::AttrValueHandler(UINT32 tok)
{
	if (tok < 0x85 || tok > 0x88)
		return 0;

	return CacheOperation_WBXML_attr_value_tokens[tok - 0x85];
}
