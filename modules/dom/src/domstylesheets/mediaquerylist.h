/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_MEDIAQUERYLIST_H
#define DOM_MEDIAQUERYLIST_H

#include "modules/dom/src/domobj.h"
#include "modules/style/css_media.h"
#include "modules/util/adt/opvector.h"

class CSSCollection;

class DOM_MediaQueryList :
	public DOM_Object,
	public CSS_MediaQueryList::Listener
{
protected:
	DOM_MediaQueryList() : m_mediaquerylist(NULL) {}

	CSS_MediaQueryList* m_mediaquerylist;
	OpVector<ES_Object> m_handlers;

public:
	static OP_STATUS Make(DOM_MediaQueryList*& medialist, DOM_Runtime* runtime, CSSCollection* collection, const uni_char* media_string);

	virtual ~DOM_MediaQueryList();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_MEDIAQUERYLIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(addListener);
	DOM_DECLARE_FUNCTION(removeListener);
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

	// CSS_MediaQueryList::Listener implementation.
	virtual void OnChanged(const CSS_MediaQueryList* media_query_list);
	virtual void OnRemove(const CSS_MediaQueryList* media_query_list) { m_mediaquerylist = NULL; }
};

#endif // DOM_MEDIAQUERYLIST_H
