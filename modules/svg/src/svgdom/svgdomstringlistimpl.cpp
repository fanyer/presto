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
#include "modules/svg/src/svgdom/svgdomstringlistimpl.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGValue.h"

SVGDOMStringListImpl::SVGDOMStringListImpl(SVGVector* vector) :
		m_vector(vector)
{
	OP_ASSERT(m_vector->VectorType() == SVGOBJECT_STRING);
	SVGObject::IncRef(m_vector);
}

/* virtual */
SVGDOMStringListImpl::~SVGDOMStringListImpl()
{
	SVGObject::DecRef(m_vector);
}

/* virtual */ const char*
SVGDOMStringListImpl::GetDOMName()
{
	return "SVGStringList";
}

/* virtual */ void
SVGDOMStringListImpl::Clear()
{
	m_vector->Clear();
}

/* virtual */ OP_STATUS
SVGDOMStringListImpl::Initialize(const uni_char* new_item)
{
	Clear();
	OP_BOOLEAN res = InsertItemBefore(new_item, 0);
	RETURN_IF_MEMORY_ERROR(res);
	return res == OpBoolean::IS_TRUE ? OpStatus::OK : OpStatus::ERR;
}

/* virtual */ OP_BOOLEAN
SVGDOMStringListImpl::GetItem(UINT32 idx, const uni_char*& new_item)
{
	SVGObject* obj = m_vector->Get(idx);
	OP_ASSERT(obj != NULL);
	OP_ASSERT(obj->Type() == SVGOBJECT_STRING);
	SVGString* str_val = static_cast<SVGString*>(obj);
	new_item = str_val->GetString();
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
SVGDOMStringListImpl::InsertItemBefore(const uni_char* new_item, UINT32 index)
{
	SVGString* str_val = OP_NEW(SVGString, ());
	if (!str_val ||
		OpStatus::IsMemoryError(str_val->SetString(new_item, uni_strlen(new_item))) ||
		OpStatus::IsMemoryError(m_vector->Insert(index, str_val)))
	{
		OP_DELETE(str_val);
		return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		return OpBoolean::IS_TRUE;
	}
}

/* virtual */ OP_STATUS
SVGDOMStringListImpl::RemoveItem(UINT32 index, const uni_char*& removed_item)
{
	// Should be checked by the caller
	OP_ASSERT(m_vector->GetCount() > 0 && index < m_vector->GetCount());

	SVGObject* obj = m_vector->Get(index);
	OP_ASSERT(obj);

	TempBuffer* buffer = g_svg_manager_impl->GetEmptyTempBuf();
	RETURN_IF_ERROR(obj->GetStringRepresentation(buffer));
	removed_item = buffer->GetStorage();
	m_vector->Remove(index);
	return OpStatus::OK;
}

/* virtual */ UINT32
SVGDOMStringListImpl::GetCount()
{
	return m_vector->GetCount();
}

/* virtual */ OP_BOOLEAN
SVGDOMStringListImpl::ApplyChange(UINT32 idx, const uni_char* new_item)
{
	if (idx < m_vector->GetCount())
	{
		/* Replace */
		SVGString* str_val = OP_NEW(SVGString, ());
		if (!str_val ||
			OpStatus::IsMemoryError(str_val->SetString(new_item, uni_strlen(new_item))) ||
			OpStatus::IsMemoryError(m_vector->Replace(idx, str_val)))
		{
			OP_DELETE(str_val);
			return OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			return OpBoolean::IS_TRUE;
		}
	}
	else
	{
		/* Append */
		return InsertItemBefore(new_item, idx);
	}
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
