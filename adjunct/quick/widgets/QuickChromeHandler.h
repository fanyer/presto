/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_CHROME_HANDLER_H
#define QUICK_CHROME_HANDLER_H
#ifdef MDE_OPWINDOW_CHROME_SUPPORT

#include "modules/widgets/OpButton.h"
#include "modules/libgogi/pi_impl/mde_chrome_handler.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

class OpWindow;
class WidgetContainer;
class OpBitmap;
class PagebarButton;
class OpChromeResizer;

class QuickChromeHandler : public MDEChromeHandler, public OpWidgetListener, public DesktopWindowListener
{
public:
	QuickChromeHandler() : m_widget_container(NULL), m_target_desktop_window(NULL) {}
	virtual ~QuickChromeHandler();

	virtual OP_STATUS InitChrome(OpWindow *chrome_window, OpWindow *target_window, int &chrome_sz_left, int &chrome_sz_top, int &chrome_sz_right, int &chrome_sz_bottom);
	virtual void OnResize(int new_w, int new_h);
	virtual void SetIconAndTitle(OpBitmap* icon, uni_char* title) {}

	// Implementing OpWidgetListener
	virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

	// DesktopWindowListener
	virtual void OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);
private:
	friend class OpChromeResizer;
	OpWindow *m_target_window;
	WidgetContainer *m_widget_container;
	PagebarButton *m_tab_button;
	OpChromeResizer *m_background;
	OpChromeResizer *m_top_resizer;
	int m_sz_tab, m_sz_left, m_sz_top, m_sz_right, m_sz_bottom;
	DesktopWindow *m_target_desktop_window;
};

#endif // MDE_OPWINDOW_CHROME_SUPPORT
#endif // QUICK_CHROME_HANDLER_H
