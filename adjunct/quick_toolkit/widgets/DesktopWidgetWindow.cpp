/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "adjunct/quick_toolkit/widgets/DesktopWidgetWindow.h"

#include "modules/widgets/WidgetContainer.h"

#include "modules/scope/src/scope_manager.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"


OP_STATUS DesktopWidgetWindow::Init(OpWindow::Style style, const OpStringC8& name,
									OpWindow* parent_window,
									OpView* parent_view , 
									UINT32 effects, 
									void* native_handle)
{
	OP_STATUS status = WidgetWindow::Init(style, parent_window, parent_view, effects, native_handle);

	// Add the scope listener for each window
	if (g_scope_manager->desktop_window_manager)
		m_listener = g_scope_manager->desktop_window_manager;
	
	m_window_name.Set(name);
	return status;
}

OpWidget* DesktopWidgetWindow::GetWidgetByName(const OpStringC8& name)
{
	if (name.IsEmpty())
		return NULL;
  
	if (GetWidgetContainer())
	{
		OpWidget* root_widget = GetWidgetContainer()->GetRoot();
		if (root_widget)
			return root_widget->GetWidgetByName(name.CStr());
	}
	return NULL;
}

OpWidget* DesktopWidgetWindow::GetWidgetByText(const OpStringC8& text)
{
	if (text.IsEmpty())
		return NULL;
  
	OpVector<OpWidget> widgets;
	if (OpStatus::IsSuccess(ListWidgets(widgets)))
	{
		for (UINT32 i = 0; i < widgets.GetCount(); i++)
		{
			OpWidget* widget = widgets.Get(i);
			OpString wdg_text;
			if (widget && OpStatus::IsSuccess(widget->GetText(wdg_text)) && wdg_text.HasContent())
			{
				OpString8 widget_text;
				widget_text.Set(wdg_text);
				if (widget_text.Compare(text) == 0)
				{
					return widget;
				}
			}
		}
	}
  
  return NULL;
}

// TODO: Move common parts of these from here and DesktopWindow to scope
OP_STATUS DesktopWidgetWindow::ListWidgets(OpVector<OpWidget> &widgets) 
{
	if (!GetWidgetContainer())
	{
		OP_ASSERT(!"Missing subclass implementation\n");
		return OpStatus::ERR;
	}
	
	OpWidget* widget = GetWidgetContainer()->GetRoot(); 
	if (!widget)
		return OpStatus::ERR;
	
	OpWidget* child = widget->GetFirstChild();
	while (child) 
	{
		widgets.Add(child);
		ListWidgets(child, widgets);
		child = child->GetNextSibling();
	}

	return OpStatus::OK;
}

OP_STATUS DesktopWidgetWindow::ListWidgets(OpWidget* widget, OpVector<OpWidget> &widgets)
{
	if (!widget)
		return OpStatus::ERR;
	OpWidget* child = widget->GetFirstChild();
	while (child)
	{
		widgets.Add(child);
		ListWidgets(child, widgets);
		child = child->GetNextSibling();
	} 

	return OpStatus::OK;
}

