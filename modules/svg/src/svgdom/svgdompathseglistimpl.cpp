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
#include "modules/svg/src/svgdom/svgdompathsegimpl.h"
#include "modules/svg/src/svgdom/svgdompathseglistimpl.h"

#include "modules/dom/domutils.h"

SVGDOMPathSegListImpl::SVGDOMPathSegListImpl(OpBpath* path, BOOL normalized) :
		SVGDOMList(SVG_DOM_ITEMTYPE_PATHSEG), m_path(path), m_normalized(normalized),
		m_last_iterator_idx((UINT32)-1), m_cached_iterator(NULL)
{
	SVGObject::IncRef(m_path);
}

SVGDOMPathSegListImpl::~SVGDOMPathSegListImpl()
{
	m_objects.ForEach(ReleaseFromListFunc);
	m_objects.RemoveAll();

	if (!m_cached_iterator)
		m_cached_iterator = m_path->GetPathIterator(m_normalized);
	else
		m_cached_iterator->Reset();

	// If we failed to get an iterator, we are now left with
	// potentially dangling objects
	if (m_cached_iterator)
	{
		SVGPathSegObject* obj = m_cached_iterator->GetNextObject();
		while (obj)
		{
			obj->SetInList(NULL);
			obj = m_cached_iterator->GetNextObject();
		}

		OP_DELETE(m_cached_iterator);
	}

	SVGObject::DecRef(m_path);
}

SVGPathSegObject*
SVGDOMPathSegListImpl::GetSVGPathSeg(SVGDOMItem* item)
{
	SVGDOMPathSegImpl* impl = static_cast<SVGDOMPathSegImpl*>(item);
	SVGPathSegObject* obj = impl->GetSVGPathSegObj();
	return obj;
}

void
SVGDOMPathSegListImpl::SetInList(SVGDOMItem* item)
{
	OP_ASSERT(!m_normalized);

	if (!item)
		return;

	SVGPathSegObject* obj = GetSVGPathSeg(item);
	obj->SetInList(this);
}

void
SVGDOMPathSegListImpl::DropObject(SVGPathSegObject *svg_obj)
{
	if (!svg_obj)
		return;

	DOM_Object* dom_obj;
	OpStatus::Ignore(m_objects.Remove(svg_obj, &dom_obj));

	if (dom_obj)
		DOM_Utils::ReleaseSVGDOMObjectFromLists(dom_obj);

	if (!m_normalized)
		svg_obj->SetInList(NULL);
}

#if 0
void
SVGDOMPathSegListImpl::ReleaseFromList(SVGDOMItem* item)
{
	if (!item)
		return;

	SVGPathSegObject* obj = GetSVGPathSeg(item);
	if (!m_normalized)
		obj->SetInList(NULL);
}
#endif

/* static */ void
SVGDOMPathSegListImpl::ReleaseFromListFunc(const void *item, void *o)
{
	DOM_Object* dom_obj = (DOM_Object*)o;
	if (dom_obj)
		DOM_Utils::ReleaseSVGDOMObjectFromLists(dom_obj);

	SVGPathSegObject* obj = (SVGPathSegObject*)item;
	obj->SetInList(NULL); // This doesn't hurt normalized segments, but is only necessary for un-normalized.
}

/* virtual */ void
SVGDOMPathSegListImpl::Clear()
{
	ResetIterator();
	m_objects.ForEach(ReleaseFromListFunc);
	m_objects.RemoveAll();
	m_path->Clear();
}

/* virtual */ OP_STATUS
SVGDOMPathSegListImpl::Initialize(SVGDOMItem* new_item)
{
	ResetIterator();
	Clear();
	OP_BOOLEAN res = InsertItemBefore(new_item, 0);
	RETURN_IF_MEMORY_ERROR(res);
	return res == OpBoolean::IS_TRUE ? OpStatus::OK : OpStatus::ERR;
}

/* virtual */ OP_BOOLEAN
SVGDOMPathSegListImpl::CreateItem(UINT32 idx, SVGDOMItem*& item)
{
	item = OP_NEW(SVGDOMPathSegImpl, (m_path->Get(idx, m_normalized)));
	if (!item)
		return OpStatus::ERR_NO_MEMORY;
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
SVGDOMPathSegListImpl::InsertItemBefore(SVGDOMItem* new_item, UINT32 index)
{
	ResetIterator();
	SVGPathSegObject* src = GetSVGPathSeg(new_item);

	if (!src)
		return OpBoolean::IS_FALSE;

	src->seg.MakeExplicit();

	OP_ASSERT(index <= (m_path->GetCount(m_normalized) + 1));
	RETURN_IF_MEMORY_ERROR(m_path->Insert(index, src, m_normalized));
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_STATUS
SVGDOMPathSegListImpl::RemoveItem (UINT32 index)
{
	// Should be checked by the caller
	OP_ASSERT (m_path->GetCount(m_normalized) > 0 &&
			   index < m_path->GetCount(m_normalized));

	ResetIterator();
	DropObject(m_path->Get(index, m_normalized));
	return m_path->Delete(index, m_normalized);
}

/* virtual */ OP_BOOLEAN
SVGDOMPathSegListImpl::ApplyChange(UINT32 idx, SVGDOMItem* item)
{
	SVGPathSegObject* src = GetSVGPathSeg(item);
	if (src == NULL)
		return OpBoolean::IS_FALSE;

	ResetIterator();
	src->seg.MakeExplicit();

	if (idx < m_path->GetCount(m_normalized))
	{
		/* Replace */
		DropObject(m_path->Get(idx, m_normalized));
		RETURN_IF_MEMORY_ERROR(m_path->Replace(idx, src, m_normalized));
	}
	else if (idx == m_path->GetCount(m_normalized))
	{
		/* Append */
		RETURN_IF_MEMORY_ERROR(m_path->Add(src /*, m_normalized */));
	}

	return OpBoolean::IS_TRUE;
}

/* virtual */ UINT32
SVGDOMPathSegListImpl::GetCount()
{
	return m_path->GetCount(m_normalized);
}

/* virtual */ DOM_Object*
SVGDOMPathSegListImpl::GetDOMObject(UINT32 idx)
{
	SVGObject *svg_obj = NULL;
	if (idx == m_last_iterator_idx + 1 || idx == 0)
	{
		if (idx == 0)
		{
			ResetIterator();
		}

		if (!m_cached_iterator)
		{
			m_cached_iterator = m_path->GetPathIterator(m_normalized);
			if (!m_cached_iterator)
				return NULL;
		}

		svg_obj = m_cached_iterator->GetNextObject();
		m_last_iterator_idx++;
	}
	else
	{
		svg_obj = m_path->Get(idx, m_normalized);
	}

	if (!svg_obj)
		return NULL;

//	OP_ASSERT(svg_obj == m_path->Get(idx, m_normalized));

	DOM_Object* obj;
	if (OpStatus::IsSuccess(m_objects.GetData(svg_obj, &obj)))
		return obj;
	else
		return NULL;
}

/* virtual */ OP_STATUS
SVGDOMPathSegListImpl::SetDOMObject(SVGDOMItem* item, DOM_Object* obj)
{
	OP_ASSERT(item);
	SVGObject* svg_obj = item->GetSVGObject();
	OP_ASSERT(svg_obj);
	OP_STATUS status = m_objects.Add(svg_obj, obj);
	if (status == OpStatus::ERR)
	{
 		OpStatus::Ignore(m_objects.Remove(svg_obj, &obj));
		status = m_objects.Add(svg_obj, obj);
	}
	OP_ASSERT(status != OpStatus::ERR);
	if (OpStatus::IsMemoryError(status))
		return OpStatus::ERR_NO_MEMORY;

	// Only un-normalized segments need to know what lists they belong
	// to.
	if (!m_normalized)
		SetInList(item);
	return OpStatus::OK;
}

void
SVGDOMPathSegListImpl::ResetIterator()
{
	m_last_iterator_idx = (UINT32)-1;
	if (m_cached_iterator)
		m_cached_iterator->Reset();
}

/* virtual */ void
SVGDOMPathSegListImpl::ReleaseDOMObject(SVGDOMItem* item)
{
	if (!item)
		return;
	SVGObject* svg_obj = item->GetSVGObject();
	OP_ASSERT(svg_obj);
	DOM_Object* obj;
	OpStatus::Ignore(m_objects.Remove(svg_obj, &obj));
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
