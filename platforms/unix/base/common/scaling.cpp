/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

#include "scaling.h"


namespace Scaling
{
	OpRect ToScreen(const OpRect &rect, int scale)
	{
		if (scale == 100)
			return rect;
		OpRect r;
		r.x = (int)(((long) rect.x * scale) / 100);
		r.y = (int)(((long) rect.y * scale) / 100);
		r.width = (int)(((long) (rect.x + rect.width) * scale) / 100) - r.x;
		r.height = (int)(((long) (rect.y + rect.height) * scale) / 100) - r.y;
		return r;
	}

#if 0 // unused
	void PointToScaled(int scale, OpPoint &dest, const OpPoint &src, BOOL low)
	{
		OpPoint sc_off(0, 0);
		OpPoint abs_off;
		abs_off.x = ( sc_off.x * 100 + ((sc_off.x < 0) ? 0 : scale-1) ) / scale;
		abs_off.y = ( sc_off.y * 100 + ((sc_off.y < 0) ? 0 : scale-1) ) / scale;
	
		dest.x = (( src.x + abs_off.x ) * scale );
		dest.y = (( src.y + abs_off.y ) * scale );
		if (low && scale > 100)
		{
			dest.x -= (scale-100);
			dest.y -= (scale-100);
		}
		dest.x = dest.x / 100 - sc_off.x;
		dest.y = dest.y / 100 - sc_off.y;
	// dest.y = (( src.y + abs_off.y ) * scale ) / 100 - sc_off.y;
	}

	void RectToScaled(int scale, OpRect &dest, const OpRect &src)
	{
		if (scale != 100)
		{
			OpPoint orig(src.x, src.y);
			OpPoint p(orig);
			PointToScaled(scale, p, p);
			dest.x = p.x;
			dest.y = p.y;
			p.x = orig.x + src.width;
			p.y = orig.y + src.height;
			PointToScaled(scale, p, p, FALSE);
			dest.width = p.x - dest.x;
			dest.height = p.y - dest.y;
		}
		else dest = src;
	}
#endif
}
