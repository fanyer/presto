/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MODULES_HARDCORE_BASE_OP_DBLPOINT_H
#define MODULES_HARDCORE_BASE_OP_DBLPOINT_H

#ifdef VEGA_OPPAINTER_SUPPORT

/** Two-dimensional point in a co-ordinate system, with double precision. */
// Needed to correctly and elegantly calculate a CSS gradient line.
class OpDoublePoint
{
public:
	double x, y;

	OpDoublePoint() : x(0), y(0) {}
	OpDoublePoint(double x, double y) : x(x), y(y) {}
	OpDoublePoint(OpPoint p) : x(p.x), y(p.y) {}

	/** Set this point to the specified co-ordinates. */
	void Set(double xx, double yy) { x = xx; y = yy; }

	/** Return TRUE if this point is equal to the other point. */
	BOOL Equals(const OpDoublePoint& p) const {
		const double epsilon = 2.2204460492503131e-016;
		return op_fabs(x - p.x) < epsilon
			&& op_fabs(y - p.y) < epsilon;
	}

	/** Return TRUE if this point is equal to the other point. */
	BOOL operator==(const OpDoublePoint& p) const { return Equals(p); }

	/** Return TRUE if this point is not equal to the other point. */
	BOOL operator!=(const OpDoublePoint& p) const { return !Equals(p); }

	/** Return the negative value of this point. */
	OpDoublePoint operator-() const { return OpDoublePoint(-x, -y); }

	/** Add the specified point to this point. */
	OpDoublePoint& operator+=(const OpDoublePoint& p) { x += p.x; y += p.y; return *this; }

	/** Subtract the specified point from this point. */
	OpDoublePoint& operator-=(const OpDoublePoint& p) { x -= p.x; y -= p.y; return *this; }

	/** Return the sum of this point with the specified point. */
	OpDoublePoint operator+(const OpDoublePoint& p) const { return OpDoublePoint(x+p.x, y+p.y); }

	/** Return the difference of this point and the specified point. */
	OpDoublePoint operator-(const OpDoublePoint& p) const { return OpDoublePoint(x-p.x, y-p.y); }
};
#endif // VEGA_OPPAINTER_SUPPORT

#endif // MODULES_HARDCORE_BASE_OP_DBLPOINT_H
