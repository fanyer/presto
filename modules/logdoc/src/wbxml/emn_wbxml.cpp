/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2004-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
**
*/

/* This is the implementation of WAP E-Mail Notification binary XML decoder, as
   specified in OMA-Push-EMN-v1_0-20020830-C */


CONST_ARRAY(EmailNotification_WBXML_tag_tokens, uni_char*)
	CONST_ENTRY(UNI_L("Emn"))
CONST_END(EmailNotification_WBXML_tag_tokens)

CONST_ARRAY(EmailNotification_WBXML_attr_start_tokens, uni_char*)
	CONST_ENTRY(UNI_L("timestamp=\"")),
	CONST_ENTRY(UNI_L("mailbox=\"")),
	CONST_ENTRY(UNI_L("mailbox=\"mailat:")),
	CONST_ENTRY(UNI_L("mailbox=\"pop://")),
	CONST_ENTRY(UNI_L("mailbox=\"imap://")),
	CONST_ENTRY(UNI_L("mailbox=\"http://")),
	CONST_ENTRY(UNI_L("mailbox=\"http://www.")),
	CONST_ENTRY(UNI_L("mailbox=\"https://")),
	CONST_ENTRY(UNI_L("mailbox=\"https://www."))
CONST_END(EmailNotification_WBXML_attr_start_tokens)

CONST_ARRAY(EmailNotification_WBXML_attr_value_tokens, uni_char*)
	CONST_ENTRY(UNI_L(".com")),
	CONST_ENTRY(UNI_L(".edu")),
	CONST_ENTRY(UNI_L(".net")),
	CONST_ENTRY(UNI_L(".org"))
CONST_END(EmailNotification_WBXML_attr_value_tokens)

class EmailNotification_WBXML_ContentHandler : public WBXML_ContentHandler
{
public:
	EmailNotification_WBXML_ContentHandler(WBXML_Parser *parser)
		: WBXML_ContentHandler(parser) {}

	OP_WXP_STATUS Init();

	const uni_char *TagHandler(UINT32 tok);
	const uni_char *AttrStartHandler(UINT32 tok);
	const uni_char *AttrValueHandler(UINT32 tok);
	const uni_char *OpaqueAttrHandlerL(char **buf);
};

OP_WXP_STATUS EmailNotification_WBXML_ContentHandler::Init()
{
	return OpStatus::OK;
}

const uni_char *EmailNotification_WBXML_ContentHandler::TagHandler(UINT32 tok)
{
	if (tok != 0x05)
		return 0;

	return EmailNotification_WBXML_tag_tokens[tok - 0x05];
}

const uni_char *EmailNotification_WBXML_ContentHandler::AttrStartHandler(UINT32 tok)
{
	if (tok < 0x05 || tok > 0x0d)
		return 0;

	return EmailNotification_WBXML_attr_start_tokens[tok - 0x05];
}

const uni_char *EmailNotification_WBXML_ContentHandler::AttrValueHandler(UINT32 tok)
{
	if (tok < 0x85 || tok > 0x88)
		return 0;

	return EmailNotification_WBXML_attr_value_tokens[tok - 0x85];
}

const uni_char *EmailNotification_WBXML_ContentHandler::OpaqueAttrHandlerL(char **buf)
{
	return DecodeDateTime(GetParser(), buf);
}
