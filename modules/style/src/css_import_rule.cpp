/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_import_rule.h"
#include "modules/style/src/css_parser.h"
#include "modules/logdoc/htm_elm.h"

/* virtual */ OP_STATUS
CSS_ImportRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	RETURN_IF_ERROR(buf->Append("@import url("));
	RETURN_IF_ERROR(buf->Append(m_element->GetStringAttr(Markup::HA_HREF)));
	RETURN_IF_ERROR(buf->Append(")"));
	CSS_MediaObject* media_object = m_element->GetLinkStyleMediaObject();
	if (media_object && !media_object->IsEmpty())
	{
		RETURN_IF_ERROR(buf->Append(" "));
		TRAP_AND_RETURN(stat, media_object->GetMediaStringL(buf));
	}
	return buf->Append(";");
}

/* virtual */ CSS_PARSE_STATUS
CSS_ImportRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, const int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, NULL, NULL, TRUE, CSS_TOK_DOM_IMPORT_RULE, text, len);
}

CSS_MediaObject*
CSS_ImportRule::GetMediaObject(BOOL create)
{
	CSS_MediaObject* media_obj = m_element->GetLinkStyleMediaObject();
	if (!media_obj && create)
	{
		media_obj = OP_NEW(CSS_MediaObject, ());
	}
	return media_obj;
}
