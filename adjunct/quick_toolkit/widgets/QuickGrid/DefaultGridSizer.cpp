/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/DefaultGridSizer.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCellIterator.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridLayouter.h"


DefaultGridSizer::DefaultGridSizer(GridCellContainer& grid)
	: m_grid(grid)
{
}

unsigned DefaultGridSizer::CalculateMinimumSize(LineOrientation orientation)
{
	unsigned min_size = 0;
	unsigned count = GetLineCount(orientation);
	LineOrientation line = InvertOrientation(orientation);

	for (unsigned elem = 0; elem < count; elem++)
		min_size += GetMinimumSizeOfLine(line, elem) + GetMarginAfter(line, elem);

	return min_size;
}

unsigned DefaultGridSizer::GetMinimumSizeOfLine(LineOrientation orientation, unsigned index)
{
	unsigned min_size = 0;
	unsigned count = GetLineCount(orientation);

	for (unsigned elem = 0; elem < count; elem++)
	{
		unsigned cell_size;
		if (orientation == VERTICAL)
			cell_size = m_grid.GetCell(index, elem)->GetMinimumWidth();
		else
			cell_size = m_grid.GetCell(elem, index)->GetMinimumHeight();

		min_size = max(min_size, cell_size);
	}

	return min_size;
}

unsigned DefaultGridSizer::CalculateNominalSize(LineOrientation orientation)
{
	unsigned nom_size = 0;
	unsigned count = GetLineCount(orientation);
	LineOrientation line = InvertOrientation(orientation);

	for (unsigned elem = 0; elem < count; elem++)
		nom_size += GetNominalSizeOfLine(line, elem) + GetMarginAfter(line, elem);

	return nom_size;
}

unsigned DefaultGridSizer::GetNominalSizeOfLine(LineOrientation orientation, unsigned index)
{
	unsigned nom_size = 0;
	unsigned count = GetLineCount(orientation);

	for (unsigned elem = 0; elem < count; elem++)
	{
		unsigned cell_size;
		if (orientation == VERTICAL)
			cell_size = m_grid.GetCell(index, elem)->GetNominalWidth();
		else
			cell_size = m_grid.GetCell(elem, index)->GetNominalHeight();

		nom_size = max(nom_size, cell_size);
	}

	return nom_size;
}

unsigned DefaultGridSizer::CalculatePreferredSize(LineOrientation orientation)
{
	unsigned preferred_size = 0;
	unsigned count = GetLineCount(orientation);
	LineOrientation line = InvertOrientation(orientation);

	for (unsigned elem = 0; elem < count; elem++)
	{
		unsigned elem_pref = GetPreferredSizeOfLine(line, elem);
		if (elem_pref > WidgetSizes::UseDefault)
		{
			preferred_size = elem_pref;
			break;
		}
		else
		{
			preferred_size += elem_pref + GetMarginAfter(line, elem);
		}
	}

	return preferred_size;
}

unsigned DefaultGridSizer::GetPreferredSizeOfLine(LineOrientation orientation, unsigned index)
{
	unsigned preferred_size = 0;
	unsigned count = GetLineCount(orientation);
	bool fill = count > 0;

	for (unsigned elem = 0; elem < count && preferred_size != WidgetSizes::Infinity; elem++)
	{
		unsigned cell_size;
		if (orientation == VERTICAL)
			cell_size = m_grid.GetCell(index, elem)->GetPreferredWidth();
		else
			cell_size = m_grid.GetCell(elem, index)->GetPreferredHeight();

		if (cell_size != WidgetSizes::Fill)
		{
			fill = false;
			preferred_size = max(preferred_size, cell_size);
		}
	}

	return fill ? WidgetSizes::Fill : preferred_size;
}

unsigned DefaultGridSizer::CalculateHeight(unsigned width, SizeType type, GridLayouter& layouter)
{
	unsigned height = 0;
	unsigned count = m_grid.GetRowCount();
	layouter.SetGridLayoutWidth(width);

	for (unsigned row = 0; row < count; row++)
	{
		unsigned row_size = GetHeightOfRow(row, type, layouter);
		if (row_size > WidgetSizes::UseDefault)
			height = max(height, row_size);
		else
			height += row_size + GetMarginAfter(HORIZONTAL, row);
	}

	return height;
}

unsigned DefaultGridSizer::GetHeightOfRow(unsigned row, SizeType type, GridLayouter& layouter)
{
	unsigned height = 0;
	unsigned count = m_grid.GetColumnCount();
	bool fill = count > 0;

	for (unsigned col = 0; col < count && height != WidgetSizes::Infinity; col++)
	{
		unsigned cell_size = 0;

		switch (type)
		{
		case MINIMUM:
			cell_size = m_grid.GetCell(col, row)->GetMinimumHeight(layouter.GetColumnWidth(col));
			break;
		case NOMINAL:
			cell_size = m_grid.GetCell(col, row)->GetNominalHeight(layouter.GetColumnWidth(col));
			break;
		case PREFERRED:
			cell_size = m_grid.GetCell(col, row)->GetPreferredHeight(layouter.GetColumnWidth(col));
			break;
		}

		if (cell_size != WidgetSizes::Fill)
		{
			fill = false;
			height = max(height, cell_size);
		}
	}

	return fill ? WidgetSizes::Fill : height;
}

WidgetSizes::Margins DefaultGridSizer::GetGridMargins()
{
	WidgetSizes::Margins margins;

	const unsigned row_count = m_grid.GetRowCount();
	const unsigned col_count = m_grid.GetColumnCount();

	if (col_count == 0 || row_count == 0)
		return margins;

	for (unsigned row = 0; row < row_count; row++)
	{
		// Left and right margins depend on left and right margins of first
		// and last visible cell in each row.

		// "After -1" means "start at index 0".
		GridCell* first_cell = GetVisibleCellAfter(VERTICAL, unsigned(-1), row);
		if (first_cell == NULL)
			continue;

		WidgetSizes::Margins cell_margins = first_cell->GetMargins();
		margins.left = max(margins.left, cell_margins.left);

		GridCell* last_cell = GetVisibleCellBefore(VERTICAL, col_count, row);
		if (last_cell != NULL)
			cell_margins = last_cell->GetMargins();

		margins.right = max(margins.right, cell_margins.right);
	}

	for (unsigned col = 0; col < col_count; col++)
	{
		// Top and bottom margins depend on top and bottom margins of first and
		// last visible cell in each column

		// "After -1" means "start  at index 0".
		GridCell* first_cell = GetVisibleCellAfter(HORIZONTAL, unsigned(-1), col);
		if (first_cell == NULL)
			continue;

		WidgetSizes::Margins cell_margins = first_cell->GetMargins();
		margins.top = max(margins.top, cell_margins.top);

		GridCell* last_cell = GetVisibleCellBefore(HORIZONTAL, row_count, col);
		if (last_cell != NULL)
			cell_margins = last_cell->GetMargins();

		margins.bottom = max(margins.bottom, cell_margins.bottom);
	}

	return margins;
}

unsigned DefaultGridSizer::GetMarginAfter(LineOrientation orientation, unsigned index)
{
	if (index + 1 >= GetLineCount(InvertOrientation(orientation)))
		return 0;

	unsigned margin = 0;
	unsigned line_count = GetLineCount(orientation);

	for (unsigned line = 0; line < line_count; line++)
	{
		GridCell* cell_at = GetCellAt(orientation, index, line);
		if (!cell_at->GetContent()->IsVisible())
			continue;

		GridCell* cell_after = GetVisibleCellAfter(orientation, index, line);

		if (cell_at != cell_after && cell_after)
		{
			unsigned margin_between_cells = 0;
			WidgetSizes::Margins margins_at = cell_at->GetMargins();
			WidgetSizes::Margins margins_after(0);
			if (cell_after != NULL)
				 margins_after = cell_after->GetMargins();

			if (orientation == VERTICAL)
				margin_between_cells = max(margins_at.right, margins_after.left);
			else
				margin_between_cells = max(margins_at.bottom, margins_after.top);
			margin = max(margin, margin_between_cells);
		}
	}

	return margin;
}

GridCell* DefaultGridSizer::GetCellAt(LineOrientation orientation, unsigned index, unsigned line) const
{
	GridCell* cell = NULL;
	if (orientation == VERTICAL)
		cell = m_grid.GetCell(index, line);
	else
		cell = m_grid.GetCell(line, index);
	return cell;
}

GridCell* DefaultGridSizer::GetVisibleCellBefore(LineOrientation orientation, unsigned index, unsigned line) const
{
	for ( ; index > 0; --index)
	{
		GridCell* cell = GetCellAt(orientation, index - 1, line);
		if (cell->GetContent()->IsVisible())
			return cell;
	}
	return NULL;
}

GridCell* DefaultGridSizer::GetVisibleCellAfter(LineOrientation orientation, unsigned index, unsigned line) const
{
	const unsigned item_count = GetLineCount(InvertOrientation(orientation));
	for (++index; index < item_count; ++index)
	{
		GridCell* cell = GetCellAt(orientation, index, line);
		if (cell->GetContent()->IsVisible())
			return cell;
	}
	return NULL;
}

OP_STATUS DefaultGridSizer::SetColumnSizes(GridLayouter& layouter)
{
	RETURN_IF_ERROR(layouter.SetColumnCount(m_grid.GetColumnCount()));
	for (unsigned col = 0; col < m_grid.GetColumnCount(); col++)
		layouter.SetColumnSizes(col, GetMinimumSizeOfLine(VERTICAL, col), GetPreferredSizeOfLine(VERTICAL, col), GetMarginAfter(VERTICAL, col));

	return OpStatus::OK;
}

OP_STATUS DefaultGridSizer::SetRowSizes(GridLayouter& layouter)
{
	RETURN_IF_ERROR(layouter.SetRowCount(m_grid.GetRowCount()));
	for (unsigned row = 0; row < m_grid.GetRowCount(); row++)
		layouter.SetRowSizes(row, GetHeightOfRow(row, MINIMUM, layouter), GetHeightOfRow(row, PREFERRED, layouter), GetMarginAfter(HORIZONTAL, row));

	return OpStatus::OK;
}
