/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef GRID_LAYOUTER_H
#define GRID_LAYOUTER_H

/** @brief A helper class for calculating the bounding rectangles of cells in a grid
  */
class GridLayouter
{
public:
	GridLayouter();
	virtual ~GridLayouter();

	/** @param col_count Amount of columns that should be layed out
	  */
	OP_STATUS SetColumnCount(unsigned col_count);

	/** @param row_count Amount of rows that should be layed out
	  */
	OP_STATUS SetRowCount(unsigned row_count);

	/** Set various size parameters for a column - should only be called once pr column
	  * @param col Column to set - value must be smaller than previously set column count
	  * @param minimum_size - See QuickWidget
	  * @param preferred_size - See QuickWidget
	  * @note If the preferred_size given is smaller than the minimum_size, it will be
	  *       changed to minimum_size
	  * @param margin Margin to the right of this column
	  */
	void SetColumnSizes(unsigned col, unsigned minimum_size, unsigned preferred_size, unsigned margin);

	/** Set various size parameters for a row - should only be called once pr row
	  * @param row Row to set - value must be smaller than previously set row count
	  * @param minimum_size - See QuickWidget
	  * @param preferred_size - See QuickWidget
	  * @note If the preferred_size given is smaller than the minimum_size, it will be
	  *       changed to minimum_size
	  * @param margin Margin to the bottom of this row
	  */
	void SetRowSizes(unsigned row, unsigned minimum_size, unsigned preferred_size, unsigned margin);

	/** @param layout_rect rectangle that specifies limits in which grid should be layed out
	  */
	void SetGridLayoutRect(const OpRect& layout_rect) { m_grid_rect = layout_rect; }

	/** Set the width of the layout (shorthand for SetGridLayoutRect with only a width)
	  * @param width Width of grid layout
	  */
	void SetGridLayoutWidth(unsigned width) { m_grid_rect.width = width; }

	/** Get the width of a specific column
	  */
	virtual unsigned GetColumnWidth(unsigned col);

	/** Get the bounding rectangle for a specific cell
	  * @param col Column of cell
	  * @param row Row of cell
	  * @param colspan Number of columns this cell should span
	  * @param rowspan Number of rows this cell should span
	  */
	virtual OpRect GetLayoutRectForCell(unsigned col, unsigned row, unsigned colspan, unsigned rowspan);

protected:
	struct SizeInfo
	{
		unsigned minimum_size;
		unsigned preferred_size;
		unsigned dynamic_size;	// This is the difference between preferred_size and minimum_size
		unsigned size;
		unsigned margin;
		unsigned position;

		SizeInfo()
			: minimum_size(0)
			, preferred_size(0)
			, dynamic_size(0)
			, size(0)
			, margin(0)
			, position(0) {}
	};

	OP_STATUS InitializeSizeInfo(SizeInfo*& sizeinfo_array, unsigned& sizeinfo_count);
	void	  SetSizes(SizeInfo* sizes, unsigned index, unsigned minimum_size, unsigned preferred_size, unsigned margin);
	void	  CalculateColumns();
	void	  CalculateRows();

	void	  GrowIfNecessary(SizeInfo* sizes, unsigned sizes_count, unsigned fillroom);
	void	  ShrinkIfNecessary(SizeInfo* sizes, unsigned sizes_count, unsigned minimum_size, unsigned surplus);
	void	  CalculatePositions(SizeInfo* sizes, unsigned sizes_count, int start_pos);

	BOOL	  m_calculated_columns;
	BOOL	  m_calculated_rows;
	SizeInfo* m_column_info;
	SizeInfo* m_row_info;
	unsigned  m_col_count;
	unsigned  m_row_count;
	unsigned  m_minimum_width;
	unsigned  m_minimum_height;
	OpRect    m_grid_rect;
};

#endif // GRID_LAYOUTER_H
