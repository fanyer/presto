/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon
 */

#ifndef FISH_EYE_LAYOUTER_H
#define FISH_EYE_LAYOUTER_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridLayouter.h"

/** @brief An alternative layouter that makes one cell the largest, and others around it progressively smaller
  */
class FishEyeLayouter : public GridLayouter
{
public:
	FishEyeLayouter() : m_selected(-1) {}

	void SetSelected(int selected) { m_selected = selected; }

	virtual OpRect GetLayoutRectForCell(unsigned col, unsigned row, unsigned colspan, unsigned rowspan);

private:
	void CalculateFishSizes();
	int m_selected;
};

#endif // FISH_EYE_LAYOUTER_H
