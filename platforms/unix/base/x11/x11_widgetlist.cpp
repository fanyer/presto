/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#include "core/pch.h"

#include "x11_widgetlist.h"

#include "x11_widget.h"


X11WidgetList::~X11WidgetList()
{
	/* TODO: proper garbage collection for widgets. It's a bit late to do it
	   here in the destructor. */
	OP_ASSERT(!widgets.First());

	/* Don't delete the elements in the widget list here.  It WILL
	 * crash as there are plenty of things the X11Widget depends on
	 * that have already been destroyed.  I don't think it is
	 * particularly useful to crash here, and even if we want to crash
	 * here, it is better to do so explicitly (e.g. "((int*)0) = 0"),
	 * rather than through code that looks like it is trying to
	 * recover.
	 */
}

X11Widget *X11WidgetList::FindWidget(X11Types::Window handle)
{
	for (Element *e = widgets.First(); e; e = e->Suc())
	{
		if (e->widget.GetWindowHandle() == handle)
			return &e->widget;
	}
	return NULL;
}

void X11WidgetList::AddWidget(Element& widget)
{
	widget.IntoStart(&widgets);
}

void X11WidgetList::RemoveWidget(Element& widget)
{
	if (captured_widget == &widget.widget)
		captured_widget = NULL;
	if (popup_parent_widget == &widget.widget)
		popup_parent_widget = NULL;
	if (grabbed_widget == &widget.widget)
		grabbed_widget = NULL;
	if (dragged_widget == &widget.widget)
		dragged_widget = NULL;
	if (drag_source == &widget.widget)
		drag_source = NULL;
	if (drop_target == &widget.widget)
		drop_target = NULL;
	if (modal_toolkit_parent == &widget.widget)
		modal_toolkit_parent = NULL;
	if (input_widget == &widget.widget)
		input_widget = NULL;

	OpStatus::Ignore(RemoveGrabbedWidget(&widget.widget));
	widget.Out();
}

void X11WidgetList::AddModalWidget(Element& widget)
{
	OP_ASSERT(widget.widget.IsModal());

	// Move to end of list
	widget.Out();
	widget.Into(&widgets);
}

void X11WidgetList::RemoveModalWidget(Element& widget)
{
	OP_ASSERT(!widget.widget.IsModal());

	// Move to start of list
	widget.Out();
	widget.IntoStart(&widgets);
}

X11Widget *X11WidgetList::GetModalWidget()
{
	Element* elem = widgets.Last();
	return elem && elem->widget.IsModal() ? &elem->widget : NULL;
}

OP_STATUS X11WidgetList::AddGrabbedWidget(X11Widget* widget)
{
	grabbed_widget = widget;

	INT32 index = m_grab_stack.Find(widget);
	if (index >= 0)
		m_grab_stack.Remove(index);
	return m_grab_stack.Add(widget);
}

OP_STATUS X11WidgetList::RemoveGrabbedWidget(X11Widget* widget)
{
	X11Widget* old_grabbed_widget = grabbed_widget;

	if (grabbed_widget == widget)
		grabbed_widget = NULL;

	INT32 index = m_grab_stack.Find(widget);
	if (index == -1)
		return OpStatus::ERR;
	else
	{
		m_grab_stack.Remove(index);
		// Give grab to new top of stack. grabbed_widget is always top of stack
		if (index > 0 && old_grabbed_widget == widget)
		{
			m_grab_stack.Get(index-1)->Grab();
		}
	}

	return OpStatus::OK;

}

void X11WidgetList::UpdateCursor()
{
	for (Element* e = widgets.First(); e; e = e->Suc())
		e->widget.UpdateCursor();
}

void X11WidgetList::OnApplicationShow(bool show)
{
	for (Element* e = widgets.First(); e; e = e->Suc())
		e->widget.OnApplicationShow(show);
}

void X11WidgetList::OnApplicationExit()
{
	for (Element* e = widgets.First(); e; e = e->Suc())
		e->widget.OnApplicationExit();
}

void X11WidgetList::SetCursor(unsigned int cursor)
{
	for (Element* e = widgets.First(); e; e = e->Suc())
	{
		e->widget.SaveCursor();
		e->widget.SetCursor(cursor);
	}
}

void X11WidgetList::RestoreCursor()
{
	for (Element* e = widgets.First(); e; e = e->Suc())
	{
		e->widget.RestoreCursor();
	}
}
