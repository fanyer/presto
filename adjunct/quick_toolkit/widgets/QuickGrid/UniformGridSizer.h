/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef UNIFORM_GRID_SIZER_H
#define UNIFORM_GRID_SIZER_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridSizer.h"

class GridCellContainer;

/** A GridSizer that assigns uniform sizes to all cells.
  *
  * @author Wojciech Dzierzanowski (wdzierzanowski)
  * @see GenericGrid::SetUniformCells
  */
class UniformGridSizer : public GridSizer
{
public:
	explicit UniformGridSizer(GridCellContainer& grid);

	// GridSizer

	virtual OP_STATUS SetColumnSizes(GridLayouter& layouter);
	virtual OP_STATUS SetRowSizes(GridLayouter& layouter);

	virtual void Invalidate() { m_valid = false; }

	virtual unsigned GetMinimumGridWidth() { return GetGridWidth(MINIMUM); }
	virtual unsigned GetMinimumGridHeight() { return GetGridHeight(MINIMUM); }
	virtual unsigned GetMinimumGridHeight(unsigned width, GridLayouter& layouter) { return GetGridHeight(MINIMUM, width, layouter); }

	virtual unsigned GetNominalGridWidth() { return GetGridWidth(NOMINAL); }
	virtual unsigned GetNominalGridHeight() { return GetGridHeight(NOMINAL); }
	virtual unsigned GetNominalGridHeight(unsigned width, GridLayouter& layouter) { return GetGridHeight(NOMINAL, width, layouter); }

	virtual unsigned GetPreferredGridWidth() { return GetGridWidth(PREFERRED); }
	virtual unsigned GetPreferredGridHeight() { return GetGridHeight(PREFERRED); }
	virtual unsigned GetPreferredGridHeight(unsigned width, GridLayouter& layouter) { return GetGridHeight(PREFERRED, width, layouter); }

	virtual WidgetSizes::Margins GetGridMargins();

private:
	enum SizeType
	{
		MINIMUM,
		NOMINAL,
		PREFERRED,
		SIZE_COUNT
	};

	void CalculateCellSizes();

	unsigned GetGridWidth(SizeType type);
	unsigned GetGridHeight(SizeType type);
	unsigned GetGridHeight(SizeType type, unsigned grid_width, GridLayouter& layouter);

	unsigned CalculateGridHeight(unsigned cell_height);
	unsigned CalculateCellHeight(SizeType type, GridLayouter& layouter);

	GridCellContainer& m_grid;
	bool m_valid;

	unsigned m_cell_width[SIZE_COUNT];
	unsigned m_cell_height[SIZE_COUNT];
	unsigned m_cell_horizontal_margins;
	unsigned m_cell_vertical_margins;
};

#endif // UNIFORM_GRID_SIZER_H
