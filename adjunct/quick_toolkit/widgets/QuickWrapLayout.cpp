/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 *
 * \test "quick_toolkit.quickwraplayout"
 *
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickWrapLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridLayouter.h"

QuickWrapLayout::QuickWrapLayout()
	: m_valid_defaults(false)
	, m_horizontal_margins(0)
	, m_vertical_margins(0)
	, m_max_visible_lines(0)
	, m_hidden_items(false)
{
}

OP_STATUS QuickWrapLayout::Layout(const OpRect& rect)
{
	if (!m_valid_defaults)
		CalculateMargins();

	OpRect row_rect(rect.x, rect.y, 0, 0);
	unsigned start_of_row = 0;
	unsigned count_in_row = 0;
	// This will overflow at first decrement if max is 0, which is what we want,
	// since max == 0 means "no limit".
	unsigned available_rows = m_max_visible_lines;
	m_hidden_items = false;
	
	for (unsigned i = 0; i < GetWidgetCount(); ++i)
	{
		QuickWidget* widget = GetWidget(i);
		if (!widget->IsVisible())
			continue;
		widget->SetParentOpWidget(GetParentOpWidget());

		unsigned widget_width = widget->GetNominalWidth();

		if (row_rect.width > 0)
		{
			if ((unsigned)row_rect.width + m_horizontal_margins + widget_width > (unsigned)rect.width)
			{
				// Wrap!

				// Layout the row that was filled.
				row_rect.width = rect.width;
				row_rect.height = GetRowHeight(NOMINAL, start_of_row, count_in_row, row_rect.width);
				RETURN_IF_ERROR(LayoutRow(row_rect, start_of_row, count_in_row));

				if (--available_rows == 0)
				{
					m_hidden_items = true;
					for (; i < GetWidgetCount(); ++i)
						GetWidget(i)->SetParentOpWidget(NULL);
					return OpStatus::OK;
				}

				row_rect.y += m_vertical_margins + row_rect.height;
				row_rect.width = widget_width;

				start_of_row = i;
				count_in_row = 1;
				continue;
			}
			row_rect.width += m_horizontal_margins;
		}
		row_rect.width += widget_width;
		++count_in_row;
	}

	row_rect.width = rect.width;
	row_rect.height = GetRowHeight(NOMINAL, start_of_row, count_in_row, row_rect.width);
	return LayoutRow(row_rect, start_of_row, count_in_row);
}

OP_STATUS QuickWrapLayout::LayoutRow(const OpRect& row_rect, unsigned start, unsigned count)
{
	// Configure the layouter -- set the extreme column sizes.
	GridLayouter layouter;
	layouter.SetColumnCount(count);
	layouter.SetRowCount(1);
	layouter.SetGridLayoutRect(row_rect);
	layouter.SetRowSizes(0, GetRowHeight(MINIMUM, start, count, row_rect.width), row_rect.height, 0);
	for (unsigned i = 0, invisible_count = 0; i - invisible_count < count; ++i)
	{
		QuickWidget* widget = GetWidget(start + i);
		if (!widget->IsVisible())
		{
			++invisible_count;
			continue;
		}

		unsigned col = i - invisible_count;
		layouter.SetColumnSizes(col, widget->GetMinimumWidth(), widget->GetPreferredWidth(), col < count - 1 ? m_horizontal_margins : 0);
	}

	// Get the resulting cell rects from the layouter.  Use them to layout
	// each widget.
	for (unsigned i = 0, invisible_count = 0; i - invisible_count < count; ++i)
	{
		QuickWidget* widget = GetWidget(start + i);
		if (!widget->IsVisible())
		{
			++invisible_count;
			continue;
		}

		unsigned col = i - invisible_count;
		const OpRect cell_rect = layouter.GetLayoutRectForCell(col, 0, 1, 1);

		// Each widget gets no more than its preferred width, but all widgets
		// in a row are in cells of same height.
		OpRect widget_rect = cell_rect;
		widget_rect.height = min((unsigned)cell_rect.height, widget->GetPreferredHeight(cell_rect.width));
		widget_rect.y += (cell_rect.height - widget_rect.height) / 2;

		RETURN_IF_ERROR(widget->Layout(widget_rect));
	}

	return OpStatus::OK;
}

unsigned QuickWrapLayout::GetDefaultHeight(SizeType type, unsigned width)
{
	if (!m_valid_defaults)
		CalculateDefaults();

	if (width == WidgetSizes::UseDefault || width == m_default_sizes.width[type])
		return m_default_sizes.height[type];

	unsigned height = 0;
	unsigned row_width = 0;
	unsigned start_of_row = 0;
	unsigned count_in_row = 0;
	// This will overflow at first decrement if max is 0, which is what we want,
	// since max == 0 means "no limit".
	unsigned available_rows = m_max_visible_lines;

	for (unsigned i = 0; i < GetWidgetCount(); ++i)
	{
		QuickWidget* widget = GetWidget(i);
		if (!widget->IsVisible())
			continue;

		unsigned widget_width = widget->GetNominalWidth();

		if (row_width > 0)
		{
			if (row_width + m_horizontal_margins + widget_width > width)
			{
				// Wrap!

				// Get the height of the row that was filled.
				height += height > 0 ? m_vertical_margins : 0;
				height += GetRowHeight(type, start_of_row, count_in_row, width);

				if (--available_rows == 0)
					return height;

				row_width = widget_width;
				start_of_row = i;
				count_in_row = 1;
				continue;
			}
			row_width += m_horizontal_margins;
		}
		row_width += widget_width;
		++count_in_row;
	}

	height += height > 0 ? m_vertical_margins : 0;
	height += GetRowHeight(type, start_of_row, count_in_row, width);
	return height;
}

unsigned QuickWrapLayout::GetRowHeight(SizeType type, unsigned start, unsigned count, unsigned width)
{
	// Configure the layouter -- set the extreme column sizes.
	GridLayouter layouter;
	layouter.SetColumnCount(count);
	layouter.SetRowCount(1);
	layouter.SetGridLayoutWidth(width);
	for (unsigned i = 0, invisible_count = 0; i - invisible_count < count; ++i)
	{
		QuickWidget* widget = GetWidget(start + i);
		if (!widget->IsVisible())
		{
			++invisible_count;
			continue;
		}

		unsigned col = i - invisible_count;
		layouter.SetColumnSizes(col, widget->GetMinimumWidth(), widget->GetPreferredWidth(), col < count - 1 ? m_horizontal_margins : 0);
	}

	// Get the resulting column widths from the layouter.  Use them to
	// calculate the height of each widget.
	unsigned row_height = 0;
	for (unsigned i = 0, invisible_count = 0; i - invisible_count < count; ++i)
	{
		QuickWidget* widget = GetWidget(start + i);
		if (!widget->IsVisible())
		{
			++invisible_count;
			continue;
		}

		unsigned col = i - invisible_count;
		row_height = max(row_height, GetWidgetHeight(widget, type, layouter.GetColumnWidth(col)));
	}

	return row_height;
}

void QuickWrapLayout::CalculateDefaults()
{
	CalculateMargins();

	op_memset(&m_default_sizes, 0, sizeof(m_default_sizes));

	for (unsigned i = 0; i < GetWidgetCount(); ++i)
	{
		QuickWidget* widget = GetWidget(i);
		if (!widget->IsVisible())
			continue;

		unsigned widget_width = widget->GetMinimumWidth();
		m_default_sizes.width[MINIMUM] = max(m_default_sizes.width[MINIMUM], widget_width);
		m_default_sizes.height[MINIMUM] = max(m_default_sizes.height[MINIMUM], widget->GetMinimumHeight(widget_width));

		widget_width = widget->GetNominalWidth();
		m_default_sizes.width[NOMINAL] += widget_width + (i > 0 ? m_horizontal_margins : 0);
		m_default_sizes.height[NOMINAL] = max(m_default_sizes.height[NOMINAL], widget->GetNominalHeight(widget_width));

		widget_width = widget->GetPreferredWidth();
		if (m_default_sizes.width[PREFERRED] < WidgetSizes::UseDefault && widget_width < WidgetSizes::UseDefault)
			m_default_sizes.width[PREFERRED] += widget_width + (i > 0 ? m_horizontal_margins : 0);
		else
			m_default_sizes.width[PREFERRED] = max(m_default_sizes.width[PREFERRED], widget_width);
		m_default_sizes.height[PREFERRED] = max(m_default_sizes.height[PREFERRED], widget->GetPreferredHeight(widget_width));
	}

	m_valid_defaults = true;
}

void QuickWrapLayout::CalculateMargins()
{
	m_horizontal_margins = 0;
	m_vertical_margins = 0;

	for (unsigned i = 0; i < GetWidgetCount(); ++i)
		if (GetWidget(i)->IsVisible())
		{
			WidgetSizes::Margins margins = GetWidget(i)->GetMargins();
			m_horizontal_margins = max(m_horizontal_margins, max(margins.left, margins.right));
			m_vertical_margins = max(m_vertical_margins, max(margins.top, margins.bottom));
		}
}

unsigned QuickWrapLayout::GetWidgetHeight(QuickWidget* widget, SizeType type, unsigned width)
{
	switch (type)
	{
		case MINIMUM: return widget->GetMinimumHeight(width);
		case NOMINAL: return widget->GetNominalHeight(width);
		case PREFERRED: return widget->GetPreferredHeight(width);
	}
	return 0;
}

void QuickWrapLayout::GetDefaultMargins(WidgetSizes::Margins& margins)
{
	if (!m_valid_defaults)
		CalculateDefaults();
	margins.left = margins.right = m_horizontal_margins;
	margins.top = margins.bottom = m_vertical_margins;
}

void QuickWrapLayout::OnContentsChanged()
{
	m_valid_defaults	 = false;
	m_horizontal_margins = 0;
	m_vertical_margins	 = 0;
	m_hidden_items		 = false;
	BroadcastContentsChanged();
}

void QuickWrapLayout::SetMaxVisibleLines(unsigned max_lines, bool broadcast_contents_changed) 
{
	m_max_visible_lines = max_lines;

	if (!broadcast_contents_changed)
		return;

	OnContentsChanged(); 
	
	if (max_lines != 0) 
		m_hidden_items = true; 
}
