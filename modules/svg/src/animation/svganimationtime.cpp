/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/animation/svganimationtime.h"

/* virtual */ OP_STATUS
SVGAnimationTimeObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	OpString8 a;
	if (SVGAnimationTime::IsNumeric(value))
	{
		if (value == SVGAnimationTime::Earliest())
		{
			RETURN_IF_ERROR(a.AppendFormat("%dms", INT_MIN));
		}
		else
		{
			RETURN_IF_ERROR(a.AppendFormat("%dms", (int)value));
		}
		buffer->Append(a.CStr());
	}
	else if (SVGAnimationTime::IsIndefinite(value))
	{
		RETURN_IF_ERROR(buffer->Append("indefinite"));
	}
	else if (SVGAnimationTime::IsUnresolved(value))
	{
		RETURN_IF_ERROR(buffer->Append("unresolved"));
	}
	else
	{
		OP_ASSERT(!"Not reached.");
	}
	return OpStatus::OK;
}

/* virtual */ BOOL
SVGAnimationTimeObject::IsEqual(const SVGObject& other) const
{
	if (other.Type() == SVGOBJECT_ANIMATION_TIME)
	{
		const SVGAnimationTimeObject &other_animation_time = static_cast<const SVGAnimationTimeObject &>(other);
		return other_animation_time.value == value;
	}
	else
	{
		return FALSE;
	}
}

/* virtual */ SVGObject *
SVGAnimationTimeObject::Clone() const
{
	SVGAnimationTimeObject *time_object = OP_NEW(SVGAnimationTimeObject, (value));
	if (time_object)
		time_object->CopyFlags(*this);
	return time_object;
}

#endif // SVG_SUPPORT
