/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 * @author Cihat Imamoglu (cihati)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/NullWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickRadioButton.h"

QuickStackLayout::QuickStackLayout(Orientation orientation)
  : m_orientation(orientation)
{
}

QuickStackLayout::~QuickStackLayout()
{
	for (unsigned i = 0; i < m_cells.GetCount(); i++)
		OP_DELETE(m_cells[i]);
}

unsigned QuickStackLayout::GetColumnCount() const
{
	if (m_orientation == VERTICAL)
		return 1;

	return m_cells.GetCount();
}

unsigned QuickStackLayout::GetRowCount() const
{
	if (m_orientation == HORIZONTAL)
		return 1;

	return m_cells.GetCount();
}

GridCell* QuickStackLayout::GetCell(unsigned col, unsigned row)
{
	if (m_orientation == HORIZONTAL)
	{
		OP_ASSERT(row == 0 && col < m_cells.GetCount() &&
				  m_cells[col]);

		return m_cells[col];
	}
	else
	{
		OP_ASSERT(col == 0 && row < m_cells.GetCount() &&
				  m_cells[row]);

		return m_cells[row];
	}
}

OP_STATUS QuickStackLayout::InsertWidget(QuickWidget *widget, int pos) 
{
	OpAutoPtr<GridCell> cell (OP_NEW(GridCell, ()));
	if (!cell.get())
	{
		OP_DELETE(widget);
		return OpStatus::ERR_NO_MEMORY;
	}
	
	cell->SetContent(widget);
	cell->SetContainer(this);
	cell->SetParentOpWidget(GetParentOpWidget());
	
	if (pos == -1)
		pos = m_cells.GetCount();
	
	RETURN_IF_ERROR(m_cells.Insert(pos,cell.get()));
	
	cell.release();
	
	Invalidate();

	if (widget && widget->GetTypedObject<QuickRadioButton>())
		RETURN_IF_ERROR(RegisterToButtonGroup(widget->GetTypedObject<QuickRadioButton>()->GetOpWidget()));

	return OpStatus::OK;
}

OP_STATUS QuickStackLayout::InsertEmptyWidget(unsigned min_width, unsigned min_height, unsigned pref_width, unsigned pref_height, int pos)
{
	QuickWidget* widget = OP_NEW(NullWidget, ());
	if (!widget)
		return OpStatus::ERR_NO_MEMORY;

	widget->SetMinimumWidth(min_width);
	widget->SetMinimumHeight(min_height);
	widget->SetPreferredWidth(pref_width);
	widget->SetPreferredHeight(pref_height);

	return InsertWidget(widget, pos);
}

void QuickStackLayout::RemoveWidget(int pos)
{
	OP_DELETE(m_cells[pos]);
	m_cells.Remove(pos);
	Invalidate();
}

void QuickStackLayout::RemoveAllWidgets()
{
	for (unsigned i = 0; i < m_cells.GetCount(); i++)
		OP_DELETE(m_cells[i]);
	m_cells.Remove(0, m_cells.GetCount());
	Invalidate();
}
