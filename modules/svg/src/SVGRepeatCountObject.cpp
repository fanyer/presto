/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGRepeatCountObject.h"

BOOL
SVGRepeatCount::operator==(const SVGRepeatCount& other) const
{
	if (other.repeat_count_type == repeat_count_type)
	{
		if (repeat_count_type == SVGREPEATCOUNTTYPE_VALUE)
		{
			return ::op_fabs(value - other.value) < DBL_EPSILON;
		}
		else
		{
			return TRUE;
		}
	}
	else
	{
		return FALSE;
	}
}

/* virtual */ BOOL
SVGRepeatCountObject::IsEqual(const SVGObject &v1) const
{
	if (v1.Type() == SVGOBJECT_REPEAT_COUNT)
	{
		const SVGRepeatCountObject &other = static_cast<const SVGRepeatCountObject &>(v1);
		return other.repeat_count == repeat_count;
	}

	return FALSE;
}

/* virtual */ OP_STATUS
SVGRepeatCountObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	OP_ASSERT(repeat_count.repeat_count_type != SVGRepeatCount::SVGREPEATCOUNTTYPE_UNSPECIFIED);
	if (repeat_count.repeat_count_type == SVGRepeatCount::SVGREPEATCOUNTTYPE_INDEFINITE)
	{
		RETURN_IF_ERROR(buffer->Append("indefinite"));
	}
	else
	{
		OP_ASSERT(repeat_count.repeat_count_type == SVGRepeatCount::SVGREPEATCOUNTTYPE_VALUE);
		RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(repeat_count.value));
	}
	return OpStatus::OK;
}

#endif // SVG_SUPPORT
