/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_CSSSTYLEDECLARATION_H
#define DOM_CSSSTYLEDECLARATION_H

#include "modules/dom/src/domobj.h"

class DOM_Element;
class CSS_DOMStyleDeclaration;
class DOM_CSSRule;

class DOM_CSSStyleDeclaration
	: public DOM_Object
{
public:
	enum DOMStyleType { DOM_ST_INLINE, DOM_ST_RULE, DOM_ST_COMPUTED, DOM_ST_CURRENT };

protected:
	DOM_CSSStyleDeclaration(DOM_Element *element, DOMStyleType type);

	DOM_Element *element;
	CSS_DOMStyleDeclaration *style;
	DOMStyleType type;

#ifdef DOM2_MUTATION_EVENTS
	class MutationState
	{
	protected:
		DOM_Element *element;
		DOM_Runtime *origining_runtime;
		TempBuffer prevBuffer;
		BOOL send_attrmodified;

	public:
		MutationState(DOM_Element *element, DOM_Runtime *origining_runtime);

		OP_STATUS BeforeChange();
		OP_STATUS AfterChange();
	};
#endif // DOM2_MUTATION_EVENTS

public:
	static OP_STATUS Make(DOM_CSSStyleDeclaration *&styledeclaration, DOM_Element *element, DOMStyleType type, const uni_char *pseudo_class = NULL);
	static OP_STATUS Make(DOM_CSSStyleDeclaration *&styledeclaration, DOM_CSSRule *rule);

	virtual ~DOM_CSSStyleDeclaration();

	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_CSS_STYLEDECLARATION || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	CSS_DOMStyleDeclaration *GetStyleDeclaration() { return style; }

	DOM_DECLARE_FUNCTION(getPropertyValue);
	DOM_DECLARE_FUNCTION(getPropertyCSSValue);
	DOM_DECLARE_FUNCTION(getPropertyPriority);
	DOM_DECLARE_FUNCTION(setProperty);
	DOM_DECLARE_FUNCTION(removeProperty);
	DOM_DECLARE_FUNCTION(item);
	enum { FUNCTIONS_ARRAY_SIZE = 7 };
};

#endif // DOM_CSSSTYLEDECLARATION_H
