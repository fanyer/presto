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

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/svgdom/svgdomanimatedvalueimpl.h"

#include "modules/svg/src/svgdom/svgdomnumberimpl.h"
#include "modules/svg/src/svgdom/svgdompreserveaspectratioimpl.h"
#include "modules/svg/src/svgdom/svgdomenumerationimpl.h"
#include "modules/svg/src/svgdom/svgdomlengthimpl.h"
#include "modules/svg/src/svgdom/svgdomangleimpl.h"
#include "modules/svg/src/svgdom/svgdomrectimpl.h"
#include "modules/svg/src/svgdom/svgdompointimpl.h"
#include "modules/svg/src/svgdom/svgdomtransformimpl.h"
#include "modules/svg/src/svgdom/svgdompaintimpl.h"
#include "modules/svg/src/svgdom/svgdomlistimpl.h"

#include "modules/dom/domutils.h"

SVGDOMListImpl::SVGDOMListImpl(SVGDOMItemType type, SVGVector* vector) :
		SVGDOMList(type),
		m_vector(vector)
{
	SVGObject::IncRef(m_vector);
}

SVGDOMListImpl::~SVGDOMListImpl()
{
	SVGObject::DecRef(m_vector);
}

/* virtual */ const char*
SVGDOMListImpl::GetDOMName()
{
	switch(list_type)
	{
	case SVG_DOM_ITEMTYPE_NUMBER:
		return "SVGNumberList";
	case SVG_DOM_ITEMTYPE_TRANSFORM:
		return "SVGTransformList";
	case SVG_DOM_ITEMTYPE_LENGTH:
		return "SVGLengthList";
	case SVG_DOM_ITEMTYPE_POINT:
		return "SVGPointList";
	default:
		OP_ASSERT(!"Not reached");
		return "SVGList";
	}
}

/* virtual */ void
SVGDOMListImpl::Clear()
{
	m_objects.ForEach(ReleaseFromListFunc);
	m_objects.RemoveAll();
	m_vector->Clear();
}

/* static */ void
SVGDOMListImpl::ReleaseFromListFunc(const void *item, void *o)
{
	DOM_Object* dom_obj = (DOM_Object*)o;
	if (dom_obj)
		DOM_Utils::ReleaseSVGDOMObjectFromLists(dom_obj);
}

/* virtual */ OP_STATUS
SVGDOMListImpl::Initialize(SVGDOMItem* new_item)
{
	Clear();
	OP_BOOLEAN res = InsertItemBefore(new_item, 0);
	RETURN_IF_MEMORY_ERROR(res);
	return res == OpBoolean::IS_TRUE ? OpStatus::OK : OpStatus::ERR;
}

/* virtual */ OP_STATUS
SVGDOMListImpl::CreateItem(UINT32 idx, SVGDOMItem*& item)
{
	// Should be checked by the caller
	OP_ASSERT(idx < m_vector->GetCount());

	item = NULL;

	switch(ListType())
	{
		case SVG_DOM_ITEMTYPE_TRANSFORM:
		{
			OP_ASSERT(m_vector->VectorType() == SVGOBJECT_TRANSFORM);
			item = OP_NEW(SVGDOMTransformImpl, (static_cast<SVGTransform*>(m_vector->Get(idx))));
		}
		break;
		case SVG_DOM_ITEMTYPE_LENGTH:
		{
			OP_ASSERT(m_vector->VectorType() == SVGOBJECT_LENGTH);
			item = OP_NEW(SVGDOMLengthImpl, (static_cast<SVGLengthObject*>(m_vector->Get(idx))));
		}
		break;
		case SVG_DOM_ITEMTYPE_NUMBER:
		{
			OP_ASSERT(m_vector->VectorType() == SVGOBJECT_NUMBER);
			item = OP_NEW(SVGDOMNumberImpl, (static_cast<SVGNumberObject*>(m_vector->Get(idx))));
		}
		break;
		case SVG_DOM_ITEMTYPE_POINT:
		{
			OP_ASSERT(m_vector->VectorType() == SVGOBJECT_POINT);
			item = OP_NEW(SVGDOMPointImpl, (static_cast<SVGPoint*>(m_vector->Get(idx))));
		}
		break;
		default:
			OP_ASSERT(!"Not reached");
			return OpStatus::ERR;
	}

	if (!item)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/* virtual */ OP_BOOLEAN
SVGDOMListImpl::InsertItemBefore(SVGDOMItem* new_item, UINT32 index)
{
	SVGObject* src = new_item->GetSVGObject();

	// Should be checked by the caller
	OP_ASSERT(index <= (m_vector->GetCount() + 1));
	RETURN_IF_MEMORY_ERROR(m_vector->Insert(index, src));
	return OpBoolean::IS_TRUE;
}

/* virtual */ void
SVGDOMListImpl::DropObject(SVGObject* obj)
{
	if (obj)
	{
		DOM_Object* dom_obj;
		OpStatus::Ignore(m_objects.Remove(obj, &dom_obj));

		if (dom_obj)
			DOM_Utils::ReleaseSVGDOMObjectFromLists(dom_obj);
	}
}

/* virtual */ OP_STATUS
SVGDOMListImpl::RemoveItem(UINT32 index)
{
	// Should be checked by the caller
	OP_ASSERT(m_vector->GetCount() > 0 && index < m_vector->GetCount());
	DropObject(m_vector->Get(index));
	m_vector->Remove(index);
	return OpStatus::OK;
}

/* virtual */ OP_BOOLEAN
SVGDOMListImpl::ApplyChange(UINT32 idx, SVGDOMItem* item)
{
	OP_ASSERT(item != NULL);
	SVGObject* src = item->GetSVGObject();
	OP_ASSERT(src != NULL);

	if (idx < m_vector->GetCount())
	{
		/* replace */
		DropObject(m_vector->Get(idx));
		RETURN_IF_MEMORY_ERROR(m_vector->Replace(idx, src));
	}
	else if (idx == m_vector->GetCount())
	{
		/* append */
		RETURN_IF_MEMORY_ERROR(m_vector->Append(src));
	}

	return OpBoolean::IS_TRUE;
}

/* virtual */ UINT32
SVGDOMListImpl::GetCount()
{
	return m_vector->GetCount();
}

/* virtual */ OP_BOOLEAN
SVGDOMListImpl::Consolidate()
{
	// Should be checked by the caller
	OP_ASSERT (m_vector->VectorType() == SVGOBJECT_TRANSFORM);

	m_objects.ForEach(ReleaseFromListFunc);
	m_objects.RemoveAll();

	RETURN_IF_MEMORY_ERROR(m_vector->Consolidate());
	return OpBoolean::IS_TRUE;
}

/* virtual */ DOM_Object*
SVGDOMListImpl::GetDOMObject(UINT32 idx)
{
	SVGObject *svg_obj = m_vector->Get(idx);
	if (!svg_obj)
		return NULL;

	DOM_Object* obj;
	if (OpStatus::IsSuccess(m_objects.GetData(svg_obj, &obj)))
		return obj;
	else
		return NULL;
}

/* virtual */ OP_STATUS
SVGDOMListImpl::SetDOMObject(SVGDOMItem* item, DOM_Object* obj)
{
	OP_ASSERT(item);
	SVGObject* svg_obj = item->GetSVGObject();
	OP_ASSERT(svg_obj);
	OP_STATUS status = m_objects.Add(svg_obj, obj);
	if (status == OpStatus::ERR)
	{
		DOM_Object* old_obj;
		OpStatus::Ignore(m_objects.Remove(svg_obj, &old_obj));

		if (old_obj)
			DOM_Utils::ReleaseSVGDOMObjectFromLists(old_obj);

		status = m_objects.Add(svg_obj, obj);
	}
	OP_ASSERT(status != OpStatus::ERR);
	if (OpStatus::IsMemoryError(status))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/* virtual */ void
SVGDOMListImpl::ReleaseDOMObject(SVGDOMItem* item)
{
	if (!item)
		return;
	SVGObject* svg_obj = item->GetSVGObject();
	OP_ASSERT(svg_obj);
	DOM_Object* obj;
	OpStatus::Ignore(m_objects.Remove(svg_obj, &obj));
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11


