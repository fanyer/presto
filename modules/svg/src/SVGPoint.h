/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef SVG_POINT_H
#define SVG_POINT_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGObject.h"

#include "modules/svg/src/animation/svganimationinterval.h"

class SVGPoint : public SVGObject
{
public:
	SVGPoint() : SVGObject(SVGOBJECT_POINT), x(0), y(0) {}
	SVGPoint(SVGNumber nx, SVGNumber ny) : SVGObject(SVGOBJECT_POINT), x(nx), y(ny) {}
	virtual ~SVGPoint() {}

	virtual BOOL IsEqual(const SVGObject& obj) const;
	virtual SVGObject *Clone() const { return OP_NEW(SVGPoint, (x, y)); }
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	void Interpolate(const SVGPoint& p1, const SVGPoint& p2, SVG_ANIMATION_INTERVAL_POSITION where);

	SVGNumber x;
	SVGNumber y;
};

#endif // SVG_SUPPORT
#endif // SVG_POINT_H
