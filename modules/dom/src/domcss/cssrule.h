/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_RULE_H
#define DOM_RULE_H

#include "modules/dom/src/domobj.h"

class CSS_DOMRule;
class DOM_CSSStyleSheet;

class DOM_CSSRule
	: public DOM_Object
{
public:
	~DOM_CSSRule();
	static OP_STATUS Make(DOM_CSSRule*& rule, DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);
	static OP_STATUS GetRule(DOM_CSSRule*& rule, CSS_DOMRule* css_rule, DOM_Runtime* runtime);
	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_RULE || DOM_Object::IsA(type); }
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	CSS_DOMRule* GetCSS_DOMRule() { return m_css_rule; }

	static void ConstructCSSRuleObjectL(ES_Object *object, DOM_Runtime *runtime);

protected:
	DOM_CSSRule(DOM_CSSStyleSheet* sheet, CSS_DOMRule* css_rule);

	CSS_DOMRule* m_css_rule;
	DOM_CSSStyleSheet* m_sheet;
};

#endif // DOM_RULE_H
