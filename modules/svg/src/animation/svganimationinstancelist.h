/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_INSTANCE_LIST_H
#define SVG_ANIMATION_INSTANCE_LIST_H

#define SVG_ANIMATION_IL_CLEAR_LIMIT 10
#include "modules/svg/src/animation/svganimationtime.h"

class SVGAnimationInstanceList
{
public:
    SVGAnimationInstanceList();
    ~SVGAnimationInstanceList();

    OP_STATUS Add(SVG_ANIMATION_TIME instance);
    /**< Adds the instance to the instance list. The instance list is
     * always kept sorted, so the index that the instance is inserted
     * at is depending on the the instance and the current content of
     * the instance list. */

	OP_STATUS HintAddition(unsigned count);

    SVG_ANIMATION_TIME FirstPast(SVG_ANIMATION_TIME animation_time);
    /**< Get the first instance that is past @[animation_time]. */

    SVG_ANIMATION_TIME FirstEqualOrPast(SVG_ANIMATION_TIME animation_time);
    /**< Get the first instance that is equal or past
     * @[animation_time]. */

    void Clear();
    /**< Clear the instance list */

	SVG_ANIMATION_TIME operator[](unsigned idx);

	unsigned GetCount() { return instance_array_count; }

private:
    OP_STATUS Init();
    OP_STATUS Insert(unsigned idx, SVG_ANIMATION_TIME instance);
    OP_STATUS GrowInsert(unsigned insert_idx, SVG_ANIMATION_TIME item);
    void NormalInsert(unsigned insertidx, SVG_ANIMATION_TIME item);

    unsigned instance_array_size;
    unsigned instance_array_count;
    SVG_ANIMATION_TIME *instance_array;
};

#endif // !SVG_ANIMATION_INSTANCE_LIST_H
