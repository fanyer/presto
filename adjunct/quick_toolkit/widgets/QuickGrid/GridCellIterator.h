/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef GRID_CELL_ITERATOR_H
#define GRID_CELL_ITERATOR_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCellContainer.h"

/** @brief Helper for iterating over all cells in a grid
  * Usage:
  *   for (GridCellIterator it(grid); it; ++it)
  *   {
  *     // Do things with it.Get()
  *   }
  */
class GridCellIterator
{
public:
	/** Constructor
	  * @param grid Grid to iterate over
	  */
	explicit GridCellIterator(GridCellContainer& grid) : m_grid(grid), m_col(0), m_row(0), m_current(InitialCell()) {}

	/** @return Whether iterator is pointing to valid cell
	  */
	bool IsValid() { return m_current != 0; }
	operator bool() { return IsValid(); }

	/** @return Widget in cell the iterator is pointing to
	  */
	QuickWidget* Get() { return m_current->GetContent(); }
	QuickWidget* operator->() { return Get(); }

	/** Increase iterator to next cell
	  */
	void Next();
	GridCellIterator& operator++() { Next(); return *this; }

private:
	GridCell* InitialCell() { return m_row < m_grid.GetRowCount() && m_col < m_grid.GetColumnCount() ? m_grid.GetCell(m_col, m_row) : 0; }

	GridCellContainer& m_grid;
	unsigned m_col;
	unsigned m_row;
	GridCell* m_current;
};


inline void GridCellIterator::Next()
{
	for (m_current = 0; !m_current; m_current = m_grid.GetStrictCell(m_col, m_row))
	{
		m_col = (m_col + 1) % m_grid.GetColumnCount();
		if (m_col == 0 && ++m_row >= m_grid.GetRowCount())
			return;
	}
}

#endif // GRID_CELL_ITERATOR_H
