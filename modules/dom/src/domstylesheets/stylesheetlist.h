/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_STYLESHEETLIST_H
#define DOM_STYLESHEETLIST_H

#include "modules/dom/src/domobj.h"

class DOM_Document;
class DOM_Collection;

class DOM_StyleSheetList
	: public DOM_Object
{
protected:
	DOM_StyleSheetList(DOM_Document *document);

	DOM_Document *document;
	DOM_Collection *collection;

public:
	static OP_STATUS Make(DOM_StyleSheetList *&stylesheetlist, DOM_Document *document);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_STYLESHEETLIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	DOM_DECLARE_FUNCTION(item);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // DOM_STYLESHEETLIST_H
