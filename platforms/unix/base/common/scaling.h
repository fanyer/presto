/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#ifndef __SCALING_H__
#define __SCALING_H__

namespace Scaling
{

	/** Transformation for widths, measures (point sizes, pen width, etc.)
	 *	These will be rounded down to the nearest whole number.
	 */
	inline int WidthToScaled(int scale, int i)
	{
		return (i * scale) / 100;
	} 

	/** Scale from screen coordinates to document coordinates. */
	inline int ToDoc(int v, int scale)
	{
		return ((long)v * 100 + scale - 1) / scale;
	}

	/** Scale from document coordinates to screen coordinates. */
	inline int ToScreen(int v, int scale)
	{
		return ((long)v * scale) / 100;
	}

	/** Scale point from document coordinates to screen coordinates. */
	inline OpPoint ToScreen(const OpPoint &p, int scale) {
		return OpPoint(ToScreen(p.x, scale), ToScreen(p.y, scale));
	}

	/** Scale rectangle from document coordinates to screen coordinates. */
	OpRect ToScreen(const OpRect &rect, int scale);

	/**	Transformation for point coordinates.
	 * We implement the convention that the scaled coordinate (ie pixel coordinate)
	 * shall be the rounded down scaling of the layout coordinate. 
	 * Conversions are performed relative to the viewport origin, so that
	 * scrolling won't introduce off-by-one-pixel errors due to rounding.
	 *
	 * It is important that the absolute offset (in layout coordinates)
	 * computed in QtOpView::SetDocumentPos and QtOpView::GetDocumentPos, 
	 * and the initial transformation from pixel to layout coordinates in
	 * ScrollView::viewportPaintEvent(QPaintEvent *e) [scrollview.cpp around
	 * line 223] follows these conventions so that we can consistently convert
	 * between scaled and unscaled coordinates without rounding errors.
	 *
	 * A special case occurs when scale > 100. Then one layout coordinate can
	 * correspond to two (or more) pixel coordinates. To be sure that we repaint
	 * the entire repaint area the convention adopted is to use the low
	 * coordinate for upper/left corners and the high coordinate for lower/left.
	 * The BOOL argument low selects whether to use the high or low coordinate
	 * in this situation.
	 *
	 * Stein Kulseth
	 */
	void PointToScaled(int scale, OpPoint &dest, const OpPoint &src, BOOL low=TRUE);

	/* Transformation for rectangles.
	 * The scaled rect coordinates (ie in pixel coordinates) are rounded
	 * as explained above.
	 */
	void RectToScaled(int scale, OpRect &dest, const OpRect &src);
}

#endif // __SCALING_H__
