/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/svgdom/svgdompathseglistimpl.h"

SVGDOMPathSegListImpl::SVGDOMPathSegListImpl(OpBpath* path, BOOL normalized) :
		m_path(path), m_normalized(normalized)
{
}

SVGPathSeg*
SVGDOMPathSegListImpl::GetSVGPathSeg(SVGDOMItem* item)
{
	SVGDOMPathSegImpl* impl = static_cast<SVGDOMPathSegImpl*>(item);
	SVGPathSegObject* obj = impl->GetSVGPathSegObj();
	return obj;
}

/* virtual */ void
SVGDOMPathSegListImpl::Clear()
{
	m_path->Clear();
}

/* virtual */ OP_STATUS
SVGDOMPathSegListImpl::Initialize(SVGDOMItem* new_item)
{
	Clear();
	OP_BOOLEAN res = InsertItemBefore(new_item, 0);
	RETURN_IF_MEMORY_ERROR(res);
	return res == OpBoolean::IS_TRUE ? OpStatus::OK : OpStatus::ERR;
}

/* virtual */ OP_BOOLEAN
SVGDOMPathSegListImpl::GetItem(UINT32 idx, SVGDOMItem*& item)
{
	item = OP_NEW(SVGDOMPathSegImpl, (m_path->Get(idx)));
	return item ? OpBoolean::IS_TRUE : OpStatus::ERR_NO_MEMORY;
}

/* virtual */ OP_BOOLEAN
SVGDOMPathSegListImpl::InsertItemBefore(SVGDOMItem* new_item, UINT32 index)
{
	SVGPathSeg* src = GetSVGPathSeg(new_item);

	if (!src)
		return OpBoolean::IS_FALSE;

	src->MakeExplicit();

	OP_ASSERT(index <= (m_path->GetCount() + 1));
	RETURN_IF_MEMORY_ERROR(m_path->Insert(index, src, m_normalized));
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_STATUS
SVGDOMPathSegListImpl::RemoveItem (UINT32 index)
{
	// Should be checked by the caller
	OP_ASSERT (m_path->GetCount(norm) > 0 &&
			   index < m_path->GetCount(norm));

	OP_STATUS status = m_path->Delete(index, norm);
	return status;
}

/* virtual */ OP_BOOLEAN
SVGDOMPathSegListImpl::ApplyChange(UINT32 idx, SVGDOMItem* item)
{
	SVGPathSeg* src = GetSVGPathSeg(item);
	if (src == NULL)
		return OpBoolean::IS_FALSE;

	if (idx < m_path->GetCount(norm))
	{
		// not used
		OP_ASSERT(!"Not used anymore");
		return OpBoolean::IS_FALSE;
	}
	else if (idx == path->GetCount(norm))
	{
		src->MakeExplicit();
		RETURN_IF_MEMORY_ERROR(m_path->Add(src, /* m_normalized */));
		return OpBoolean::IS_TRUE;
	}

	return OpBoolean::IS_FALSE;
}

/* virtual */ UINT32
SVGDOMPathSegListImpl::GetCount()
{
	return m_path->GetCount(norm);
}

#endif // SVG_SUPPORT && SVG_DOM
