/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MODULES_HARDCORE_BASE_OP_RECT_H
#define MODULES_HARDCORE_BASE_OP_RECT_H

#include "modules/hardcore/base/op_point.h"

#ifndef INT32_MAX
# define INT32_MAX 0x7fffffff
#endif

/** Description of a co-ordinate-aligned recangle.
 *
 * The left and top boundaries are "inside", the right and bottom boundaries are
 * not. See Contains(const OpPoint &OpPoint) method for ramifications.
 */
class OpRect
{
public:
	INT32 x, y, width, height;  // width and height must be >= 0.

	OpRect()
		: x(0), y(0), width(0), height(0) {}

	OpRect(INT32 x, INT32 y, INT32 width, INT32 height)
		: x(x), y(y), width(width), height(height) {}

	explicit OpRect(const RECT* sys) // See system.txt, op_types.h in hardcore/base/.
		: x(sys->left), y(sys->top), width(sys->right - sys->left), height(sys->bottom - sys->top) {}

	// Associated values, point and rectangles:

	/** Return the left edge of the rectangle. */
	INT32 Left() const { return x; }

	/** Return the top edge of the rectangle. */
	INT32 Top () const { return y; }

	/** Return the right edge of the rectangle. */
	INT32 Right() const { return x + width;  }

	/** Return the right edge of the rectangle. Compared to plain
		Right(), this method handles integer overflow. */
	INT32 SafeRight() const {
		if (x < INT32_MAX - width)
			return x + width;
		else
			return INT32_MAX;
	}

	/** Return the bottom edge of the rectangle. */
	INT32 Bottom() const { return y + height; }

	/** Return the bottom edge of the rectangle. Compared to plain
		Bottom(), this method handles integer overflow. */
	INT32 SafeBottom() const {
		if (y < INT32_MAX - height)
			return y + height;
		else
			return INT32_MAX;
	}

	/** Return the point in the center of the rectangle. */
	OpPoint Center() const { return OpPoint(x + width / 2, y + height / 2); }

	/** Return the top-left point of the rectangle. */
	OpPoint TopLeft() const { return OpPoint(Left(), Top()); }

	/** Return the top-right point of the rectangle. */
	OpPoint TopRight() const { return OpPoint(Right(), Top()); }

	/** Return the bottom-left point of the rectangle. */
	OpPoint BottomLeft() const { return OpPoint(Left(), Bottom()); }

	/** Return the bottom-right point of the rectangle. */
	OpPoint BottomRight() const { return OpPoint(Right(), Bottom()); }

	/** Return a rectangle with each of the four edges inset by the specified amount.
	 *
	 * @param dx Number of pixels to increase the left and decrease the right edges.
	 * @param dy Number of pixels to increase the top and decrease the bottom edges.
	 */
	OpRect InsetBy(int dx, int dy) const
	{
		return OpRect(x + dx, y + dy, width - dx * 2, height - dy * 2);
	}

	/** Return a rectangle with each of the four edges inset by the specified amount.
	 *
	 * This is the same as calling InsetBy(gap, gap).
	 */
	OpRect InsetBy(int gap) const { return InsetBy(gap, gap); }

	// Predicates:

	/** Return TRUE if the rectangle is empty. */
	BOOL IsEmpty() const { return width <= 0 || height <= 0; }

	/** Return TRUE if the two rectangles are equal.
	 *
	 * Empty rectangles are equal, even at distinct places.
	 */
	BOOL Equals(const OpRect& rect) const
	{
		return (IsEmpty() && rect.IsEmpty()) ||
			(rect.x == x && rect.y == y && rect.width == width && rect.height == height);
	}

	/** Return TRUE if the two rectangles intersect. */
	BOOL Intersecting(const OpRect &rect) const
	{
		return x < rect.Right() && rect.x < Right() && y < rect.Bottom() && rect.y < Bottom();
	}

	/** Return TRUE if the specified point is within this rectangle. */
	BOOL Contains(const OpPoint &point) const
	{
		return point.x >= x && point.y >= y && point.x < Right() && point.y < Bottom();
	}

	/** Return TRUE if all of the specified rectangle is within the bounds of this rectangle. */
	BOOL Contains(const OpRect& rect) const
	{
		/* <pedant> In principle, rect.IsEmpty() should imply
		 * this->Contains(rect), even if this->IsEmpty(); but, historically,
		 * we've not been that pedantic. </pedant> */
		return !IsEmpty() &&
			rect.x >= x && rect.y >= y && rect.Right() <= Right() && rect.Bottom() <= Bottom();
	}

	// Modifiers:

	/** Set this to an empty rectangle. */
	void Empty() { x = y = width = height = 0; }

#ifndef DOXYGEN_DOCUMENTATION
	/** Set this to the specified rectangle.
	 *
	 * @deprecated Just use the default copy-assignment operator (or copy-constructor)
	 */
	DEPRECATED(void Set(const OpRect& rect));
#endif // !DOXYGEN_DOCUMENTATION

	/** Move the rectangle.
	 *
	 * @param dx Number of pixels to move this rectangle rightwards. Negative values will move it leftwards.
	 * @param dy Number of pixels to move this rectangle downwards. Negative values will move it upwards.
	 */
	void OffsetBy(INT32 dx, INT32 dy)
	{
		x += dx;
		y += dy;
	}

	/** Move the rectangle.
	 *
	 * This is the same as calling OffsetBy(offset.x, offset.y).
	 */
	void OffsetBy(const OpPoint& offset) { OffsetBy(offset.x, offset.y); }

	/** Set this rectangle to the specified co-ordinates and dimensions. */
	void Set(INT32 x, INT32 y, INT32 width, INT32 height)
	{
		this->x = x;
		this->y = y;
		this->width = width;
		this->height = height;
	}

	/** Shrink this to its overlap with the other rect. */
	void IntersectWith(const OpRect& rect)
	{
		if (rect.IsEmpty())
			Empty();
		else
			if (!IsEmpty())
			{
				const INT32 minx = MAX(x, rect.x);
				const INT32 miny = MAX(y, rect.y);
				const INT32 maxx = MIN(Right(),  rect.Right());
				const INT32 maxy = MIN(Bottom(), rect.Bottom());

				Set(minx, miny, MAX(0, maxx - minx), MAX(0, maxy - miny));
			}
	}

	/** Shrink this to its overlap with the other rect. Compared to
		plain IntersectWith(), this method handle integer overflow. */
	void SafeIntersectWith(const OpRect& rect)
	{
		if (rect.IsEmpty())
			Empty();
		else
			if (!IsEmpty())
			{
				const INT32 minx = MAX(x, rect.x);
				const INT32 miny = MAX(y, rect.y);
				const INT32 maxx = MIN(SafeRight(),  rect.SafeRight());
				const INT32 maxy = MIN(SafeBottom(), rect.SafeBottom());

				Set(minx, miny, MAX(0, maxx - minx), MAX(0, maxy - miny));
			}
	}

	/** Expand this to include all of the other rect. */
	void UnionWith(const OpRect &rect)
	{
		if (IsEmpty())
			*this = rect;
		else
			if (!rect.IsEmpty())
			{
				const INT32 minx = MIN(x, rect.x);
				const INT32 miny = MIN(y, rect.y);
				const INT32 maxx = MAX(Right(),  rect.Right());
				const INT32 maxy = MAX(Bottom(), rect.Bottom());

				Set(minx, miny, maxx - minx, maxy - miny);
			}
	}

	/** Convert to a RECT that is mathematically equal to this.
	 *
	 * Uses the same convention that OpRect does,
	 * so returned.right == this->Right() and returned.bottom == this->Bottom()
	 * after the operation.
	 */
	operator RECT() const
	{
		RECT r = {x, y, x + width, y + height};
		return r;
	}
};

#ifndef DOXYGEN_DOCUMENTATION
inline void OpRect::Set(const OpRect& rect)
{
	*this = rect;
}
#endif // !DOXYGEN_DOCUMENTATION

#endif // MODULES_HARDCORE_BASE_OP_RECT_H
