/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl@opera.com)
 */

#include "core/pch.h"

#include "adjunct/quick/controller/PluginCrashController.h"

#include "adjunct/desktop_util/crash/logsender.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickRichTextLabel.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/viewers/plugins.h"

PluginCrashController::PluginCrashController(OpWidget* anchor_widget, const uni_char* url, LogSender& logsender)
  : PopupDialogContext(anchor_widget)
  , m_logsender(logsender)
{
	m_url.Set(url);
}

void PluginCrashController::InitL()
{
	QuickOverlayDialog* overlay_dialog = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Plugin Crash Dialog", overlay_dialog));

	if (GetAnchorWidget())
	{
		CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (*GetAnchorWidget()));
		overlay_dialog->SetDialogPlacer(placer);
		overlay_dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_CALLOUT);
	}

	// Get text with link for description label
	ANCHORD(OpString, description);
	LEAVE_IF_ERROR(I18n::Format(description, Str::D_PLUGIN_REPORT_ISSUE_PRIVACY_POLICY,
		UNI_L("<a href=\"opera:/action/Open url in new page, &quot;http://www.opera.com/privacy&quot;\">http://www.opera.com/privacy</a>")));
	LEAVE_IF_ERROR(SetWidgetText<QuickRichTextLabel>("report_issue_description", description));

	LEAVE_IF_ERROR(GetBinder()->Connect("last_visited", m_url));
	LEAVE_IF_ERROR(GetBinder()->Connect("email", *g_pcui, PrefsCollectionUI::UserEmail));
	LEAVE_IF_ERROR(GetBinder()->Connect("comments", m_comments));
}

BOOL PluginCrashController::CanHandleAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_CRASH_VIEW_REPORT)
		return TRUE;

	return PopupDialogContext::CanHandleAction(action);
}

OP_STATUS PluginCrashController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
			RETURN_IF_ERROR(m_logsender.SetEmail(g_pcui->GetStringPref(PrefsCollectionUI::UserEmail)));
			RETURN_IF_ERROR(m_logsender.SetComments(m_comments.Get()));
			RETURN_IF_ERROR(m_logsender.SetURL(m_url.Get()));
			RETURN_IF_ERROR(m_logsender.Send());
			break; // We want the default handling for ACTION_OK as well
		case OpInputAction::ACTION_CRASH_VIEW_REPORT:
			g_application->GoToPage(m_logsender.GetLogFile(), TRUE);
			return OpStatus::OK;
	}

	return PopupDialogContext::HandleAction(action);
}
