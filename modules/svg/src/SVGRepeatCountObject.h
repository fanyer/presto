/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_REPEAT_COUNT_OBJECT_H
#define SVG_REPEAT_COUNT_OBJECT_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_number.h"
#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGObject.h"

class SVGRepeatCount
{
public:
	enum RepeatCountType
	{
		SVGREPEATCOUNTTYPE_INDEFINITE,
		SVGREPEATCOUNTTYPE_VALUE,
		SVGREPEATCOUNTTYPE_UNSPECIFIED
	};

	SVGRepeatCount(RepeatCountType rct, float v) :
		repeat_count_type(rct),
		value(v)
		{}

	SVGRepeatCount() :
		repeat_count_type(SVGREPEATCOUNTTYPE_UNSPECIFIED),
		value(0.0)
		{}

	BOOL operator==(const SVGRepeatCount& other) const;

	RepeatCountType repeat_count_type;
	float value;

	BOOL IsIndefinite() { return repeat_count_type == SVGREPEATCOUNTTYPE_INDEFINITE; }
	BOOL IsValue() { return repeat_count_type == SVGREPEATCOUNTTYPE_VALUE; }
	BOOL IsUnspecified() { return repeat_count_type == SVGREPEATCOUNTTYPE_UNSPECIFIED; }
};

class SVGRepeatCountObject : public SVGObject
{
public:

	SVGRepeatCountObject() : SVGObject(SVGOBJECT_REPEAT_COUNT) {}
	SVGRepeatCountObject(SVGRepeatCount::RepeatCountType rct, float v)
		: SVGObject(SVGOBJECT_REPEAT_COUNT),
		  repeat_count(rct, v)
		{}
	SVGRepeatCountObject(const SVGRepeatCount& rc)
		: SVGObject(SVGOBJECT_REPEAT_COUNT),
		  repeat_count(rc)
		{}

	SVGRepeatCount repeat_count;

	virtual BOOL IsEqual(const SVGObject &v1) const;
	virtual SVGObject *Clone() const { return OP_NEW(SVGRepeatCountObject, (repeat_count)); }
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	void Copy(const SVGRepeatCountObject& other_repeat_count_object) {
		repeat_count = other_repeat_count_object.repeat_count;
	}

	BOOL IsValue() { return repeat_count.repeat_count_type == SVGRepeatCount::SVGREPEATCOUNTTYPE_VALUE; }
	BOOL IsIndefinite() { return repeat_count.repeat_count_type == SVGRepeatCount::SVGREPEATCOUNTTYPE_INDEFINITE; }
};

#endif // SVG_SUPPORT
#endif // !SVG_REPEAT_COUNT_OBJECT_H

