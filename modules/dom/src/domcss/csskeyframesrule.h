/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_KEYFRAMES_RULE_H
#define DOM_KEYFRAMES_RULE_H

#include "modules/dom/src/domcss/cssrule.h"

#ifdef CSS_ANIMATIONS

class DOM_CSSKeyframesRule : public DOM_CSSRule
{
public:
	friend class DOM_CSSRuleList;

	static OP_STATUS Make(DOM_CSSKeyframesRule*& rule, DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_KEYFRAMESRULE || DOM_CSSRule::IsA(type); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	DOM_DECLARE_FUNCTION(appendRule);
	DOM_DECLARE_FUNCTION(deleteRule);
	DOM_DECLARE_FUNCTION(findRule);
	enum { FUNCTIONS_ARRAY_SIZE = 4 };

protected:
	DOM_CSSKeyframesRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);
};

#endif // CSS_ANIMATIONS
#endif // DOM_MEDIA_RULE_H
