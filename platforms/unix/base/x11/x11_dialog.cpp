/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "platforms/unix/base/x11/x11_dialog.h"
#include "platforms/unix/base/x11/x11_icon.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"

OP_STATUS X11Dialog::Init(X11Widget *parent, const char *name, int flags)
{
	RETURN_IF_ERROR(X11WindowWidget::Init(parent, name, m_modal ? flags | MODAL : flags));
	X11Icon::SetApplicationIcon( this );

	bool stack_modal  = false;
	if (GetOpWindow()->GetStyle() == OpWindow::STYLE_BLOCKING_DIALOG)
		stack_modal = true;

	PrepareForShowingDialog(stack_modal);

	return OpStatus::OK;
}

void X11Dialog::HandleCallBack(INTPTR ev)
{
	if (ev == X11WindowWidget::DELAYED_SHOW)
		Show();
	else
		X11WindowWidget::HandleCallBack(ev);
}

void X11Dialog::PrepareForShowingDialog(bool stack_modal)
{
	// We must always close an active popup window before showing a dialog. Activating
	// a dialog from a popup menu is not a problem (because we close the popup before
	// executing the action from the popup menu), but sometimes a dialog can be activated
	// by other means (like from a callback triggered by downloaded data). This fixes
	// bug DSK-314344

	if (X11Widget::GetPopupWidget())
		X11Widget::GetPopupWidget()->Close();

	// When initializing dialog from open extension overlay ("extension popup")
	// we want to ungrab it first.

	X11Widget *grabbed_widget = g_x11->GetWidgetList()->GetGrabbedWidget();
	if (grabbed_widget)
		grabbed_widget->Ungrab();


	if (stack_modal)
	{
		// If a dialog is created by clicking on a button it will be opened on mouse up.
		// A mouse capture is released on mouse up (in X11Widget) but only after core
		// has received the event and thus after a stack modal dialog has closed. That is
		// too late so we have to release capture here (example: font dialog from prefs)
		if (g_x11->GetWidgetList()->GetCapturedWidget())
			g_x11->GetWidgetList()->GetCapturedWidget()->ReleaseCapture();
	}
}
