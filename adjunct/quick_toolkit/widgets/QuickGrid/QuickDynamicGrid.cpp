/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickDynamicGrid.h"
#include "adjunct/quick_toolkit/creators/QuickGridRowCreator.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridLayouter.h"
#include "modules/widgets/OpWidget.h"

/** @brief Widget used on background. Makes it easy for testing
  * frameworks to recognize a dynamic grid in the widget structure
  */
class QuickDynamicGrid::BackgroundWidget : public OpWidget
{
public:
	virtual Type GetType() { return WIDGET_TYPE_DYNAMIC_GRID_LAYOUT; }
	virtual BOOL IsOfType(Type type) { return type == WIDGET_TYPE_DYNAMIC_GRID_LAYOUT; }
};

/** @brief Widget used as background for each row in the grid.
  * Makes it easy for testing frameworks to recognize individual
  * rows in the widget structure
  */
class QuickDynamicGrid::ItemWidget : public OpWidget
{
public:
	virtual Type GetType() { return WIDGET_TYPE_DYNAMIC_GRID_ITEM; }
	virtual BOOL IsOfType(Type type) { return type == WIDGET_TYPE_DYNAMIC_GRID_ITEM; }
};

/** @brief Custom layouter for use when testing
  * Makes cells stay within ItemWidgets
  */
class QuickDynamicGrid::TestLayouter : public GridLayouter
{
public:
	TestLayouter(OpWidget* background) : m_background(background) {}
	virtual OpRect GetLayoutRectForCell(unsigned col, unsigned row, unsigned colspan, unsigned rowspan);
private:
	OpWidget* m_background;
};


QuickDynamicGrid::QuickDynamicGrid()
  : m_background(0)
  , m_instantiator(0)
{
}


QuickDynamicGrid::~QuickDynamicGrid()
{
	OP_DELETE(m_instantiator);
	if (m_background)
		m_background->Delete();
}

OP_STATUS QuickDynamicGrid::Init(TestState teststate)
{
	if (teststate == FORCE_TESTABLE)
	{
		m_background = OP_NEW(BackgroundWidget, ());
		RETURN_OOM_IF_NULL(m_background);
	}

	return OpStatus::OK;
}

void QuickDynamicGrid::SetName(const OpStringC8& name)
{
	if (!m_background)
		return;

	m_background->SetName(name);
}

void QuickDynamicGrid::SetTemplateInstantiator(QuickGridRowCreator* instantiator)
{
	OP_DELETE(m_instantiator);
	m_instantiator = instantiator;
}


OP_STATUS QuickDynamicGrid::Instantiate(TypedObjectCollection& collection)
{
	if (!m_instantiator)
		return OpStatus::ERR;
	
	return m_instantiator->Instantiate(*this, collection);
}

void QuickDynamicGrid::SetParentOpWidget(OpWidget* parent)
{
	if (!m_background)
		return QuickGrid::SetParentOpWidget(parent);

	if (parent)
		parent->AddChild(m_background);
	else
		m_background->Remove();
}


OP_STATUS QuickDynamicGrid::Layout(const OpRect& rect)
{
	if (m_background)
		m_background->SetRect(rect, TRUE, FALSE);

	return QuickGrid::Layout(rect);
}


GridLayouter* QuickDynamicGrid::CreateLayouter()
{
	if (!m_background)
		return QuickGrid::CreateLayouter();

	return OP_NEW(TestLayouter, (m_background));
}


OpRect QuickDynamicGrid::TestLayouter::GetLayoutRectForCell(unsigned col, unsigned row, unsigned colspan, unsigned rowspan)
{
	OpRect rect(GridLayouter::GetLayoutRectForCell(col, row, colspan, rowspan));

	// Adapt position of row widget to information from cell
	OpWidget* rowwidget = m_background->GetFirstChild();
	for (unsigned i = 0; rowwidget && i < row; i++)
		rowwidget = rowwidget->GetNextSibling();

	OP_ASSERT(rowwidget);
	OpRect rowrect = rowwidget->GetRect();
	rowrect.x = 0;
	rowrect.y = rect.y - m_grid_rect.y;
	rowrect.width = m_background->GetRect().width;
	rowrect.height = max(rowrect.height, rect.height);

	rowwidget->SetRect(rowrect, TRUE, FALSE);

	// Position of the cell is relative to the row widget when testing
	rect.y = 0;
	rect.x -= m_grid_rect.x;

	return rect;
}


OP_STATUS QuickDynamicGrid::OnRowAdded()
{
	if (!m_background)
		return OpStatus::OK;

	// Add row widget used for testing
	OpWidget* rowwidget = OP_NEW(ItemWidget, ());
	RETURN_OOM_IF_NULL(rowwidget);

	m_background->AddChild(rowwidget);
	SetCurrentParentOpWidget(rowwidget);

	OpString8 name;
	RETURN_IF_ERROR(name.AppendFormat("GridItem%u", m_background->childs.Cardinal() - 1));
	rowwidget->SetName(name);

	return OpStatus::OK;
}


void QuickDynamicGrid::OnRowRemoved(unsigned row)
{
	if (!m_background)
		return;

	// Remove row widget placed for testing
	OpWidget* rowwidget = m_background->GetFirstChild();
	for (unsigned i = 0; i < row && rowwidget; i++)
	{
		rowwidget = rowwidget->GetNextSibling();
	}

	if (rowwidget)
	{
		OpWidget* todelete = rowwidget;
		rowwidget = rowwidget->GetNextSibling();
		todelete->Delete();
	}

	// Adapt names of other row widgets
	while (rowwidget)
	{
		OpString8 name;
		OpStatus::Ignore(name.AppendFormat("GridItem%u", row));
		rowwidget->SetName(name);
		rowwidget = rowwidget->GetNextSibling();
		row++;
	}
}


void QuickDynamicGrid::OnRowMoved(unsigned from_row, unsigned to_row)
{
	// Move cell widget for testing
	if (!m_background)
		return;

	OpWidget* from_row_widget = NULL;
	OpWidget* to_row_widget = NULL;
	unsigned skip_counter = 0;

	for (OpWidget* row_widget = m_background->GetFirstChild();
		row_widget != NULL; 
		row_widget = row_widget->GetNextSibling())
	{
		if (skip_counter == from_row && !from_row_widget)
			from_row_widget = row_widget;

		if (skip_counter == to_row && !to_row_widget)
			to_row_widget = row_widget;

		if (from_row_widget && to_row_widget)
			break;

		skip_counter++;
	}

	if (from_row_widget && to_row_widget)
	{
		OpWidget* from_pred = from_row_widget->Pred();
		OpWidget* to_suc = to_row_widget->Suc();

		if (from_pred == to_row_widget || from_row_widget == to_suc || !to_suc || !from_pred)
		{
			from_row_widget->Out();
			from_row_widget->Precede(to_row_widget);
		}
		else
		{
			from_row_widget->Out();
			to_row_widget->Out();
			
			from_row_widget->Precede(to_suc);
			to_row_widget->Follow(from_pred);
		}

		// Update name
		unsigned i = 0;
		for (OpWidget* row_widget = m_background->GetFirstChild();
			row_widget != NULL; 
			row_widget = row_widget->GetNextSibling())
		{
			OpString8 name;
			OpStatus::Ignore(name.AppendFormat("GridItem%u", i));
			row_widget->SetName(name);
			i++;
		}
	}
}

void QuickDynamicGrid::OnCleared()
{
	if (!m_background)
		return;

	OpWidget* rowwidget = m_background->GetFirstChild();
	while(rowwidget)
	{
		OpWidget* todelete = rowwidget;
		rowwidget = rowwidget->GetNextSibling();
		todelete->Delete();
	}
}
