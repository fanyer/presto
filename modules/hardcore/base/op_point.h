/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MODULES_HARDCORE_BASE_OP_POINT_H
#define MODULES_HARDCORE_BASE_OP_POINT_H

/** Two-dimensional point in a co-ordinate system. */
class OpPoint
{
public:
	INT32 x, y;

	OpPoint() : x(0), y(0) {}
	OpPoint(INT32 x,INT32 y) : x(x), y(y) {}

	/** Set this point to the specified co-ordinates. */
	void Set(INT32 xx, INT32 yy) { x = xx; y = yy; }

	/** Return TRUE if this point is equal to the other point. */
	BOOL Equals(const OpPoint& p) const { return x == p.x && y == p.y; }

	/** Return TRUE if this point is equal to the other point. */
	BOOL operator==(const OpPoint& p) const { return Equals(p); }

	/** Return TRUE if this point is not equal to the other point. */
	BOOL operator!=(const OpPoint& p) const { return !Equals(p); }

	/** Return the negative value of this point. */
	OpPoint operator-() const { return OpPoint(-x, -y); }

	/** Add the specified point to this point. */
	OpPoint& operator+=(const OpPoint& p) { x += p.x; y += p.y; return *this; }

	/** Subtract the specified point from this point. */
	OpPoint& operator-=(const OpPoint& p) { x -= p.x; y -= p.y; return *this; }

	/** Return the sum of this point with the specified point. */
	OpPoint operator+(const OpPoint& p) const { return OpPoint(x+p.x, y+p.y); }

	/** Return the difference of this point and the specified point. */
	OpPoint operator-(const OpPoint& p) const { return OpPoint(x-p.x, y-p.y); }
};


#endif // MODULES_HARDCORE_BASE_OP_POINT_H
