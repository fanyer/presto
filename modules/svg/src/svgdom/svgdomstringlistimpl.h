/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_STRING_LIST_IMPL_H
#define SVG_DOM_STRING_LIST_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGVector.h"

class SVGDOMStringListImpl : public SVGDOMStringList
{
public:
	SVGDOMStringListImpl(SVGVector* vector);
	virtual ~SVGDOMStringListImpl();

	virtual	const char* GetDOMName();
	virtual SVGObject*	GetSVGObject() { return m_vector; }
	virtual void 		Clear();
	virtual OP_STATUS 	Initialize(const uni_char* new_item);
	virtual OP_BOOLEAN	GetItem(UINT32 idx, const uni_char*& new_item);
	virtual	OP_BOOLEAN 	InsertItemBefore(const uni_char* new_item, UINT32 index);
	virtual OP_STATUS	RemoveItem(UINT32 index, const uni_char*& removed_item);
	virtual UINT32		GetCount();
	virtual OP_BOOLEAN	ApplyChange(UINT32 idx, const uni_char* new_item);

private:
	SVGVector*			m_vector;
};

# endif // SVG_FULL_11
#endif // SVG_DOM_STRING_LIST_IMPL_H
