/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef DOM_SVGOBJECTSTORE_H
#define DOM_SVGOBJECTSTORE_H

#ifdef SVG_DOM

#include "modules/dom/src/domsvg/domsvgenum.h"
struct DOM_SVGInterfaceEntry;

class DOM_SVGObjectStore
{
public:
	enum {
		SVG_NUM_ELEMENT_INTERFACE_ENTRIES = 195,
		SVG_NUM_OBJECT_INTERFACE_ENTRIES = 6
	};

	~DOM_SVGObjectStore();

	static OP_STATUS Make(DOM_SVGObjectStore*& store,
						  const DOM_SVGInterfaceEntry* entries,
						  DOM_SVGInterfaceSpec ifs);

	DOM_Object* GetObject(OpAtom atom);
	void SetObject(OpAtom atom, DOM_Object* obj);

	void GCTrace(DOM_Runtime *runtime);

	DOM_Object* Get(int i) { OP_ASSERT(i>=0&&i<m_count); return m_objects[i]; }
	int GetCount() { return m_count; }

private:
	DOM_SVGObjectStore() : m_count(0), m_objects(NULL) {}
	DOM_SVGObjectStore(const DOM_SVGObjectStore&); // Don't clone these objects

	int m_count;
	DOM_SVGInterfaceSpec m_ifs;
	const DOM_SVGInterfaceEntry* m_entries;
	DOM_Object** m_objects;
};

#endif // SVG_DOM
#endif // DOM_SVGOBJECTSTORE_H

