/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_MEDIALIST_H
#define DOM_MEDIALIST_H

#include "modules/dom/src/domobj.h"
#include "modules/style/css_dom.h"

class DOM_StyleSheet;
class DOM_CSSRule;
class CSS_DOMMediaList;

class DOM_MediaList
	: public DOM_Object
{
protected:
	DOM_MediaList(DOM_Object* owner);

	DOM_Object* m_owner;
	CSS_DOMMediaList* m_medialist;

public:
	static OP_STATUS Make(DOM_MediaList*& medialist, DOM_StyleSheet* stylesheet);
	static OP_STATUS Make(DOM_MediaList*& medialist, DOM_CSSRule* rule);

	virtual ~DOM_MediaList();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_MEDIALIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *runtime);

	DOM_DECLARE_FUNCTION(appendMedium);
	DOM_DECLARE_FUNCTION(deleteMedium);
	DOM_DECLARE_FUNCTION(item);
	enum { FUNCTIONS_ARRAY_SIZE = 4 };
};

#endif // DOM_MEDIALIST_H
