/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/GridLayouter.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick_toolkit/widgets/WidgetSizes.h"


GridLayouter::GridLayouter()
	: m_calculated_columns(FALSE)
	, m_calculated_rows(FALSE)
	, m_column_info(0)
	, m_row_info(0)
	, m_col_count(0)
	, m_row_count(0)
	, m_minimum_width(0)
	, m_minimum_height(0)
{
}


GridLayouter::~GridLayouter()
{
	OP_DELETEA(m_column_info);
	OP_DELETEA(m_row_info);
}


OP_STATUS GridLayouter::SetColumnCount(unsigned col_count)
{
	m_col_count = col_count;
	return InitializeSizeInfo(m_column_info, m_col_count);

}


OP_STATUS GridLayouter::SetRowCount(unsigned row_count)
{
	m_row_count = row_count;
	return InitializeSizeInfo(m_row_info, m_row_count);
}


/** Initializes a SizeInfo array
  * @param sizeinfo_array Array to initialize
  * @param sizeinfo_count Number of elements to create in array
  */
OP_STATUS GridLayouter::InitializeSizeInfo(SizeInfo*& sizeinfo_array, unsigned& sizeinfo_count)
{
	OP_DELETEA(sizeinfo_array);
	sizeinfo_array = OP_NEWA(SizeInfo, sizeinfo_count);

	if (!sizeinfo_array)
	{
		sizeinfo_count = 0;
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}


void GridLayouter::SetColumnSizes(unsigned col, unsigned minimum_size, unsigned preferred_size, unsigned margin)
{
	OP_ASSERT(col < m_col_count);

	m_minimum_width += minimum_size + margin;
	SetSizes(m_column_info, col, minimum_size, preferred_size, margin);

	m_calculated_columns = FALSE;
}


void GridLayouter::SetRowSizes(unsigned row, unsigned minimum_size, unsigned preferred_size, unsigned margin)
{
	OP_ASSERT(row < m_row_count);

	m_minimum_height += minimum_size + margin;
	SetSizes(m_row_info, row, minimum_size, preferred_size, margin);

	m_calculated_rows = FALSE;
}


/** Sets sizes in an element of a SizeInfo array to specified values
  * @param sizes Array to use
  * @param index Element to change
  * @param ... Sizes
  */
void GridLayouter::SetSizes(SizeInfo* sizes, unsigned index, unsigned minimum_size, unsigned preferred_size, unsigned margin)
{
	sizes[index].minimum_size	= minimum_size;
	sizes[index].preferred_size = max(minimum_size, preferred_size);
	sizes[index].dynamic_size	= sizes[index].preferred_size - minimum_size;
	sizes[index].size			= minimum_size;
	sizes[index].margin			= margin;
}


unsigned GridLayouter::GetColumnWidth(unsigned col)
{
	if (col >= m_col_count)
		return 0;

	CalculateColumns();

	return m_column_info[col].size;
}


OpRect GridLayouter::GetLayoutRectForCell(unsigned col, unsigned row, unsigned colspan, unsigned rowspan)
{
	if (col + colspan > m_col_count || row + rowspan > m_row_count)
		return OpRect();

	CalculateColumns();
	CalculateRows();

	OpRect cell_rect;

	for (unsigned i = 0; i < colspan; i++)
	{
		cell_rect.width += m_column_info[col + i].size;
		if (i > 0)
			cell_rect.width += m_column_info[col + i - 1].margin;
	}

	for (unsigned i = 0; i < rowspan; i++)
	{
		cell_rect.height += m_row_info[row + i].size;
		if (i > 0)
			cell_rect.height += m_row_info[row + i - 1].margin;
	}
	
	cell_rect.x		 = m_column_info[col].position;
	cell_rect.y		 = m_row_info[row].position;
	
	// Why 2*m_grid_rect.x? Because the cells of a generic grid are not children of the grid(i.e via SetParentOpWidget etc)
	// Therefore, column positions are not calculated relative to the grid, a start_pos is given(look CalculatePositions
	// and CalculateSizes, where it is called). Hence, to even out the start_pos effect and actually give an offset, m_grid_rect.x
	// is added twice.
	if (UiDirection::Get() == UiDirection::RTL)
		cell_rect.x = m_grid_rect.width - m_column_info[col].position - cell_rect.width + 2*m_grid_rect.x ;
	
	return cell_rect;
}


void GridLayouter::CalculateColumns()
{
	if (m_calculated_columns)
		return;

	unsigned column_fillroom = max(0, m_grid_rect.width - (int)m_minimum_width);
	GrowIfNecessary(m_column_info, m_col_count, column_fillroom);

	unsigned column_surplus = max(0, (int)m_minimum_width - max(0, m_grid_rect.width));
	ShrinkIfNecessary(m_column_info, m_col_count, m_minimum_width, column_surplus);

	CalculatePositions(m_column_info, m_col_count, m_grid_rect.x);

	m_calculated_columns = TRUE;
}


void GridLayouter::CalculateRows()
{
	if (m_calculated_rows)
		return;

	unsigned row_fillroom = max(0, m_grid_rect.height - (int)m_minimum_height);
	GrowIfNecessary(m_row_info, m_row_count, row_fillroom);

	unsigned row_surplus = max(0, (int)m_minimum_height - m_grid_rect.height);
	ShrinkIfNecessary(m_row_info, m_row_count, m_minimum_height, row_surplus);

	CalculatePositions(m_row_info, m_row_count, m_grid_rect.y);

	m_calculated_rows = TRUE;
}


/** Grows size in SizeInfo array if there is fill room
  * @param sizes Array of sizes to change
  * @param sizes_count Number of elements in sizes
  * @param fillroom How much room there is to fill
  */
void GridLayouter::GrowIfNecessary(SizeInfo* sizes, unsigned sizes_count, unsigned fillroom)
{
	if (fillroom == 0)
		return;

	bool grow_fills = false;
	unsigned fill_count = 0;

	for (unsigned i = 0; i < sizes_count; i++)
	{
		if (sizes[i].preferred_size == WidgetSizes::Fill)
			fill_count++;
	}

	while (fillroom > 0)
	{
		// Find columns/rows that have potential to grow
		unsigned dynamic_count = 0;

		for (unsigned i = 0; i < sizes_count; i++)
		{
			if (sizes[i].preferred_size != WidgetSizes::Fill && sizes[i].dynamic_size > 0)
				dynamic_count++;
		}

		// If there is no more growing potential, return
		if (dynamic_count == 0)
		{
			if (fill_count > 0)
				grow_fills = true;
			else
				return;
		}

		// Determine how much a column/row can grow. If possible growth per
		// each is smaller than 1 pixel, make it 1 pixel(smallest discrete
		// possible size)
		unsigned fillroom_per_column = fillroom / (grow_fills ? fill_count : dynamic_count);
		if (fillroom_per_column == 0)
			break;

		for (unsigned i = 0; i < sizes_count && fillroom > 0; i++)
		{
			if ((grow_fills || sizes[i].preferred_size != WidgetSizes::Fill) && sizes[i].dynamic_size > 0)
			{
				unsigned grow = min(fillroom_per_column, sizes[i].dynamic_size);
				sizes[i].size += grow;
				sizes[i].dynamic_size -= grow;
				fillroom -= grow;
			}
		}

		if (fill_count > 0)
			grow_fills = true;
	}
}


/** Shrinks size in sizes array where necessary if there is a surplus
  * @param sizes Array of sizes to change
  * @param sizes_count Number of elements in sizes
  * @param minimum_size Total minimum size
  * @param surplus How much size should be reduced
  */
void GridLayouter::ShrinkIfNecessary(SizeInfo* sizes, unsigned sizes_count, unsigned minimum_size, unsigned surplus)
{
	if (surplus == 0)
		return;

	unsigned shrinkage = 0;

	// Resize proportionally to minimum size
	for (unsigned i = 0; i < sizes_count; i++)
	{
		unsigned shrink = (surplus * sizes[i].minimum_size) / minimum_size;
		shrinkage += shrink;

		sizes[i].size -= shrink;
	}

	surplus -= shrinkage;

	// Divide the remainder
	while (surplus > 0)
	{
		unsigned remainder = surplus;
		for (unsigned i = 0; i < sizes_count && remainder > 0; i++)
		{
			if (sizes[i].size > 0)
			{
				sizes[i].size--;
				remainder--;
			}
		}

		if (surplus == remainder)
			break;
		surplus = remainder;
	}
}


/** Calculates 'position' property of elements in SizeInfo array using size
  * @param sizes Sizes array to change
  * @param sizes_count Number of elements in sizes
  * @param start_pos Position of the first element
  */
void GridLayouter::CalculatePositions(SizeInfo* sizes, unsigned sizes_count, int start_pos)
{
	for (unsigned i = 0; i < sizes_count; i++)
	{
		sizes[i].position = start_pos;
		start_pos += sizes[i].size + sizes[i].margin;
	}
}
