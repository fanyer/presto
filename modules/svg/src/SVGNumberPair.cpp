/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGNumberPair.h"

SVGNumberPair
SVGNumberPair::Normalize() const
{
	if (x.Equal(SVGNumber(0)) && y.Equal(SVGNumber(0)))
	{
		return *this;
	}

	SVGNumber len = SVGNumber(x*x + y*y).sqrt();
	return SVGNumberPair(SVGNumber(x / len), SVGNumber(y / len));
}

/* static */ SVGNumber
SVGNumberPair::Dot(const SVGNumberPair& p1, const SVGNumberPair& p2)
{
	return p1.x*p2.x + p1.y*p2.y;
}

#endif // SVG_SUPPORT
