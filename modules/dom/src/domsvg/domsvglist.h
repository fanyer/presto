/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_SVGLIST_H
#define DOM_SVGLIST_H

#if defined(SVG_DOM) && defined(SVG_FULL_11)

#include "modules/dom/src/domsvg/domsvgenum.h"
#include "modules/dom/src/domsvg/domsvglocation.h"
#include "modules/util/adt/opvector.h"

class DOM_EnvironmentImpl;
class DOM_SVGObject;
class SVGDOMList;

class DOM_SVGList : public DOM_Object
{
public:
	virtual ~DOM_SVGList();
	static OP_STATUS Make(DOM_SVGList *&list, SVGDOMList* svg_val, DOM_SVGLocation location, DOM_EnvironmentImpl *environment);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_SVG_LIST || DOM_Object::IsA(type); }
	virtual void DOMChangeRuntime(DOM_Runtime *new_runtime);

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState	GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	/* Standard list methods */
	DOM_DECLARE_FUNCTION(clear);
	DOM_DECLARE_FUNCTION(initialize);
	DOM_DECLARE_FUNCTION(getItem);
	DOM_DECLARE_FUNCTION(insertItemBefore);
	DOM_DECLARE_FUNCTION(replaceItem);
	DOM_DECLARE_FUNCTION(removeItem);
	DOM_DECLARE_FUNCTION(appendItem);

	/* Transform lists methods */
	DOM_DECLARE_FUNCTION(createSVGTransformFromMatrix);
	DOM_DECLARE_FUNCTION(consolidate);

	enum { FUNCTIONS_ARRAY_SIZE = 11 };

	SVGDOMList* GetSVGList() { return svg_list; }
	virtual void GCTrace();
	static BOOL IsValid(DOM_SVGObject* obj, UINT32 idx);

	const DOM_SVGLocation& Location() { return location; }
	BOOL IsValid() { return location.IsValid(); }

	void ReleaseObject(DOM_SVGObject* obj);

protected:
	DOM_SVGList() {}
	DOM_SVGList(const DOM_SVGList&) : DOM_Object() {}

	OP_STATUS	AddObject(DOM_SVGObject* obj, SVGDOMItem* item = NULL);

	OP_STATUS	RemoveObject(DOM_SVGObject* obj, UINT32& idx);
	OP_STATUS	InsertObject(DOM_SVGObject* obj, UINT32 index, FramesDocument* frm_doc);

	ES_GetState	GetItemAtIndex(UINT32 idx, ES_Value* value, DOM_Runtime* origining_runtime);

	DOM_SVGLocation		  	location;
	SVGDOMList*				svg_list;
};

class DOM_SVGStringList : public DOM_Object
{
public:
	virtual ~DOM_SVGStringList();
	static OP_STATUS Make(DOM_SVGStringList *&list, SVGDOMStringList* svg_val, DOM_SVGLocation location,
						  DOM_EnvironmentImpl *environment);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_SVG_STRING_LIST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState	GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState	GetIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	SVGDOMStringList* GetSVGList() { return svg_list; }
	BOOL IsValid() { return location.IsValid(); }

	/* Standard list methods */
	DOM_DECLARE_FUNCTION(clear);
	DOM_DECLARE_FUNCTION(initialize);
	DOM_DECLARE_FUNCTION(getItem);
	DOM_DECLARE_FUNCTION(insertItemBefore);
	DOM_DECLARE_FUNCTION(replaceItem);
	DOM_DECLARE_FUNCTION(removeItem);
	DOM_DECLARE_FUNCTION(appendItem);

	enum { FUNCTIONS_ARRAY_SIZE = 9 };

protected:
	ES_GetState	GetItemAtIndex(UINT32 idx, ES_Value* value);

	DOM_SVGLocation location;
	SVGDOMStringList* svg_list;
};

#endif // SVG_DOM && SVG_FULL_11
#endif // DOM_SVGLIST_H
