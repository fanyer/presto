/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

//Platform header files
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_pi/desktop_pi_module.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/quick/quick-version.h"
#include "adjunct/quick_toolkit/widgets/OpButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpRichTextLabel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"

//Core hearder files
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/pi/system/OpFolderLister.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/accessors/lnglight.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/util_module.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/WidgetContainer.h"

//Platform's windows header files
#include "platforms/windows/windows_ui/winshell.h"
#include "platforms/windows/installer/HandleInfo.h"
#include "platforms/windows/installer/InstallerWizard.h"
#include "platforms/windows/installer/ProcessModel.h"
#include "platforms/windows/utils/authorization.h"

#include <LMCons.h>

const InstallerWizard::WidgetToTranslateStrMapping InstallerWizard::m_widget_str [] = { 
	{"installer_options_lang_label", Str::D_NEW_PREFERENCES_LANGUAGE},
	{"installer_options_install_type_label", Str::D_INSTALLER_LABEL_INSTALL_TYPE},
	{"installer_options_install_path_label", Str::D_INSTALLER_LABEL_INSTALL_PATH},
	{"installer_options_change_path", Str::D_FEATURE_SETTINGS_CHANGE},
	{"installer_options_shortcut_label", Str::D_INSTALLER_LABEL_SHORTCUT},
	{"installer_options_shortcut_on_desktop", Str::D_INSTALLER_SHORTCUT_ON_DESKTOP},
	{"installer_options_shortcut_in_start_menu", Str::D_INSTALLER_SHORTCUT_IN_START_MENU},
	{"installer_options_shortcut_in_launchbar", Str::D_INSTALLER_SHORTCUT_IN_QUICK_LAUNCH_TB},
	{"installer_options_shortcut_in_taskbar", Str::D_INSTALLER_PIN_TO_TASKBAR},
	{"installer_options_defaults_label", Str::D_INSTALLER_LABEL_DEFAULTS},
	{"installer_options_make_defaultbrowser", Str::D_INSTALLER_DEFAULT_BROWSER},
	{"installer_options_yandex_search_engine", Str::D_INSTALLER_YANDEX_SEARCH},
	{"installer_upgrade_info_message_0", Str::D_INSTALLER_UPGRADE_MESSAGE},
	{"installer_upgrade_info_message_1", Str::D_INSTALLER_UPGRADE_MESSAGE},
	{"install_help_tooltip", Str::D_INSTALLER_OPTIONS_HELP},
};

const OpStringC8 InstallerWizard::m_remove_profile_widget_names[REMOVE_PROFILE_WIDGETS_COUNT] =
{
	"remove_generated",
	"remove_passwords",
	"remove_customizations",
	"remove_mail",
	"remove_bookmarks",
	"remove_unite_apps",
};

InstallerWizard::InstallerWizard(OperaInstaller* opera_installer)
: m_opera_installer(opera_installer)
, m_process_model(NULL)
, m_file_chooser(NULL)
, m_progressbar(NULL)
, m_selected_install_type_index(INSTALLATION_TYPE_JUST_ME)
, m_forbid_closing(FALSE)
, m_initiated_process_termination(FALSE)
, m_remove_full_profile_old_value(TRUE)
{
	m_settings = m_opera_installer->GetSettings();

	if (m_settings.all_users)
		m_selected_install_type_index = INSTALLATION_TYPE_ALL_USERS;
	else if (m_settings.copy_only)
		m_selected_install_type_index = INSTALLATION_TYPE_PORTABLE_DEVICE;
	else
		m_selected_install_type_index = INSTALLATION_TYPE_JUST_ME;

	for (UINT i = 0; i < REMOVE_PROFILE_WIDGETS_COUNT; i++)
	{
		m_remove_profile_old_value[i] = TRUE;
	}

	switch (g_desktop_product->GetProductType())
	{
		case PRODUCT_TYPE_OPERA_NEXT:
		case PRODUCT_TYPE_OPERA_LABS:
			m_dialog_skin = "Installer Wizard Next Dialog Skin";
			m_graphic_skin = "Installer Opera Next Graphic Skin";
			break;
		default:
			m_dialog_skin = "Installer Wizard Dialog Skin";
			m_graphic_skin = "Installer Opera Graphic Skin";
	}

	OpFile yandex_file;
	yandex_file.Construct(UNI_L("Yandex.exe"), OPFILE_RESOURCES_FOLDER);
	OpStatus::Ignore(yandex_file.Exists(m_show_yandex_checkbox));
}

InstallerWizard::~InstallerWizard()
{
	OP_DELETE(m_file_chooser);

	OpDropDown *dropdown = static_cast<OpDropDown*>(GetWidgetByName("installer_options_lang"));
	if (dropdown)
	{
		INT32 count = dropdown->CountItems();
		for (INT32 i = 0 ; i < count ; i++)
		{
			OP_DELETE(reinterpret_cast<LocaleInfo*>(dropdown->GetItemUserData(i)));
		}
	}

	//Free allocated process list
	OP_DELETE(m_process_model);

	//Unregister message
	g_main_message_handler->UnsetCallBack(this,MSG_TERMINATE_APPS_LOCKING_INSTALLATION);
	g_main_message_handler->RemoveDelayedMessage(MSG_TERMINATE_APPS_LOCKING_INSTALLATION, 0,0);

	g_main_message_handler->UnsetCallBack(this,MSG_FILE_HANDLE_NO_LONGER_LOCKED);
	g_main_message_handler->RemoveDelayedMessage(MSG_FILE_HANDLE_NO_LONGER_LOCKED, 0,0);
}

OP_STATUS InstallerWizard::Init(InstallerPage page /*= INSTALLER_DLG_STARTUP*/)
{
	//Always start (un)installation setup with STANDARD_SKIN_FILE
	OP_STATUS skin_err = SelectDefaultSkin();

	//Register listener for OnDesktopWindowIsCloseAllowed() method
	RETURN_IF_ERROR(AddListener(this));

	//Initialize Dialog base first
	RETURN_IF_ERROR(Dialog::Init(NULL, page));

	if (OpStatus::IsError(skin_err) && m_opera_installer->GetOperation() == OperaInstaller::OpUninstall && page == INSTALLER_DLG_UNINSTALL)
	{
		Show(FALSE);

		OpString format;
		OpString dialog_title;
		OpString message;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_TITLE, format));

		RETURN_IF_ERROR(dialog_title.AppendFormat(format, VER_NUM_STR_UNI));

		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_UNINSTALL_WARNING, message));
				
		if (::MessageBox(NULL, message.CStr(), dialog_title.CStr(), MB_OKCANCEL) == IDOK)
			m_opera_installer->Run();
		else
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

void InstallerWizard::OnInit()
{
	//Apply translated string to dialog elements
	RETURN_VOID_IF_ERROR(TranslateString());

	//Fix a standard size for buttons belonging to buttonset. 	
	OpButtonStrip* btn_strip = GetButtonStrip();
	if (btn_strip)
	{
		btn_strip->SetDefaultButton(BTN_ID_ACCEPT_INSTALL, 1.5);
		btn_strip->SetDynamicSpaceAfter(BTN_ID_ACCEPT_INSTALL);
		btn_strip->SetButtonSize(120, 25, TRUE);
	}

	//Get UAC shield icon
	static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetImage(DesktopOpSystemInfo::PLATFORM_IMAGE_SHIELD, m_uac_shield); //Ignore returned error!
}

BOOL InstallerWizard::OnInputAction(OpInputAction* action)
{
	UINT page = GetCurrentPage();

	switch(action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_INITIATE_OPERA_INSTALLATION:
				{
					if (page == INSTALLER_DLG_HANDLE_LOCKED)
					{
						if (m_initiated_process_termination)
							child_action->SetEnabled(FALSE);

						return TRUE;
					}
					else if (page == INSTALLER_DLG_OPTIONS || page == INSTALLER_DLG_STARTUP || page == INSTALLER_DLG_TERM_OF_SERVICE)
					{
						OpString installation_path;
						GetInstallerPath(installation_path);

						if (installation_path.IsEmpty())
							child_action->SetEnabled(FALSE);

						return TRUE;
					}
				}
				break;
				case OpInputAction::ACTION_FORWARD:
				{
					if (page == INSTALLER_DLG_UNINSTALL)
					{
						child_action->SetEnabled(GetWidgetValue("remove_profile_options"));
						return TRUE;
					}
				}
				break;
				case OpInputAction::ACTION_CANCEL:
				{
					if (page == INSTALLER_DLG_PROGRESS_INFO)
					{
						child_action->SetEnabled(!m_forbid_closing);
						return TRUE;
					}
				}
				break;
			}
			return FALSE;
		} // OpInputAction::ACTION_GET_ACTION_STATE

		case OpInputAction::ACTION_BACK:
		case OpInputAction::ACTION_FORWARD:
		{
			//Take a look into the issues DSK-314976 and DSK-314978
			if (action->GetActionMethod() != OpInputAction::METHOD_OTHER)
				return TRUE;

			//Pass the action to the base class first.
			Dialog::OnInputAction(action);

			return TRUE;
		}

		case OpInputAction::ACTION_CHANGE:
		{
			if (action->GetActionData())
			{
				//User pressed enter key
				OpString install_path;
				if (OpStatus::IsSuccess(reinterpret_cast<OpEdit*>(action->GetActionData())->GetText(install_path)))
					SetInstallFolderAndUpdateOptions(install_path);
			}
			else
			{
				LaunchFolderExplorer();
			}

			return TRUE;
		}

		case OpInputAction::ACTION_INITIATE_OPERA_INSTALLATION:
		{
			if (page == INSTALLER_DLG_HANDLE_LOCKED)
			{
				if (m_process_model && m_process_model->GetCount() > 0)
				{
					m_initiated_process_termination = TRUE;
					
					EnableButton(BTN_ID_ACCEPT_INSTALL, FALSE);

					ShowSpinnginWaitCursor(TRUE);
					OnRelayout();  //Relayout the dialog content is needed here to showup the waiting progressbar
												
					g_main_message_handler->PostMessage(MSG_TERMINATE_APPS_LOCKING_INSTALLATION, (MH_PARAM_1)this, (MH_PARAM_2)0);
				}
			}
			else
			{
				static BOOL s_prevent_multiple_run;
				if (s_prevent_multiple_run == FALSE)
				{
					s_prevent_multiple_run = TRUE;
					
					if (page != INSTALLER_DLG_UNINSTALL && page != INSTALLER_DLG_REMOVE_PROFILE)
					{
						//Enforce user settings
						OpString locale_path, locale_code;
						if (OpStatus::IsSuccess(GetCurrentLanguageFile(locale_path, locale_code)))
						{
							m_opera_installer->SetInstallLanguage(locale_code); //TODO: Error handling
						}

						//Always update the status of all shortcuts when running from the wizard
						m_settings.update_desktop_shortcut = TRUE;
						m_settings.update_quick_launch_shortcut = TRUE;
						m_settings.update_start_menu_shortcut = TRUE;
						m_settings.update_pinned_shortcut = TRUE;

						m_opera_installer->SetSettings(m_settings);

						OpWidget* widget = GetWidgetByName("installer_options_yandex_search_engine");
						m_opera_installer->RunYandexScript(widget->GetValue());
					}
					else
					{
						if (GetWidgetValue("remove_full_profile"))
							m_opera_installer->SetDeleteProfile(OperaInstaller::FULL_PROFILE);
						else
						{
							UINT delete_profile_level = OperaInstaller::KEEP_PROFILE;

							if (GetWidgetValue("remove_generated"))
								delete_profile_level |= OperaInstaller::REMOVE_GENERATED;
							if (GetWidgetValue("remove_passwords"))
								delete_profile_level |= OperaInstaller::REMOVE_PASSWORDS;
							if (GetWidgetValue("remove_customizations"))
								delete_profile_level |= OperaInstaller::REMOVE_CUSTOMIZATION;
							if (GetWidgetValue("remove_mail"))
								delete_profile_level |= OperaInstaller::REMOVE_EMAIL;
							if (GetWidgetValue("remove_bookmarks"))
								delete_profile_level |= OperaInstaller::REMOVE_BOOKMARKS;
							if (GetWidgetValue("remove_unite_apps"))
								delete_profile_level |= OperaInstaller::REMOVE_UNITE_APPS;

							m_opera_installer->SetDeleteProfile(delete_profile_level);
						}
					}

					//Initiate (un)installation
					if (m_opera_installer->Run() == OpStatus::ERR)
						Close();

					s_prevent_multiple_run = FALSE;
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_CANCEL:
		{
			if (OpStatus::IsSuccess((HandleActionCancel())))
				return TRUE;

			break;
		}
	}

	return Dialog::OnInputAction(action);
}

void InstallerWizard::OnLayout()
{
	GetButtonStrip()->PlaceButtons();
}

void InstallerWizard::OnChangeWhenLostFocus(OpWidget *widget)
{
	OpString installation_path;
	if (widget->IsNamed("installer_options_install_path") && OpStatus::IsSuccess(widget->GetText(installation_path)))
	{
		//Passing TRUE in second argument will result in applying typed path to backend system.
		SetInstallFolderAndUpdateOptions(installation_path);
	}
}

void InstallerWizard::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("installer_options_lang"))
	{
		INT32 index = static_cast<OpDropDown*>(widget)->GetSelectedItem();
		if (index != -1)
		{
			LocaleInfo *selected_lang_info = reinterpret_cast<LocaleInfo*>(static_cast<OpDropDown*>(widget)->GetItemUserData(index));
			if (selected_lang_info)
			{
				ActivateNewLanguage(selected_lang_info);
			}
		}
	}

	else if (widget->IsNamed("installer_options_install_type"))
	{
		INT32 index = static_cast<OpDropDown*>(widget)->GetSelectedItem();
		if (index == -1)
			return; 
		
		m_selected_install_type_index = static_cast<INT32>(static_cast<OpDropDown*>(widget)->GetItemUserData(index));
		
		m_settings.all_users = m_selected_install_type_index == INSTALLATION_TYPE_ALL_USERS ? TRUE : FALSE;
		
		m_settings.single_profile = m_settings.copy_only = m_selected_install_type_index == INSTALLATION_TYPE_PORTABLE_DEVICE ? TRUE : FALSE;
		
		//Based on current selected installer options, fetch updated settings from backend system.
		SetAndFetchUpdatedInstallerSettings(m_settings);

		//Disable(greyout) installation options when install type is portable type(standalone)
		EnableShortcutsOptions(TRUE, !m_settings.copy_only);
		EnableDefaultBrowserOption(TRUE, !m_settings.copy_only);

		//Update or show UAC shield icon
		ShowShieldIconIfNeeded();
	}

	else if (widget->IsNamed("installer_options_install_path"))
	{
		OpString installation_path;
		if (OpStatus::IsSuccess(widget->GetText(installation_path)))
		{
			//Windows API named GetDriveType is used here to know type of drive.
			UINT drv_type= GetDriveType(installation_path.CStr());

			//Intention here is to prevent slow down of manual typing when input points to network path.
			if (drv_type == DRIVE_FIXED || drv_type == DRIVE_RAMDISK)
				SetInstallFolderAndUpdateOptions(installation_path);

			//(Dis/e)nable buttons
			EnableButton(BTN_ID_ACCEPT_INSTALL, installation_path.HasContent());
		}
	}

	else if (widget->IsNamed("installer_options_shortcut_on_desktop"))
	{
		m_settings.desktop_shortcut = widget->GetValue();
		SetAndFetchUpdatedInstallerSettings(m_settings);
	}

	else if (widget->IsNamed("installer_options_shortcut_in_start_menu"))
	{
		m_settings.start_menu_shortcut = widget->GetValue();
		SetAndFetchUpdatedInstallerSettings(m_settings);
	}

	else if (widget->IsNamed("installer_options_shortcut_in_launchbar"))
	{
		m_settings.quick_launch_shortcut = widget->GetValue();
		SetAndFetchUpdatedInstallerSettings(m_settings);
	}

	else if (widget->IsNamed("installer_options_shortcut_in_taskbar"))
	{
		m_settings.pinned_shortcut = widget->GetValue();
		SetAndFetchUpdatedInstallerSettings(m_settings);
	}

	else if (widget->IsNamed("installer_options_make_defaultbrowser"))
	{
		m_settings.set_default_browser = widget->GetValue();
		SetAndFetchUpdatedInstallerSettings(m_settings);
	}

	else if (widget->IsNamed("remove_profile_options"))
	{
		OpButtonStrip* btn_strip = GetButtonStrip();
		btn_strip->EnableButton(BTN_ID_OPTIONS, widget->GetValue());


		if (widget->GetValue())
		{
			for (UINT i = 0; i < REMOVE_PROFILE_WIDGETS_COUNT; i++)
				SetWidgetValue(m_remove_profile_widget_names[i], m_remove_profile_old_value[i]);

			SetWidgetValue("remove_full_profile", m_remove_full_profile_old_value);
		}
		else
		{
			m_remove_full_profile_old_value = GetWidgetValue("remove_full_profile");
			SetWidgetValue("remove_full_profile", 0);

			for (UINT i = 0; i < REMOVE_PROFILE_WIDGETS_COUNT; i++)
			{
				m_remove_profile_old_value[i] = GetWidgetValue(m_remove_profile_widget_names[i]);
				SetWidgetValue(m_remove_profile_widget_names[i], 0);
			}
		}
	}
	else if (widget->IsNamed("remove_full_profile"))
	{
		OpWidget* profile_widget;
		if (!widget->GetValue())
		{
			for (UINT i = 0; i < REMOVE_PROFILE_WIDGETS_COUNT; i++)
			{
				if ((profile_widget = GetWidgetByName(m_remove_profile_widget_names[i])) != NULL)
				{
					profile_widget->SetEnabled(TRUE);
					profile_widget->SetValue(m_remove_profile_old_value[i]);
				}
			}
		}
		else
		{
			for (UINT i = 0; i < REMOVE_PROFILE_WIDGETS_COUNT; i++)
			{
				if ((profile_widget = GetWidgetByName(m_remove_profile_widget_names[i])) != NULL)
				{
					profile_widget->SetEnabled(FALSE);
					m_remove_profile_old_value[i] = profile_widget->GetValue();
					profile_widget->SetValue(1);
				}
			}
		}
	}
}

void InstallerWizard::OnClick(OpWidget *widget, UINT32 id/* = 0*/)
{
	OP_ASSERT(g_input_manager);

	OpWidget* helptooltip = GetWidgetByName("install_help_tooltip");
	
	if (!helptooltip)
		return;

	//Hide HelpTooptip widget on mouse click over the OpGroup(dialog box) and its child widgets 
	if (!widget->IsNamed("installers_options_image_help") && helptooltip->IsVisible())
	{
		helptooltip->SetVisibility(FALSE);
	}

	if (widget->IsNamed("installers_options_image_help"))
	{
		helptooltip->SetVisibility(!helptooltip->IsVisible());
		helptooltip->SetFocus(FOCUS_REASON_MOUSE);
	}
}

BOOL InstallerWizard::OnContextMenu(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	OP_ASSERT(g_application || g_application->GetMenuHandler());
	switch(widget->GetType())
	{
		case OpTypedObject::WIDGET_TYPE_EDIT:
		{
			g_application->GetMenuHandler()->ShowPopupMenu("Installer Edit Folderchooser Popup Menu", PopupPlacement::AnchorAtCursor());
			return TRUE;
		}
		break;

		case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT: //Terms of Service is Multiline Readonly Editbox
		{
			g_application->GetMenuHandler()->ShowPopupMenu("Installer Readonly Edit Widget Popup Menu", PopupPlacement::AnchorAtCursor());
			return TRUE;
		}
		break;
	}

	return FALSE;
}

void InstallerWizard::OnInitPage(INT32 page_number, BOOL first_time)
{
	//Set skin named 'Installer Wizard Dialog' which removes Opera 'O' logo/image from the dialog
	SetSkin("Installer Wizard Dialog"); 

	switch(page_number)
	{
		case INSTALLER_DLG_STARTUP:			InitializeStartUpDlgGroup(first_time); break;
		case INSTALLER_DLG_OPTIONS:			InitializeOptionsDlgGroup(first_time); break;
		case INSTALLER_DLG_PROGRESS_INFO:	InitializeProgressInfoDlgGroup(first_time); break;
		case INSTALLER_DLG_HANDLE_LOCKED:	InitializeHandleLockedDlgGroup(first_time);	break;
		case INSTALLER_DLG_UNINSTALL:		InitializeUnintstallDlgGroup(first_time); break;			
		case INSTALLER_DLG_REMOVE_PROFILE:	InitializeRemoveProfileDlgGroup(first_time); break;			
		case INSTALLER_DLG_TERM_OF_SERVICE:	InitializeToSDlgGroup(first_time); break;
		default:
		{
			OP_ASSERT("!This page does not exist. Fix the code, or the dialog in dialog.ini.");
		}
	}
}

void InstallerWizard::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (result.files.GetCount())
	{
		m_opera_installer->SetInstallFolder(result.active_directory);

		/*
		Set default installation path.
		By setting an installation path to an editbox widget, installer_options_install_path, an OnChange() event is generated 
		which then makes a call to SetInstallFolderAndUpdateOptions(); indirectly making a call to said method.
		*/		
		OpWidget *widget = GetWidgetByName("installer_options_install_path");
		if (widget)
			widget->SetText(result.active_directory);

		FocusButton(BTN_ID_ACCEPT_INSTALL);
	}
}

BOOL InstallerWizard::OnDesktopWindowIsCloseAllowed(DesktopWindow* desktop_window)
{
	if (desktop_window == this && IsClosing() == FALSE)
	{
		if (OpStatus::IsSuccess((HandleActionCancel())))
			return FALSE;
	}

	return TRUE;
}

void InstallerWizard::OnOk(Dialog* dialog, UINT32 result)
{
	m_opera_installer->WasCanceled(TRUE);
	CloseDialog(FALSE);
}

void InstallerWizard::OnCancel(Dialog* dialog)
{
	if (m_opera_installer->IsRunning())
		m_opera_installer->Pause(FALSE); //Resume installation setup
}

void InstallerWizard::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	DesktopWindow::HandleCallback(msg, par1, par2);

	if (msg == MSG_TERMINATE_APPS_LOCKING_INSTALLATION)
	{
		if (m_process_model)
		{
			ProcessItem* pi;

			for(int i = 0; i < m_process_model->GetCount(); i++)
			{
				pi = m_process_model->GetItemByIndex(i);
				
				if (pi)
					pi->TerminateProcessGracefully();
			}
		}
	}
	else if (msg == MSG_FILE_HANDLE_NO_LONGER_LOCKED)
	{
		if (GetCurrentPage() == INSTALLER_DLG_HANDLE_LOCKED)
		{
		    g_main_message_handler->UnsetCallBack(this,MSG_FILE_HANDLE_NO_LONGER_LOCKED);
			g_main_message_handler->RemoveDelayedMessage(MSG_FILE_HANDLE_NO_LONGER_LOCKED, 0,0);
			Close();
		}
	}
}

void InstallerWizard::ShowSpinnginWaitCursor(BOOL show_progressbar)
{
	OpProgressBar *widget_pb = static_cast<OpProgressBar *>(GetWidgetByName("progressbar_closing_apps"));

	if (widget_pb)
	{
		RETURN_VOID_IF_ERROR(widget_pb->SetSpinnerImage("Thumbnail Reload Image"));
		widget_pb->SetType(OpProgressBar::Spinner);
		widget_pb->SetVisibility(show_progressbar);
		widget_pb->SetIgnoresMouse(TRUE);
	}
	
	OpLabel *widget_label = static_cast<OpLabel *>(GetWidgetByName("progressbar_closing_app_text"));

	if (widget_label)
		widget_label->SetVisibility(show_progressbar);
}

void InstallerWizard::SetAndFetchUpdatedInstallerSettings(OperaInstaller::Settings& settings)
{
	m_opera_installer->SetSettings(settings);
	settings = m_opera_installer->GetSettings();
}

OP_STATUS InstallerWizard::GetInstallerPath(OpString &installation_path)
{
	OpWidget *widget = GetWidgetByName("installer_options_install_path");

	if (widget)
		RETURN_IF_ERROR(widget->GetText(installation_path));

	return OpStatus::OK;
}

void InstallerWizard::SetProgressSteps(OpFileLength steps)
{
	if (m_progressbar)
	{
		m_progressbar->SetType(OpProgressBar::Normal);
		m_progressbar->SetProgress(0, steps);
	}
}

void InstallerWizard::IncProgress()
{
	if (m_progressbar)
	{
		OpFileLength steps = m_progressbar->GetCurrentStep();
		m_progressbar->SetProgress(++steps, m_progressbar->GetTotalSteps());
	}
}

void InstallerWizard::SetInfo(uni_char* info_text)
{
	OpLabel *label = static_cast<OpLabel*>(GetWidgetByName("progress_status_text"));

	if (label)
		label->SetText(info_text);
}

void InstallerWizard::ForbidClosing(BOOL forbid_closing)
{
	OpButtonStrip* button_strip = GetButtonStrip();
	
	if (button_strip)
		button_strip->EnableButton(BTN_ID_CANCEL, !forbid_closing);

	m_forbid_closing = forbid_closing;
}

OP_STATUS InstallerWizard::TranslateString()
{
	//Replace default buttons text, in the botton-strip, text with the custom ones.
	OpString str;
	Str::LocaleString str_id = Str::NOT_A_STRING;

	if (IsAnUpgrade())
		str_id = Str::D_INSTALLER_BTN_ACCEPT_TOS_UPGRADE;
	else
		str_id = Str::D_INSTALLER_BTN_ACCEPT_TOS_INSTALL;

	RETURN_IF_ERROR(g_languageManager->GetString(str_id, str));
	SetButtonText(BTN_ID_ACCEPT_INSTALL, str);

	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_OPTIONS, str));
	SetButtonText(BTN_ID_OPTIONS, str);

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_ID_CANCEL, str));
	SetButtonText(BTN_ID_CANCEL, str);

	//Translate various installation types
	RETURN_IF_ERROR(PopulateDropdownAndTranslateInstallationTypes());

	//Set strings for the dialog elements
	SetWidgetText("opera_version_info_startup_grp",   VER_NUM_STR_UNI); 
	SetWidgetText("opera_version_info_pb_grp", VER_NUM_STR_UNI);
	SetWidgetText("opera_version_info_uninstall_grp", VER_NUM_STR_UNI);
	
	//Set strings for the dialog elements.
	INT32 limit = ARRAY_SIZE(m_widget_str); 
	for (INT32 index = 0 ; index < limit; index++)
	{
		SetWidgetText(m_widget_str[index].widget, Str::LocaleString(m_widget_str[index].str_code)); 
	}

	SetWindowTitle();

	return OpStatus::OK;
}

OP_STATUS InstallerWizard::PopulateDropdownAndTranslateInstallationTypes()
{
	OpDropDown * dropdown = static_cast<OpDropDown*>(GetWidgetByName("installer_options_install_type"));
	if (dropdown)
	{
		dropdown->Clear();
		dropdown->SetListener(this);
		
		OpString str;

		//All users [Computer name]
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_LABEL_INSTALL_TYPE_ALL_USERS, str));
		RETURN_IF_ERROR(dropdown->AddItem(str.CStr(), -1, NULL, INSTALLATION_TYPE_ALL_USERS));
		
		//Current users [User name]
		TCHAR  infoBuf[UNLEN + 1];
		DWORD  bufCharCount = UNLEN + 1;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_LABEL_INSTALL_TYPE_ME, str));
		if (GetUserName( infoBuf, &bufCharCount ))
		{
			RETURN_IF_ERROR(str.AppendFormat(UNI_L(" (%s)"), infoBuf));	
		}
		RETURN_IF_ERROR(dropdown->AddItem(str.CStr(), -1, NULL, INSTALLATION_TYPE_JUST_ME));

		//Portable devices
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_LABEL_INSTALL_TYPE_PORTABLE_DEVICE, str));
		RETURN_IF_ERROR(dropdown->AddItem(str.CStr(), -1, NULL, INSTALLATION_TYPE_PORTABLE_DEVICE));

		dropdown->SelectItem(m_selected_install_type_index, TRUE);
	}

	return OpStatus::OK;
}	

OP_STATUS InstallerWizard::OverrideButtonProperty(BtnId button_id, Str::LocaleString str_id, OpInputAction action_id, INTPTR action_data)
{
	OpButtonStrip*	btn_strip = GetButtonStrip();
	
	if (btn_strip)
	{
		OpString8 name;
		OpString str;
		RETURN_IF_ERROR(g_languageManager->GetString(str_id, str));

		OpInputAction* action = OP_NEW(OpInputAction, (action_id));
		RETURN_OOM_IF_NULL(action);
		action->SetActionData(action_data);

		return btn_strip->SetButtonInfo(button_id, action, str, TRUE, TRUE, name);
	}

	return OpStatus::ERR;
}

void InstallerWizard::SetWindowTitle()
{
	OpString format;
	OpString dialog_title;
	OP_STATUS status;
	status = g_languageManager->GetString(Str::D_INSTALLER_TITLE, format);

	if (OpStatus::IsSuccess(status))
	{
		OpString ver;
		switch(g_desktop_product->GetProductType())
		{
			case PRODUCT_TYPE_OPERA_NEXT:
				ver.Set(UNI_L("Next ") VER_NUM_STR_UNI);
				break;
			case PRODUCT_TYPE_OPERA_LABS:
				ver.Set(g_desktop_product->GetLabsProductName());
				ver.Insert(0, "Labs ");
				ver.Append(UNI_L(" ") VER_NUM_STR_UNI);
				break;
			default:
				ver.Set(VER_NUM_STR_UNI);
		}

		RETURN_VOID_IF_ERROR(dialog_title.AppendFormat(format, ver.CStr()));

		SetTitle(dialog_title.CStr());
	}
}

OP_STATUS InstallerWizard::PopulateLocaleDropDown()
{
#ifdef FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
	OP_ASSERT(g_folder_manager);
	OpDropDown *dropdown = static_cast<OpDropDown*>(GetWidgetByName("installer_options_lang"));
	if (dropdown && dropdown->CountItems() <= 0 )
	{
		//IMPORTANT NOTE: make sure to delete the user data before clearing the list
		dropdown->Clear(); 
		dropdown->SetListener(this);

		//Get path to opera's locale file depending on current system(OS) locale.
		OpString current_path, lng_code;
		OpStatus::Ignore(GetCurrentLanguageFile(current_path, lng_code));
	
		//Enum available language and populate the dropdown box
		OpStringC searchtemplate(UNI_L("*.lng"));
		UINT32 total_available_locales =  g_folder_manager->GetLocaleFoldersCount();
		INT32  system_locale_index = 0, got_index = 0;

		for (UINT32 counter = 0; counter < total_available_locales; ++counter)
		{
			OpString locale_folder;
			if (OpStatus::IsError(g_folder_manager->GetLocaleFolderPath(counter, locale_folder)))
				continue;
			OpFolderLister* lister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, searchtemplate.CStr(), locale_folder.CStr());
			if (!lister)
			{
				return OpStatus::ERR_NO_MEMORY;
			}

			if (lister->Next())
			{
				const uni_char *candidate = lister->GetFullPath();
				if (candidate)
				{
					OpFile lng_file;
					RETURN_IF_ERROR(lng_file.Construct(candidate));

					LangAccessorLight accessor;
					RETURN_IF_ERROR(accessor.LoadL(&lng_file, NULL));
					
					OpString lang_code_in_file, lang_country_name, file_build;
					RETURN_IF_ERROR(accessor.GetFileInfo(lang_code_in_file, lang_country_name, file_build));

					if (lang_country_name.HasContent() && lang_code_in_file.HasContent())
					{	
						LocaleInfo* locale_info = OP_NEW(LocaleInfo,());
						RETURN_OOM_IF_NULL(locale_info);
						if (OpStatus::IsError(locale_info->locale_file_path.Set(candidate)))
						{
							OP_DELETE(locale_info);
							return OpStatus::ERR;
						}

						if (OpStatus::IsError(locale_info->locale_code.Set(lang_code_in_file)))
						{
							OP_DELETE(locale_info);
							return OpStatus::ERR;
						}
						
						if (OpStatus::IsError(dropdown->AddItem(lang_country_name,  -1, &got_index, reinterpret_cast<INTPTR>(locale_info))))
						{
							OP_DELETE(locale_info);
							return OpStatus::ERR;
						}

						if (!current_path.IsEmpty() && uni_stricmp(current_path.CStr(), candidate) == 0)
						{
							system_locale_index = got_index;
						}
					}
				}
			}
			OP_DELETE(lister);
			lister = NULL;
		}
		dropdown->SelectItem(system_locale_index, TRUE);
	}

#endif //FOLDER_MANAGER_LOCALE_FOLDER_SUPPORT
	return OpStatus::OK;
}

void InstallerWizard::ShowShieldIconIfNeeded()
{
	INT32 current_page = GetCurrentPage();
	if (current_page != INSTALLER_DLG_OPTIONS && current_page != INSTALLER_DLG_STARTUP && current_page != INSTALLER_DLG_TERM_OF_SERVICE)
		return;

	BOOL is_elevation_req = FALSE;
	BOOL show_install_shield = m_selected_install_type_index == INSTALLATION_TYPE_ALL_USERS ||
		(OpStatus::IsSuccess(m_opera_installer->PathRequireElevation(is_elevation_req)) && is_elevation_req);

	OpButtonStrip* btn_strip = GetButtonStrip();
	if (!btn_strip)
		return;

	OpButton *button = static_cast<OpButton *>(btn_strip->GetWidgetByTypeAndId(OpTypedObject::WIDGET_TYPE_BUTTON, BTN_ID_ACCEPT_INSTALL));
	if (!button)
		return;

	OpButton::ButtonStyle prev_btn_style = button->m_button_style;
	if (show_install_shield)
	{
		button->GetForegroundSkin()->SetBitmapImage(m_uac_shield, FALSE);
		button->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_CENTER);
	}
	else
	{
		button->SetButtonStyle(OpButton::STYLE_TEXT);
	}

	if (button->m_button_style != prev_btn_style)
	{
		Relayout();
	}
}

void InstallerWizard::SetInstallFolderAndUpdateOptions(const OpString& path)
{
	static BOOL update_installer_option_entered = FALSE; //TODO: Use postmessage to avoid this stack-overflow situation!

	if (update_installer_option_entered)
		return;

	update_installer_option_entered = TRUE;

	if (OpStatus::IsError(m_opera_installer->SetInstallFolder(path))) 
	{
		update_installer_option_entered = FALSE;
		return;
	}

	/*
	Fetch install options(settings) after installer backend is updated with install path. 
	Note: Its important to fetch new settings whenever backend is updated with install path. Installer backend checks for installed logfile 
	in specified path and update its internal states including settings.
	*/
	m_settings = m_opera_installer->GetSettings();

	OpString language;
	m_opera_installer->GetInstallLanguage(language);
	OpDropDown* lang_dropdown = static_cast<OpDropDown*>(GetWidgetByName("installer_options_lang"));
	if (lang_dropdown && language.HasContent())
	{
		INT32 count = lang_dropdown->CountItems();
		for (INT32 i = 0 ; i < count ; i++)
		{
			LocaleInfo* locale_info = reinterpret_cast<LocaleInfo*>(lang_dropdown->GetItemUserData(i));
			if (language.CompareI(locale_info->locale_code) == 0)
			{
				lang_dropdown->SelectItem(i, TRUE);
				ActivateNewLanguage(locale_info);
			}
		}
	}


	if (m_opera_installer->GetOperation() == OperaInstaller::OpUpdate)
		m_selected_install_type_index = m_settings.all_users ? INSTALLATION_TYPE_ALL_USERS : (m_settings.copy_only ? INSTALLATION_TYPE_PORTABLE_DEVICE : INSTALLATION_TYPE_JUST_ME);

	/*
	Disable/Enable installer options based on installed folder path. If this is an update over existing installation, disable 
	all install options. EnableWidgetInstallationType() generates OnChange() events which makes calls to EnableDefaultBrowserOption()
	and EnableShortcutsOptions(); no need to make calls to later mentioned functions here.
	*/
	EnableWidgetInstallationType(m_opera_installer->GetOperation() != OperaInstaller::OpUpdate, (InstallationType)m_selected_install_type_index);

	// Make sure the upgrade label is turned on and off.
	InitializeOptionsDlgGroup(FALSE);

	//Show shield icon
	ShowShieldIconIfNeeded();

	//Relayout the dialog content
	OnRelayout();

	update_installer_option_entered = FALSE;
}

void InstallerWizard::EnableShortcutsOptions(BOOL is_visible /*= TRUE*/, BOOL is_enable /*= TRUE*/)
{
	OP_ASSERT(g_op_ui_info);

	OpWidget *widget = GetWidgetByName("installer_options_shortcut_label");
	if (widget)
	{
		widget->SetVisibility(is_visible);
		widget->SetEnabled(is_enable);
	}

	widget = static_cast<OpCheckBox*>(GetWidgetByName("installer_options_shortcut_on_desktop"));
	if (widget)
	{
		widget->SetVisibility(is_visible);
		if (is_visible)
		{
			widget->SetValue(m_settings.desktop_shortcut);
			widget->SetEnabled(is_enable);
			widget->SetForegroundColor(is_enable ? 
				g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_FONT) : g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_DISABLED_FONT));
	 	}
	}

	widget = static_cast<OpCheckBox*>(GetWidgetByName("installer_options_shortcut_in_start_menu"));
	if (widget)
	{
		widget->SetVisibility(is_visible);
		if (is_visible)
		{
			widget->SetValue(m_settings.start_menu_shortcut);
			widget->SetEnabled(is_enable);
			widget->SetForegroundColor(is_enable ? 
				g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_FONT) : g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_DISABLED_FONT));
		}
	}

	BOOL is_systemwin7 = IsSystemWin7();
	widget = is_systemwin7 ? static_cast<OpCheckBox*>(GetWidgetByName("installer_options_shortcut_in_taskbar")) : 
		static_cast<OpCheckBox*>(GetWidgetByName("installer_options_shortcut_in_launchbar"));
	if (widget)
	{
		widget->SetVisibility(is_visible);
		if (is_visible)
		{
			widget->SetValue(is_systemwin7 ? m_settings.pinned_shortcut : m_settings.quick_launch_shortcut);
			widget->SetEnabled(is_enable);
			widget->SetForegroundColor(is_enable ? 
				g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_FONT) : g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_DISABLED_FONT));
		}
	}
}

void InstallerWizard::EnableDefaultBrowserOption(BOOL is_visible /*= TRUE*/, BOOL is_enable /*= TRUE*/)
{
	OpWidget * widget = GetWidgetByName("installer_options_defaults_label");
	if (widget)
	{
		widget->SetVisibility(is_visible);
		widget->SetEnabled(is_enable);
	}

	widget = static_cast<OpCheckBox*>(GetWidgetByName("installer_options_make_defaultbrowser"));
	if (widget)
	{
		widget->SetVisibility(is_visible);
		if (is_visible)
		{
			widget->SetValue(m_settings.set_default_browser);
			widget->SetEnabled(is_enable);
			widget->SetForegroundColor(is_enable ? 
				g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_FONT) : g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_DISABLED_FONT));
		}
	}

	if (widget = GetWidgetByName("installer_options_yandex_search_engine"))
	{
		widget->SetVisibility(is_visible && m_show_yandex_checkbox);
		widget->SetEnabled(is_enable);
		widget->SetForegroundColor(is_enable ? 
			g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_FONT) : g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_DISABLED_FONT));
	}
}

void InstallerWizard::EnableWidgetInstallationType(BOOL is_enable /*= TRUE*/, InstallationType installation_type)
{
	OpDropDown *dropdown = static_cast<OpDropDown*>(GetWidgetByName("installer_options_install_type"));
	if (dropdown)
	{
		dropdown->SetAlwaysInvoke(TRUE);
		dropdown->SetEnabled(is_enable);
		dropdown->SelectItemAndInvoke(installation_type, TRUE, TRUE);
	}
}

OP_STATUS InstallerWizard::GetCurrentLanguageFile(OpString &lng_file_path, OpString &lng_code)
{
	OpFile current_lng_file;
	TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::LanguageFile, current_lng_file));
	RETURN_IF_ERROR(lng_file_path.Set(current_lng_file.GetFullPath()));
	if (lng_file_path.IsEmpty())
		return OpStatus::ERR;

	OpFile lng_file;
	RETURN_IF_ERROR(lng_file.Construct(lng_file_path.CStr()));
	
	LangAccessorLight accessor;
	RETURN_IF_ERROR(accessor.LoadL(&lng_file, NULL));
					
	OpString lang_country_name, file_build;
	return accessor.GetFileInfo(lng_code, lang_country_name, file_build);
}

OP_STATUS InstallerWizard::ActivateNewLanguage(LocaleInfo *selected_lang_info)
{
	OP_ASSERT(g_pcfiles || selected_lang_info);
	if (selected_lang_info->locale_file_path.IsEmpty() || selected_lang_info->locale_code.IsEmpty())
		return OpStatus::ERR;

	OpFile languagefile;
	RETURN_IF_ERROR(languagefile.Construct(selected_lang_info->locale_file_path.CStr()));

	BOOL exists;
	if (OpStatus::IsError(languagefile.Exists(exists)) || !exists)
		return OpStatus::ERR;

	TRAP_AND_RETURN(err, g_pcfiles->WriteFilePrefL(PrefsCollectionFiles::LanguageFile, &languagefile); \
						 g_prefsManager->CommitForceL());

	g_application->SettingsChanged(SETTINGS_LANGUAGE);

	TranslateString();
	ReadEndUserLicenseAgreement();
	
	// Update the options screen, since this is where language can change.
	InitializeOptionsDlgGroup(FALSE);

	return OpStatus::OK;
}

void InstallerWizard::LaunchFolderExplorer()
{
	OP_ASSERT(g_folder_manager || g_languageManager);

	if (!m_file_chooser)
		RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_file_chooser));
	
	OpString installation_path;
	m_opera_installer->GetInstallFolder(installation_path);
	if (installation_path.HasContent())
		m_file_chooser_request.initial_path.Set(installation_path);
	else
	{
		RETURN_VOID_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_OPEN_FOLDER, m_file_chooser_request.initial_path));
	}
	
	m_file_chooser_request.action = DesktopFileChooserRequest::ACTION_DIRECTORY;
	g_languageManager->GetString(Str::D_INSTALLER_LABEL_INSTALL_PATH, m_file_chooser_request.caption);

	OpStatus::Ignore(m_file_chooser->Execute(GetOpWindow(), this, m_file_chooser_request));
}

OP_STATUS InstallerWizard::ReadEndUserLicenseAgreement()
{
	OpString license_file_path, lng_code;
	RETURN_IF_ERROR(GetCurrentLanguageFile(license_file_path, lng_code));
	
	OP_STATUS status = OpStatus::ERR;

	OpMultilineEdit* edit = static_cast<OpMultilineEdit*>(GetWidgetByName("installer_term_of_service"));
	if (edit)
	{
		INT	 pos;	
		//Pick license.txt file for the current/active language file
		if (KNotFound != (pos = license_file_path.FindLastOf(UNI_L('\\'))))
		{
			license_file_path.Delete(pos+1);
			RETURN_IF_ERROR(license_file_path.Append(UNI_L("license.txt")));

			OpFile file;
			RETURN_IF_ERROR(file.Construct(license_file_path));

			OpFileLength size;
			if (OpStatus::IsError(file.GetFileLength(size)) || size == 0)
			{
				//If current/active locale folder does not contain license.txt file, try to find in the ENGLISH locale path
				if (KNotFound != (pos = license_file_path.FindLastOf(UNI_L('\\'))))
					license_file_path.Delete(pos);
				
				if (KNotFound != (pos = license_file_path.FindLastOf(UNI_L('\\'))))
				{
					license_file_path.Delete(pos+1);
					RETURN_IF_ERROR(license_file_path.Append(UNI_L("en\\")));
					RETURN_IF_ERROR(license_file_path.Append(UNI_L("license.txt")));

					RETURN_IF_ERROR(file.Construct(license_file_path));
					RETURN_IF_ERROR(file.GetFileLength(size));

				}
			}

			if (!size)
				return OpStatus::ERR;

			if (!file.IsOpen())
				RETURN_IF_ERROR(file.Open(OPFILE_READ));
			
			char* buffer = OP_NEWA(char, size+1);
			RETURN_OOM_IF_NULL(buffer);
			OpFileLength bytes_read = 0;
			file.Read(buffer, size, &bytes_read);
			buffer[size] = 0;

			OpString message;
			CharsetDetector detector;
			const char* encoding = detector.GetUTFEncodingFromBOM(buffer, size);
			if (encoding)
			{
				TRAPD(rc, SetFromEncodingL(&message, encoding, buffer, size));
			}
			else
			{
				TRAPD(rc, SetFromEncodingL(&message, "windows-1252", buffer, size));
			}
			OP_DELETEA(buffer);
			file.Close();

			edit->SetReadOnly(TRUE);
			edit->SetTabStop(TRUE);
			RETURN_IF_ERROR(edit->SetText(message.CStr()));
	
			status = OpStatus::OK;
		}
	}
	return status;
}

void InstallerWizard::InitializeStartUpDlgGroup(BOOL first_time)
{
	//Set skin named 'Installer Wizard Dialog Skin' which applies Opera 'O' logo/image to the dialog
	SetSkin(m_dialog_skin);

	if (first_time)
	{
		OpWidget* image_label = GetWidgetByName("installers_opera_txt_graphic");
		if(image_label)
			image_label->GetBorderSkin()->SetImage(m_graphic_skin);

		/*
		Set default installation path.
		By setting an installation path to an editbox widget, installer_options_install_path, an OnChange() event is generated 
		which then makes a call to SetInstallFolderAndUpdateOptions(); indirectly making a call to said method.
		*/
		OpString installation_path;
		m_opera_installer->GetInstallFolder(installation_path);
		if (installation_path.HasContent())
		{
			OpWidget *widget = GetWidgetByName("installer_options_install_path");
			if (widget)
				widget->SetText(installation_path.CStr());
		}

		// Set the upgrade message's style
		OpWidget *label = GetWidgetByName("installer_upgrade_info_message_0");
		if(label)
			static_cast<OpLabel*>(label)->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);

	}

	// Update license label string with link
	UpdateTermOfServiceLabel("license_label_on_start_page");

	// Set the upgrade message's visibility
	OpWidget *label = GetWidgetByName("installer_upgrade_info_message_0");
	if(label)
		label->SetVisibility(IsAnUpgrade());

	// Update buttons in button strip
	Str::LocaleString str_id = IsAnUpgrade() ? Str::D_INSTALLER_BTN_ACCEPT_TOS_UPGRADE : Str::D_INSTALLER_BTN_ACCEPT_TOS_INSTALL;
	OverrideButtonProperty(BTN_ID_OPTIONS, Str::D_INSTALLER_OPTIONS, OpInputAction::ACTION_FORWARD);
	OverrideButtonProperty(BTN_ID_ACCEPT_INSTALL, str_id, OpInputAction::ACTION_INITIATE_OPERA_INSTALLATION);
}

void InstallerWizard::InitializeOptionsDlgGroup(BOOL first_time)
{
	if (first_time)
	{
		OpButton* button = static_cast<OpButton*>(GetWidgetByName("installers_options_image_help"));
		if(button)
		{
			button->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM , OpButton::STYLE_IMAGE);
			button->GetBorderSkin()->SetImage("Installer Wizard Help Button Skin", "Push Button Skin");
			button->GetForegroundSkin()->SetImage("Menu Help");
		}
		
		//Enumerate language choices and populate dropdown widgets
		RETURN_VOID_IF_ERROR(PopulateLocaleDropDown());
		
		//Set 'Enter' action on editbox for install path
		OpEdit *install_path = static_cast<OpEdit*>(GetWidgetByName("installer_options_install_path"));
		if (install_path)
		{
			OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CHANGE));
			if (action)
			{
				action->SetActionData(reinterpret_cast<INTPTR>(install_path));
				install_path->SetAction(action);
			}
		}

		// Set the upgrade message's style
		OpWidget* widget = GetWidgetByName("installer_upgrade_info_message_1");
		if(widget)
			static_cast<OpLabel*>(widget)->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);

		// Hide option to create shortcut in launchbar on Windows 7 platform. On other Windows variant hide the option to pin to taskbar.
		widget = IsSystemWin7() ? GetWidgetByName("installer_options_shortcut_in_launchbar") : GetWidgetByName("installer_options_shortcut_in_taskbar");
		if (widget)
			widget->SetVisibility(FALSE);

		if (g_desktop_product->GetProductType() == PRODUCT_TYPE_OPERA_LABS || IsSystemWin8())
		{
			widget = GetWidgetByName("installer_options_make_defaultbrowser");
			widget->SetVisibility(FALSE);
		}
	}
	
	// Set the upgrade message's visibility
	OpWidget* label = GetWidgetByName("installer_upgrade_info_message_1");
	if(label)
		label->SetVisibility(IsAnUpgrade());

	// Update license label string with link
	UpdateTermOfServiceLabel("license_label_on_options_page");

	// Update buttons in button strip
	Str::LocaleString str_id = IsAnUpgrade() ? Str::D_INSTALLER_BTN_ACCEPT_TOS_UPGRADE : Str::D_INSTALLER_BTN_ACCEPT_TOS_INSTALL;
	OverrideButtonProperty(BTN_ID_OPTIONS, Str::SI_PREV_BUTTON_TEXT, OpInputAction::ACTION_BACK);
	OverrideButtonProperty(BTN_ID_ACCEPT_INSTALL, str_id, OpInputAction::ACTION_INITIATE_OPERA_INSTALLATION);
}

void InstallerWizard::InitializeProgressInfoDlgGroup(BOOL first_time)
{
	//Set skin named 'Installer Wizard Dialog Skin' which applies Opera 'O' logo/image to the dialog
	SetSkin(m_dialog_skin);

	if (first_time)
	{
		OpWidget* image_label = GetWidgetByName("installers_opera_txt_graphic_pb_grp");
		if(image_label)
			image_label->GetBorderSkin()->SetImage(m_graphic_skin);

		m_progressbar = static_cast<OpProgressBar*>(GetWidgetByName("installation_progress"));
		if (m_progressbar)
		{
			m_progressbar->SetProgressBarFullSkin("Installer Progress Full Skin");
			m_progressbar->SetProgressBarEmptySkin("Installer Progress Empty Skin");
		}

		if (m_opera_installer->GetOperation() == OperaInstaller::OpUninstall)
		{
			OpLabel *widget = static_cast<OpLabel*>(GetWidgetByName("installation_thank_you_1"));
			if (widget)
				widget->SetWrap(TRUE);

			SetWidgetText("installation_thank_you_1", Str::D_UNINSTALL_THANK_YOU);
		}
	}

	// Update buttons in button strip
	ShowButton(BTN_ID_OPTIONS, FALSE);
	ShowButton(BTN_ID_ACCEPT_INSTALL, FALSE);
}

void InstallerWizard::InitializeHandleLockedDlgGroup(BOOL first_time)
{
	if (first_time)
	{
		OpLabel* widget = static_cast<OpLabel*>(GetWidgetByName("in_order_to_complete"));
		if (widget)
			widget->SetWrap(TRUE);

		widget = static_cast<OpLabel*>(GetWidgetByName("installer_app_lockscreen_warning_msg"));
		if (widget)
			widget->SetWrap(TRUE);

		OpTreeView* tree_view = static_cast<OpTreeView*>(GetWidgetByName("installer_app_list"));
		if (tree_view)
		{
			tree_view->SetIgnoresMouse(TRUE);
			tree_view->SetDead(TRUE);
		}

		RETURN_VOID_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_TERMINATE_APPS_LOCKING_INSTALLATION, (MH_PARAM_1)0));	
		RETURN_VOID_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_FILE_HANDLE_NO_LONGER_LOCKED, (MH_PARAM_1)0));
	}

	ShowSpinnginWaitCursor(FALSE);

	if (m_opera_installer->GetOperation() == OperaInstaller::OpUninstall && m_opera_installer->IsRunning())
	{
		OpButtonStrip* btn_strip = GetButtonStrip();
		if (btn_strip)
		{
			OpString str;
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_IGNORE, str));
			RETURN_VOID_IF_ERROR(btn_strip->SetButtonText(BTN_ID_CANCEL, str));
		}
		SetWidgetText("in_order_to_complete", Str::D_INSTALLER_APPS_CLOSE_WARNING_UNINSTALL);
	}

	// Update buttons in button strip
	ShowButton(BTN_ID_OPTIONS, FALSE);
	OverrideButtonProperty(BTN_ID_ACCEPT_INSTALL, Str::D_INSTALLER_CONTINUE, OpInputAction::ACTION_INITIATE_OPERA_INSTALLATION);
}

void InstallerWizard::InitializeUnintstallDlgGroup(BOOL first_time)
{
	//Set skin named 'Installer Wizard Dialog Skin' which applies Opera 'O' logo/image to the dialog
	SetSkin("Installer Wizard Dialog Skin");

	if (first_time)
	{
		OpWidget* image_label = GetWidgetByName("installers_opera_txt_graphic_uninstall_grp");
		if(image_label)
			image_label->GetBorderSkin()->SetImage(m_graphic_skin);

		OpLabel *widget = static_cast<OpLabel*>(GetWidgetByName("installerwizard_uninstall_warning"));
		if (widget)
			widget->SetWrap(TRUE);
	}

	// Update buttons in button strip
	OverrideButtonProperty(BTN_ID_OPTIONS, Str::D_UNINSTALL_REMOVE_PROFILE_OPTIONS, OpInputAction::ACTION_FORWARD);
	OverrideButtonProperty(BTN_ID_ACCEPT_INSTALL, Str::D_INSTALLWIZARD_UNINSTALL_OPERA, OpInputAction::ACTION_INITIATE_OPERA_INSTALLATION);
}

void InstallerWizard::InitializeRemoveProfileDlgGroup(BOOL first_time)
{
	// Update buttons in button strip
	OverrideButtonProperty(BTN_ID_OPTIONS, Str::SI_PREV_BUTTON_TEXT, OpInputAction::ACTION_BACK);
	OverrideButtonProperty(BTN_ID_ACCEPT_INSTALL, Str::D_INSTALLWIZARD_UNINSTALL_OPERA, OpInputAction::ACTION_INITIATE_OPERA_INSTALLATION);
}

void InstallerWizard::InitializeToSDlgGroup(BOOL first_time)
{
	if (first_time)
	{
		//Read End User License Agreement. ReadEndUserLicenseAgreement() also sets the MultilineEditbox with read license text.
		RETURN_VOID_IF_ERROR(ReadEndUserLicenseAgreement());
	}

	OpWidget *widget = GetWidgetByName("license_label_on_tos_page");
	if (widget)
	{
		OpString install_button_str;
		if (IsAnUpgrade())
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_BTN_ACCEPT_TOS_UPGRADE, install_button_str));
		else
			RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_BTN_ACCEPT_TOS_INSTALL, install_button_str));

		OpString license_label_str;
		OpString temp_license_label_str;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_LICENSE_LABEL, temp_license_label_str));
		RETURN_VOID_IF_ERROR(license_label_str.AppendFormat(temp_license_label_str, install_button_str.CStr()));
		RETURN_VOID_IF_ERROR(widget->SetText(license_label_str.CStr()));
	}

	// Update buttons in button strip
	OverrideButtonProperty(BTN_ID_OPTIONS, Str::SI_PREV_BUTTON_TEXT, OpInputAction::ACTION_BACK);
}

OP_STATUS InstallerWizard::HandleActionCancel()
{
	if (m_forbid_closing)
		return OpStatus::OK;

	if ((!m_opera_installer->IsRunning() || m_opera_installer->GetOperation() ==  OperaInstaller::OpUninstall) && GetCurrentPage() == INSTALLER_DLG_HANDLE_LOCKED)
	{
		m_opera_installer->WasCanceled(TRUE);
		CloseDialog(FALSE);
		return OpStatus::OK;
	}


	OpString message;

	if (m_opera_installer->GetOperation() ==  OperaInstaller::OpUninstall)
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_UNINSTALL_EXIT_CONFIRMATION, message));
	else 
		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_INSTALLER_EXIT_CONFIRMATION, message));

	RETURN_IF_ERROR(SimpleDialog::ShowDialog(WINDOW_NAME_INSTALLER_CONFIRM_EXIT, this, message, UNI_L("Opera"), Dialog::TYPE_YES_NO, Dialog::IMAGE_QUESTION, this));

	if (m_opera_installer->IsRunning())
		m_opera_installer->Pause(TRUE); //Pause installation setup
	
	return OpStatus::OK;
}

OP_STATUS InstallerWizard::SelectDefaultSkin()
{
	//TODO: Are we suppose to release memory ?
	OP_ASSERT(g_skin_manager);
	OpFile defaultfile; ANCHOR(OpFile, defaultfile);
	LEAVE_IF_ERROR(defaultfile.Construct(STANDARD_SKIN_FILE, OPFILE_DEFAULT_SKIN_FOLDER));
	return g_skin_manager->SelectSkinByFile(&defaultfile, FALSE);
}

UINT InstallerWizard::GetDriveType(uni_char *path)
{
	if (!(path && *path))
		return DRIVE_FIXED;

	uni_char drive[_MAX_DRIVE]={0,};
	uni_char dir[_MAX_DIR]={0,};
	uni_char filename[_MAX_FNAME]={0,};

	_wsplitpath_s(path, drive, _MAX_DRIVE, dir, _MAX_DIR, filename, _MAX_FNAME, NULL, 0);

	if (*drive)
	{
		return ::GetDriveType(drive);
	}
	else
	{ 
		if (*dir != UNI_L('\\') ) //Relative directory
			return ::GetDriveType(NULL);
		else if (uni_strlen(dir) > 2 && *dir == UNI_L('\\') && *(dir+1) != UNI_L('\\')) //Root path 
			return ::GetDriveType(NULL);
		else
		{
			//TODO:
		}
	}
	return ::GetDriveType(dir);
}

OP_STATUS InstallerWizard::ShowLockingProcesses(const uni_char* file_name, OpVector<ProcessItem>& processes)
{
	if (!m_process_model)
	{
		m_process_model = OP_NEW(ProcessModel, ());
		RETURN_OOM_IF_NULL(m_process_model);
		
		m_process_model->SetProcesses(processes);

		OpTreeView* tree_view = static_cast<OpTreeView*>(GetWidgetByName("installer_app_list"));
		if (tree_view)
			tree_view->SetTreeModel(m_process_model, 0, TRUE);
		else
			return OpStatus::ERR;
	}

	m_process_model->FindProcessLocking(file_name);
	m_process_model->Update(ENUMERATE_PROCESS_LIST_TIMEOUT);
	
	return OpStatus::OK;
}

OP_STATUS InstallerWizard::UpdateTermOfServiceLabel(const char* label_name)
{
	OpRichTextLabel* rich_label = static_cast<OpRichTextLabel*>(GetWidgetByName(label_name));

	if (rich_label && rich_label->IsOfType(WIDGET_TYPE_RICHTEXT_LABEL))
	{
		OpString url;
		RETURN_IF_ERROR(url.AppendFormat(UNI_L("opera:/action/Forward,%d"), INSTALLER_DLG_TERM_OF_SERVICE));

		Str::LocaleString button_id = IsAnUpgrade() ? Str::D_INSTALLER_BTN_ACCEPT_TOS_UPGRADE : Str::D_INSTALLER_BTN_ACCEPT_TOS_INSTALL;

		OpString labelmsg;
		RETURN_IF_ERROR(I18n::Format(labelmsg, Str::D_INSTALLER_LICENSE_LABEL_WITH_LINK, button_id, url));

		rich_label->SetText(labelmsg.CStr());
	}

	return OpStatus::OK;
}

//This method tells whether the strings displayed in the dialog should indicate to the user that an upgrade will happen
//It should not be used to affect the dialog functionally.
BOOL InstallerWizard::IsAnUpgrade()
{
	return (m_opera_installer->GetOperation() == OperaInstaller::OpUpdate) || OperaExeExists();
}

BOOL InstallerWizard::OperaExeExists()
{
	OP_ASSERT(m_opera_installer);

	OpString installation_path;
	RETURN_VALUE_IF_ERROR(GetInstallerPath(installation_path), FALSE);

	if (installation_path.IsEmpty())
		return FALSE;

	OpString exe_path;
	RETURN_VALUE_IF_ERROR(exe_path.Set(installation_path), FALSE);

	if (!HasTrailingSlash(exe_path.CStr())) // if(exe_path[exe_path.Length() - 1] != PATHSEPCHAR)
		RETURN_VALUE_IF_ERROR(exe_path.Append(PATHSEP), FALSE);

	OpString dll_path;
	dll_path.Set(exe_path);

	RETURN_VALUE_IF_ERROR(exe_path.Append("Opera.exe"),FALSE);
	RETURN_VALUE_IF_ERROR(dll_path.Append("Opera.dll"),FALSE);

	OpFile file;
	BOOL  exists;

	RETURN_VALUE_IF_ERROR(file.Construct(exe_path),FALSE);
	if (OpStatus::IsError(file.Exists(exists)) || !exists)
		return FALSE;

	RETURN_VALUE_IF_ERROR(file.Construct(dll_path),FALSE);
	if (OpStatus::IsError(file.Exists(exists)) || !exists)
		return FALSE;

	return TRUE;
}
