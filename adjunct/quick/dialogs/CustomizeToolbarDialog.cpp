/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/dialogs/CustomizeToolbarDialog.h"

#include "adjunct/desktop_util/prefs/WidgetPrefs.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/widgets/OpDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpImageWidget.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "modules/dochand/win.h"
#include "modules/util/handy.h" // needed for ARRAY_SIZE
#include "modules/logdoc/logdoc_util.h" // for ReplaceEscapes()
#include "modules/locale/oplanguagemanager.h"
#include "modules/skin/OpSkinManager.h"

/***********************************************************************************
**
**	CustomizeToolbarDialog
**
***********************************************************************************/

CustomizeToolbarDialog::CustomizeToolbarDialog() :
	m_treeview(NULL),
	m_target_toolbar(NULL),
	m_defaults_toolbar(NULL),
	m_show_hidden_toolbars_while_customizing(FALSE),
	m_old_skin(NULL), // m_old_...
	m_skin_changed(FALSE),
	m_is_initiating(FALSE)
{
	g_application->SettingsChanged(SETTINGS_CUSTOMIZE_BEGIN);
}

CustomizeToolbarDialog::~CustomizeToolbarDialog()
{
	SetTargetToolbar(NULL);
	OP_DELETE(m_old_skin);
}

void CustomizeToolbarDialog::SetShowHiddenToolbarsWhileCustomizing(BOOL show)
{
	if (m_show_hidden_toolbars_while_customizing == show)
		return;

	m_show_hidden_toolbars_while_customizing = show;

	SetWidgetValue("Show_hidden_checkbox", show);
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowHiddenToolbarsWhileCustomizing, show));
	g_application->SettingsChanged(SETTINGS_CUSTOMIZE_UPDATE);
}

void CustomizeToolbarDialog::AddToolbar(const uni_char* title, const char* name, BOOL locked)
{
	OpToolbar* toolbar = (OpToolbar*) GetWidgetByName(name);

	if (!toolbar)
		return;

	toolbar->SetLocked(locked);
	toolbar->GetBorderSkin()->SetImage("Customize Toolbar Skin", "Toolbar Skin");

	m_treeview_model.AddItem(title, NULL, 0, -1, toolbar);
}

OpToolbar* CustomizeToolbarDialog::GoToCustomToolbar()
{
	OpToolbar* toolbar = (OpToolbar*) GetWidgetByName("Customize Toolbar Custom");

	if (!toolbar)
		return NULL;

	m_treeview->SetSelectedItem(m_treeview->GetItemByModelPos(m_treeview_model.FindItem(toolbar)));

	SetCurrentPage(CUSTOMIZE_PAGE_BUTTONS);

	return toolbar;
}

void CustomizeToolbarDialog::OnInit()
{
	m_is_initiating = TRUE;
	m_defaults_toolbar = (OpToolbar*) GetWidgetByName("Customize Toolbar Defaults");

	if (m_defaults_toolbar)
	{
		m_defaults_toolbar->SetMaster(TRUE);
	}

	m_treeview = (OpTreeView*) GetWidgetByName("Buttons_treeview");

	if (m_treeview)
	{
		OpString languagestring;

		g_languageManager->GetString(Str::SI_IDLABEL_CATEGORY, languagestring);
		m_treeview_model.SetColumnData(0, languagestring.CStr());

//		g_languageManager->GetString(Str::DI_IDM_START_PREF_BOX, languagestring);
//		AddToolbar(languagestring.CStr(), "Customize Toolbar Start");

		g_languageManager->GetString(Str::D_CUSTOMIZE_TOOLBAR_DEFAULTS, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Defaults");
		g_languageManager->GetString(Str::D_CUSTOMIZE_TOOLBAR_BROWSER, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Browser");
		g_languageManager->GetString(Str::D_CUSTOMIZE_TOOLBAR_BROWSER_VIEW, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Browser View");
		g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_EMAIL, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Mail");
		g_languageManager->GetString(Str::D_CUSTOMIZE_TOOLBAR_MAIL_VIEW, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Mail View");
		g_languageManager->GetString(Str::D_CUSTOMIZE_TOOLBAR_MAIL_COMPOSE_VIEW, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Mail Compose");
		g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_CHAT, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Chat");
//		g_languageManager->GetString(Str::D_HOTLISTTAB_TRANSFERS, languagestring);
//		AddToolbar(languagestring.CStr(), "Customize Toolbar Transfers");
		g_languageManager->GetString(Str::M_BROWSER_VIEW_MENU_PANELS, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Panels");
		g_languageManager->GetString(Str::SI_IDPREFS_SEARCH, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Search");
		g_languageManager->GetString(Str::S_STATUS, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Status");
		g_languageManager->GetString(Str::DI_PREFERENCES, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Preferences");
		g_languageManager->GetString(Str::D_CUSTOMIZE_TOOLBAR_SPACERS, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Spacers");
		g_languageManager->GetString(Str::D_CUSTOMIZE_TOOLBAR_BUTTONS, languagestring);
		AddToolbar(languagestring.CStr(), "Customize Toolbar Custom", FALSE);

		m_treeview->SetDeselectable(FALSE);
		m_treeview->SetTreeModel(&m_treeview_model);
		m_treeview->SetSelectedItem(0);
	}
	//
	// Skin related
	//
	OpDropDown* dropdown = (OpDropDown*)GetWidgetByName("Skin_zoom_dropdown");

	m_old_skin_scale = g_skin_manager->GetScale();

	if (dropdown)
	{
		static const int scale_values[] =
		{
			40, 50, 60, 70, 80, 90, 100, 110, 120, 140, 160, 200
		};

		OpString buffer;
		if (buffer.Reserve(30))
		{
			int pos;

			for (size_t i = 0; i < ARRAY_SIZE(scale_values); i++)
			{
				uni_ltoa(scale_values[i], buffer.CStr(), 10);

				uni_sprintf(buffer.CStr(), UNI_L("%d%%"), scale_values[i]);
				dropdown->AddItem(buffer.CStr(), -1, &pos, scale_values[i]);

				if (m_old_skin_scale == scale_values[i])
				{
					dropdown->SelectItem(i, TRUE);
				}
			}
		}
	}
	OpWidget *label = GetWidgetByName("label_for_Skin_zoom_dropdown");
	if(label)
	{
		label->SetJustify(JUSTIFY_CENTER, TRUE);
	}
	OpTreeView* skin_configurations = (OpTreeView*)GetWidgetByName("Skin_file_treeview");

	if (skin_configurations)
	{
		skin_configurations->SetDeselectable(FALSE);
		skin_configurations->SetTreeModel(g_skin_manager->GetTreeModel());
		skin_configurations->SetColumnVisibility(0, FALSE);
		skin_configurations->SetSelectedItem(g_skin_manager->GetSelectedSkin(), FALSE, FALSE);
	}

	BOOL can_delete_skin = g_skin_manager->IsUserInstalledSkin(g_skin_manager->GetSelectedSkin());
	SetWidgetEnabled("Delete_skinconfig", can_delete_skin);

	SetShowHiddenToolbarsWhileCustomizing(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowHiddenToolbarsWhileCustomizing));
	ShowWidget("Large_images_checkbox", g_skin_manager->GetOptionValue("Large Images", 1));

	m_old_skin = OP_NEW(OpFile, ());
	if (m_old_skin)
	{
		TRAPD(err, g_pcfiles->GetFileL(PrefsCollectionFiles::ButtonSet, *m_old_skin));
	}
	m_old_color_scheme_mode = g_pccore->GetIntegerPref(PrefsCollectionCore::ColorSchemeMode);
	m_old_color_scheme_color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_SKIN);
	m_old_special_effects = g_pccore->GetIntegerPref(PrefsCollectionCore::SpecialEffects);
	SetWidgetValue("Special_effects_checkbox", m_old_special_effects);
	
#ifdef WEBSERVER_SUPPORT
	ShowWidget("unite_panel", g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUnite));
#endif
	m_is_initiating = FALSE;
}

void CustomizeToolbarDialog::ApplyNewSelectedSkin(OpTreeView *skin_configurations)
{
	INT32 pos = skin_configurations->GetSelectedItemModelPos();

	OP_ASSERT(pos != -1);

	if(pos != -1)
	{
		g_skin_manager->SelectSkin(pos);
		m_skin_changed = TRUE;
	}
	BOOL can_delete = g_skin_manager->IsUserInstalledSkin(pos);
	SetWidgetEnabled("Delete_skinconfig", can_delete);
}

void CustomizeToolbarDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if(!down)
	{
		OpTreeView* skin_configurations = static_cast<OpTreeView*>(widget);

		if (widget->IsNamed("Skin_file_treeview"))
		{
			ApplyNewSelectedSkin(skin_configurations);
		}
	}
}

void CustomizeToolbarDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_treeview)
	{
		for (INT32 i = 0; i < m_treeview->GetItemCount(); i++)
		{
			OpToolbar* toolbar = m_treeview_model.GetItem(m_treeview->GetModelPos(i));
			BOOL visible = i == m_treeview->GetSelectedItemPos();

			toolbar->SetVisibility(visible);
			//Force relayout the visible toolbar, it might not be initialized yet because it
			//was invisible. (Normal usage of toolbars probably doesn't need this)
			if (visible)
				toolbar->Relayout();

			if (toolbar->GetMaster())
			{
				ShowWidget("Reset button", visible);
			}
		}
	}
	else if (widget->IsNamed("Show_hidden_checkbox"))
	{
		SetShowHiddenToolbarsWhileCustomizing(widget->GetValue());
	}
	else if (widget->IsNamed("Skin_zoom_dropdown") && !m_is_initiating)
	{
		OpDropDown* dropdown = (OpDropDown*) widget;

		INT32 scale = (INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());

		g_skin_manager->SetScale(scale);
		TRAPD(err, g_pccore->WriteIntegerL(PrefsCollectionCore::SkinScale, scale));
		g_application->SettingsChanged(SETTINGS_SKIN);
		m_skin_changed = TRUE;
	}
	else if (widget->IsNamed("Special_effects_checkbox") && !m_is_initiating)
	{
		WidgetPrefs::SetIntegerPref(GetWidgetByName("Special_effects_checkbox"), PrefsCollectionCore::SpecialEffects);
		g_application->SettingsChanged(SETTINGS_SKIN);
		m_skin_changed = TRUE;
	}
	else if(widget->IsNamed("Skin_file_treeview") && !changed_by_mouse)
	{
		OpTreeView* skin_configurations = static_cast<OpTreeView*>(widget);

		ApplyNewSelectedSkin(skin_configurations);
	}
	Dialog::OnChange(widget, changed_by_mouse);
}

void CustomizeToolbarDialog::SetTargetToolbar(OpToolbar* target_toolbar)
{
	if (m_target_toolbar == target_toolbar)
		return;

	if (target_toolbar)
	{
		if (target_toolbar->GetParentDesktopWindow() == this)
			return;

		if (!target_toolbar->GetSupportsReadAndWrite())
			return;
	}

	if (m_target_toolbar)
		m_target_toolbar->InvalidateAll();

	m_target_toolbar = target_toolbar;

	if (m_target_toolbar)
	{
		m_target_toolbar->InvalidateAll();
		SetWidgetEnabled("Enable_tab_thumbnails", m_target_toolbar->SupportsThumbnails());
	}
//	OpToolbar* toolbar = (OpToolbar*) GetWidgetByName("Customize Toolbar Defaults");

	if (m_defaults_toolbar)
	{
		if (m_target_toolbar && m_target_toolbar->IsStandardToolbar())
		{
			m_defaults_toolbar->SetName(m_target_toolbar->GetName());
			m_defaults_toolbar->SetWrapping(OpBar::WRAPPING_NEWLINE);
			m_defaults_toolbar->SetAlignment(OpBar::ALIGNMENT_OLD_VISIBLE);
		}
		else
		{
			while (m_defaults_toolbar->GetWidgetCount() > 0)
			{
				if(OpStatus::IsError(m_defaults_toolbar->RemoveWidget(0)))
					break;
			}
		}
	}

	g_input_manager->UpdateAllInputStates(FALSE);
}

UINT32 CustomizeToolbarDialog::OnOk()
{
	g_application->SettingsChanged(SETTINGS_CUSTOMIZE_END_OK);
	// we must sync now since "my buttons" toolbar in this dialog
	// might want to save its changed content before it is destroyed
	g_application->SyncSettings();

	OP_STATUS err;
	TRAP(err, g_setup_manager->CommitL());
	TRAP(err, g_prefsManager->CommitL());

	return 0;
}

void CustomizeToolbarDialog::OnCancel()
{
	if (m_skin_changed)
	{
		TRAPD(err, g_pccore->WriteIntegerL(PrefsCollectionCore::ColorSchemeMode, m_old_color_scheme_mode));
		TRAP(err, g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR_SKIN, m_old_color_scheme_color));
		g_skin_manager->SetColorSchemeMode((OpSkin::ColorSchemeMode) m_old_color_scheme_mode);
		g_skin_manager->SetColorSchemeColor(m_old_color_scheme_color);
		g_skin_manager->SetScale(m_old_skin_scale);
		TRAP(err, g_pccore->WriteIntegerL(PrefsCollectionCore::SkinScale, m_old_skin_scale));
		TRAP(err, g_pccore->WriteIntegerL(PrefsCollectionCore::SpecialEffects, m_old_special_effects));
		g_skin_manager->SelectSkinByFile(m_old_skin);
	}
	g_application->SettingsChanged(SETTINGS_CUSTOMIZE_END_CANCEL);
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void CustomizeToolbarDialog::OnSettingsChanged(DesktopSettings* settings)
{
	Dialog::OnSettingsChanged(settings);

	if (settings->IsChanged(SETTINGS_SKIN))
	{
		ShowWidget("Large_images_checkbox", g_skin_manager->GetOptionValue("Large Images", 1));
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL CustomizeToolbarDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SHOW_PANEL:
				case OpInputAction::ACTION_HIDE_PANEL:
					if (child_action->HasActionDataString())
					{
#ifdef M2_SUPPORT
						if ((uni_stricmp(child_action->GetActionDataString(), UNI_L("Mail")) == 0 && !g_application->HasMail()) ||
							(uni_stricmp(child_action->GetActionDataString(), UNI_L("Contacts")) == 0 && !g_application->HasMail() && !g_application->HasChat()) ||
							(uni_stricmp(child_action->GetActionDataString(), UNI_L("Chat")) == 0 && !g_application->HasChat()))
						{
							child_action->SetEnabled(FALSE);
							return TRUE;
						}
#endif
					}
					break;

				case OpInputAction::ACTION_ADD_PANEL:
				{
					// DSK-351304: adding a panel creates new bookmark - disable until bookmarks are loaded
					child_action->SetEnabled(g_desktop_bookmark_manager->GetBookmarkModel()->Loaded());
					return TRUE;
				}

				case OpInputAction::ACTION_DELETE_SKIN:
				{
					BOOL can_delete_skin = g_skin_manager->IsUserInstalledSkin(g_skin_manager->GetSelectedSkin());
					child_action->SetEnabled(can_delete_skin);
					return TRUE;
				}
				break;
			}
			break;
		}
		case OpInputAction::ACTION_FIND_MORE_SKINS:
		{
			// unwind the call stack so the dialog closes before we open the page
			OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE));
			if (!action)
				return FALSE;
			action->SetActionDataString(UNI_L("http://redir.opera.com/community/skins/"));

			OpInputAction async_action(OpInputAction::ACTION_DELAY);
			async_action.SetActionData(1);
			async_action.SetNextInputAction(action);

			g_input_manager->InvokeAction(&async_action);

			CloseDialog(FALSE);
			return TRUE;
		}
		case OpInputAction::ACTION_DELETE_SKIN:
		{
			OpTreeView* skin_configurations = (OpTreeView*)GetWidgetByName("Skin_file_treeview");
			if(skin_configurations)
			{
				INT32 pos = skin_configurations->GetSelectedItemModelPos();
				if(pos != -1)
				{
					if (g_skin_manager->IsUserInstalledSkin(pos))
					{
						g_skin_manager->DeleteSkin(pos);
						skin_configurations->SetSelectedItem(skin_configurations->GetItemByModelPos(g_skin_manager->GetSelectedSkin()));
					}
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SET_SKIN_COLORING:
		case OpInputAction::ACTION_DISABLE_SKIN_COLORING:
		case OpInputAction::ACTION_USE_SYSTEM_SKIN_COLORING:
		{
			m_skin_changed = TRUE;
			return FALSE;
		}
		
		case OpInputAction::ACTION_ADD_PANEL:
		{
			DesktopWindow* active_window = g_application->GetActiveDesktopWindow(FALSE);

			OpWindowCommander* wc = NULL;
			if (active_window)
				wc = active_window->GetWindowCommander();

			BookmarkItemData item_data;

			const URL url = WindowCommanderProxy::GetCurrentURL(wc);
			url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, item_data.url);

			if (wc && wc->GetCurrentTitle())
			{
				item_data.name.Set(wc->GetCurrentTitle());
			}

			if( item_data.name.IsEmpty() )
			{
				item_data.name.Set( item_data.url );
			}
			else
			{
				// Bug #177155 (remove quotes)
				ReplaceEscapes( item_data.name.CStr(), item_data.name.Length(), TRUE );
			}

			item_data.name.Strip();
			item_data.panel_position = 0; // add to panel

			g_desktop_bookmark_manager->NewBookmark(item_data, -1, this, TRUE);
		}
		return TRUE;

	}
	return Dialog::OnInputAction(action);
}

BOOL CustomizeToolbarDialog::OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context)
{
	// We are disabling context menu for straight DOCUMENT,
	// if other types are ored in, then we let OpBrowserView handle it
	if (!context.HasImage() && !context.HasLink() && !context.HasTextSelection())
		return TRUE;
	else
		return FALSE;
}
