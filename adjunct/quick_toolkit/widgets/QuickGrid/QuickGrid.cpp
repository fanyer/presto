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

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridLayouter.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"
#include "modules/widgets/OpWidget.h"


QuickGrid::QuickGrid()
  : m_col_count(0)
{
}


QuickGrid::~QuickGrid()
{
	DeleteCells(0, m_cells.GetCount());
}


OP_STATUS QuickGrid::AddRow()
{
	if (m_col_count == 0)
		return OnRowAdded();

	// Fill previous rows with zeroes if necessary to make
	// sure that we don't create an invalid grid
	while (m_cells.GetCount() % m_col_count)
		RETURN_IF_ERROR(m_cells.Add(0));

	m_col_count = 0;
	return OnRowAdded();
}


void QuickGrid::RemoveRow(unsigned row)
{
	unsigned rowstart = GetIndex(0, row);
	OP_ASSERT(rowstart < m_cells.GetCount() && rowstart + m_col_count <= m_cells.GetCount());

	DeleteCells(rowstart, m_col_count);
	m_cells.Remove(rowstart, m_col_count);
	Invalidate();

	return OnRowRemoved(row);
}

OP_STATUS QuickGrid::InsertWidget(QuickWidget* widget, int colspan)
{
	OP_ASSERT(colspan >= 1);

	OpAutoPtr<QuickWidget> widgetholder(widget);
	OpAutoPtr<GridCell> cell;

	if (colspan == 1)
		cell.reset(OP_NEW(GridCell, ()));
	else
		cell.reset(OP_NEW(SpannedGridCell, (1, colspan)));

	if (!cell.get())
		return OpStatus::ERR_NO_MEMORY;

	cell->SetContent(widgetholder.release());
	cell->SetContainer(this);
	cell->SetParentOpWidget(GetParentOpWidget());

	for (int i = 0; i < colspan; i++)
		RETURN_IF_ERROR(m_cells.Add(cell.get()));

	m_col_count += colspan;

	cell.release();

	Invalidate();
	return OpStatus::OK;
}


void QuickGrid::Clear()
{
	DeleteCells(0, m_cells.GetCount());
	m_cells.Clear();
	m_col_count = 0;
	Invalidate();

	OnCleared();
}


void QuickGrid::DeleteCells(unsigned startpos, unsigned count)
{
	unsigned endpos = startpos + count;
	for (unsigned i = startpos; i < endpos; i++)
	{
		while (i + 1 < endpos && m_cells[i + 1] == m_cells[i])
			i++;
		OP_DELETE(m_cells[i]);
	}
}


OP_STATUS QuickGrid::MoveRow(unsigned from_row, unsigned to_row)
{
	if (from_row != to_row)
	{
		RETURN_IF_ERROR(m_cells.Move(GetIndex(0, from_row), GetIndex(0, to_row), m_col_count));
		Invalidate();

		OnRowMoved(from_row, to_row);
	}

	return OpStatus::OK;
}
