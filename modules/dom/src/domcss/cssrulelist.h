/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_RULELIST_H
#define DOM_RULELIST_H

#include "modules/dom/src/domobj.h"

class DOM_StyleSheet;
class DOM_CSSRule;

class DOM_CSSRuleList
	: public DOM_Object
{
protected:
	DOM_CSSRuleList(DOM_Object* listable);

public:
	static OP_STATUS Make(DOM_CSSRuleList *&rulelist, DOM_Object* listable);
	static OP_STATUS GetFromListable(DOM_CSSRuleList*& rulelist, DOM_Object* listable);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_RULELIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	OP_STATUS GetRule(CSS_DOMRule* css_rule, DOM_CSSStyleSheet* sheet, DOM_CSSRule*& dom_rule);

	DOM_DECLARE_FUNCTION(item);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

private:
	DOM_Object* m_list_object;
};

#endif // DOM_RULELIST_H
