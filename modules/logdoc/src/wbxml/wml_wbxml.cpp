/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

CONST_ARRAY(WML_WBXML_tag_tokens, uni_char*)
	CONST_ENTRY(UNI_L("pre")),
	CONST_ENTRY(UNI_L("a")),
	CONST_ENTRY(UNI_L("td")),
	CONST_ENTRY(UNI_L("tr")),
	CONST_ENTRY(UNI_L("table")),
	CONST_ENTRY(UNI_L("p")),
	CONST_ENTRY(UNI_L("postfield")),
	CONST_ENTRY(UNI_L("anchor")),
	CONST_ENTRY(UNI_L("access")),
	CONST_ENTRY(UNI_L("b")),
	CONST_ENTRY(UNI_L("big")),
	CONST_ENTRY(UNI_L("br")),
	CONST_ENTRY(UNI_L("card")),
	CONST_ENTRY(UNI_L("do")),
	CONST_ENTRY(UNI_L("em")),
	CONST_ENTRY(UNI_L("fieldset")),
	CONST_ENTRY(UNI_L("go")),
	CONST_ENTRY(UNI_L("head")),
	CONST_ENTRY(UNI_L("i")),
	CONST_ENTRY(UNI_L("img")),
	CONST_ENTRY(UNI_L("input")),
	CONST_ENTRY(UNI_L("meta")),
	CONST_ENTRY(UNI_L("noop")),
	CONST_ENTRY(UNI_L("prev")),
	CONST_ENTRY(UNI_L("onevent")),
	CONST_ENTRY(UNI_L("optgroup")),
	CONST_ENTRY(UNI_L("option")),
	CONST_ENTRY(UNI_L("refresh")),
	CONST_ENTRY(UNI_L("select")),
	CONST_ENTRY(UNI_L("small")),
	CONST_ENTRY(UNI_L("strong")),
	CONST_ENTRY(UNI_L("")), // 0x3A
	CONST_ENTRY(UNI_L("template")),
	CONST_ENTRY(UNI_L("timer")),
	CONST_ENTRY(UNI_L("u")),
	CONST_ENTRY(UNI_L("setvar")),
	CONST_ENTRY(UNI_L("wml"))
CONST_END(WML_WBXML_tag_tokens)

CONST_ARRAY(WML_WBXML_attr_start_tokens, uni_char*)
	CONST_ENTRY(UNI_L("accept-charset=\"")),
	CONST_ENTRY(UNI_L("align=\"bottom")),
	CONST_ENTRY(UNI_L("align=\"center")),
	CONST_ENTRY(UNI_L("align=\"left")),
	CONST_ENTRY(UNI_L("align=\"middle")),
	CONST_ENTRY(UNI_L("align=\"right")),
	CONST_ENTRY(UNI_L("align=\"top")),
	CONST_ENTRY(UNI_L("alt=\"")),
	CONST_ENTRY(UNI_L("content=\"")),
	CONST_ENTRY(UNI_L("")), // 0x0E
	CONST_ENTRY(UNI_L("domain=\"")),
	CONST_ENTRY(UNI_L("emptyok=\"false")),
	CONST_ENTRY(UNI_L("emptyok=\"true")),
	CONST_ENTRY(UNI_L("format=\"")),
	CONST_ENTRY(UNI_L("height=\"")),
	CONST_ENTRY(UNI_L("hspace=\"")),
	CONST_ENTRY(UNI_L("ivalue=\"")),
	CONST_ENTRY(UNI_L("iname=\"")),
	CONST_ENTRY(UNI_L("")), // 0x17
	CONST_ENTRY(UNI_L("label=\"")),
	CONST_ENTRY(UNI_L("localsrc=\"")),
	CONST_ENTRY(UNI_L("maxlength=\"")),
	CONST_ENTRY(UNI_L("method=\"get")),
	CONST_ENTRY(UNI_L("method=\"post")),
	CONST_ENTRY(UNI_L("mode=\"nowrap")),
	CONST_ENTRY(UNI_L("mode=\"wrap")),
	CONST_ENTRY(UNI_L("multiple=\"false")),
	CONST_ENTRY(UNI_L("multiple=\"true")),
	CONST_ENTRY(UNI_L("name=\"")),
	CONST_ENTRY(UNI_L("newcontext=\"false")),
	CONST_ENTRY(UNI_L("newcontext=\"true")),
	CONST_ENTRY(UNI_L("onpick=\"")),
	CONST_ENTRY(UNI_L("onenterbackward=\"")),
	CONST_ENTRY(UNI_L("onenterforward=\"")),
	CONST_ENTRY(UNI_L("ontimer=\"")),
	CONST_ENTRY(UNI_L("optional=\"false")),
	CONST_ENTRY(UNI_L("optional=\"true")),
	CONST_ENTRY(UNI_L("path=\"")),
	CONST_ENTRY(UNI_L("")), // 0x2B
	CONST_ENTRY(UNI_L("")), // 0x2C
	CONST_ENTRY(UNI_L("")), // 0x2D
	CONST_ENTRY(UNI_L("scheme=\"")),
	CONST_ENTRY(UNI_L("sendreferer=\"false")),
	CONST_ENTRY(UNI_L("sendreferer=\"true")),
	CONST_ENTRY(UNI_L("size=\"")),
	CONST_ENTRY(UNI_L("src=\"")),
	CONST_ENTRY(UNI_L("ordered=\"true")),
	CONST_ENTRY(UNI_L("ordered=\"false")),
	CONST_ENTRY(UNI_L("tabindex=\"")),
	CONST_ENTRY(UNI_L("title=\"")),
	CONST_ENTRY(UNI_L("type=\"")),
	CONST_ENTRY(UNI_L("type=\"accept")),
	CONST_ENTRY(UNI_L("type=\"delete")),
	CONST_ENTRY(UNI_L("type=\"help")),
	CONST_ENTRY(UNI_L("type=\"password")),
	CONST_ENTRY(UNI_L("type=\"onpick")),
	CONST_ENTRY(UNI_L("type=\"onenterbackward")),
	CONST_ENTRY(UNI_L("type=\"onenterforward")),
	CONST_ENTRY(UNI_L("type=\"ontimer")),
	CONST_ENTRY(UNI_L("")), // 0x40
	CONST_ENTRY(UNI_L("")), // 0x41
	CONST_ENTRY(UNI_L("")), // 0x42
	CONST_ENTRY(UNI_L("")), // 0x43
	CONST_ENTRY(UNI_L("")), // 0x44
	CONST_ENTRY(UNI_L("type=\"options")),
	CONST_ENTRY(UNI_L("type=\"prev")),
	CONST_ENTRY(UNI_L("type=\"reset")),
	CONST_ENTRY(UNI_L("type=\"text")),
	CONST_ENTRY(UNI_L("type=\"vnd.")),
	CONST_ENTRY(UNI_L("href=\"")),
	CONST_ENTRY(UNI_L("href=\"http://")),
	CONST_ENTRY(UNI_L("href=\"https://")),
	CONST_ENTRY(UNI_L("value=\"")),
	CONST_ENTRY(UNI_L("vspace=\"")),
	CONST_ENTRY(UNI_L("width=\"")),
	CONST_ENTRY(UNI_L("xml:lang=\"")),
	CONST_ENTRY(UNI_L("")), // 0x51
	CONST_ENTRY(UNI_L("align=\"")),
	CONST_ENTRY(UNI_L("columns=\"")),
	CONST_ENTRY(UNI_L("class=\"")),
	CONST_ENTRY(UNI_L("id=\"")),
	CONST_ENTRY(UNI_L("forua=\"false")),
	CONST_ENTRY(UNI_L("forua=\"true")),
	CONST_ENTRY(UNI_L("src=\"http://")),
	CONST_ENTRY(UNI_L("src=\"https://")),
	CONST_ENTRY(UNI_L("http-equiv=\"")),
	CONST_ENTRY(UNI_L("http-equiv=\"Content-Type")),
	CONST_ENTRY(UNI_L("content=\"application/vnd.wap.wmlc;charset=")),
	CONST_ENTRY(UNI_L("http-equiv=\"Expires")),
	CONST_ENTRY(UNI_L("accesskey=\"")),
	CONST_ENTRY(UNI_L("enctype=\"")),
	CONST_ENTRY(UNI_L("enctype=\"application/x-www-form-urlencoded")),
	CONST_ENTRY(UNI_L("enctype=\"multipart/form-data")),
	CONST_ENTRY(UNI_L("xml:space=\"preserve")),
	CONST_ENTRY(UNI_L("xml:space=\"default")),
	CONST_ENTRY(UNI_L("cache-control=\"no-cache"))
CONST_END(WML_WBXML_attr_start_tokens)

CONST_ARRAY(WML_WBXML_attr_value_tokens, uni_char*)
	CONST_ENTRY(UNI_L(".com/")),
	CONST_ENTRY(UNI_L(".edu/")),
	CONST_ENTRY(UNI_L(".net/")),
	CONST_ENTRY(UNI_L(".org/")),
	CONST_ENTRY(UNI_L("accept")),
	CONST_ENTRY(UNI_L("bottom")),
	CONST_ENTRY(UNI_L("clear")),
	CONST_ENTRY(UNI_L("delete")),
	CONST_ENTRY(UNI_L("help")),
	CONST_ENTRY(UNI_L("http://")),
	CONST_ENTRY(UNI_L("http://www.")),
	CONST_ENTRY(UNI_L("https://")),
	CONST_ENTRY(UNI_L("https://www.")),
	CONST_ENTRY(UNI_L("")),
	CONST_ENTRY(UNI_L("middle")),
	CONST_ENTRY(UNI_L("nowrap")),
	CONST_ENTRY(UNI_L("onpick")),
	CONST_ENTRY(UNI_L("onenterbackward")),
	CONST_ENTRY(UNI_L("onenterforward")),
	CONST_ENTRY(UNI_L("ontimer")),
	CONST_ENTRY(UNI_L("options")),
	CONST_ENTRY(UNI_L("password")),
	CONST_ENTRY(UNI_L("reset")),
	CONST_ENTRY(UNI_L("")),
	CONST_ENTRY(UNI_L("text")),
	CONST_ENTRY(UNI_L("top")),
	CONST_ENTRY(UNI_L("unknown")),
	CONST_ENTRY(UNI_L("wrap")),
	CONST_ENTRY(UNI_L("Www."))
CONST_END(WML_WBXML_attr_value_tokens)

class WML_WBXML_ContentHandler : public WBXML_ContentHandler
{
public:
			WML_WBXML_ContentHandler(WBXML_Parser *parser) : WBXML_ContentHandler(parser) {}

	virtual OP_WXP_STATUS	Init();

	virtual const uni_char*	TagHandler(UINT32 tok);
	virtual const uni_char* AttrStartHandler(UINT32 tok);
	virtual const uni_char* AttrValueHandler(UINT32 tok);
	virtual const uni_char* ExtHandlerL(UINT32 tok, const char **buf);
};

OP_WXP_STATUS
WML_WBXML_ContentHandler::Init()
{
	// possibly load the tables here
	return OpStatus::OK;
}

const uni_char*
WML_WBXML_ContentHandler::TagHandler(UINT32 tok)
{
	if (tok <= 0x3F && tok >= 0x1B)
	{
		const uni_char *tmp_str = WML_WBXML_tag_tokens[tok - 0x1B];
		if (*tmp_str)
			return tmp_str;
	}

	return NULL;
}

const uni_char*
WML_WBXML_ContentHandler::AttrStartHandler(UINT32 tok)
{
	if (tok <= 0x64 && tok >= 0x05)
	{
		const uni_char *tmp_str = WML_WBXML_attr_start_tokens[tok - 0x05];
		if (*tmp_str)
			return tmp_str;
	}

	return NULL;
}

const uni_char*
WML_WBXML_ContentHandler::AttrValueHandler(UINT32 tok)
{
	if (tok <= 0xa1 && tok >= 0x85)
	{
		const uni_char *tmp_str = WML_WBXML_attr_value_tokens[tok - 0x85];
		if (*tmp_str)
			return tmp_str;
	}

	return NULL;
}

const uni_char*
WML_WBXML_ContentHandler::ExtHandlerL(UINT32 tok, const char **buf)
{
	const char *str = NULL;

	switch (tok)
	{
	case 0x40:
	case 0x41:
	case 0x42:
		{
			str = *buf;
			GetParser()->PassNextStringL(buf);
		}
		break;
		
	case 0x80:
	case 0x81:
	case 0x82:
		{
			UINT32 idx = GetParser()->GetNextTokenL(buf, TRUE);
			str = GetParser()->LookupStr(idx);
		}
		break;
	}
	
	if (str)
	{
		int len = op_strlen(str);
		OpString s;
		ANCHOR(OpString, s);
		s.SetL(str);
		
		switch (tok)
		{
		case 0x40:
		case 0x80:
			{
				GetParser()->EnsureTmpBufLenL(len + WXSTRLEN("$(:escape)") + 1);
				len = uni_sprintf(GetParser()->GetTmpBuf(), UNI_L("$(%s:escape)"), s.CStr());
			}
			break;
			
		case 0x41:
		case 0x81:
			{
				GetParser()->EnsureTmpBufLenL(len + WXSTRLEN("$(:unesc)") + 1);
				len = uni_sprintf(GetParser()->GetTmpBuf(), UNI_L("$(%s:unesc)"), s.CStr());
			}
			break;
			
		case 0x42:
		case 0x82:
			{
				GetParser()->EnsureTmpBufLenL(len + WXSTRLEN("$(:noesc)") + 1);
				len = uni_sprintf(GetParser()->GetTmpBuf(), UNI_L("$(%s:noesc)"), s.CStr());
			}
			break;
		}

		return GetParser()->GetTmpBuf();
	}

	return NULL;
}
