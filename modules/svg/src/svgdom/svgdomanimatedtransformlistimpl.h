/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_ANIMATED_TRANSFORM_LIST_IMPL_H
#define SVG_DOM_ANIMATED_TRANSFORM_LIST_IMPL_H

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGVector.h"

class SVGDOMListImpl;

/**
 * This is a read-only list designed to handle the fact that we store
 * the animated transform in another attribute and object than the
 * base value for animateTransform. When the animateTransform is
 * additive, the total animation value is the concatination of base
 * value (a transform list) and animation transform.
 */
class SVGDOMAnimatedTransformListImpl : public SVGDOMList
{
public:
	static OP_STATUS Make(SVGDOMAnimatedTransformListImpl *&anim_trfm_lst,
						  SVGVector *base_part_list,
						  SVGTransform *animated_transform);

	virtual ~SVGDOMAnimatedTransformListImpl();

	virtual	const char* GetDOMName();

	/** There is _no_ one-to-one mapping between SVGDOMItems and
	 * SVGObjects for this class. This means that we can't have such a
	 * list in another list or use it to set a trait value. */
	virtual SVGObject*	GetSVGObject() { OP_ASSERT(!"Not reached, see comment"); return NULL; }

	virtual void 		Clear() { /* Do nothing */ }
	virtual OP_STATUS 	Initialize(SVGDOMItem* new_item) { return OpStatus::ERR; }

	virtual OP_STATUS	CreateItem(UINT32 idx, SVGDOMItem*& item);
	virtual DOM_Object* GetDOMObject(UINT32 idx);

	virtual	OP_BOOLEAN 	InsertItemBefore(SVGDOMItem* new_item, UINT32 index) { return OpBoolean::IS_FALSE; }
	virtual OP_STATUS	RemoveItem(UINT32 index) { return OpStatus::OK; }
	virtual UINT32		GetCount();
	virtual OP_BOOLEAN	Consolidate() { return OpBoolean::IS_FALSE; }

	virtual OP_BOOLEAN	ApplyChange(UINT32 idx, SVGDOMItem* item) { return OpBoolean::IS_FALSE; }

	virtual OP_STATUS	SetDOMObject(SVGDOMItem* item, DOM_Object* obj);
	virtual void		ReleaseDOMObject(SVGDOMItem* item);

private:
	SVGDOMAnimatedTransformListImpl(SVGDOMListImpl *base_list,
									SVGTransform *animated_transform);
	SVGDOMListImpl*		m_base_part_list; //< We own this list
	SVGTransform*		m_animated_transform;
	DOM_Object*			m_object;
};

#endif // SVG_DOM_ANIMATED_TRANSFORM_LIST_IMPL_H

