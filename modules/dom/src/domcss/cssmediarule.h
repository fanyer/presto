/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_MEDIA_RULE_H
#define DOM_MEDIA_RULE_H

#include "modules/dom/src/domcss/cssconditionalrule.h"

class DOM_CSSMediaRule : public DOM_CSSConditionalRule
{
public:
	friend class DOM_CSSMediaList;

	static OP_STATUS Make(DOM_CSSMediaRule*& rule, DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_MEDIARULE || DOM_CSSConditionalRule::IsA(type); }

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

protected:
	DOM_CSSMediaRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);
};

#endif // DOM_MEDIA_RULE_H
