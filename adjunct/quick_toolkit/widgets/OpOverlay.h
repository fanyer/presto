/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    emil
 *
 */

#ifndef OP_OVERLAY_H
#define OP_OVERLAY_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/widgets/WidgetContainer.h"


/*******************************************************************************
 **
 ** OpOverlayLayoutWidget
 **
 ** Will act as a positioner and sizer for a DesktopWindow. The OpWidget itself
 ** won't be visible.
 **
 *******************************************************************************/

class OpOverlayLayoutWidget : public OpWidget, public DesktopWindowListener
{
public:
	OpOverlayLayoutWidget() : m_overlay_window(NULL) {}
	virtual ~OpOverlayLayoutWidget();

	OP_STATUS Init(DesktopWindow *overlay_window, OpWidget *parent);

	// Implementing OpWidget

	virtual void OnResize(INT32* new_w, INT32* new_h);
	virtual void OnLayout();
	virtual void OnDeleted();
	virtual void OnMove();

	// Implementing DesktopWindowListener

	virtual void	OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
	virtual void	OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height);
private:
	void RemoveListeners();
	DesktopWindow *m_overlay_window;
};

#endif // OP_OVERLAY_H
