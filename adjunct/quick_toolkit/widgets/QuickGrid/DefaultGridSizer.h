/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DEFAULT_GRID_SIZER_H
#define DEFAULT_GRID_SIZER_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCellContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridSizer.h"


/** A GridSizer capable of assigning different sizes to different columns/rows.
  *
  * Used by GenericGrid as the default sizer.
  *
  * @author Arjan van Leeuwen (arjanl)
  * @author Wojciech Dzierzanowski (wdzierzanowski)
  */
class DefaultGridSizer : public GridSizer
{
public:
	explicit DefaultGridSizer(GridCellContainer& grid);

	// GridSizer

	virtual OP_STATUS SetColumnSizes(GridLayouter& layouter);
	virtual OP_STATUS SetRowSizes(GridLayouter& layouter);

	virtual void Invalidate() {}

	virtual unsigned GetMinimumGridWidth() { return CalculateMinimumSize(HORIZONTAL); }
	virtual unsigned GetMinimumGridHeight() { return CalculateMinimumSize(VERTICAL); }
	virtual unsigned GetMinimumGridHeight(unsigned width, GridLayouter& layouter) { return CalculateHeight(width, MINIMUM, layouter); }

	virtual unsigned GetNominalGridWidth() { return CalculateNominalSize(HORIZONTAL); }
	virtual unsigned GetNominalGridHeight() { return CalculateNominalSize(VERTICAL); }
	virtual unsigned GetNominalGridHeight(unsigned width, GridLayouter& layouter) { return CalculateHeight(width, NOMINAL, layouter); }

	virtual unsigned GetPreferredGridWidth() { return CalculatePreferredSize(HORIZONTAL); }
	virtual unsigned GetPreferredGridHeight() { return CalculatePreferredSize(VERTICAL); }
	virtual unsigned GetPreferredGridHeight(unsigned width, GridLayouter& layouter) { return CalculateHeight(width, PREFERRED, layouter); }

	virtual WidgetSizes::Margins GetGridMargins();

private:
	enum LineOrientation
	{
		VERTICAL,
		HORIZONTAL
	};

	enum SizeType
	{
		MINIMUM,
		NOMINAL,
		PREFERRED
	};

	unsigned CalculatePreferredSize(LineOrientation orientation);
	unsigned GetPreferredSizeOfLine(LineOrientation orientation, unsigned index);
	unsigned CalculateMinimumSize(LineOrientation orientation);
	unsigned GetMinimumSizeOfLine(LineOrientation orientation, unsigned index);
	unsigned CalculateNominalSize(LineOrientation orientation);
	unsigned GetNominalSizeOfLine(LineOrientation orientation, unsigned index);
	unsigned CalculateHeight(unsigned width, SizeType type, GridLayouter& layouter);
	unsigned GetHeightOfRow(unsigned row, SizeType type, GridLayouter& layouter);
	unsigned GetMarginAfter(LineOrientation orientation, unsigned index);
	GridCell* GetCellAt(LineOrientation orientation, unsigned index, unsigned line) const;
	GridCell* GetVisibleCellBefore(LineOrientation orientation, unsigned index, unsigned line) const;
	GridCell* GetVisibleCellAfter(LineOrientation orientation, unsigned index, unsigned line) const;
	unsigned GetLineCount(LineOrientation orientation) const { return orientation == VERTICAL ? m_grid.GetRowCount() : m_grid.GetColumnCount(); }
	static LineOrientation InvertOrientation(LineOrientation orientation) { return orientation == VERTICAL ? HORIZONTAL : VERTICAL; }

	GridCellContainer& m_grid;
};


#endif // DEFAULT_GRID_SIZER_H
