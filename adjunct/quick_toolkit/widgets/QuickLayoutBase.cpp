/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "QuickLayoutBase.h"

QuickLayoutBase::QuickLayoutBase()
	: m_visible(TRUE),
	  m_parent_op_widget(0)
{
}

OP_STATUS QuickLayoutBase::InsertWidget(QuickWidget * widget, int pos)
{
	if (pos < 0)
		pos = m_widgets.GetCount();

	RETURN_IF_ERROR(m_widgets.Insert(pos, widget));

	widget->SetContainer(this);
	widget->SetParentOpWidget(m_parent_op_widget);

	OnContentsChanged();
	return OpStatus::OK;
}

void QuickLayoutBase::RemoveWidget(UINT32 pos)
{
	m_widgets.Delete(pos);
	OnContentsChanged();
}

UINT32 QuickLayoutBase::GetWidgetCount()
{
	return m_widgets.GetCount();
}

QuickWidget * QuickLayoutBase::GetWidget(UINT32 pos)
{
	return m_widgets.Get(pos);
}

void QuickLayoutBase::SetParentOpWidget(OpWidget* parent)
{
	m_parent_op_widget = parent;
	for (UINT32 widget_no = 0; widget_no < m_widgets.GetCount(); widget_no ++)
		m_widgets.Get(widget_no)->SetParentOpWidget(parent);
}

void QuickLayoutBase::Show()
{
	m_visible = TRUE;
	for (UINT32 widget_no = 0; widget_no < m_widgets.GetCount(); widget_no ++)
		m_widgets.Get(widget_no)->Show();
}

void QuickLayoutBase::Hide()
{
	m_visible = FALSE;
	for (UINT32 widget_no = 0; widget_no < m_widgets.GetCount(); widget_no ++)
		m_widgets.Get(widget_no)->Hide();
}

void QuickLayoutBase::SetEnabled(BOOL enabled)
{
	for (UINT32 widget_no = 0; widget_no < m_widgets.GetCount(); widget_no ++)
		m_widgets.Get(widget_no)->SetEnabled(enabled);
}

void QuickLayoutBase::GetWidthRange(QuickWidget * widget, unsigned upper_bound, unsigned * min_width, unsigned * max_width)
{
	*min_width = widget->GetMinimumWidth();
	unsigned large = widget->GetPreferredWidth();
	if (large == WidgetSizes::UseDefault)
		*max_width = *min_width;
	else if (large > upper_bound)
		*max_width = upper_bound;
	else
		*max_width = large;
}

void QuickLayoutBase::GetHeightRange(QuickWidget * widget, unsigned width, unsigned upper_bound, unsigned * min_height, unsigned * max_height)
{
	*min_height = widget->GetMinimumHeight(width);
	unsigned large = widget->GetPreferredHeight(width);
	if (large == WidgetSizes::UseDefault)
		*max_height = *min_height;
	else if (large > upper_bound)
		*max_height = upper_bound;
	else
		*max_height = large;
}
