/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Erman Doser (ermand)
 */
#include "core/pch.h"

#include "adjunct/quick/controller/ScriptOptionsController.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick/managers/MessageConsoleManager.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"

void ScriptOptionsController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Script Options Dialog"));
	LEAVE_IF_ERROR(InitOptions());
}

OP_STATUS ScriptOptionsController::InitOptions()
{
	//Preference bindings for Checkboxes.
	RETURN_IF_ERROR(GetBinder()->Connect("Allow_resize_checkbox", *g_pcdisplay, PrefsCollectionDisplay::AllowScriptToResizeWindow));
	RETURN_IF_ERROR(GetBinder()->Connect("Allow_move_checkbox", *g_pcdisplay, PrefsCollectionDisplay::AllowScriptToMoveWindow));
	RETURN_IF_ERROR(GetBinder()->Connect("Allow_raise_checkbox", *g_pcdisplay, PrefsCollectionDisplay::AllowScriptToRaiseWindow));
	RETURN_IF_ERROR(GetBinder()->Connect("Allow_lower_checkbox", *g_pcdisplay, PrefsCollectionDisplay::AllowScriptToLowerWindow));
	RETURN_IF_ERROR(GetBinder()->Connect("Allow_status_checkbox", *g_pcdisplay, PrefsCollectionDisplay::AllowScriptToChangeStatus));
	RETURN_IF_ERROR(GetBinder()->Connect("Allow_clicks_checkbox", *g_pcdisplay, PrefsCollectionDisplay::AllowScriptToReceiveRightClicks));
	RETURN_IF_ERROR(GetBinder()->Connect("Allow_hide_address_checkbox", *g_pcdisplay, PrefsCollectionDisplay::AllowScriptToHideURL));

	RETURN_IF_ERROR(GetBinder()->Connect("Javascript_console_checkbox", m_open_js_console));
	if (g_message_console_manager)
		m_open_js_console.Set(g_message_console_manager->GetJavaScriptConsoleSetting() == TRUE);

	// Bind to property first, so that the prerefence value is the one that is
	// used.
	RETURN_IF_ERROR(GetBinder()->Connect("My_user_javascript_chooser", m_js_filepath));
	RETURN_IF_ERROR(GetBinder()->Connect("My_user_javascript_chooser", *g_pcjs, PrefsCollectionJS::UserJSFiles));

	return OpStatus::OK;
}

void ScriptOptionsController::OnOk()
{
	if (g_message_console_manager)
		g_message_console_manager->SetJavaScriptConsoleLogging(m_open_js_console.Get());

	TRAPD(result,
			g_pcjs->WriteIntegerL(PrefsCollectionJS::UserJSEnabled, m_js_filepath.Get().HasContent());
			g_pcjs->WriteIntegerL(PrefsCollectionJS::UserJSAlwaysLoad, m_js_filepath.Get().HasContent())
	);
}
