/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_PATH_SEG_LIST_IMPL_H
#define SVG_DOM_PATH_SEG_LIST_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/OpBpath.h"
#include "modules/svg/src/svgdom/svgdomhashtable.h"

class SVGDOMPathSegListImpl : public SVGDOMList
{
public:
						SVGDOMPathSegListImpl(OpBpath* path, BOOL normalized);
	virtual				~SVGDOMPathSegListImpl();

	virtual const char* GetDOMName() { return "SVGPathSegList"; }
	virtual SVGObject*	GetSVGObject() { return m_path; }

	virtual void 		Clear();
	virtual OP_STATUS 	Initialize(SVGDOMItem* new_item);
	virtual OP_BOOLEAN	CreateItem(UINT32 idx, SVGDOMItem*& item);
	virtual	OP_BOOLEAN 	InsertItemBefore(SVGDOMItem* new_item, UINT32 index);
	virtual OP_STATUS	RemoveItem(UINT32 index);
	virtual UINT32		GetCount();
	virtual OP_BOOLEAN	Consolidate() { return OpBoolean::IS_FALSE; }

	virtual OP_BOOLEAN	ApplyChange(UINT32 idx, SVGDOMItem* item);

	virtual DOM_Object* GetDOMObject(UINT32 idx);
	virtual OP_STATUS	SetDOMObject(SVGDOMItem* item, DOM_Object* obj);

	virtual void		ReleaseDOMObject(SVGDOMItem* item);

	// Callback from a pathseg when it is release from a OpBpath
	void				DropObject(SVGPathSegObject *svg_obj);

	static void			ReleaseFromListFunc(const void *item, void *o);

protected:
	SVGPathSegObject*	GetSVGPathSeg(SVGDOMItem* item);
	void				SetInList(SVGDOMItem* item);
#if 0
	void				ReleaseFromList(SVGDOMItem* item);
#endif

private:
	OpBpath*			m_path;
	SVGDOMHashTable		m_objects;
	BOOL				m_normalized;

	void				ResetIterator();
	UINT32				m_last_iterator_idx;
	PathSegListIterator* m_cached_iterator;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_PATH_SEG_LIST_IMPL_H
