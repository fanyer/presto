/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _DOM_HTML_DOCUMENT_
#define _DOM_HTML_DOCUMENT_

#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domhtml/htmlimplem.h"

class DOM_Collection;

class DOM_HTMLDocument
	: public DOM_Document
{
private:
	DOM_Collection *documentnodes;

	DOM_HTMLDocument(DOM_DOMImplementation *implementation, BOOL is_xml);

	ES_GetState GetCollection(ES_Value *value, OpAtom property_name);
public:
	static OP_STATUS Make(DOM_HTMLDocument *&document, DOM_DOMImplementation *implementation, BOOL create_placeholder = FALSE, BOOL is_xml = FALSE);

	virtual BOOL SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState	GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState	PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState	PutNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object *restart_object);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTML_DOCUMENT || DOM_Document::IsA(type); }

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	virtual OP_STATUS SetRootElement(DOM_Element *new_root);

	HTML_Element *GetElement(HTML_ElementType type);

	OP_STATUS GetBodyNode(DOM_Element *&element);

	ES_GetState GetTitle(ES_Value *value);
	ES_PutState SetTitle(ES_Value *value, ES_Runtime *origining_runtime);

	OP_STATUS GetAllCollection(DOM_Collection *&all);

	// Also: clear and {capture,release}Events
	DOM_DECLARE_FUNCTION(open);
	DOM_DECLARE_FUNCTION(close);
	DOM_DECLARE_FUNCTION(getElementsByName);
	DOM_DECLARE_FUNCTION(getItems);
	enum { FUNCTIONS_ARRAY_SIZE = 8 };

	DOM_DECLARE_FUNCTION_WITH_DATA(write);
#ifdef _SPAT_NAV_SUPPORT_
	DOM_DECLARE_FUNCTION_WITH_DATA(moveFocus);
#endif // _SPAT_NAV_SUPPORT_
	enum {
		FUNCTIONS_WITH_DATA_ARRAY_BASIC,
		FUNCTIONS_write,
		FUNCTIONS_writeln,
#ifdef _SPAT_NAV_SUPPORT_
		FUNCTIONS_moveFocusUp,
		FUNCTIONS_moveFocusDown,
		FUNCTIONS_moveFocusLeft,
		FUNCTIONS_moveFocusRight,
#endif // _SPAT_NAV_SUPPORT_
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

#endif /* _DOM_ */
