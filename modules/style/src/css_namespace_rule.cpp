/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_namespace_rule.h"

/* virtual */ OP_STATUS
CSS_NamespaceRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned indent_level)
{
	RETURN_IF_ERROR(buf->Append("@namespace "));
	const uni_char* prefix = GetPrefix();
	if (prefix && *prefix)
	{
		RETURN_IF_ERROR(buf->Append(prefix));
		RETURN_IF_ERROR(buf->Append(" "));
	}
	RETURN_IF_ERROR(buf->Append("url("));
	RETURN_IF_ERROR(buf->Append(GetURI()));
	return buf->Append(");");
}
