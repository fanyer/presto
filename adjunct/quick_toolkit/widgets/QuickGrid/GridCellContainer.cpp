/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCellContainer.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"


GridCell* GridCellContainer::GetStrictCell(unsigned col, unsigned row)
{
	GridCell* cell = GetCell(col, row);
	OP_ASSERT(cell != 0);
	//If the current cell belongs to a span, GetCell returns the topmost and
	//leftmost cell of the span. This is, by convention, the starting cell of the span.
	//Whether the given cell on (col,row) is a starting cell is determined
	//based on this fact.

	if (cell->GetColSpan() > 1 && col > 0 && GetCell(col - 1, row) == cell)
		return 0;

	if (cell->GetRowSpan() > 1 && row > 0 && GetCell(col, row - 1) == cell)
		return 0;

	return cell;
}
