/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_SUPPORTS_RULE_H
#define DOM_SUPPORTS_RULE_H

#include "modules/dom/src/domcss/cssconditionalrule.h"

class DOM_CSSSupportsRule : public DOM_CSSConditionalRule
{
public:
	static OP_STATUS Make(DOM_CSSSupportsRule*& rule, DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_SUPPORTSRULE || DOM_CSSConditionalRule::IsA(type); }

protected:
	DOM_CSSSupportsRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);
};

#endif // DOM_SUPPORTS_RULE_H
