/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef GRID_CELL_CONTAINER_H
#define GRID_CELL_CONTAINER_H

class GridCell;

/**
 * A container for GridCells.
 *
 * @author Arjan van Leeuwen (arjanl)
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GridCellContainer
{
public:
	virtual ~GridCellContainer() {}

	/** @return Number of columns in this grid */
	virtual unsigned GetColumnCount() const = 0;

	/** @return Number of rows in this grid */
	virtual unsigned GetRowCount() const = 0;

	/** @param col Column of cell to get, must be less than GetColumnCount()
	 * @param row Row of cell to get, must be less than GetRowCount()
	 * @return Cell in the span that contains the given coordinates (col, row)
	 */
	virtual GridCell* GetCell(unsigned col, unsigned row) = 0;

	/** @return cell on (col, row). Returns NULL if there is no cell starting
	  * at (col, row) (ie. when part of a colspan/rowspan)
	  */
	GridCell* GetStrictCell(unsigned col, unsigned row);
};

#endif // GRID_CELL_CONTAINER_H
