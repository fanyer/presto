/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGPoint.h"

#include "modules/svg/src/animation/svganimationvalue.h"

/* virtual */ BOOL
SVGPoint::IsEqual(const SVGObject& obj) const
{
    if (obj.Type() == SVGOBJECT_POINT)
    {
		const SVGPoint& other = static_cast<const SVGPoint&>(obj);
		return !x.NotEqual(other.x) && !y.NotEqual(other.y);
    }
    return FALSE;
}

/* virtual */ OP_STATUS
SVGPoint::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(x.GetFloatValue()));
	RETURN_IF_ERROR(buffer->Append(' '));
	return buffer->AppendDoubleToPrecision(y.GetFloatValue());
}

void
SVGPoint::Interpolate(const SVGPoint& p1, const SVGPoint& p2, SVG_ANIMATION_INTERVAL_POSITION interval_position)
{
	x = p1.x + ((p2.x - p1.x) * interval_position);
	y = p1.y + ((p2.y - p1.y) * interval_position);
}

/* virtual */ BOOL
SVGPoint::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_POINT;
	animation_value.reference.svg_point = this;
	return TRUE;
}


#endif // SVG_SUPPORT
