/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef SVG_NUMBERPAIR_H
#define SVG_NUMBERPAIR_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_number.h"

class SVGNumberPair
{
public:
	SVGNumberPair() : x(0), y(0) {}
	SVGNumberPair(SVGNumber nx, SVGNumber ny) : x(nx), y(ny) {}

	SVGNumberPair Scaled(SVGNumber l) { return SVGNumberPair(x * l, y * l); }
	SVGNumberPair Normalize() const;

	SVGNumber x;
	SVGNumber y;

	static SVGNumber Dot(const SVGNumberPair& p1, const SVGNumberPair& p2);
};

#endif // SVG_SUPPORT
#endif // SVG_NUMBERPAIR_H
