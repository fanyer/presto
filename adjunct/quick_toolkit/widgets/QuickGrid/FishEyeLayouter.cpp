/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author spoon
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/FishEyeLayouter.h"

void FishEyeLayouter::CalculateFishSizes()
{
	if (m_calculated_rows && m_calculated_columns)
		return;

	// check if fisheye is required
	int i;
	int grid_width = m_grid_rect.width;
	for (i = 0; i < (int)m_col_count-1; ++i)
		grid_width -= m_column_info[i].margin;

	int total_width = 0;
	for (i = 0; i < (int)m_col_count; ++i)
		total_width += this->m_column_info[i].preferred_size;
	// If all items fit with their requested size, just do a normal layout
	if (total_width <= grid_width)
	{
		int curpos = 0;
		for (i = 0; i < (int)m_col_count; ++i)
		{
			m_column_info[i].size = m_column_info[i].preferred_size;
			m_column_info[i].position = curpos;
			curpos += m_column_info[i].size+m_column_info[i].margin;
		}
		m_calculated_columns = m_calculated_rows = TRUE;
		return;
	}

	// Count how many fisheye slots are in use
	// The selected has prefered width, the ones next to it common width + 2/3 of prefered width - common width and the one next to that 1/3...
	// The slots is defined as number of 1/3
	int fish_slots = 0;
	for (i = max(m_selected-2, 0); i <= m_selected+2 && i < (int)m_col_count; ++i)
	{
		fish_slots += 3-abs(i-m_selected);
	}
	// Try to keep the selected tab it's prefered size.
	// The total width of all fisheye tabs are 3*prefered width
	// Leave some space for other tabs by making sure one tab is less than 25% of the space
	// gives a maximum coverage of 1/4*3 = 75%
	int fish_base_width = m_column_info[m_selected].preferred_size;
	if (fish_base_width > grid_width/4)
		fish_base_width = grid_width/4;
	// The base width which is used on non fisheye tabs is fish_slots/3 * (prefered_width-base_width) + base_width * num_items = total_width
	// or, base_width = (total_width-(prefered_width*fish_slots)/3) / (num_items-fish_slots/3)
	int base_width = 3*grid_width-(fish_base_width*fish_slots);
	base_width = base_width / (3*m_col_count-fish_slots);

	// The width of the second is base_width+(prefered-base)*2/3 to make sure they are not smaller than the non-fisheye tabs
	fish_base_width -= base_width;
	int fish_widths[3];
	fish_widths[0] = base_width+fish_base_width;
	fish_widths[1] = base_width+(fish_base_width*2)/3;
	fish_widths[2] = base_width+fish_base_width/3;

	// Spread out the unused amount on the slots
	int unused_width = grid_width-m_col_count*base_width;
	for (i = max(m_selected-2, 0); i <= m_selected+2 && i < (int)m_col_count; ++i)
	{
		unused_width -= fish_widths[op_abs(i-m_selected)]-base_width;
	}
	int fp_increase = (unused_width<<16)/m_col_count;
	int fp_fracpoint = 0;

	int curpos = 0;
	for (i = 0; i < (int)m_col_count; ++i)
	{
		fp_fracpoint += fp_increase;
		if (i >= m_selected-2 && i <= m_selected+2)
			m_column_info[i].size = fish_widths[op_abs(i-m_selected)];
		else
			m_column_info[i].size = base_width;
		m_column_info[i].size += fp_fracpoint>>16;
		fp_fracpoint -= fp_fracpoint&0xffff0000;
		m_column_info[i].position = curpos;
		curpos += m_column_info[i].size+m_column_info[i].margin;
	}
	m_calculated_columns = m_calculated_rows = TRUE;
}

OpRect FishEyeLayouter::GetLayoutRectForCell(unsigned col, unsigned row, unsigned colspan, unsigned rowspan)
{
	OP_ASSERT(row == 0 && colspan == 1 && rowspan == 1);

	CalculateFishSizes();

	return OpRect(m_column_info[col].position, 0, m_column_info[col].size, m_row_info[row].preferred_size);
}

