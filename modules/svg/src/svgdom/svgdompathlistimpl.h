/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_POINT_IMPL_H
#define SVG_DOM_POINT_IMPL_H

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/OpBpath.h"

class SVGDOMPathSegListImpl : public SVGDOMList
{
public:
						SVGDOMPathSegListImpl(OpBpath* path, BOOL normalized);
	virtual				~SVGDOMPathSegListImpl() {}

	virtual const char* GetDOMName() { return "SVGPathSegList"; }

	virtual void 		Clear();
	virtual OP_STATUS 	Initialize(SVGDOMItem* new_item);
	virtual OP_BOOLEAN	GetItem(UINT32 idx, SVGDOMItem*& item);
	virtual	OP_BOOLEAN 	InsertItemBefore(SVGDOMItem* new_item, UINT32 index);
	virtual OP_STATUS	RemoveItem(UINT32 index);
	virtual UINT32		GetCount();
	virtual OP_BOOLEAN	Consolidate() { return OpBoolean::IS_FALSE; }

	virtual OP_BOOLEAN	ApplyChange(UINT32 idx, SVGDOMItem* item);

	virtual void		AccessDOMObject(DOM_Object*& in_out_object, BOOL set_value, UINT32 idx) {}

protected:
	SVGPathSegObject*	GetSVGPathSeg(SVGDOMItem* item);

private:
	OpBpath*			m_path;
	BOOL				m_normalized;
};

#endif // SVG_DOM_POINT_IMPL_H
