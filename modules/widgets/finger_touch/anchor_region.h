/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ANCHOR_REGION_H
#define ANCHOR_REGION_H

#include "modules/widgets/finger_touch/eoi_fragments.h"

/** List of anchor bounding rectangles (or bounding region, if you like).
 *
 * The rectangles are sorted in paint / stacking order.
 */
class AnchorRegion : public Head
{
public:
	~AnchorRegion() { Clear(); }

	AnchorFragment* First() const { return (AnchorFragment*) Head::First(); }
	AnchorFragment* Last() const { return (AnchorFragment*) Head::Last(); }
};


#endif // ANCHOR_REGION_H

