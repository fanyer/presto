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

#include "adjunct/quick_toolkit/widgets/QuickFlowLayout.h"


/** @brief Widget used on background. Makes it easy for testing
  * frameworks to recognize a flow-layout in the widget structure
  */
class QuickFlowLayout::BackgroundWidget : public OpWidget
{
public:
	virtual Type GetType() { return WIDGET_TYPE_FLOW_LAYOUT; }
	virtual BOOL IsOfType(Type type) { return type == WIDGET_TYPE_FLOW_LAYOUT; }
};

/** @brief Widget used as background for each cell in the layout.
  * Makes it easy for testing frameworks to recognize individual
  * cells in the widget structure
  */
class QuickFlowLayout::ItemWidget : public OpWidget
{
public:
	virtual Type GetType() { return WIDGET_TYPE_FLOW_LAYOUT_ITEM; }
	virtual BOOL IsOfType(Type type) { return type == WIDGET_TYPE_FLOW_LAYOUT_ITEM; }
};

QuickFlowLayout::QuickFlowLayout()
  : m_min_cell_width(0)
  , m_nominal_cell_width(0)
  , m_preferred_cell_width(0)
  , m_min_cell_height(0)
  , m_nominal_cell_height(0)
  , m_preferred_cell_height(0)
  , m_horizontal_margins(0)
  , m_vertical_margins(0)
  , m_min_break(0)
  , m_max_break(0)
  , m_visible_cell_count(0)
  , m_fixed_padding_width(0)
  , m_fixed_padding_height(0)
  , m_parent_op_widget(0)
  , m_background(NULL)
  , m_visible(true)
  , m_valid(true)
  , m_keep_ratio(false)
{
}

QuickFlowLayout::~QuickFlowLayout()
{
	if (m_background)
		m_background->Delete();
}

OP_STATUS QuickFlowLayout::Init(TestState teststate)
{
	if (teststate == FORCE_TESTABLE)
	{
		m_background = OP_NEW(BackgroundWidget, ());
		RETURN_OOM_IF_NULL(m_background);
	}

	return OpStatus::OK;
}

OP_STATUS QuickFlowLayout::InsertWidget(QuickWidget* widget, int pos)
{
	OpAutoPtr<QuickWidget> widget_holder(widget);

	if (pos < 0)
		pos = m_cells.GetCount();

	RETURN_IF_ERROR(m_cells.Insert(pos, widget_holder.get()));
	widget_holder.release();

	widget->SetContainer(this);
	widget->SetParentOpWidget(m_parent_op_widget);
	if (widget->IsVisible())
	{
		UpdateSizes(*widget);
		++m_visible_cell_count;
	}

	UpdateZOrder();
	BroadcastContentsChanged();

	if (m_background)
		RETURN_IF_ERROR(PrepareTestWidget(widget, pos));

	return OpStatus::OK;
}

OP_STATUS QuickFlowLayout::MoveWidget(unsigned from_pos, unsigned to_pos)
{
	if (to_pos != from_pos)
	{
		QuickWidget* qw = m_cells.Remove(from_pos);
		m_cells.Insert(to_pos, qw);
 
		UpdateZOrder();
		BroadcastContentsChanged();
	}

	if (m_background)
		RETURN_IF_ERROR(UpdateTestWidgetNames());

	return OpStatus::OK;
}

void QuickFlowLayout::RemoveWidget(unsigned pos)
{
	m_cells.Delete(pos);
	UpdateZOrder();
	Invalidate();

	if (m_background)
		RemoveTestWidget(pos);
}

void QuickFlowLayout::RemoveAllWidgets() 
{ 
	m_cells.DeleteAll(); 
	Invalidate(); 

	if (m_background)
		RemoveAllTestWidgets();
}

void QuickFlowLayout::SetMinBreak(unsigned count)
{
	if (m_min_break != count)
	{
		m_min_break = count;
		OP_ASSERT(m_max_break == 0 || m_min_break <= m_max_break);
		BroadcastContentsChanged();
	}
}

void QuickFlowLayout::SetMaxBreak(unsigned count)
{
	if (m_max_break != count)
	{
		m_max_break = count;
		OP_ASSERT(m_max_break == 0 || m_min_break <= m_max_break);
		BroadcastContentsChanged();
	}
}

void QuickFlowLayout::SetHardBreak(unsigned count)
{
	if (m_min_break != count || m_max_break != count)
	{
		m_min_break = count;
		m_max_break = count;
		BroadcastContentsChanged();
	}
}

OP_STATUS QuickFlowLayout::Layout(const OpRect& rect)
{
	if (!m_valid)
		CalculateSizes();

	if (m_visible_cell_count == 0)
		return OpStatus::OK;

	unsigned colbreak = CalculateColumnBreak(rect.width);
	unsigned rows = GetRowCount(colbreak);

	OpRect cellrect = rect;
	cellrect.width = min(max((rect.width - (colbreak - 1) * m_horizontal_margins) / colbreak, m_min_cell_width), m_preferred_cell_width);
	cellrect.height = min(max((rect.height - (rows - 1) * m_vertical_margins) / rows, m_min_cell_height), m_preferred_cell_height);

	return m_background
		? DoTestLayout(rect, cellrect)
		: DoLayout(rect, cellrect);
}

OP_STATUS QuickFlowLayout::DoLayout(const OpRect& rect, OpRect cellrect)
{
	unsigned colbreak = CalculateColumnBreak(rect.width);
	unsigned cellpos = 0;
	for (UINT32 i = 0; i < m_cells.GetCount(); ++i)
	{
		if (!m_cells.Get(i)->IsVisible())
			continue;

		RETURN_IF_ERROR(m_cells.Get(i)->Layout(cellrect, rect));

		CalculateNextCellRect(cellrect, cellpos, colbreak, rect);
	}
	return OpStatus::OK;
}

OP_STATUS QuickFlowLayout::DoTestLayout(const OpRect& rect, OpRect cellrect)
{
	m_background->SetRect(rect, TRUE, FALSE);

	cellrect.x = cellrect.y = 0;

	const OpRect outer_rect(0, 0, rect.width, rect.height);
	const OpRect fixed_cell_rect(0, 0, cellrect.width, cellrect.height);

	unsigned colbreak = CalculateColumnBreak(rect.width);
	unsigned cellpos = 0;
	OpWidget* cell_widget = m_background->GetFirstChild();
	for (UINT32 i = 0; i < m_cells.GetCount(); ++i)
	{
		OP_ASSERT(cell_widget);

		cell_widget->SetVisibility(m_cells.Get(i)->IsVisible());
		if (m_cells.Get(i)->IsVisible())
			m_background->SetChildRect(cell_widget, cellrect, TRUE, FALSE);

		cell_widget = cell_widget->GetNextSibling();

		if (!m_cells.Get(i)->IsVisible())
			continue;

		RETURN_IF_ERROR(m_cells.Get(i)->Layout(fixed_cell_rect));

		CalculateNextCellRect(cellrect, cellpos, colbreak, outer_rect);
	}
	return OpStatus::OK;
}

void QuickFlowLayout::CalculateNextCellRect(OpRect& cellrect, unsigned& cellpos, unsigned colbreak, const OpRect& outer_rect) const
{
	++cellpos;
	if (cellpos % colbreak == 0)
	{
		cellrect.x = outer_rect.x;
		cellrect.y += cellrect.height + m_vertical_margins;
	}
	else
	{
		cellrect.x += cellrect.width + m_horizontal_margins;
	}
}

void QuickFlowLayout::SetParentOpWidget(OpWidget* parent)
{
	if (m_background)
	{
		if (parent)
			parent->AddChild(m_background);
		else
			m_background->Remove();

		return;
	}

	m_parent_op_widget = parent;

	for (unsigned i = 0; i < m_cells.GetCount(); i++)
		m_cells.Get(i)->SetParentOpWidget(parent);
}

void QuickFlowLayout::Show()
{
	m_visible = true;

	for (unsigned i = 0; i < m_cells.GetCount(); i++)
		m_cells.Get(i)->Show();

	if (m_background)
		m_background->SetVisibility(TRUE);
}

void QuickFlowLayout::Hide()
{
	m_visible = false;

	for (unsigned i = 0; i < m_cells.GetCount(); i++)
		m_cells.Get(i)->Hide();

	if (m_background)
		m_background->SetVisibility(FALSE);
}

void QuickFlowLayout::SetEnabled(BOOL enabled)
{
	for (unsigned i = 0; i < m_cells.GetCount(); i++)
		m_cells.Get(i)->SetEnabled(enabled);
}

unsigned QuickFlowLayout::GetDefaultMinimumWidth()
{
	if (!m_valid)
		CalculateSizes();

	unsigned width = m_min_cell_width;
	if (m_min_break > 0)
		width += (m_min_cell_width + m_horizontal_margins) * (GetMinBreak() - 1);

	return width;
}

unsigned QuickFlowLayout::GetDefaultMinimumHeight(unsigned width)
{
	if (!m_valid)
		CalculateSizes();
	return GetHeight(m_min_cell_height, width);
}

unsigned QuickFlowLayout::GetDefaultNominalWidth()
{
	if (!m_valid)
		CalculateSizes();

	unsigned width = m_nominal_cell_width;
	if (m_max_break > 0)
		width += (m_nominal_cell_width + m_horizontal_margins) * (GetMaxBreak() - 1);
	else if (m_min_break > 0)
		width += (m_nominal_cell_width + m_horizontal_margins) * (GetMinBreak() - 1);

	return width;
}

unsigned QuickFlowLayout::GetDefaultNominalHeight(unsigned width)
{
	if (!m_valid)
		CalculateSizes();
	return GetHeight(m_nominal_cell_height, width);
}

unsigned QuickFlowLayout::GetDefaultPreferredWidth()
{
	if (!m_valid)
		CalculateSizes();

	if (m_preferred_cell_width > WidgetSizes::UseDefault)
		return m_preferred_cell_width;

	unsigned width = m_preferred_cell_width;
	if (GetMaxBreak() > 0)
		width += (m_preferred_cell_width + m_horizontal_margins) * (GetMaxBreak() - 1);

	return width;
}

unsigned QuickFlowLayout::GetDefaultPreferredHeight(unsigned width)
{
	if (!m_valid)
		CalculateSizes();
	return GetHeight(m_preferred_cell_height, width);
}

unsigned QuickFlowLayout::GetHeight(unsigned cell_height, unsigned width)
{
	width = max(width, GetMinimumWidth());

	unsigned colbreak = CalculateColumnBreak(width);
	unsigned rows = GetRowCount(colbreak);
	
	if (m_keep_ratio)
	{
		unsigned cell_width = min(max((width - (colbreak - 1) * m_horizontal_margins) / colbreak, m_min_cell_width), m_preferred_cell_width);
		cell_height = m_min_cell_width ? (cell_width * m_min_cell_height) / m_min_cell_width : 0;
	}

	return cell_height * rows + m_vertical_margins * (rows - 1);
}

void QuickFlowLayout::GetDefaultMargins(WidgetSizes::Margins& margins)
{
	if (!m_valid)
		CalculateSizes();
	margins.left = margins.right = m_horizontal_margins;
	margins.top = margins.bottom = m_vertical_margins;
}

unsigned QuickFlowLayout::CalculateColumnBreak(unsigned width)
{
	unsigned colbreak = 1;

	// number of items that fit on one row when each item has minimum width
	colbreak += max((int)width - (int)m_min_cell_width, 0) / max(m_min_cell_width + m_horizontal_margins, 1u);

	// take user-set breaks into account
	colbreak = min(colbreak, GetMaxBreak());
	colbreak = max(colbreak, GetMinBreak());

	return colbreak;
}

void QuickFlowLayout::SetName(const OpStringC8& name)
{
	if (m_background)
		m_background->SetName(name);
}

void QuickFlowLayout::CalculateSizes()
{
	m_min_cell_width = 0;
	m_nominal_cell_width = 0;
	m_preferred_cell_width = 0;
	m_min_cell_height = 0;
	m_nominal_cell_height = 0;
	m_preferred_cell_height = 0;
	m_horizontal_margins = 0;
	m_vertical_margins = 0;
	m_visible_cell_count = 0;

	for (unsigned i = 0; i < m_cells.GetCount(); i++)
	{
		if (m_cells.Get(i)->IsVisible())
		{
			UpdateSizes(*m_cells.Get(i));
			++m_visible_cell_count;
		}
	}

	m_valid = true;
}

void QuickFlowLayout::CalculateSizesToFitRect(const OpRect& rect)
{
	Dimensions dimensions;
	unsigned colbreak = 1;
	unsigned minbreak = 0;
	unsigned maxbreak = 0;
	bool breaks_cleared = m_min_break == 0 && m_max_break == 0;

	if (!breaks_cleared)
	{
		minbreak = GetMinBreak();
		maxbreak = GetMaxBreak();
	}
	else
	{
		minbreak = 1;
		// number of items that fit on one row when each item has minimum width
		maxbreak = 1 + max(rect.width - (int)m_min_cell_width, 0) / max(m_min_cell_width + m_horizontal_margins , 1u);
	}

	for (unsigned cols = minbreak; cols <= maxbreak; ++cols)
	{
		Dimensions new_dimensions = CalculateDimensionsAndFillLevel(rect, cols);
		if (cols == minbreak ||	(!dimensions.fit_the_window && new_dimensions.fit_the_window))
		{ // if this is a first iteration or old dimensions don't fit and the new do: take newly calculated dimensions
			dimensions = new_dimensions;
			colbreak = cols;
		}
		else if (dimensions.fit_the_window && new_dimensions.fit_the_window)
		{ // if both dimensions fit the window take the one with bigger width
			if (new_dimensions.width >= dimensions.width)
			{
				dimensions = new_dimensions;
				colbreak = cols;
			}
		}
		// If new dimensions does not fit don't consider them as the result
	}

	// If we weren't able to find a fit for any of the possible colbreak take the minimum sizes.
	if (!dimensions.fit_the_window)
	{
		dimensions.width = m_min_cell_width;
		dimensions.height = m_min_cell_height;
	}

	// Add fixed padding to calculated dimensions
	m_preferred_cell_width = dimensions.width + m_fixed_padding_width;
	m_min_cell_width = dimensions.width + m_fixed_padding_width;
	m_preferred_cell_height = dimensions.height + m_fixed_padding_height;
	m_min_cell_height = dimensions.height + m_fixed_padding_height;

	/* If we weren't able to find a fit for any of the possible colbreak we must recalculate it as right now
	 * it is set to maxbreak.
	 */
	if (!dimensions.fit_the_window && breaks_cleared)
		colbreak = max(rect.width - (int)m_min_cell_width, 0) / max(m_min_cell_width + m_horizontal_margins , 1u) + 1;

	SetHardBreak(colbreak);

	for (unsigned i = 0; i < m_cells.GetCount(); ++i)
	{
		m_cells.Get(i)->SetFixedWidth(m_preferred_cell_width);
		m_cells.Get(i)->SetFixedHeight(m_preferred_cell_height);
	}
}

QuickFlowLayout::Dimensions QuickFlowLayout::CalculateDimensionsAndFillLevel(const OpRect& rect, unsigned cols)
{
	double ratio = m_preferred_cell_width / static_cast<double>(m_preferred_cell_height);
	unsigned rows = GetRowCount(cols);
	Dimensions result;

	result.width = (rect.width - (cols - 1) * m_horizontal_margins) / cols;
	result.width -= m_fixed_padding_width;
	result.width = min(result.width, m_preferred_cell_width);
	result.width = max(result.width, m_min_cell_width);
	result.height = result.width / ratio;
	unsigned cells_width  = cols * (result.width + m_fixed_padding_width) + (cols - 1) * m_horizontal_margins;
	unsigned cells_height = rows * (result.height + m_fixed_padding_height) + (rows - 1) * m_vertical_margins;
	bool horizontal_fit = cells_width <= static_cast<unsigned>(rect.width);
	bool vertical_fit = cells_height <= static_cast<unsigned>(rect.height);
	result.fit_the_window = horizontal_fit && vertical_fit;

	/* If we haven't found horizontal fit for minimum cell height or
	 * cells with best width fit also horizontally there is no
	 * point to continue calculations...
     */
	if (horizontal_fit && !vertical_fit)
	{
		/* .. but in another case we still can find dimensions which will
		 * fit rect both horizontally and vertically.
		 */
		result.height = (rect.height - (rows - 1) * m_vertical_margins) / rows;
		result.height -= m_fixed_padding_height;
		result.height = min(result.height, m_preferred_cell_height);
		result.height = max(result.height, m_min_cell_height);
		result.width = result.height * ratio;
		cells_width  = cols * (result.width + m_fixed_padding_width) + (cols - 1) * m_horizontal_margins;
		cells_height = rows * (result.height + m_fixed_padding_height) + (rows - 1) * m_vertical_margins;
		horizontal_fit = cells_width <= static_cast<unsigned>(rect.width);
		vertical_fit = cells_height <= static_cast<unsigned>(rect.height);
		result.fit_the_window = horizontal_fit && vertical_fit;
	}
	return result;
}

void QuickFlowLayout::UpdateSizes(QuickWidget& cell)
{
	m_min_cell_width = max(m_min_cell_width, cell.GetMinimumWidth());
	m_nominal_cell_width = max(m_nominal_cell_width, cell.GetNominalWidth());
	m_preferred_cell_width = max(m_preferred_cell_width, cell.GetPreferredWidth());
	m_min_cell_height = max(m_min_cell_height, cell.GetMinimumHeight(m_min_cell_width));
	m_nominal_cell_height = max(m_nominal_cell_height, cell.GetNominalHeight(m_nominal_cell_width));
	m_preferred_cell_height = max(m_preferred_cell_height, cell.GetPreferredHeight(m_preferred_cell_width));
	m_horizontal_margins = max(max(m_horizontal_margins, cell.GetMargins().left), cell.GetMargins().right);
	m_vertical_margins = max(max(m_vertical_margins, cell.GetMargins().top), cell.GetMargins().bottom);
}

void QuickFlowLayout::FitToWidth(unsigned width)
{
	if (!m_valid)
		CalculateSizes();

	unsigned colbreak = 1;
	if (width > m_preferred_cell_width)
		colbreak += (width - m_preferred_cell_width) / (max(m_preferred_cell_width + m_horizontal_margins, 1u));

	SetMinBreak(0);
	SetMaxBreak(colbreak);
}

void QuickFlowLayout::FitToRect(const OpRect& rect)
{
	if (!m_valid)
		CalculateSizes();
	CalculateSizesToFitRect(rect);
}

void QuickFlowLayout::SetFixedPadding(unsigned width, unsigned height)
{
	m_fixed_padding_width = width;
	m_fixed_padding_height = height;
}

void QuickFlowLayout::UpdateZOrder()
{
	// Reorder the children at parent level. This helps to maintain the TAB flow.
	for (int i = 0; i < (int)m_cells.GetCount(); i++)
	{
		m_cells.Get(i)->SetZ(OpWidget::Z_BOTTOM);
	}
}

OP_STATUS QuickFlowLayout::PrepareTestWidget(QuickWidget* widget, int pos)
{
	ItemWidget* cell_widget = OP_NEW(ItemWidget, ());
	RETURN_OOM_IF_NULL(cell_widget);

	widget->SetParentOpWidget(cell_widget);

	OpWidget* next_cell_widget = m_background->GetFirstChild();
	for ( ; pos > 0; pos--)
	{
		OP_ASSERT(next_cell_widget != NULL);
		next_cell_widget = next_cell_widget->GetNextSibling();
	}

	m_background->InsertChild(cell_widget, next_cell_widget, FALSE);

	return UpdateTestWidgetNames();
}

OP_STATUS QuickFlowLayout::RemoveTestWidget(unsigned pos)
{
	unsigned cell_to_skip_index = 0;
	for (OpWidget* cell_widget = m_background->GetFirstChild();
		cell_widget;
		cell_widget = cell_widget->GetNextSibling())
	{
		if (cell_to_skip_index == pos)
		{
			cell_widget->Delete();
			break;
		}

		cell_to_skip_index++;
	}

	return UpdateTestWidgetNames();
}

void QuickFlowLayout::RemoveAllTestWidgets()
{
	OpWidget* cell_widget = m_background->GetFirstChild();
	while(cell_widget)
	{
		OpWidget* todelete = cell_widget;
		cell_widget= cell_widget->GetNextSibling();
		todelete->Delete();
	}
}

OP_STATUS QuickFlowLayout::UpdateTestWidgetNames()
{
	unsigned id = 0;
	for (OpWidget* fake_widget = m_background->GetFirstChild();
		fake_widget;
		fake_widget = fake_widget->GetNextSibling())
	{
		OpString8 name;
		RETURN_IF_ERROR(name.AppendFormat("Cell%u", id));
		fake_widget->SetName(name);
		id++;
	}

	return OpStatus::OK;
}
