/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/animation/svganimationvalue.h"
#include "modules/layout/layoutprops.h"

void SVGRect::Set(const SVGRect& other)
{
	x = other.x;
	y = other.y;
	width = other.width;
	height = other.height;
}

void SVGRect::Set(SVGNumber x, SVGNumber y, SVGNumber width, SVGNumber height)
{
	this->x = x;
	this->y = y;
	this->width = width;
	this->height = height;
}

BOOL SVGRect::IsEqual(const SVGRect& other) const
{
	return x.Equal(other.x) && y.Equal(other.y) &&
		width.Equal(other.width) && height.Equal(other.height);
}

void SVGRect::UnionWith(const SVGRect& rect) ///< Smallest rect enclosing the both rects.
{
	if (IsEmpty())
	{
		Set(rect);
	}
	else if (!rect.IsEmpty())
	{
		SVGNumber minx = SVGNumber::min_of(x, rect.x);
		SVGNumber miny = SVGNumber::min_of(y, rect.y);
		SVGNumber maxx = SVGNumber::max_of(x + width, rect.x + rect.width);
		SVGNumber maxy = SVGNumber::max_of(y + height, rect.y + rect.height);

		Set(minx, miny, maxx - minx, maxy - miny);
	}
}

OpRect SVGRect::GetEnclosingRect() const
{
	int int_x = x.GetIntegerValue();
	int int_y = y.GetIntegerValue();
	SVGNumber right = x + width;
	SVGNumber bottom = y + height;
	int int_width = (int)op_ceil(right.GetFloatValue()) - int_x;
	int int_height = (int)op_ceil(bottom.GetFloatValue()) - int_y;
	return OpRect(int_x, int_y, int_width, int_height);
}

void SVGBoundingBox::UnionWith(const SVGBoundingBox& bbox)
{
	if (!bbox.IsEmpty())
	{
		minx = SVGNumber::min_of(bbox.minx, minx);
		miny = SVGNumber::min_of(bbox.miny, miny);
		maxx = SVGNumber::max_of(bbox.maxx, maxx);
		maxy = SVGNumber::max_of(bbox.maxy, maxy);
	}
}

void SVGBoundingBox::UnionWith(const SVGRect& rect)
{
	if (!rect.IsEmpty())
	{
		minx = SVGNumber::min_of(rect.x, minx);
		miny = SVGNumber::min_of(rect.y, miny);
		maxx = SVGNumber::max_of(rect.x + rect.width, maxx);
		maxy = SVGNumber::max_of(rect.y + rect.height, maxy);
	}
}

void SVGBoundingBox::IntersectWith(const SVGRect& rect)
{
	if (rect.IsEmpty())
	{
		Clear();
	}
	else
	{
		if (!IsEmpty())
		{
			minx = SVGNumber::max_of(minx, rect.x);
			miny = SVGNumber::max_of(miny, rect.y);
			maxx = SVGNumber::min_of(maxx, rect.x + rect.width);
			maxy = SVGNumber::min_of(maxy, rect.y + rect.height);
		}
	}
}

void SVGBoundingBox::Extend(SVGNumber extra_x, SVGNumber extra_y)
{
	if (IsEmpty())
		return;

	minx -= extra_x;
	miny -= extra_y;
	maxx += extra_x;
	maxy += extra_y;
}

OpRect SVGBoundingBox::GetEnclosingRect() const
{
	int iminx = minx.GetIntegerValue();
	int iminy = miny.GetIntegerValue();
	int imaxx = maxx.ceil().GetIntegerValue();
	int imaxy = maxy.ceil().GetIntegerValue();
	return OpRect(iminx, iminy, imaxx - iminx, imaxy - iminy);
}

void
SVGRect::Interpolate(const SVGRect &fr, const SVGRect& to,
					 SVG_ANIMATION_INTERVAL_POSITION interval_position)
{
	x = fr.x + (to.x - fr.x) * interval_position;
	y = fr.y + (to.y - fr.y) * interval_position;
	width = fr.width + (to.width - fr.width) * interval_position;
	height = fr.height + (to.height - fr.height) * interval_position;
}

/* virtual */ OP_STATUS
SVGRectObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(rect.x.GetFloatValue()));
	RETURN_IF_ERROR(buffer->Append(' '));
	RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(rect.y.GetFloatValue()));
	RETURN_IF_ERROR(buffer->Append(' '));
	RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(rect.width.GetFloatValue()));
	RETURN_IF_ERROR(buffer->Append(' '));
	return buffer->AppendDoubleToPrecision(rect.height.GetFloatValue());
}

/* virtual */
BOOL SVGRectObject::IsEqual(const SVGObject& other) const
{
	if (other.Type() == SVGOBJECT_RECT)
	{
		const SVGRectObject& other_rect =
			static_cast<const SVGRectObject&>(other);
		return rect.IsEqual(other_rect.rect);
	}
	return FALSE;
}

/* virtual */ BOOL
SVGRectObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_RECT;
	animation_value.reference.svg_rect = &rect;
	return TRUE;
}

#endif // SVG_SUPPORT
