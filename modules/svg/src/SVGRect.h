/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_RECT_H
#define SVG_RECT_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_number.h"
#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGNumberPair.h"

#include "modules/util/tempbuf.h"

#include "modules/svg/src/animation/svganimationinterval.h"

class SVGBoundingBox
{
public:
	// An empty box is represented by the upper limit set to 
	// SVGNumber::min and the lower limit set to SVGNumber::max
	SVGBoundingBox() : minx(SVGNumber::max_num()), miny(SVGNumber::max_num()), maxx(SVGNumber::min_num()), maxy(SVGNumber::min_num()) {}
	SVGBoundingBox(const SVGRect& r) : minx(r.x), miny(r.y), maxx(r.x + r.width), maxy(r.y + r.height) {}

	void Clear() {
		minx = SVGNumber::max_num();
		miny = SVGNumber::max_num();
		maxx = SVGNumber::min_num();
		maxy = SVGNumber::min_num();
	}
	
	void UnionWith(const SVGBoundingBox& bbox);
	void UnionWith(const SVGRect& rect);

	void IntersectWith(const SVGRect& rect);

	void Extend(SVGNumber extra) { Extend(extra, extra); }
	void Extend(SVGNumber extra_x, SVGNumber extra_y);

	BOOL Contains(const SVGNumberPair point) {
		return (point.x >= minx && point.x <= maxx && point.y >= miny && point.y <= maxy);
	}

	OpRect GetEnclosingRect() const;
	SVGRect GetSVGRect() const { return SVGRect(minx, miny, maxx - minx, maxy - miny); }

	BOOL IsEmpty() const { return minx > maxx || miny > maxy; }

	SVGNumber Width() { return maxx - minx; }
	SVGNumber Height() { return maxy - miny; }

	SVGNumber minx;
	SVGNumber miny;
	SVGNumber maxx;
	SVGNumber maxy;
};

class SVGRectObject : public SVGObject
{
public:
	SVGRect rect;

	SVGRectObject() : SVGObject(SVGOBJECT_RECT) {}
	SVGRectObject(const SVGRect& r) : SVGObject(SVGOBJECT_RECT), rect(r) {}

	static SVGRect* p(SVGRectObject* obj) { return obj ? &obj->rect : NULL; }
	static const SVGRect* p(const SVGRectObject* obj) { return obj ? &obj->rect : NULL; }

	virtual SVGObject *Clone() const { return OP_NEW(SVGRectObject, (rect)); }
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual BOOL IsEqual(const SVGObject& other) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);
};

#endif // SVG_SUPPORT
#endif // SVG_RECT_H
