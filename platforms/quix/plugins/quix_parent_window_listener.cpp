/**
 * Copyright (C) 2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "platforms/quix/plugins/quix_parent_window_listener.h"

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/x11api/plugins/unix_oppluginadapter.h"

OP_STATUS PlugixParentWindowListener::Create(PlugixParentWindowListener** listener, UnixOpPluginAdapter* adapter)
{
	OP_ASSERT(adapter);

	*listener = OP_NEW(QuixParentWindowListener, (*adapter));
	RETURN_OOM_IF_NULL(*listener);

	return OpStatus::OK;
}

static DesktopWindow* GetPluginRealParentWindow(OpWindow* opwin)
{
	// We always have this windowing structure on desktop:
	// OpWindow (page) -> OpView -> OpWindow (DesktopWindow)
	MDE_OpView* view = static_cast<MDE_OpView*>(opwin->GetParentView());
	if (!view)
		return NULL;
	DesktopOpWindow* parent_win = static_cast<DesktopOpWindow*>(view->GetParentWindow());
	return parent_win ? parent_win->GetDesktopWindow() : NULL;
}

QuixParentWindowListener::~QuixParentWindowListener()
{
	if (m_parent_window)
		m_parent_window->RemoveListener(this);
}

OP_STATUS QuixParentWindowListener::SetParent(OpWindow* parent)
{
	if (m_parent_window)
		m_parent_window->RemoveListener(this);
	m_parent_window = parent ? GetPluginRealParentWindow(parent) : NULL;
	if (m_parent_window)
		RETURN_IF_ERROR(m_parent_window->AddListener(this));

	OnDesktopWindowParentChanged(m_parent_window);

	return OpStatus::OK;
}

void QuixParentWindowListener::OnDesktopWindowClosing(DesktopWindow*, BOOL)
{
	m_parent_window = NULL;
	m_adapter.OnParentChanged(0);
}

void QuixParentWindowListener::OnDesktopWindowParentChanged(DesktopWindow* desktop_window)
{
	X11OpWindow* window = desktop_window ? static_cast<X11OpWindow*>(desktop_window->GetOpWindow())->GetNativeWindow() : NULL;
	X11Types::Window handle = window && window->GetWidget() ? window->GetWidget()->GetWindowHandle() : 0;

	m_adapter.OnParentChanged(handle);
}
