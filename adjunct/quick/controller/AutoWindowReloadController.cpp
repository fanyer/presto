/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author: Blazej Kazmierczak (bkazmierczak)
 *
 */
#include "core/pch.h"

#include "adjunct/quick/controller/AutoWindowReloadController.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/speeddial/DesktopSpeedDial.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/util/OpLineParser.h"
#include "modules/widgets/OpEdit.h"

// Let default be 10 minutes
#define RELOAD_INTERVAL_DEFAULT				600

AutoWindowReloadController::AutoWindowReloadController(OpWindowCommander* win_comm, const DesktopSpeedDial* sd):
					m_activewindow_commander(win_comm),
					m_sd(sd),
					m_verification_ok(false)
{
}

void AutoWindowReloadController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Auto Window Reload Dialog"));
	LEAVE_IF_ERROR(InitControls());
}
 

OP_STATUS AutoWindowReloadController::InitControls()
{
	int seconds = 0;
	if (m_activewindow_commander)
		seconds = WindowCommanderProxy::GetUserAutoReload(m_activewindow_commander)->GetOptSecUserSetting();
	else if (m_sd)
	{
		SpeedDialData::ReloadPolicy policy = m_sd->GetReloadPolicy();
		seconds = m_sd->GetReloadTimeout();
		if (policy != SpeedDialData::Reload_UserDefined)
			seconds = 0;
	}

	if (seconds <= 0 || seconds == SPEED_DIAL_RELOAD_INTERVAL_DEFAULT)
		seconds = RELOAD_INTERVAL_DEFAULT;

	int minutes = 0;
	if (seconds >= 60)
	{
		minutes = (seconds / 60);
		seconds -= (minutes*60);
	}
	const TypedObjectCollection* widgets = m_dialog->GetWidgetCollection();
	QuickDropDown* minutes_dropdown = widgets->Get<QuickDropDown>("Autoreload_minutes");
	if(!minutes_dropdown)
		return OpStatus::ERR;

	minutes_dropdown->GetOpWidget()->edit->SetOnChangeOnEnter(FALSE); // Always, so that we can verify input as the user types
	QuickDropDown* seconds_dropdown = widgets->Get<QuickDropDown>("Autoreload_seconds");
	if(!seconds_dropdown)
		return OpStatus::ERR;
	seconds_dropdown->GetOpWidget()->edit->SetOnChangeOnEnter(FALSE); // Always, so that we can verify input as the user types

	RETURN_IF_ERROR(GetBinder()->Connect("Reload_only_expired",m_reload_only_expired));

	OpString time;
	RETURN_IF_ERROR(time.AppendFormat("%i",minutes));	
	m_minutes.Set(time);

	time.Empty();
	RETURN_IF_ERROR(time.AppendFormat("%i",seconds));	
	m_seconds.Set(time);

	RETURN_IF_ERROR(GetBinder()->Connect(*seconds_dropdown, m_seconds));
	RETURN_IF_ERROR(m_seconds.Subscribe(MAKE_DELEGATE(*this, &AutoWindowReloadController::OnDropDownChanged)));

	RETURN_IF_ERROR(GetBinder()->Connect(*minutes_dropdown, m_minutes));
	RETURN_IF_ERROR(m_minutes.Subscribe(MAKE_DELEGATE(*this, &AutoWindowReloadController::OnDropDownChanged)));
	
	if (m_activewindow_commander)
	{
		m_reload_only_expired.Set(WindowCommanderProxy::GetUserAutoReload(m_activewindow_commander)->GetOptOnlyIfExpired() != 0);
	}
	else if (m_sd)
	{
		m_reload_only_expired.Set(m_sd->GetReloadOnlyIfExpired()?true:false);
	}
	m_verification_ok = VerifyInput();

	return OpStatus::OK;
}

BOOL AutoWindowReloadController::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_OK:
			return !m_verification_ok;
	}
	return OkCancelDialogContext::DisablesAction(action);
}

void AutoWindowReloadController::OnOk()
{
	int min = m_minutes.Get().IsEmpty() ? 0 : uni_atoi(m_minutes.Get().CStr());
	int sec = m_seconds.Get().IsEmpty() ? 0 : uni_atoi(m_seconds.Get().CStr());

	int seconds = ((min*60) + sec);

	if (seconds == 0)
		seconds = SPEED_DIAL_RELOAD_INTERVAL_DEFAULT;

	if (m_activewindow_commander)
		WindowCommanderProxy::GetUserAutoReload(m_activewindow_commander)->SetParams(TRUE, seconds, m_reload_only_expired.Get()?TRUE:FALSE);
	else if (m_sd)
	{
		DesktopSpeedDial new_entry;
		new_entry.Set(*m_sd);
		new_entry.SetReload(SpeedDialData::Reload_UserDefined, seconds, m_reload_only_expired.Get()?TRUE:FALSE);
		OpStatus::Ignore(g_speeddial_manager->ReplaceSpeedDial(m_sd, &new_entry));
	}
	return OkCancelDialogContext::OnOk();
}


bool AutoWindowReloadController::VerifyInput()
{
	UINT32 min = 0;
	if (m_minutes.Get().HasContent())
	{		
		uni_char *end = NULL;
		min = uni_strtoul(m_minutes.Get().DataPtr(), &end, 10);
		if (*end != '\0')
			return false;
	}

	UINT32 sec = 0;
	if (m_seconds.Get().HasContent())
	{
		uni_char *end = NULL;
		sec = uni_strtoul(m_seconds.Get().DataPtr(), &end, 10);
		if (*end != '\0')
			return false;
	}

	return min + sec > 0;
}


void AutoWindowReloadController::OnDropDownChanged(const OpStringC& text)
{
	m_verification_ok = VerifyInput();
	g_input_manager->UpdateAllInputStates();
}

/////////////////////////////////////////////////////////////////////////////
BOOL IsStandardTimeoutMenuItem(int timeout)
{
	// Read in the menu and use the values from there
	PrefsSection *section = g_application->GetMenuHandler()->GetMenuSection("Reload Menu");
	
	if (section)
	{
		OpWidgetImage widget_image;
		QuickMenu::MenuType menu_type;
		OpString	item_name, default_language_item_name;
		OpString8	sub_menu_name;
		INTPTR		sub_menu_value, menu_value;
		BOOL		ghost_item;

		for (const PrefsEntry *entry = section->Entries(); entry; entry = (const PrefsEntry *) entry->Suc())
		{
			OpLineParser line(entry->Key());
			
			if (QuickMenu::ParseMenuEntry(&line, item_name, default_language_item_name, sub_menu_name, sub_menu_value, menu_value, menu_type, ghost_item, widget_image))
			{
				if (menu_type == QuickMenu::MENU_ITEM || menu_type == QuickMenu::MENU_SUBMENU)
				{
					OpInputAction *input_action;
					if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString(entry->Value(), input_action, menu_value)))
					{
						BOOL is_match = FALSE;
						// Check only for the "Set automatic reload" action
						if (input_action->GetAction() == OpInputAction::ACTION_SET_AUTOMATIC_RELOAD)
						{
							int menu_timeout = input_action->GetActionData();
							// Check if the timeout matches
							is_match = (timeout == menu_timeout);
						}
						OpInputAction::DeleteInputActions(input_action);

						if (is_match)
							return TRUE;
					}
				}
			}
		}
	}
	return FALSE;
}
