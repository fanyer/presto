/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifndef LAYOUT_COORD_DEBUG_H
#define LAYOUT_COORD_DEBUG_H

#include "modules/layout/layout_debug_utils.h"

/** Debugging coordinate handling inside the layout engine. The aim is
	to be as transparent as possible (look at it as a plain signed
	'int' for all practical purposes) but still allow the compile to
	trace when integers are assigned to variables that are considered
	coordinates in the layout engine.

	The class has an integer operator to allow some seamless
	integration with code that doesn't use layout coordinates,
	typically the display layer (VisualDevice).

	The constructor is explicit, however. So you need to explicitly
	tell the compiler when you want to construct layout coordinates
	from literals, like the very frequently used 'LayoutCoord(0)'
	(represents the zero coordinate). The advantage of this, of
	course, is that you can't assign a 'short' to a layout coordinate
	without explicitly forcing the compiler into letting you do
	that. */

class LayoutCoordDebug
{
public:
	LayoutCoordDebug() {}
	explicit LayoutCoordDebug(int v) : value(v) {}

	LayoutCoordDebug operator+(const LayoutCoordDebug& other) const { LayoutCoordDebug res(*this); res += other; return res; }
	LayoutCoordDebug operator-(const LayoutCoordDebug& other) const { LayoutCoordDebug res(*this); res -= other; return res; }
	LayoutCoordDebug operator*(const LayoutCoordDebug& other) const { LayoutCoordDebug res(*this); res *= other; return res; }
	LayoutCoordDebug operator/(const LayoutCoordDebug& other) const { LayoutCoordDebug res(*this); res /= other; return res; }

	void operator+=(const LayoutCoordDebug& other)
	{
#ifdef LAYOUT_TYPES_RUNTIME_DEBUG
		layout_type_check_add(value, other.value);
#endif // LAYOUT_TYPES_RUNTIME_DEBUG

		value += other.value;
	}

	void operator-=(const LayoutCoordDebug& other)
	{
#ifdef LAYOUT_TYPES_RUNTIME_DEBUG
		layout_type_check_sub(value, other.value);
#endif // LAYOUT_TYPES_RUNTIME_DEBUG

		value -= other.value;
	}
	void operator*=(int other) { value *= other; }
	void operator/=(int other) { value /= other; }

	LayoutCoordDebug operator-() const { LayoutCoordDebug res(*this); res.value = - value; return res; }
	LayoutCoordDebug operator>>(int o) const { LayoutCoordDebug res(*this); res.value = value >> o; return res; }
	LayoutCoordDebug operator<<(int o) const { LayoutCoordDebug res(*this); res.value = value << o; return res; }
	BOOL operator!() const { return !value; }

	BOOL operator>(const LayoutCoordDebug &other) const { return value > other.value; }
	BOOL operator<(const LayoutCoordDebug &other) const { return value < other.value; }
	BOOL operator>=(const LayoutCoordDebug &other) const { return value >= other.value; }
	BOOL operator<=(const LayoutCoordDebug &other) const { return value <= other.value; }

	BOOL operator==(const LayoutCoordDebug &other) const { return other.value == value; }
	BOOL operator!=(const LayoutCoordDebug &other) const { return other.value != value; }

	LayoutCoordDebug& operator++() { ++value; return *this; }
	LayoutCoordDebug& operator--() { --value; return *this; }
	LayoutCoordDebug operator++(int) { LayoutCoordDebug rv(value); ++value; return rv; }
	LayoutCoordDebug operator--(int) { LayoutCoordDebug rv(value); --value; return rv; }

	operator int() const { return value; }

	int value;
};

typedef LayoutCoordDebug LayoutCoord;
#define LAYOUT_COORD_MAX (LayoutCoordDebug(INT_MAX))
#define LAYOUT_COORD_MIN (LayoutCoordDebug(INT_MIN))

#endif // !LAYOUT_COORD_DEBUG_H
