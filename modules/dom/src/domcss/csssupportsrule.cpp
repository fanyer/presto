/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domcss/csssupportsrule.h"
#include "modules/dom/src/domcss/cssstylesheet.h"
#include "modules/dom/src/domcss/cssrulelist.h"

#include "modules/style/css_dom.h"

DOM_CSSSupportsRule::DOM_CSSSupportsRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule)
	: DOM_CSSConditionalRule(sheet, css_rule)
{
}

/* static */ OP_STATUS
DOM_CSSSupportsRule::Make(DOM_CSSSupportsRule*& rule, DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule)
{
	DOM_Runtime *runtime = sheet->GetRuntime();
	return DOMSetObjectRuntime(rule = OP_NEW(DOM_CSSSupportsRule, (sheet, css_rule)), runtime, runtime->GetPrototype(DOM_Runtime::CSSMEDIARULE_PROTOTYPE), "CSSSupportsRule");
}
