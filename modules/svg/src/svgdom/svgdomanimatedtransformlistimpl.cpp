/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM) && defined(SVG_FULL_11)

#include "modules/svg/src/svgdom/svgdomanimatedtransformlistimpl.h"
#include "modules/svg/src/svgdom/svgdomlistimpl.h"
#include "modules/svg/src/svgdom/svgdomtransformimpl.h"
#include "modules/svg/src/SVGTransform.h"

/* static */ OP_STATUS
SVGDOMAnimatedTransformListImpl::Make(SVGDOMAnimatedTransformListImpl *&anim_trfm_lst,
									  SVGVector *base_list,
									  SVGTransform *animated_transform)
{
	SVGDOMListImpl *wrapped_base_list = NULL;
	if (base_list != NULL)
	{
		wrapped_base_list = OP_NEW(SVGDOMListImpl, (SVG_DOM_ITEMTYPE_TRANSFORM, base_list));
		if (!wrapped_base_list)
			return OpStatus::ERR_NO_MEMORY;
	}

	anim_trfm_lst = OP_NEW(SVGDOMAnimatedTransformListImpl, (wrapped_base_list,
														animated_transform));
	if (!anim_trfm_lst)
	{
		OP_DELETE(wrapped_base_list);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

SVGDOMAnimatedTransformListImpl::SVGDOMAnimatedTransformListImpl(SVGDOMListImpl *base_list,
																 SVGTransform *animated_transform) :
	SVGDOMList(SVG_DOM_ITEMTYPE_TRANSFORM),
	m_base_part_list(base_list),
	m_animated_transform(animated_transform),
	m_object(NULL)
{
	SVGObject::IncRef(m_animated_transform);
}

SVGDOMAnimatedTransformListImpl::~SVGDOMAnimatedTransformListImpl()
{
	SVGObject::DecRef(m_animated_transform);
	OP_DELETE(m_base_part_list);
}

/* virtual */ const char*
SVGDOMAnimatedTransformListImpl::GetDOMName()
{
	return "SVGTransformList";
}

/* virtual */ UINT32
SVGDOMAnimatedTransformListImpl::GetCount()
{
	UINT32 count = 0;

	if (m_base_part_list != NULL)
	{
		count += m_base_part_list->GetCount();
	}

	if (m_animated_transform != NULL)
	{
		count++;
	}

	return count;
}

/* virtual */ OP_STATUS
SVGDOMAnimatedTransformListImpl::CreateItem(UINT32 idx, SVGDOMItem*& item)
{
	// 'idx' should be checked by the caller
	if (m_base_part_list && idx < m_base_part_list->GetCount())
	{
		return m_base_part_list->CreateItem(idx, item);
	}
	else if (m_animated_transform)
	{
		OP_ASSERT(idx == 0 || (m_base_part_list && m_base_part_list->GetCount() == idx));
		item = OP_NEW(SVGDOMTransformImpl, (m_animated_transform));
		return item ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		OP_ASSERT(!"Not reached");
		return OpStatus::ERR;
	}
}

/* virtual */ DOM_Object*
SVGDOMAnimatedTransformListImpl::GetDOMObject(UINT32 idx)
{
	if (m_base_part_list)
	{
		if (idx < m_base_part_list->GetCount())
		{
			return m_base_part_list->GetDOMObject(idx);
		}
		else
		{
			OP_ASSERT(idx == m_base_part_list->GetCount());
			return m_object;
		}
	}
	else if (m_animated_transform)
	{
		OP_ASSERT(idx == 0);
		return (idx == 0) ? m_object : NULL;
	}
	else
	{
		OP_ASSERT(!"Not reached");
		return NULL;
	}
}

/* virtual */ OP_STATUS
SVGDOMAnimatedTransformListImpl::SetDOMObject(SVGDOMItem* item, DOM_Object* obj)
{
	if (item->GetSVGObject() == m_animated_transform)
	{
		m_object = obj;
		return OpStatus::OK;
	}
	else if (m_base_part_list != NULL)
	{
		return m_base_part_list->SetDOMObject(item, obj);
	}
	else
	{
		OP_ASSERT(!"Not reached");
		return OpStatus::ERR;
	}
}

/* virtual */ void
SVGDOMAnimatedTransformListImpl::ReleaseDOMObject(SVGDOMItem* item)
{
	if (item->GetSVGObject() == m_animated_transform)
	{
		m_object = NULL;
	}
	else if (m_base_part_list != NULL)
	{
		m_base_part_list->ReleaseDOMObject(item);
	}
	else
	{
		OP_ASSERT(!"Not reached");
	}
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
