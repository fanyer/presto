/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_LIST_IMPL_H
#define SVG_DOM_LIST_IMPL_H

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/svgdom/svgdomhashtable.h"

class SVGDOMListImpl : public SVGDOMList
{
public:
	SVGDOMListImpl(SVGDOMItemType list_type, SVGVector* vector);
	virtual ~SVGDOMListImpl();

	virtual	const char* GetDOMName();
	virtual SVGObject*	GetSVGObject() { return m_vector; }

	virtual void 		Clear();
	virtual OP_STATUS 	Initialize(SVGDOMItem* new_item);

	virtual OP_STATUS	CreateItem(UINT32 idx, SVGDOMItem*& item);
	virtual DOM_Object* GetDOMObject(UINT32 idx);

	virtual	OP_BOOLEAN 	InsertItemBefore(SVGDOMItem* new_item, UINT32 index);
	virtual OP_STATUS	RemoveItem(UINT32 index);
	virtual UINT32		GetCount();
	virtual OP_BOOLEAN	Consolidate();

	virtual OP_BOOLEAN	ApplyChange(UINT32 idx, SVGDOMItem* item);

	virtual OP_STATUS	SetDOMObject(SVGDOMItem* item, DOM_Object* obj);
	virtual void		ReleaseDOMObject(SVGDOMItem* item);

protected:
	virtual void		DropObject(SVGObject* obj);
	static void			ReleaseFromListFunc(const void *item, void *o);

private:
	SVGVector*			GetVector() { return m_vector; }

	SVGVector*			m_vector;
	SVGDOMHashTable		m_objects;
};

#endif // SVG_DOM_LIST_IMPL_H

