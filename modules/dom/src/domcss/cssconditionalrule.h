/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_CONDITIONAL_RULE_H
#define DOM_CONDITIONAL_RULE_H

#include "modules/dom/src/domcss/cssrule.h"

class DOM_CSSConditionalRule : public DOM_CSSRule
{
public:
	friend class DOM_CSSRuleList;

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_CONDITIONALRULE || DOM_CSSRule::IsA(type); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	DOM_DECLARE_FUNCTION(insertRule);
	DOM_DECLARE_FUNCTION(deleteRule);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

protected:
	DOM_CSSConditionalRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);
};

#endif // DOM_CONDITIONAL_RULE_H
