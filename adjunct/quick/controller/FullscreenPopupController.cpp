/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/controller/FullscreenPopupController.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/Application.h"

FullscreenPopupController::FullscreenPopupController(OpWidget* anchor_widget)
	: PopupDialogContext(anchor_widget)
	, m_visible(false)
{
	m_timer.SetTimerListener(this);
}

void FullscreenPopupController::InitL()
{
	QuickOverlayDialog* overlay_dlg = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Fullscreen Popup", overlay_dlg));

	FullscreenPopupPlacer* placer = OP_NEW_L(FullscreenPopupPlacer, ());
	overlay_dlg->SetDialogPlacer(placer);
	overlay_dlg->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_FADE);
}

void FullscreenPopupController::Show()
{
	m_timer.Start(DELAY_BEFORE_OPENING);
}

void FullscreenPopupController::CancelDialog()
{
	m_timer.Stop();
	DialogContext::CancelDialog();
	// If hasn't been shown yet, 'this' won't be deleted automatically.
	if (!m_visible)
		OP_DELETE(this);
}

void FullscreenPopupController::OnTimeOut(OpTimer* timer)
{
	if (!m_visible)
	{
		::ShowDialog(this, g_global_ui_context, g_application->GetBrowserDesktopWindow());
		m_visible = true;
		m_timer.Start(DELAY_BEFORE_CLOSING);
	}
	else
	{
		CancelDialog();
		m_visible = false;
	}
}

BOOL FullscreenPopupController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LEAVE_FULLSCREEN:
			return TRUE;
	}
	return PopupDialogContext::CanHandleAction(action);
}

OP_STATUS FullscreenPopupController::HandleAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_LEAVE_FULLSCREEN)
	{
		g_application->GetBrowserDesktopWindow()->SetFullscreenMode(false,false);
		return OpStatus::OK;
	}

	return PopupDialogContext::HandleAction(action);
}

