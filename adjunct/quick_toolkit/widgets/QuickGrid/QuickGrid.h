/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 * @author Cihat Imamoglu(cihati)
 */

#ifndef OP_GRID_H
#define OP_GRID_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/GenericGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"
#include "adjunct/desktop_util/adt/optightvector.h"

class GridCell;
class QuickGridRowCreator;
class TypedObjectCollection;

/** @brief A layouting widget in the form of a two-dimensional grid (table) with columns and rows
  */
class QuickGrid : public GenericGrid
{
	IMPLEMENT_TYPEDOBJECT(GenericGrid);
	public:
		QuickGrid();
		~QuickGrid();

		OP_STATUS Init() { return OpStatus::OK; }

		/** Add a row to the grid. The added row will become the current row for adding widgets to
		  * Needs to be called at least once, to create an initial row
		  */
		OP_STATUS AddRow();

		/** Remove a row from the grid
		  * @param row Row to remove, must be a number smaller than GetRowCount()
		  */
		void RemoveRow(unsigned row);

		/** Add a widget to the end of the current row
		  * @param widget Widget to add (takes ownership)
		  * @param colspan If > 1, let widget span multiple columns NB: read below
		  * NB: Widgets that have a colspan do not contribute to the size of the grid.
		  *     Use with care. If possible, try to avoid them and use nested stacks/grids instead.
		  */
		OP_STATUS InsertWidget(QuickWidget* widget, int colspan = 1);

		/** Clear this grid, returning it to its initial state
		  */
		void Clear();

		/** Move row to new position.
		 * After this operation, the row that was previously at index 'from_row' is now
		 * at index 'to_row'
		 * @param from_row current position of row
		 * @param to_row desired position of row
		 */
		OP_STATUS MoveRow(unsigned from_row, unsigned to_row);

		// Implement GridCellContainer
		virtual unsigned GetColumnCount() const { return m_col_count; }
		virtual unsigned GetRowCount() const { return m_col_count ? m_cells.GetCount() / m_col_count : 0; }
		virtual GridCell* GetCell(unsigned col, unsigned row) { return m_cells[GetIndex(col, row)]; }

	private:
		// Helper functions
		unsigned GetIndex(unsigned col, unsigned row) const { return row * m_col_count + col; }
		void DeleteCells(unsigned startpos, unsigned count);
		virtual OP_STATUS OnRowAdded() { return OpStatus::OK; }
		virtual void OnRowRemoved(unsigned row) {}
		virtual void OnRowMoved(unsigned from_row, unsigned to_row) {}
		virtual void OnCleared() {}

		unsigned m_col_count;
		OpTightVector<GridCell*> m_cells;
};


#endif // OP_GRID_H
