/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *  GadgetPreferencesDialog.cpp
 *  Opera
 *
 *  Created by Bazyli Zygan on 09-11-12.
 *  Copyright 2011 Opera Software. All rights reserved.
 *
 */
#include "core/pch.h"
#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/Application.h"
#include "GadgetPreferencesDialog.h"

#include "adjunct/quick/dialogs/ProxyServersDialog.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/windows/DesktopGadget.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"


OP_STATUS GadgetPreferencesDialog::Init()
{
	RETURN_IF_ERROR(Dialog::Init(
			g_application->GetActiveDesktopWindow(TRUE)));

	OpWidget* wgt;

	wgt = GetWidgetByName("gadget_pref_notification_title");
	if (wgt)
		static_cast<OpLabel*>(wgt)->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);

	wgt = GetWidgetByName("gadget_pref_settings_title");
	if (wgt)
		static_cast<OpLabel*>(wgt)->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);

	wgt = GetWidgetByName("gadget_pref_notification_set");
	if (wgt)
		static_cast<OpButton*>(wgt)->SetValue(
						    (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationsForWidgets) ? 1 : 0));

	wgt = GetWidgetByName("gadget_pref_geolocation_section");
	if (wgt)
		static_cast<OpLabel*>(wgt)->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);

	if (!(g_pcgeolocation->GetIntegerPref(PrefsCollectionGeolocation::EnableGeolocation)))
	{
		SetWidgetEnabled("label_for_Allow_Geolocation",FALSE);
		SetWidgetEnabled("gadget_pref_geolocation_section",FALSE);
		SetWidgetEnabled("Allow_Geolocation",FALSE);
	}


#ifdef DOM_GEOLOCATION_SUPPORT
	OpString gadget_idetifier;

	OpGadget* op_gadget = g_gadget_manager->GetGadget(0);

	if (NULL == op_gadget)
		return OpStatus::ERR;

	m_gadget_identifier.Set(op_gadget->GetIdentifier());

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Allow_Geolocation");

	if (dropdown)
	{
		OpString item_name;
		g_languageManager->GetString(Str::D_GEOLOCATION_YES, item_name);
		dropdown->AddItem(item_name.CStr(), -1, NULL, YES);
		g_languageManager->GetString(Str::D_GEOLOCATION_NO, item_name);
		dropdown->AddItem(item_name.CStr(), -1, NULL, NO);
		g_languageManager->GetString(Str::D_GEOLOCATION_ASK, item_name);
		dropdown->AddItem(item_name.CStr(), -1, NULL, MAYBE);

		switch (m_permission_type)
		{
			case ALLOW:
			{
				dropdown->SelectItem(0, TRUE);
				break;
			}
			case DISALLOW:
			{
				dropdown->SelectItem(1, TRUE);
				break;
			}
			case ASK:
			{
				dropdown->SelectItem(2, TRUE);
				break;
			}
			default:
			{
				OP_ASSERT(!"Unknown permission type");
				break;
			}
		}

		dropdown->GetForegroundSkin()->SetImage("Geolocation");
	}
#endif //DOM_GEOLOCATION_SUPPORT

	return OpStatus::OK;
}

UINT32 GadgetPreferencesDialog::OnOk()
{
	OpButton* button = static_cast<OpButton*>(GetWidgetByName("gadget_pref_notification_set"));
	if (button)
	{
		TRAPD(rc,g_pcui->WriteIntegerL(PrefsCollectionUI::ShowNotificationsForWidgets, button->GetValue()));
	}

#ifdef DOM_GEOLOCATION_SUPPORT
	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Allow_Geolocation");
	if (dropdown)
	{
		DesktopWindow* window = g_application->GetActiveDesktopWindow();
		OpWindowCommander* comm = window->GetWindowCommander();
		BOOL3 allow = static_cast<BOOL3>(dropdown->GetItemUserData(dropdown->GetSelectedItem()));
		OpStatus::Ignore(comm->SetUserConsent(
					OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST,
					OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS,
					m_gadget_identifier.CStr(),
					allow));
	}
#endif //DOM_GEOLOCATION_SUPPORT

	return Dialog::OnOk();
}

BOOL GadgetPreferencesDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	
		case OpInputAction::ACTION_WIDGET_PREFERENCE_PANE:
		{
			return TRUE;
		}
			
		case OpInputAction::ACTION_SHOW_PROXY_SERVERS:
		{
			ProxyServersDialog* dialog = OP_NEW(ProxyServersDialog, ());
			if (dialog)
				dialog->Init(this);
			return TRUE;
		}

		case OpInputAction::ACTION_FORGET_WIDGET_PASSWORDS:
		{
			g_wand_manager->ClearAll();
			return TRUE;
		}
		case OpInputAction::ACTION_CLEAR_WIDGET_DATA:
		{
			if (PrivacyManager::GetInstance())
			{
				PrivacyManager::Flags flags;

				flags.Set(PrivacyManager::TEMPORARY_COOKIES, TRUE);
				flags.Set(PrivacyManager::DOCUMENTS_WITH_PASSWORD, TRUE);
				flags.Set(PrivacyManager::ALL_COOKIES, TRUE);
				flags.Set(PrivacyManager::GLOBAL_HISTORY, TRUE);
				flags.Set(PrivacyManager::TYPED_HISTORY,  TRUE);
				flags.Set(PrivacyManager::VISITED_LINKS,  TRUE);
				flags.Set(PrivacyManager::CACHE, TRUE);
				flags.Set(PrivacyManager::WAND_PASSWORDS_DONT_SYNC, TRUE);

				PrivacyManager::GetInstance()->ClearPrivateData(flags, TRUE);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_FORGET_WIDGET_PASSWORDS:
				{
					child_action->SetEnabled( (0 < g_wand_manager->GetWandPageCount()) );
					return TRUE;
				}
			}
			return FALSE;
		}					
	}
	return Dialog::OnInputAction(action);
}

#endif // WIDGET_RUNTIME_SUPPORT
