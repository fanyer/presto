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

#include "adjunct/quick_toolkit/widgets/QuickGrid/GenericGrid.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick_toolkit/widgets/OpRectPainter.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCellIterator.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridLayouter.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/UniformGridSizer.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpButtonGroup.h"
#include "modules/widgets/OpWidget.h"


GenericGrid::GenericGrid()
  : m_visible(true)
  , m_center_horizontally(false)
  , m_center_vertically(true)
  , m_parent_op_widget(0)
  , m_button_group(NULL)
  , m_default_sizer(*this)
  , m_sizer(&m_default_sizer)
{
	Invalidate(FALSE);
}

GenericGrid::~GenericGrid()
{
	if (m_sizer != &m_default_sizer)
		OP_DELETE(m_sizer);

	OP_DELETE(m_button_group); 
}

OP_STATUS GenericGrid::SetUniformCells(bool uniform_cells)
{
	if (uniform_cells && m_sizer == &m_default_sizer)
	{
		m_sizer = OP_NEW(UniformGridSizer, (*this));
		RETURN_OOM_IF_NULL(m_sizer);
	}
	else if (!uniform_cells && m_sizer != &m_default_sizer)
	{
		OP_DELETE(m_sizer);
		m_sizer = &m_default_sizer;
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** GenericGrid::OnContentsChanged
 ***********************************************************************************/
void GenericGrid::OnContentsChanged()
{
	Invalidate();
}

/***********************************************************************************
 ** Does layout on all cells using GridLayouter
 **
 ** GenericGrid::Layout
 ***********************************************************************************/
OP_STATUS GenericGrid::Layout(const OpRect& rect)
{
	OpAutoPtr<GridLayouter> layouter(CreateLayouter());
	RETURN_OOM_IF_NULL(layouter.get());

	unsigned col_count = GetColumnCount();
	unsigned row_count = GetRowCount();

	OpRect layout_rect = rect;
	layout_rect.width = max((unsigned)layout_rect.width, GetMinimumWidth());
	layout_rect.height = max((unsigned)layout_rect.height, GetMinimumHeight(layout_rect.width));

	layouter->SetGridLayoutRect(layout_rect);

	RETURN_IF_ERROR(m_sizer->SetColumnSizes(*layouter));
	RETURN_IF_ERROR(m_sizer->SetRowSizes(*layouter));

	for (unsigned col = 0; col < col_count; col++)
	{
		for (unsigned row = 0; row < row_count; row++)
		{
			//Since GetCell returns the starting cell of a span, it is
			//unnecessary to repeat the same operations on the other cells that
			//belong to the same span.
			GridCell* cell = GetStrictCell(col, row);
			if (!cell)
				continue;

			OpRect cell_rect = layouter->GetLayoutRectForCell(col, row, cell->GetColSpan(), cell->GetRowSpan());
			OpRect adapted_rect = cell_rect;
			if (UiDirection::Get() == UiDirection::RTL)
				adapted_rect.x += adapted_rect.width - min((unsigned)adapted_rect.width, cell->GetPreferredWidth());
			adapted_rect.width = min((unsigned)adapted_rect.width, cell->GetPreferredWidth());
			adapted_rect.height = min((unsigned)adapted_rect.height, cell->GetPreferredHeight(adapted_rect.width));

			// Center if necessary
			if (m_center_horizontally)
			{
				if (UiDirection::Get() == UiDirection::LTR)
					adapted_rect.x += (cell_rect.width - adapted_rect.width) / 2;
				else
					adapted_rect.x -= (cell_rect.width - adapted_rect.width) / 2;
			}
			if (m_center_vertically)
				adapted_rect.y += (cell_rect.height - adapted_rect.height) / 2;

			RETURN_IF_ERROR(cell->Layout(adapted_rect));
		}
	}

	return PrepareRectPainter(rect, *layouter);
}

/***********************************************************************************
 **
 ** GenericGrid::CreateLayouter
 ***********************************************************************************/
GridLayouter* GenericGrid::CreateLayouter()
{
	return OP_NEW(GridLayouter, ());
}

void GenericGrid::Invalidate(BOOL propagate)
{
	m_valid_defaults = false;

	m_sizer->Invalidate();

	op_memset(m_cached_size, 0, sizeof(m_cached_size));
	op_memset(m_cached_index, 0, sizeof(m_cached_index));

	if (propagate)
		BroadcastContentsChanged();
}

/***********************************************************************************
 ** Sets parent widget on all children
 **
 ** GenericGrid::SetParentOpWidget
 ***********************************************************************************/
void GenericGrid::SetParentOpWidget(OpWidget* parent)
{
	SetCurrentParentOpWidget(parent);

	for (GridCellIterator it(*this); it; ++it)
		it->SetParentOpWidget(parent);
}

/***********************************************************************************
 ** Calls Show on all children
 **
 ** GenericGrid::Show
 ***********************************************************************************/
void GenericGrid::Show()
{
	m_visible = true;

	for (GridCellIterator it(*this); it; ++it)
		it->Show();
}

/***********************************************************************************
 ** Calls Hide on all children
 **
 ** GenericGrid::Hide
 ***********************************************************************************/
void GenericGrid::Hide()
{
	m_visible = false;

	for (GridCellIterator it(*this); it; ++it)
		it->Hide();
}

/***********************************************************************************
 ** Whether the grid is visible
 **
 ** GenericGrid::IsVisible
 ***********************************************************************************/
BOOL GenericGrid::IsVisible()
{
	return m_visible;
}

/***********************************************************************************
 ** Enables/disables all children
 **
 ** GenericGrid::SetEnabled
 ***********************************************************************************/
void GenericGrid::SetEnabled(BOOL enabled)
{
	for (GridCellIterator it(*this); it; ++it)
		it->SetEnabled(enabled);
}

unsigned GenericGrid::GetDefaultHeight(SizeType type, unsigned width)
{
	if (!m_valid_defaults)
		CalculateDefaults();

	// Check if we can use the default height
	if (!m_height_depends_on_width || width == WidgetSizes::UseDefault || width == m_default_size.width[type])
		return m_default_size.height[type];

	// Check if we have a cached height we can use
	for (size_t i = 0; i < CACHED_SIZES; i++)
	{
		if (m_cached_size[i].width[type] == width)
			return m_cached_size[i].height[type];
	}

	// Nothing found, let's calculate this height
	OpAutoPtr<GridLayouter> layouter(CreateLayouter());
	RETURN_VALUE_IF_NULL(layouter.get(), 0);
	RETURN_VALUE_IF_ERROR(m_sizer->SetColumnSizes(*layouter), 0);

	unsigned height = 0;
	switch (type)
	{
	case MINIMUM:
		height = m_sizer->GetMinimumGridHeight(width, *layouter);
		break;
	case NOMINAL:
		height = m_sizer->GetNominalGridHeight(width, *layouter);
		break;
	case PREFERRED:
		height = m_sizer->GetPreferredGridHeight(width, *layouter);
		break;
	}
	m_cached_size[m_cached_index[type]].width[type] = width;
	m_cached_size[m_cached_index[type]].height[type] = height;
	m_cached_index[type] = (m_cached_index[type] + 1) % CACHED_SIZES;

	return height;
}

void GenericGrid::CalculateDefaults()
{
	m_height_depends_on_width = CheckHeightDependsOnWidth();

	m_default_margins = m_sizer->GetGridMargins();

	m_default_size.width[MINIMUM] = m_sizer->GetMinimumGridWidth();
	m_default_size.width[NOMINAL] = m_sizer->GetNominalGridWidth();
	m_default_size.width[PREFERRED] = m_sizer->GetPreferredGridWidth();

	if (m_height_depends_on_width)
	{
		OpAutoPtr<GridLayouter> layouter(CreateLayouter());
		if (!layouter.get())
			return;
		RETURN_VOID_IF_ERROR(m_sizer->SetColumnSizes(*layouter));

		m_default_size.height[MINIMUM] = m_sizer->GetMinimumGridHeight(m_default_size.width[MINIMUM], *layouter);
		m_default_size.height[NOMINAL] = m_sizer->GetNominalGridHeight(m_default_size.width[NOMINAL], *layouter);
		m_default_size.height[PREFERRED] = m_sizer->GetPreferredGridHeight(m_default_size.width[PREFERRED], *layouter);
	}
	else
	{
		m_default_size.height[MINIMUM] = m_sizer->GetMinimumGridHeight();
		m_default_size.height[NOMINAL] = m_sizer->GetNominalGridHeight();
		m_default_size.height[PREFERRED] = m_sizer->GetPreferredGridHeight();
	}

	m_valid_defaults = true;
}

bool GenericGrid::CheckHeightDependsOnWidth()
{
	for (GridCellIterator it(*this); it; ++it)
	{
		if (it->HeightDependsOnWidth())
			return true;
	}

	return false;
}

OP_STATUS GenericGrid::RegisterToButtonGroup(OpButton* button)
{
	if (!m_button_group)
	{
		m_button_group = OP_NEW(OpButtonGroup, (GetParentOpWidget()));
		RETURN_OOM_IF_NULL(m_button_group);
	}

	button->RegisterToButtonGroup(m_button_group);

	return OpStatus::OK;

}

OP_STATUS GenericGrid::PrepareRectPainter(const OpRect& rect, GridLayouter& layouter)
{
	if (!CommandLineManager::GetInstance()->GetArgument(
				CommandLineManager::DebugLayout))
		return OpStatus::OK;

	OpRectPainter* painter = GetRectPainter();
	if (!painter)
		return OpStatus::OK;

	painter->ClearElements();
	painter->Remove();
	m_parent_op_widget->AddChild(painter, FALSE, TRUE);

	painter->SetRect(rect);
	painter->ShowText(TRUE);

	for (OpWidget* sibling = painter->GetNextSibling(); sibling && sibling->GetType() == OpTypedObject::WIDGET_TYPE_RECT_PAINTER; sibling = sibling->GetNextSibling())
	{
		if (static_cast<OpRectPainter*>(sibling)->GetLayout()->GetContainer() == this)
		{
			// Disable text if we have a child here to prevent too many overlapping texts
			painter->ShowText(FALSE);
			break;
		}
	}

	unsigned col_count = GetColumnCount();
	unsigned row_count = GetRowCount();

	for (unsigned col = 0; col < col_count; col++)
	{
		for (unsigned row = 0; row < row_count; row++)
		{
			GridCell* cell = GetCell(col, row);
			

			OpRectPainter::Element element (layouter.GetLayoutRectForCell(col, row, 1, 1));
			element.rect.OffsetBy(-rect.x, -rect.y);
			element.min_width = cell->GetMinimumWidth();
			element.min_height = cell->GetMinimumHeight(element.rect.width);
			element.nom_width = cell->GetNominalWidth();
			element.nom_height = cell->GetNominalHeight(element.rect.width);
			element.pref_width = cell->GetPreferredWidth();
			element.pref_height = cell->GetPreferredHeight(element.rect.width);

			RETURN_IF_ERROR(painter->AddElement(element));
		}
	}

	return OpStatus::OK;
}


OpRectPainter* GenericGrid::GetRectPainter()
{
	if (!m_parent_op_widget)
		return 0;

	/* for (OpWidget* child = m_parent_op_widget->GetLastChild(); child; child = child->GetPreviousSibling()) */
	for (OpWidget* child = m_parent_op_widget->GetFirstChild(); child; child = child->GetNextSibling())
	{
		if (child->GetType() == OpTypedObject::WIDGET_TYPE_RECT_PAINTER)
		{
			OpRectPainter* painter = static_cast<OpRectPainter*>(child);
			if (painter->GetLayout() == this)
				return painter;
		}
	}

	return OP_NEW(OpRectPainter, (this));
}
