/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_supports_rule.h"
#include "modules/style/src/css_parser.h"

BOOL CSS_SupportsRule::Evaluate(FramesDocument* doc, CSS_MediaType medium)
{
	return m_condition->Evaluate();
}

/* virtual */ OP_STATUS
CSS_SupportsRule::GetCssTextHeader(CSS* stylesheet, TempBuffer* buf)
{
	RETURN_IF_ERROR(buf->Append("@supports "));
	return m_condition->GetCssText(buf);
}

/* virtual */ CSS_PARSE_STATUS
CSS_SupportsRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, NULL, NULL, TRUE, CSS_TOK_DOM_SUPPORTS_RULE, text, len);
}
