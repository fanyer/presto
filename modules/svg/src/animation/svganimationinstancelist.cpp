/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationinstancelist.h"

SVGAnimationInstanceList::SVGAnimationInstanceList() :
    instance_array_size(0),
    instance_array_count(0),
    instance_array(NULL)
{
}

SVGAnimationInstanceList::~SVGAnimationInstanceList()
{
	OP_DELETEA(instance_array);
}

OP_STATUS
SVGAnimationInstanceList::Add(SVG_ANIMATION_TIME instance)
{
    for (unsigned i=instance_array_count-1;i<instance_array_count; i--)
    {
		if (instance == instance_array[i])
		{
			return OpStatus::OK;
		}
		else if (instance > instance_array[i])
		{
			return Insert(i+1, instance);
		}
    }
    return Insert(0, instance);
}

OP_STATUS
SVGAnimationInstanceList::Insert(unsigned idx, SVG_ANIMATION_TIME instance)
{
    if (idx > instance_array_count)
    {
		idx = instance_array_count;
    }

    if (instance_array == NULL)
    {
		RETURN_IF_ERROR(Init());
    }

    if (instance_array_count >= instance_array_size)
    {
		return GrowInsert(idx, instance);
    }
    else
    {
		NormalInsert(idx, instance);
		return OpStatus::OK;
    }
}

SVG_ANIMATION_TIME
SVGAnimationInstanceList::FirstPast(SVG_ANIMATION_TIME animation_time)
{
    for (unsigned i=0; i<instance_array_count; i++)
    {
		SVG_ANIMATION_TIME instance = instance_array[i];
		if (instance > animation_time)
			return instance;
    }

    return SVGAnimationTime::Unresolved();
}

SVG_ANIMATION_TIME
SVGAnimationInstanceList::FirstEqualOrPast(SVG_ANIMATION_TIME animation_time)
{
    for (unsigned i=0; i<instance_array_count; i++)
    {
		SVG_ANIMATION_TIME instance = instance_array[i];
		if (instance >= animation_time)
			return instance;
    }

    return SVGAnimationTime::Unresolved();
}

OP_STATUS
SVGAnimationInstanceList::GrowInsert(unsigned insert_idx, SVG_ANIMATION_TIME instance)
{
    OP_ASSERT(insert_idx <= instance_array_count);

    unsigned new_size = MAX(1, (instance_array_size << 1));
    SVG_ANIMATION_TIME* new_items = OP_NEWA(SVG_ANIMATION_TIME, new_size);
    if (new_items == NULL)
    {
		return OpStatus::ERR_NO_MEMORY;
    }
    // Copy elements before item.
    op_memcpy(&new_items[0], &instance_array[0], insert_idx * sizeof(SVG_ANIMATION_TIME));
    // Insert item.
    new_items[insert_idx] = instance;
    // Copy elements after item.
    op_memcpy(&new_items[insert_idx + 1], &instance_array[insert_idx],
			  (instance_array_count - insert_idx) * sizeof(SVG_ANIMATION_TIME));

    OP_DELETEA(instance_array);

    instance_array_count++;
    instance_array_size = new_size;
    instance_array = new_items;

    return OpStatus::OK;
}

void
SVGAnimationInstanceList::NormalInsert(unsigned insert_idx, SVG_ANIMATION_TIME instance)
{
    OP_ASSERT(insert_idx <= instance_array_count);

    if (insert_idx < instance_array_count)
    {
		// Make room for insert.
		op_memmove(&instance_array[insert_idx + 1], &instance_array[insert_idx], (instance_array_count - insert_idx) * sizeof(SVG_ANIMATION_TIME));
    }

    instance_array[insert_idx] = instance;
    instance_array_count++;
}

OP_STATUS
SVGAnimationInstanceList::Init()
{
    instance_array = OP_NEWA(SVG_ANIMATION_TIME, 1);
    if (instance_array == NULL)
    {
		return OpStatus::ERR_NO_MEMORY;
    }
    instance_array_size = 1;
    return OpStatus::OK;
}

void
SVGAnimationInstanceList::Clear()
{
    instance_array_count = 0;

    if (instance_array_size > SVG_ANIMATION_IL_CLEAR_LIMIT)
    {
		// be seldom triggered.
		instance_array_size = 0;
		OP_DELETEA(instance_array);
		instance_array = NULL;
    }
}

SVG_ANIMATION_TIME
SVGAnimationInstanceList::operator[](unsigned idx)
{
	if (idx < instance_array_count)
	{
		return instance_array[idx];
	}
	else
	{
		return SVGAnimationTime::Unresolved();
	}
}

OP_STATUS
SVGAnimationInstanceList::HintAddition(unsigned count)
{
	if (count == 0)
		return OpStatus::OK;

    unsigned new_size = instance_array_count + count;
	if (new_size > instance_array_size)
	{
		SVG_ANIMATION_TIME* new_items = OP_NEWA(SVG_ANIMATION_TIME, new_size);
		if (new_items == NULL)
			return OpStatus::ERR_NO_MEMORY;

		if (instance_array != NULL)
		{
			op_memcpy(&new_items[0], &instance_array[0],
					  instance_array_count * sizeof(SVG_ANIMATION_TIME));
			OP_DELETEA(instance_array);
		}

		instance_array_size = new_size;
		instance_array = new_items;
	}
    return OpStatus::OK;
}

#endif // SVG_SUPPORT
