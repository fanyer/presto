/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_charset_rule.h"
#include "modules/style/src/css_parser.h"

/* virtual */ OP_STATUS
CSS_CharsetRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	RETURN_IF_ERROR(buf->Append("@charset \""));
	RETURN_IF_ERROR(buf->Append(m_charset));
	return buf->Append("\";");
}

/* virtual */ CSS_PARSE_STATUS
CSS_CharsetRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, NULL, NULL, TRUE, CSS_TOK_DOM_CHARSET_RULE, text, len);
}
