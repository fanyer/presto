/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef GRID_SIZER_H
#define GRID_SIZER_H

#include "adjunct/quick_toolkit/widgets/WidgetSizes.h"

class GridLayouter;

/** A strategy for computing the various grid sizes.
  *
  * Used by GenericGrid.
  *
  * @author Wojciech Dzierzanowski (wdzierzanowski)
  */
class GridSizer
{
public:
	virtual ~GridSizer() {}

	/** Set individual column sizes in a layouter.
	  *
	  * @note If you're going to use a layouter with any of the getter
	  * functions below that require a layouter, you must call this function,
	  * and/or SetRowSizes(), with the layouter first to initialize it.
	  *
	  * @param layouter the layouter
	  * @see SetRowSizes()
	  */
	virtual OP_STATUS SetColumnSizes(GridLayouter& layouter) = 0;

	/** Set individual row sizes in a layouter.
	  *
	  * @note If you're going to use a layouter with any of the getter
	  * functions below that require a layouter, you must call this function,
	  * and/or SetColumnSizes(), with the layouter first to initialize it.
	  *
	  * @param layouter the layouter
	  * @see SetColumnSizes()
	  */
	virtual OP_STATUS SetRowSizes(GridLayouter& layouter) = 0;

	/** Tell the sizer the contents of the grid have changed.
	  */
	virtual void Invalidate() = 0;

	/** @return the minimum width of the grid, see QuickWidget
	  */
	virtual unsigned GetMinimumGridWidth() = 0;

	/** @return the minimum height of the grid, see QuickWidget
	  */
	virtual unsigned GetMinimumGridHeight() = 0;

	/** Like GetMinimumGridHeight(), but the grid height is known to depend on
	  * the width.
	  *
	  * @param width the width of the grid to get the height for
	  * @param layouter to be used to get the widths of the individual columns
	  * @return the minimum height of the grid, see QuickWidget
	  */
	virtual unsigned GetMinimumGridHeight(unsigned width, GridLayouter& layouter) = 0;

	/** @return the nominal width of the grid, see QuickWidget
	  */
	virtual unsigned GetNominalGridWidth() = 0;

	/** @return the nominal height of the grid, see QuickWidget
	  */
	virtual unsigned GetNominalGridHeight() = 0;

	/** Like GetNominalGridHeight(), but the grid height is known to depend on
	  * the width.
	  *
	  * @param width the width of the grid to get the height for
	  * @param layouter to be used to get the widths of the individual columns
	  * @return the nominal height of the grid, see QuickWidget
	  */
	virtual unsigned GetNominalGridHeight(unsigned width, GridLayouter& layouter) = 0;

	/** @return the preferred width of the grid, see QuickWidget
	  */
	virtual unsigned GetPreferredGridWidth() = 0;

	/** @return the preferred height of the grid, see QuickWidget
	  */
	virtual unsigned GetPreferredGridHeight() = 0;

	/** Like GetPreferredGridHeight(), but the grid height is known to depend on
	  * the width.
	  *
	  * @param width the width of the grid to get the height for
	  * @param layouter to be used to get the widths of the individual columns
	  * @return the preferred height of the grid, see QuickWidget
	  */
	virtual unsigned GetPreferredGridHeight(unsigned width, GridLayouter& layouter) = 0;

	/** @return the margins around the grid, see QuickWidget
	  */
	virtual WidgetSizes::Margins GetGridMargins() = 0;
};

#endif // GRID_SIZER_H
