/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/UniformGridSizer.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCellContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCellIterator.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridLayouter.h"


UniformGridSizer::UniformGridSizer(GridCellContainer& grid)
	: m_grid(grid)
	, m_valid(false)
{
}

void UniformGridSizer::CalculateCellSizes()
{
	op_memset(m_cell_width, 0, sizeof(m_cell_width));
	op_memset(m_cell_height, 0, sizeof(m_cell_height));
	m_cell_horizontal_margins = 0;
	m_cell_vertical_margins = 0;

	for (GridCellIterator it(m_grid); it; ++it)
	{
		m_cell_width[MINIMUM] = max(m_cell_width[MINIMUM], it->GetMinimumWidth());
		m_cell_width[NOMINAL] = max(m_cell_width[NOMINAL], it->GetNominalWidth());
		m_cell_width[PREFERRED] = max(m_cell_width[PREFERRED], it->GetPreferredWidth());
		m_cell_height[MINIMUM] = max(m_cell_height[MINIMUM], it->GetMinimumHeight(m_cell_width[MINIMUM]));
		m_cell_height[NOMINAL] = max(m_cell_height[NOMINAL], it->GetNominalHeight(m_cell_width[NOMINAL]));
		m_cell_height[PREFERRED] = max(m_cell_height[PREFERRED], it->GetPreferredHeight(m_cell_width[NOMINAL]));

		const WidgetSizes::Margins margins = it->GetMargins();
		m_cell_horizontal_margins = max(max(m_cell_horizontal_margins, margins.left), margins.right);
		m_cell_vertical_margins = max(max(m_cell_vertical_margins, margins.top), margins.bottom);
	}

	m_valid = true;
}

unsigned UniformGridSizer::GetGridWidth(SizeType type)
{
	if (!m_valid)
		CalculateCellSizes();

	if (m_cell_width[type] > WidgetSizes::UseDefault)
		return m_cell_width[type];

	unsigned width = m_cell_width[type] * m_grid.GetColumnCount();
	width += m_cell_horizontal_margins * max(0, int(m_grid.GetColumnCount()) - 1);
	return width;
}

unsigned UniformGridSizer::GetGridHeight(SizeType type)
{
	if (!m_valid)
		CalculateCellSizes();

	return CalculateGridHeight(m_cell_height[type]);
}

unsigned UniformGridSizer::GetGridHeight(SizeType type, unsigned grid_width, GridLayouter& layouter)
{
	if (!m_valid)
		CalculateCellSizes();

	layouter.SetGridLayoutWidth(grid_width);
	return CalculateGridHeight(CalculateCellHeight(type, layouter));
}

unsigned UniformGridSizer::CalculateGridHeight(unsigned cell_height)
{
	OP_ASSERT(m_valid);

	if (cell_height > WidgetSizes::UseDefault)
		return cell_height;

	unsigned height = cell_height * m_grid.GetRowCount();
	height += m_cell_vertical_margins * max(0, int(m_grid.GetRowCount()) - 1);
	return height;
}

unsigned UniformGridSizer::CalculateCellHeight(SizeType type, GridLayouter& layouter)
{
	const unsigned row_count = m_grid.GetRowCount();
	unsigned max_cell_height = 0;

	for (unsigned col = 0; col < m_grid.GetColumnCount(); ++col)
	{
		const unsigned col_width = layouter.GetColumnWidth(col);

		for (unsigned row = 0; row < row_count; ++row)
		{
			GridCell* cell = m_grid.GetCell(col, row);
			unsigned cell_height = 0;
			switch (type)
			{
				case MINIMUM: cell_height = cell->GetMinimumHeight(col_width); break;
				case NOMINAL: cell_height = cell->GetNominalHeight(col_width); break;
				case PREFERRED: cell_height = cell->GetPreferredHeight(col_width); break;
			}
			max_cell_height = max(max_cell_height, cell_height);
		}
	}

	return max_cell_height;
}

WidgetSizes::Margins UniformGridSizer::GetGridMargins()
{
	if (!m_valid)
		CalculateCellSizes();

	WidgetSizes::Margins margins;
	margins.left = margins.right = m_cell_horizontal_margins;
	margins.top = margins.bottom = m_cell_vertical_margins;
	return margins;
}

OP_STATUS UniformGridSizer::SetColumnSizes(GridLayouter& layouter)
{
	if (!m_valid)
		CalculateCellSizes();

	RETURN_IF_ERROR(layouter.SetColumnCount(m_grid.GetColumnCount()));
	for (unsigned col = 0; col < m_grid.GetColumnCount(); ++col)
	{
		layouter.SetColumnSizes(col,
			m_cell_width[MINIMUM], m_cell_width[PREFERRED],
			col == m_grid.GetColumnCount() - 1 ? 0 : m_cell_horizontal_margins);
	}

	return OpStatus::OK;
}

OP_STATUS UniformGridSizer::SetRowSizes(GridLayouter& layouter)
{
	if (!m_valid)
		CalculateCellSizes();

	RETURN_IF_ERROR(layouter.SetRowCount(m_grid.GetRowCount()));
	for (unsigned row = 0; row < m_grid.GetRowCount(); ++row)
	{
		layouter.SetRowSizes(row,
			CalculateCellHeight(MINIMUM, layouter), CalculateCellHeight(PREFERRED, layouter),
			row == m_grid.GetRowCount() - 1 ? 0 : m_cell_vertical_margins);
	}

	return OpStatus::OK;
}
