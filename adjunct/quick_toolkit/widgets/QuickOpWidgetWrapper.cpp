/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"

#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/widgets/OpWidget.h"


/**
 * An OpWidgetListener able to forward widget events to multiple
 * OpWidgetListeners.  Helps solve the problem of OpWidget being able to have up
 * to one OpWidgetListener registered.
 *
 * @note Add more forwarding OpWidgetListener method implementations here as
 * required.
 */
class GenericQuickOpWidgetWrapper::OpWidgetEventForwarder
		: public OpWidgetListener
{
public:
	OP_STATUS AddListener(OpWidgetListener& listener)
		{ return m_listeners.Add(&listener); }

	OP_STATUS RemoveListener(OpWidgetListener& listener)
		{ return m_listeners.Remove(&listener); }

	// OpWidgetListener

	virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnChange(widget, changed_by_mouse);
	}

	virtual void OnSortingChanged(OpWidget* widget)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnSortingChanged(widget);
	}

	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnDragStart(widget, pos, x, y);
	}

	virtual void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnDragMove(widget, drag_object, pos, x, y);
	}

	virtual void OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnDragDrop(widget, drag_object, pos, x, y);
	}

	virtual void OnDragLeave(OpWidget* widget, DesktopDragObject* drag_object)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnDragLeave(widget, drag_object);
	}

	virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		{
			if (m_listeners.GetNext(it)->OnContextMenu(widget, child_index, menu_point, avoid_rect, keyboard_invoked))
				return TRUE;
		}
		return HandleWidgetContextMenu(widget, menu_point);
	}

	virtual void OnDropdownMenu(OpWidget* widget, BOOL show)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnDropdownMenu(widget, show);
	}

	virtual void OnMouseEvent(OpWidget* widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}

	virtual void OnMouseLeave(OpWidget *widget)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnMouseLeave(widget);
	}

	virtual void OnMouseMove(OpWidget *widget, const OpPoint &point)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnMouseMove(widget, point);
	}

	virtual void OnPaint(OpWidget *widget, const OpRect &paint_rect)
	{
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnPaint(widget, paint_rect);
	}

	virtual void OnBeforePaint(OpWidget *widget) 
	{ 
		for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
			m_listeners.GetNext(it)->OnBeforePaint(widget);
	}

private:
	OpListeners<OpWidgetListener> m_listeners;
};


GenericQuickOpWidgetWrapper::GenericQuickOpWidgetWrapper(OpWidget* widget)
	: m_opwidget(widget)
	, m_opwidget_event_forwarder(0)
{
}

GenericQuickOpWidgetWrapper::~GenericQuickOpWidgetWrapper()
{
	if (m_opwidget && !m_opwidget->IsDeleted())
		m_opwidget->Delete();

	// No need to unregister our listener from the OpWidget, it is cleared in
	// OpWidget::Delete().
	OP_DELETE(m_opwidget_event_forwarder);
	m_opwidget_event_forwarder = 0;
}


OP_STATUS GenericQuickOpWidgetWrapper::Layout(const OpRect& rect)
{
	if (m_opwidget->IsFloating())
		m_opwidget->SetOriginalRect(rect);
	else
		m_opwidget->SetRect(rect, TRUE, FALSE);
 	return OpStatus::OK;
}

void GenericQuickOpWidgetWrapper::SetOpWidget(OpWidget* opwidget)
{
	if (m_opwidget_event_forwarder)
	{
		OP_ASSERT(m_opwidget->GetListener() == m_opwidget_event_forwarder);
		m_opwidget->SetListener(0);
	}

	m_opwidget = opwidget;
	if (m_opwidget_event_forwarder)
	{
		m_opwidget->SetListener(m_opwidget_event_forwarder);
	}

	m_opwidget->UpdateSkinPadding();
	m_opwidget->UpdateSkinMargin();
}

void GenericQuickOpWidgetWrapper::SetParentOpWidget(OpWidget* widget)
{
	if (widget && !m_opwidget->GetParent())
		widget->AddChild(m_opwidget);
	else if (!widget && m_opwidget->GetParent())
		m_opwidget->Remove();
}

void GenericQuickOpWidgetWrapper::Show()
{
	m_opwidget->SetVisibility(TRUE);
	BroadcastContentsChanged();
}

void GenericQuickOpWidgetWrapper::Hide()
{
	m_opwidget->SetVisibility(FALSE);
	BroadcastContentsChanged();
}

BOOL GenericQuickOpWidgetWrapper::IsVisible()
{
	return m_opwidget->IsVisible();
}

void GenericQuickOpWidgetWrapper::SetEnabled(BOOL enabled)
{
	m_opwidget->SetEnabled(enabled);
}

OP_STATUS GenericQuickOpWidgetWrapper::AddOpWidgetListener(
		OpWidgetListener& listener)
{
	if (!m_opwidget)
	{
		return OpStatus::ERR;
	}

	if (!m_opwidget_event_forwarder)
	{
		m_opwidget_event_forwarder = OP_NEW(OpWidgetEventForwarder, ());
		RETURN_OOM_IF_NULL(m_opwidget_event_forwarder);
		m_opwidget->SetListener(m_opwidget_event_forwarder);
	}

	return m_opwidget_event_forwarder->AddListener(listener);
}

OP_STATUS GenericQuickOpWidgetWrapper::RemoveOpWidgetListener(
		OpWidgetListener& listener)
{
	return m_opwidget_event_forwarder
			? m_opwidget_event_forwarder->RemoveListener(listener)
			: OpStatus::ERR;
}

void GenericQuickOpWidgetWrapper::SetSkin(const OpStringC8& skin_element, const OpStringC8& fallback)
{
	m_opwidget->GetBorderSkin()->SetImage(skin_element.CStr(), fallback.CStr());
	m_opwidget->UpdateSkinPadding();
	m_opwidget->UpdateSkinMargin();
	m_opwidget->SetSkinned(TRUE);
}

unsigned GenericQuickOpWidgetWrapper::GetDefaultMinimumWidth()
{
	return GetOpWidgetPreferredWidth();
}

unsigned GenericQuickOpWidgetWrapper::GetDefaultMinimumHeight(unsigned width)
{
	return GetOpWidgetPreferredHeight();
}

unsigned GenericQuickOpWidgetWrapper::GetDefaultNominalWidth()
{
	return GetOpWidgetPreferredWidth();
}

unsigned GenericQuickOpWidgetWrapper::GetDefaultNominalHeight(unsigned width)
{
	return GetOpWidgetPreferredHeight();
}

unsigned GenericQuickOpWidgetWrapper::GetDefaultPreferredWidth()
{
	return GetOpWidgetPreferredWidth();
}

unsigned GenericQuickOpWidgetWrapper::GetDefaultPreferredHeight(unsigned width)
{
	return GetOpWidgetPreferredHeight();
}

void GenericQuickOpWidgetWrapper::GetDefaultMargins(WidgetSizes::Margins& margins)
{
	short left, right, top, bottom;
	m_opwidget->GetMargins(left, top, right, bottom);
	margins.left = left;
	margins.right = right;
	margins.top = top;
	margins.bottom = bottom;
}

OpRect GenericQuickOpWidgetWrapper::GetOpWidgetPreferredSize()
{
	OpRect rect;
	VisualDeviceHandler handler(m_opwidget);

	m_opwidget->GetRequiredSize(rect.width, rect.height);

	return rect;
}
