/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_media_rule.h"
#include "modules/style/src/css_parser.h"

BOOL CSS_MediaRule::Evaluate(FramesDocument* doc, CSS_MediaType medium)
{
	return medium == CSS_MEDIA_TYPE_ALL || !m_media_object || (m_media_object->EvaluateMediaTypes(doc) & (medium | CSS_MEDIA_TYPE_ALL)) != 0;
}

CSS_MediaObject*
CSS_MediaRule::GetMediaObject(BOOL create)
{
	if (!m_media_object && create)
	{
		m_media_object = OP_NEW(CSS_MediaObject, ());
	}
	return m_media_object;
}

/* virtual */ OP_STATUS
CSS_MediaRule::GetCssTextHeader(CSS* stylesheet, TempBuffer* buf)
{
	RETURN_IF_ERROR(buf->Append("@media "));
	if (!m_media_object || m_media_object->IsEmpty())
		RETURN_IF_ERROR(buf->Append("all"));
	else
		TRAP_AND_RETURN(stat, m_media_object->GetMediaStringL(buf));
	return OpStatus::OK;
}

/* virtual */ CSS_PARSE_STATUS
CSS_MediaRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, NULL, NULL, TRUE, CSS_TOK_DOM_MEDIA_RULE, text, len);
}
