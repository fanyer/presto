/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/speeddial/SpeedDialGenericHandler.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/data_types/OpenURLSetting.h"
#include "adjunct/quick/dialogs/MidClickDialog.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"

#include "modules/inputmanager/inputaction.h"
#include "modules/url/url_man.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

SpeedDialGenericHandler::SpeedDialGenericHandler(const DesktopSpeedDial& entry)
	: m_entry(&entry)
{}


OP_STATUS SpeedDialGenericHandler::GetButtonInfo(GenericThumbnailContent::ButtonInfo& info) const
{
	info.m_name.Empty();
	RETURN_IF_ERROR(info.m_name.AppendFormat("Speed Dial %d", GetNumber()));

	info.m_accessibility_text.Empty();
	RETURN_IF_ERROR(info.m_accessibility_text.AppendFormat(
				UNI_L("Thumbnail %d"), GetNumber()));

	info.m_action = OpInputAction::ACTION_GOTO_SPEEDDIAL;
	info.m_action_data = g_speeddial_manager->GetSpeedDialActionData(m_entry);

	OpString tooltip_format;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CLICK_TO_GO_TO_SPEED_DIAL_ENTRY, tooltip_format));
	const URL url = urlManager->GetURL(m_entry->GetDisplayURL());
	OpString url_string;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username, url_string));
	RETURN_IF_ERROR(info.m_tooltip_text.AppendFormat(tooltip_format.CStr(), url_string.CStr()));

	return OpStatus::OK;
}


int SpeedDialGenericHandler::GetNumber() const
{
	return g_speeddial_manager->FindSpeedDial(m_entry) + 1;
}


OP_STATUS SpeedDialGenericHandler::GetCloseButtonInfo(GenericThumbnailContent::ButtonInfo& info) const
{
	if (!IsEntryManaged())
		// No sensible way to close a speed dial thumbnail that is not managed by
		// SpeedDialManager.
		return OpStatus::OK;

	RETURN_IF_ERROR(info.m_name.Set(WIDGET_NAME_THUMBNAIL_BUTTON_CLOSE));

	info.m_accessibility_text.Empty();
	RETURN_IF_ERROR(info.m_accessibility_text.AppendFormat(
				UNI_L("Thumbnail close %d"), GetNumber()));

	info.m_action = OpInputAction::ACTION_THUMB_CLOSE_PAGE;
	info.m_action_data = g_speeddial_manager->GetSpeedDialActionData(m_entry);
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_REMOVE_SPEED_DIAL_ENTRY, info.m_tooltip_text));

	return OpStatus::OK;
}


bool SpeedDialGenericHandler::IsEntryManaged() const
{
	return g_speeddial_manager->FindSpeedDial(m_entry) >= 0;
}

GenericThumbnailContent::MenuInfo SpeedDialGenericHandler::GetContextMenuInfo(
		const char* menu_name) const
{
	GenericThumbnailContent::MenuInfo info;

	if (!IsEntryManaged())
		// No sensible menu for a speed dial thumbnail that is not managed by
		// SpeedDialManager.
		return info;

	info.m_name = menu_name;
	info.m_context = g_speeddial_manager->GetSpeedDialActionData(m_entry);
	return info;
}


OP_STATUS SpeedDialGenericHandler::HandleMidClick()
{
	const INT32 key_state = g_op_system_info->GetShiftKeyState();
	if (key_state == SHIFTKEY_SHIFT)
	{
		MidClickDialog::Create(g_application->GetActiveDesktopWindow());
		return OpStatus::OK;
	}

	if (key_state != 0)
		return OpStatus::OK;

	const int action = g_pcui->GetIntegerPref(PrefsCollectionUI::CenterMouseButtonAction);
	if (0 <= action && action <= 4)
	{
		const URL url = urlManager->GetURL(m_entry->GetURL());
		OpString url_string;
		RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Username, url_string));

		OpenURLSetting setting;
		setting.m_address.Set(url_string);
		setting.m_src_commander = NULL;
		setting.m_document      = NULL;
		setting.m_new_window    = action == 3 || action == 4 ? YES : NO;
		setting.m_new_page      = action == 1 || action == 2 ? YES : NO;
		setting.m_in_background = action == 2 || action == 4 ? YES : NO;
		DocumentDesktopWindow* document_desktop_window = g_application->GetActiveDocumentDesktopWindow();
		if (document_desktop_window)
		 	setting.m_is_privacy_mode = document_desktop_window->PrivacyMode(); 

		if (!g_application->OpenURL(setting))
			return OpStatus::ERR;
	}
	 
	return OpStatus::OK;
}
