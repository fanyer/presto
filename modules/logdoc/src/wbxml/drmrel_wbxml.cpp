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

/* This is the implementation of OMA DRM Rights Expression Language binary XML
 * decoder, as specified in OMA-TDownloadT-TDRMRELT-V1_0-20031031-C.
 * WBXML MIME type:      application/vnd.oma.drm.rights+wbxml
 * XML MIME type:        application/vnd.oma.drm.rights+xml
 * WBXML file extension: .drc
 * XML file extension:   .dr
 */


CONST_ARRAY(RightsExpressionLanguage_WBXML_tag_tokens, uni_char*)
	CONST_ENTRY(UNI_L("o-ex:rights")), // 0x05
	CONST_ENTRY(UNI_L("o-ex:context")),
	CONST_ENTRY(UNI_L("o-dd:version")),
	CONST_ENTRY(UNI_L("o-dd:uid")),
	CONST_ENTRY(UNI_L("o-ex:agreement")),
	CONST_ENTRY(UNI_L("o-ex:asset")),
	CONST_ENTRY(UNI_L("ds:KeyInfo")),
	CONST_ENTRY(UNI_L("ds:KeyValue")),
	CONST_ENTRY(UNI_L("o-ex:permission")),
	CONST_ENTRY(UNI_L("o-dd:play")),
	CONST_ENTRY(UNI_L("o-dd:display")),
	CONST_ENTRY(UNI_L("o-dd:execute")),
	CONST_ENTRY(UNI_L("o-dd:print")),
	CONST_ENTRY(UNI_L("o-ex:constraint")),
	CONST_ENTRY(UNI_L("o-dd:count")),
	CONST_ENTRY(UNI_L("o-dd:datetime")),
	CONST_ENTRY(UNI_L("o-dd:start")),
	CONST_ENTRY(UNI_L("o-dd:end")),
	CONST_ENTRY(UNI_L("o-dd:interval")) // 0x17
CONST_END(RightsExpressionLanguage_WBXML_tag_tokens)

CONST_ARRAY(RightsExpressionLanguage_WBXML_attr_start_tokens, uni_char*)
	CONST_ENTRY(UNI_L("xmlns:o-ex=\"")), // 0x05
	CONST_ENTRY(UNI_L("xmlns:o-dd=\"")),
	CONST_ENTRY(UNI_L("xmlns:ds=\""))
CONST_END(RightsExpressionLanguage_WBXML_attr_start_tokens)

CONST_ARRAY(RightsExpressionLanguage_WBXML_attr_value_tokens, uni_char*)
	CONST_ENTRY(UNI_L("http://odrl.net/1.1/ODRL-EX")), // 0x85
	CONST_ENTRY(UNI_L("http://odrl.net/1.1/ODRL-DD")),
	CONST_ENTRY(UNI_L("http://www.w3.org/2000/09/xmldsig#/"))
CONST_END(RightsExpressionLanguage_WBXML_attr_value_tokens)

class RightsExpressionLanguage_WBXML_ContentHandler : public WBXML_ContentHandler
{
public:
	RightsExpressionLanguage_WBXML_ContentHandler(WBXML_Parser *parser)
		: WBXML_ContentHandler(parser) {}

	OP_WXP_STATUS Init();

	const uni_char *TagHandler(UINT32 tok);
	const uni_char *AttrStartHandler(UINT32 tok);
	const uni_char *AttrValueHandler(UINT32 tok);
	const uni_char *OpaqueTextNodeHandlerL(char **buf);
};

OP_WXP_STATUS RightsExpressionLanguage_WBXML_ContentHandler::Init()
{
	return OpStatus::OK;
}

const uni_char *RightsExpressionLanguage_WBXML_ContentHandler::TagHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x17)
		return 0;

	return RightsExpressionLanguage_WBXML_tag_tokens[tok - 0x05];
}

const uni_char *RightsExpressionLanguage_WBXML_ContentHandler::AttrStartHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x07)
		return 0;

	return RightsExpressionLanguage_WBXML_attr_start_tokens[tok - 0x05];
}

const uni_char *RightsExpressionLanguage_WBXML_ContentHandler::AttrValueHandler(UINT32 tok)
{
	if (tok < 0x85 || tok > 0x87)
		return 0;

	return RightsExpressionLanguage_WBXML_attr_value_tokens[tok - 0x85];
}

const uni_char *RightsExpressionLanguage_WBXML_ContentHandler::OpaqueTextNodeHandlerL(char **buf)
{
	UINT32 len = GetParser()->GetNextTokenL(buf, TRUE);

	char *base64 = OP_NEWA_L(char, (len+2)*4/3+1);
	ANCHOR_ARRAY(char, base64);
	int UU_encode(unsigned char *bufin, unsigned int nbytes,
				  char* bufcoded);
	int encoded_len = UU_encode((unsigned char *)(*buf), len, base64);
	(*buf) += len;
	GetParser()->EnsureTmpBufLenL(encoded_len);
	uni_char *p = GetParser()->GetTmpBuf();
	for (int i=0; i<encoded_len; i++)
		p[i] = (unsigned char)base64[i];
	p[encoded_len] = 0;
	ANCHOR_ARRAY_DELETE(base64);
	return p;
}
