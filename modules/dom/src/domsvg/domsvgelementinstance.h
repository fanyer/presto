/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_SVGELEMINST_H
#define DOM_SVGELEMINST_H

#ifdef SVG_DOM

#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domevents/domeventtarget.h"

class DOM_SVGElementInstance
	: public DOM_Element // Not really a DOM_Element (see IsA) but we do this to share code
{
public:
	static OP_STATUS Make(DOM_SVGElementInstance *&element, HTML_Element *html_element, DOM_EnvironmentImpl *environment);

	virtual void GCTraceSpecial(BOOL via_tree);
	// Note, this is _not_ a DOM_Node as far as anyone but the event handling is concerned.
	// The event handling is patched to understand the DOM_TYPE_SVG_ELEMENT_INSTANCE type.
	virtual BOOL IsA(int type) { return type == DOM_TYPE_SVG_ELEMENT_INSTANCE || DOM_Object::IsA(type); }

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);

	enum {FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3}; // 2*accessEventListener + base
	static int accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data);

	enum {FUNCTIONS_ARRAY_SIZE = 2};

private:
	DOM_SVGElementInstance() : the_real_node(NULL) {node_type = SVG_ELEMENTINSTANCE_NODE;}

	DOM_Object* the_real_node;
};

#ifdef SVG_FULL_11
class DOM_SVGElementInstanceList : public DOM_Object
{
public:
	static OP_STATUS Make(DOM_SVGElementInstanceList*& list, DOM_SVGElementInstance* list_parent, DOM_EnvironmentImpl *environment);

	virtual void GCTrace();
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	enum {FUNCTIONS_ARRAY_SIZE = 2};
	static int item(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);

	/* This function should be overridden by all subclasses that ever need to be identified. */
	virtual BOOL IsA(int type) { return type == DOM_TYPE_SVG_ELEMENT_INSTANCE_LIST || DOM_Object::IsA(type); }

private:
	DOM_SVGElementInstanceList(DOM_SVGElementInstance* dom_node) :
		m_dom_node(dom_node) {}

	ES_GetState GetNodeAtIndex(int index, ES_Value* value);
	int CalculateNodeCount();

	DOM_SVGElementInstance* m_dom_node;
};
#endif // SVG_FULL_11
#endif // SVG_DOM
#endif // DOM_SVGELEMINST_H
