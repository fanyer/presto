/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/managers/DesktopExtensionsManager.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/file_utils/folder_recurse.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/controller/ExtensionPrefsController.h"
#include "adjunct/quick/controller/ExtensionPopupController.h"
#include "adjunct/quick/controller/ExtensionInstallController.h"
#include "adjunct/quick/controller/ExtensionUninstallController.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/widgets/OpExtensionButton.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/models/ServerWhiteList.h"

#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/libgogi/pi_impl/mde_bitmap_window.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/util/path.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/inputmanager/inputmanager.h"
#include "adjunct/autoupdate/autoupdate_checker/adaptation_layer/global_storage.h"
#include "adjunct/autoupdate/autoupdater.h"
#include "modules/forms/tempbuffer8.h"

namespace
{
	const char ALLOWED_UPDATE_SERVER_NAME[] = "extension-updates.opera.com";
	const uni_char RUNNING_GADGET[] = UNI_L("running");
	const uni_char STOPPED_GADGET[] = UNI_L("stopped");
};

using namespace opera_update_checker::global_storage;
using namespace opera_update_checker::status;

inline OpBrowserView* GetBrowserView(OpRichMenuWindow* window)
{
	if (window)
	{
		OpWidget* widget = window->GetToolbarWidgetOfType(
						 OpTypedObject::WIDGET_TYPE_BROWSERVIEW);
		return static_cast<OpBrowserView*>(widget);
	}
	return NULL;
}


DesktopExtensionsManager::DesktopExtensionsManager() :
		m_last_updated(NULL),
		m_next_to_last_updated(NULL),
		m_number_of_updates_made(0),
		m_popup_window(NULL),
		m_popup_active(FALSE),
		m_extension_prefs_controller(NULL),
		m_popup_controller(NULL),
		m_extension_popup_btn(NULL),
		m_silent_install(false),
		m_uid_retrieval_retry_count(0)
{
	AddExtensionsModelListener(&m_speeddial_handler);
	m_extension_installer.AddListener(&m_speeddial_handler);
	g_speeddial_manager->AddListener(m_speeddial_handler);
}

DesktopExtensionsManager::~DesktopExtensionsManager() 
{
	m_uid_retrieval_retry_timer.Stop();
	g_windowCommanderManager->SetTabAPIListener(NULL);
	g_speeddial_manager->RemoveListener(m_speeddial_handler);
	m_extension_installer.RemoveListener(&m_speeddial_handler);
	RemoveExtensionsModelListener(&m_speeddial_handler);
}

OP_STATUS DesktopExtensionsManager::Init()
{
	//
	// We have to clear content of Extensions Toolbar.content
	// in case previous opera session crashed and left dirty
	// prefs file behind.
	// We don't have to notify g_application about changed
	// toolbar settings, because Init() takes place before
	// any toolbar reads its content.
	// These two lines sit in here and not in ToolbarManager
	// because we don't want to introduce manager dependency,
	// that is not really needed; if this dependency
	// will ever be introduced for other reasons, this two
	// lines should be moved to ToolbarManager.
	// [pobara 2010-11-30]
	//
	PrefsFile* prefs_file = g_setup_manager->GetSetupFile(OPTOOLBAR_SETUP, TRUE);
	TRAPD(err, prefs_file->DeleteSectionL("Extensions Toolbar.content"));
	OP_ASSERT(OpStatus::IsSuccess(err)); // failing here should not block init
 
	RETURN_IF_ERROR(CheckForDelayedUpdates());
	
	RETURN_IF_ERROR(m_extension_ui_listener.Init());
	RETURN_IF_ERROR(m_tab_api_listener.Init());
	RETURN_IF_ERROR(m_extensions_model.Init());
	m_uid_retrieval_retry_timer.SetTimerListener(this);

	g_windowCommanderManager->SetTabAPIListener(&m_tab_api_listener);
	return OpStatus::OK;
}

void DesktopExtensionsManager::StartAutoStartServices()
{
	m_extensions_model.StartAutoStartServices();
}

OP_STATUS DesktopExtensionsManager::InstallCustomExtensions()
{
	if (!g_speeddial_manager->HasLoadedConfig()) // DSK-351304
		return OpStatus::ERR;
	OpString custom_dir_path;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_EXTENSION_CUSTOM_FOLDER, custom_dir_path));
	m_silent_install = true; // don't open extra tab when installing extensions from custom folder (DSK-362260)
	OP_STATUS status = m_extension_installer.InstallExtensionsFromDirectory(custom_dir_path);
	m_silent_install = false;
	return status;
}

OP_STATUS DesktopExtensionsManager::InstallFromExternalURL(const OpStringC& url_name,BOOL silent_installation, BOOL temp_mode)
{
	if (!g_speeddial_manager->HasLoadedConfig()) // DSK-351304
		return OpStatus::ERR;
	if (url_name.IsEmpty())
		return OpStatus::ERR;
	return m_extension_installer.InstallRemote(url_name,silent_installation,temp_mode);
}

OP_STATUS DesktopExtensionsManager::InstallFromLocalPath(const OpStringC& extension_path)
{
	if (!g_speeddial_manager->HasLoadedConfig()) // DSK-351304
		return OpStatus::ERR;
	return m_extension_installer.InstallLocal(extension_path);
}

OP_STATUS DesktopExtensionsManager::OpenExtensionOptionsPage(const OpStringC& extension_id)
{
	return m_extensions_model.OpenExtensionOptionsPage(extension_id);
}

OP_STATUS DesktopExtensionsManager::OpenExtensionOptionsPage(
	  const OpStringC& extension_id, ExtensionPrefsController& controller)
{
	m_extension_prefs_controller = &controller;
	const OP_STATUS result = OpenExtensionOptionsPage(extension_id);
	m_extension_prefs_controller = NULL;
	return result;
}

BOOL DesktopExtensionsManager::CanShowOptionsPage(
		const OpStringC& extension_id)
{
	return m_extensions_model.CanShowOptionsPage(extension_id); 	
}

OP_STATUS DesktopExtensionsManager::UninstallExtension(const OpStringC& extension_id)
{
	return m_extensions_model.UninstallExtension(extension_id); 
}

OP_STATUS DesktopExtensionsManager::DisableExtension(
		const OpStringC& extension_id,BOOL user_requested, BOOL notify_listeners)
{ 
	return m_extensions_model.DisableExtension(extension_id, user_requested, notify_listeners); 
}

OP_STATUS DesktopExtensionsManager::OpenExtensionUrl(const OpStringC& url,const OpStringC& wuid)
{
	return m_extensions_model.OpenExtensionUrl(url,wuid);
}

OP_STATUS DesktopExtensionsManager::GetExtensionFallbackUrl(const OpStringC& extension_id,OpString& fallback_url)
{
	ExtensionsModelItem* model_item = m_extensions_model.FindExtensionById(extension_id);
	RETURN_VALUE_IF_NULL(model_item,OpStatus::ERR);

	return fallback_url.Set(model_item->GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_FALLBACK_URL));
}

void DesktopExtensionsManager::ExtensionDataUpdated(const OpStringC& extension_id)
{
	ExtensionsModelItem* model_item = m_extensions_model.FindExtensionById(extension_id);
	if (!model_item)
		return;

	m_extensions_model.UpdateExtendedName(extension_id,
		model_item->GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE));

	ExtensionDataListener* listener = NULL;
	RETURN_VOID_IF_ERROR(m_extension_data_listeners.GetData(extension_id.CStr(), &listener));
	OP_ASSERT(listener != NULL);

	listener->OnExtensionDataUpdated(
			model_item->GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_TITLE),
			model_item->GetGadget()->GetAttribute(WIDGET_EXTENSION_SPEEDDIAL_URL));
}

OP_STATUS DesktopExtensionsManager::SetExtensionTemporary(const OpStringC& extension_id, BOOL temporary)
{
	ExtensionsModelItem* model_item = m_extensions_model.FindExtensionById(extension_id);
	if (!model_item)
		return OpStatus::ERR;

	model_item->SetTemporary(temporary);
	return OpStatus::OK;
}

BOOL DesktopExtensionsManager::IsExtensionTemporary(const OpStringC& extension_id)
{
	ExtensionsModelItem* model_item = m_extensions_model.FindExtensionById(extension_id);
	if (!model_item)
		return FALSE;

	return model_item->IsTemporary();
}

OP_STATUS DesktopExtensionsManager::EnableExtension(const OpStringC& extension_id)
{
	OpGadget* gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID,extension_id,FALSE);
	BOOL updated = FALSE; 
	if (gadget)
		CheckForDelayedUpdate(gadget,updated);

	if (updated)
		ShowUpdateNotification(gadget, NULL, 1);

	return m_extensions_model.EnableExtension(extension_id); 
}

OP_STATUS DesktopExtensionsManager::SetExtensionSecurity(const OpStringC& extension_id, 
		BOOL ssl_access, BOOL private_access)
{
	ExtensionsModelItem* model_item = m_extensions_model.FindExtensionById(extension_id);
	RETURN_VALUE_IF_NULL(model_item, OpStatus::ERR);
	
	model_item->AllowUserJSOnSecureConn(ssl_access);
	model_item->AllowUserJSInPrivateMode(private_access);
	
	return OpStatus::OK;
}

OP_STATUS DesktopExtensionsManager::GetExtensionSecurity(const OpStringC& extension_id, 
		BOOL& ssl_access, BOOL& private_access)
{
	ExtensionsModelItem* model_item = m_extensions_model.FindExtensionById(extension_id);
	RETURN_VALUE_IF_NULL(model_item, OpStatus::ERR);
	
	ssl_access = model_item->IsUserJSAllowedOnSecureConn();
	private_access = model_item->IsUserJSAllowedInPrivateMode();
	
	return OpStatus::OK;
}

void DesktopExtensionsManager::ShowUntrustedRepositoryWarningDialog(OpStringC repository) const
{
	OpString message;
	RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_EXTENSIONS_INSTALLATION_SECURE_ADDONS_REPOSITORY, message));

	OpString title;
	RETURN_VOID_IF_ERROR(g_languageManager->GetStringL(Str::D_EXTENSIONS_INSTALLATION_BLOCKED_TITLE, title));

	OpString repository_domain;
	unsigned domain_offset = 0;
	RETURN_VOID_IF_ERROR(StringUtils::FindDomain(repository, repository_domain, domain_offset));
	RETURN_VOID_IF_ERROR(repository_domain.Append(UNI_L("\n\n")));
	RETURN_VOID_IF_ERROR(message.Insert(0, repository_domain.CStr()));

	SimpleDialogController* controller = OP_NEW(SimpleDialogController, (
		SimpleDialogController::TYPE_CLOSE,
		SimpleDialogController::IMAGE_WARNING,
		"Untrusted Repository Dialog",
		message,
		title
		)
	);
	if (controller)
	{
		controller->SetHelpAnchor(UNI_L("extensions.html#on"));
		OpStatus::Ignore(ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow()));
	}
}

bool DesktopExtensionsManager::IsUpdateFromTrustedHost(OpGadget& gadget)
{
	OpString host;
	OP_STATUS status = GetURL(gadget, host);
	if (OpStatus::IsError(status))
	{
		// if file does not exist we should allow to update
		return status == OpStatus::ERR_FILE_NOT_FOUND ? true : false;
	}

	OpString url_server;
	RETURN_VALUE_IF_ERROR(g_server_whitelist->GetRepositoryFromAddress(url_server, host), false);
	return g_server_whitelist->FindMatch(url_server) != FALSE;
}

bool DesktopExtensionsManager::IsUpdateFromTrustedHost(const OpStringC& id)
{
	OpGadget* gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, id);
	RETURN_VALUE_IF_NULL(gadget, false);
	return IsUpdateFromTrustedHost(*gadget);
}

OP_STATUS DesktopExtensionsManager::AskForExtensionUpdate(const OpStringC& extension_id)
{
	ExtensionsModelItem* model_item = m_extensions_model.FindExtensionById(extension_id);
	RETURN_VALUE_IF_NULL(model_item, OpStatus::ERR);

	ExtensionUpdateController* controller = OP_NEW(ExtensionUpdateController,(*model_item->GetGadgetClass(), OpStringC(model_item->GetExtensionId())));

	RETURN_IF_ERROR(ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow()));

	return OpStatus::OK;
}

void DesktopExtensionsManager::UpdateExtension(const OpStringC& extension_id)
{
	ExtensionsModelItem* item = m_extensions_model.FindExtensionById(extension_id);
	if (!item)
		return;

	DisableExtension(extension_id, TRUE, item->IsInDevMode());
	BOOL updated;
	OpGadget* op_gadget = item->GetGadget();

	BOOL is_old_sd_extension = ExtensionUtils::RequestsSpeedDialWindow(*op_gadget->GetClass());

	OpGadgetClass* new_extension_class;
	OpString update_file_path;

	g_desktop_gadget_manager->GetUpdateFileName(*op_gadget,update_file_path);

	if (OpStatus::IsError(ExtensionUtils::CreateExtensionClass(update_file_path,&new_extension_class)))
		return;

	BOOL is_new_sd_extension = ExtensionUtils::RequestsSpeedDialWindow(*new_extension_class);

	if (is_old_sd_extension && !is_new_sd_extension)
	{
		RemoveDuplicatedSDExtensions(op_gadget);

		item = m_extensions_model.FindExtensionById(extension_id); // reload item since it might get replaced. 
		if (!item)
			return;
	}

	OpStatus::Ignore(m_extension_installer.InstallDelayedUpdate(op_gadget,updated, TRUE));
	OpString  extensions_name;
	OpStatus::Ignore(item->GetGadget()->GetGadgetName(extensions_name));

	if (updated)
	{
		item->SetIsUpdateAvailable(FALSE);
		m_extensions_model.UpdateFinished(extension_id);
		ShowUpdateNotification(op_gadget, NULL, 1);

		is_new_sd_extension = ExtensionUtils::RequestsSpeedDialWindow(*op_gadget->GetClass());
	}
	else
	{
		if (OpStatus::IsError(m_extensions_model.UpdateFinished(extension_id)))
		{
			OpString title;
			OpString tmp;
			OpString message;
	
			g_languageManager->GetString(Str::S_ERROR_EXTENSION_FAILURE_TITLE, title);
			g_languageManager->GetString(Str::D_INSTALL_EXTENSION_UPDATE_ERROR, tmp);
			message.AppendFormat(tmp.CStr(), extensions_name.CStr());
			
			SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
			if (dialog)
			{
				if(OpStatus::IsError(dialog->Init(WINDOW_NAME_SERVICE_INSTALL_FAILED, title, message,
					g_application->GetActiveDesktopWindow(), Dialog::TYPE_OK)))
					OP_DELETE(dialog);
			}
		}
	}

	EnableExtension(extension_id);

	if (!is_old_sd_extension && is_new_sd_extension)
			ExtensionSpeedDialHandler::AnimateInSpeedDial(*op_gadget);
}

OP_STATUS DesktopExtensionsManager::ShowUpdateAvailable(const OpStringC& extension_id)
{
	m_extensions_model.UpdateAvailable(extension_id);
	return OpStatus::OK;
}



BOOL DesktopExtensionsManager::HandleAction(OpInputAction* action)
{
	OP_ASSERT(action);
	if (NULL == action)
		return FALSE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			OP_ASSERT(child_action);

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_EXTENSION_ACTION:
				{
					INT32 ui_item_id = child_action->GetActionData();
					ExtensionButtonComposite *button = 
						m_extensions_model.GetButton(ui_item_id);
					child_action->SetEnabled(
							button ? button->GetEnabled() : FALSE);

					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_EXTENSION_OPTIONS:
				{
					const uni_char* extension_id = 
							reinterpret_cast<const uni_char*>(child_action->GetActionData());
					child_action->SetEnabled(m_extensions_model.CanShowOptionsPage(
							extension_id));

					return TRUE;
				}
				case OpInputAction::ACTION_EXTENSION_PRIVACY:
				case OpInputAction::ACTION_SHOW_EXTENSION_COGWHEEL_MENU:
				case OpInputAction::ACTION_OPEN /* Extension's name and author button URLs */:
				case OpInputAction::ACTION_DEVTOOLS_RELOAD_EXTENSION:
				{
					const uni_char* extension_id = 
							reinterpret_cast<const uni_char*>(child_action->GetActionData());
					child_action->SetEnabled(m_extensions_model.IsEnabled(extension_id));

					return TRUE;
				}
				case OpInputAction::ACTION_RELOAD:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
			}
			break;
		}
		case OpInputAction::ACTION_EXTENSION_ACTION:
		{
			INT32 ui_item_id = action->GetActionData();
			ExtensionButtonComposite *button = 
				m_extensions_model.GetButton(ui_item_id);

			if (button)
			{
				if (!m_popup_active)
				{
					// no active popup implies either:
					// action originated from extender button OR
					// there is url to display in popup
					// in both cases callbacks->OnClick should be called.

					OpWidget *target_widget = NULL;
					OpTypedObject *object = action->GetActionObject();

					if (object && object->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
					{
						target_widget = static_cast<OpWidget*>(object);
						ShowPopup(target_widget,ui_item_id);
					}
				}
				OP_ASSERT(button->GetCallbacks());
				button->GetCallbacks()->OnClick(ui_item_id);

				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_UNINSTALL_EXTENSION:
		{
			const uni_char* extension_id = reinterpret_cast<const uni_char*>(action->GetActionData());
						
			RETURN_VALUE_IF_ERROR(ShowExtensionUninstallDialog(extension_id), FALSE);

			return TRUE;
		}
		case OpInputAction::ACTION_ENABLE_EXTENSION:
		{
			const uni_char* extension_id = 
					reinterpret_cast<const uni_char*>(action->GetActionData());

			RETURN_VALUE_IF_ERROR(EnableExtension(extension_id), FALSE);
			
			return TRUE;
		}

		case OpInputAction::ACTION_UPDATE_EXTENSION:
		{
			const uni_char* extension_id = 
					reinterpret_cast<const uni_char*>(action->GetActionData());

			if (IsUpdateFromTrustedHost(extension_id))
			{
				RETURN_VALUE_IF_ERROR(AskForExtensionUpdate(extension_id), FALSE);
			}
			else
			{
				OpString repository;
				RETURN_VALUE_IF_ERROR(GetURL(extension_id, repository), FALSE);
				ShowUntrustedRepositoryWarningDialog(repository);
			}

			return TRUE;
		}

		case OpInputAction::ACTION_DISABLE_EXTENSION:
		{
			const uni_char* extension_id = 
					reinterpret_cast<const uni_char*>(action->GetActionData());

			RETURN_VALUE_IF_ERROR(
					m_extensions_model.DisableExtension(extension_id, TRUE), FALSE);

			return TRUE;
		}
		case OpInputAction::ACTION_DEVTOOLS_RELOAD_EXTENSION:
		{
			const uni_char* extension_id = 
					reinterpret_cast<const uni_char*>(action->GetActionData());

			return ReloadExtension(extension_id);
		}
		case OpInputAction::ACTION_GET_MORE_EXTENSIONS:
		{
			OpInputAction action(OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE);
			action.SetActionDataString(UNI_L("http://addons.opera.com/addons/extensions/"));
			return g_input_manager->InvokeAction(&action);
		}
		case OpInputAction::ACTION_OPEN_FOLDER:
		{
			OpString path; 
			RETURN_VALUE_IF_ERROR(
					path.Set(reinterpret_cast<const uni_char*>(action->GetActionData())), FALSE);
			RETURN_VALUE_IF_ERROR(OpPathDirFileCombine(path, path, 
					OpStringC(GADGET_CONFIGURATION_FILE)), FALSE);
			RETURN_VALUE_IF_ERROR(
					g_desktop_op_system_info->OpenFileFolder(path, TRUE), FALSE);

			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_EXTENSION_OPTIONS:
		{
			const uni_char* extension_id = 
					reinterpret_cast<const uni_char*>(action->GetActionData());

			RETURN_VALUE_IF_ERROR(
					m_extensions_model.OpenExtensionOptionsPage(extension_id),
						FALSE);

			return TRUE;
		}
		case OpInputAction::ACTION_EXTENSION_PRIVACY:
		{
			const uni_char* extension_id = 
					reinterpret_cast<const uni_char*>(action->GetActionData());

			ExtensionsModelItem* model_item =
					m_extensions_model.FindExtensionById(extension_id);
			RETURN_VALUE_IF_NULL(model_item, FALSE);

			ExtensionPrivacyController* controller = OP_NEW(ExtensionPrivacyController,(
				*model_item->GetGadgetClass(),model_item->GetExtensionId(),
				model_item->IsUserJSAllowedInPrivateMode()?true:false,model_item->IsUserJSAllowedOnSecureConn()?true:false));

			RETURN_VALUE_IF_ERROR(ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow()),FALSE);
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_EXTENSION_COGWHEEL_MENU:
		{
			OpRect rect;
			action->GetActionPosition(rect);
			g_application->GetMenuHandler()->ShowPopupMenu(
					"Extensions Panel Button Cogwheel Menu", PopupPlacement::AnchorBelow(rect),
					action->GetActionData(), action->IsKeyboardInvoked());

			return TRUE;
		}
		case OpInputAction::ACTION_OPEN:
		{
			const uni_char* data_str = action->GetActionDataString();
			if (data_str)
			{
				g_application->GoToPage(OpStringC(data_str));	
			}

			return TRUE;
		}

		case OpInputAction::ACTION_RELOAD:
			m_extensions_model.ReloadDevModeExtensions();
			return TRUE;
	}

	if (m_speeddial_handler.HandleAction(action))
		return TRUE;

	return FALSE;
}

OP_STATUS DesktopExtensionsManager::ShowExtensionUninstallDialog(const uni_char* extension_id)
{
	ExtensionsModelItem* model_item = m_extensions_model.FindExtensionById(extension_id);
	if (!model_item)
		return OpStatus::ERR;

	ExtensionUninstallController* controller = OP_NEW(ExtensionUninstallController, (*model_item->GetGadgetClass(), extension_id));
	
	return ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow());
}

OP_STATUS DesktopExtensionsManager::AddExtensionsModelListener(
		ExtensionsModel::Listener* listener)
{
	RETURN_IF_ERROR(m_extensions_model.AddListener(listener));

	return OpStatus::OK;
}

OP_STATUS DesktopExtensionsManager::RemoveExtensionsModelListener(
		ExtensionsModel::Listener* listener)
{
	RETURN_IF_ERROR(m_extensions_model.RemoveListener(listener));

	return OpStatus::OK;
}

OP_STATUS DesktopExtensionsManager::SetExtensionDataListener(const OpStringC& extension_id, ExtensionDataListener* listener)
{
	OP_ASSERT(extension_id.HasContent());
	if (extension_id.IsEmpty())
		return OpStatus::ERR;

	if (m_extensions_model.FindExtensionById(extension_id) == NULL)
		return OpStatus::ERR;

	if (listener)
		return m_extension_data_listeners.Add(extension_id, listener);
	else
		return m_extension_data_listeners.Remove(extension_id, &listener);
}

OP_STATUS DesktopExtensionsManager::RefreshModel()
{
	RETURN_IF_ERROR(m_extensions_model.Refresh());

	return OpStatus::OK;
}

OP_STATUS DesktopExtensionsManager::CreateExtensionWindow(OpWindowCommander* commander, const OpUiWindowListener::CreateUiWindowArgs& args)
{
	OP_ASSERT(args.type == OpUiWindowListener::WINDOWTYPE_EXTENSION);
	if (args.type != OpUiWindowListener::WINDOWTYPE_EXTENSION)
		return OpStatus::ERR_NOT_SUPPORTED;
	else if (args.extension.type == OpUiWindowListener::EXTENSIONTYPE_OPTIONS)
		RETURN_IF_ERROR(CreateExtensionOptionsWindow(commander, NULL));
	else if (args.extension.type  == OpUiWindowListener::EXTENSIONTYPE_POPUP)
		RETURN_IF_ERROR(CreateExtensionPopupWindow(commander, NULL));
	else if (ExtensionUtils::RequestsSpeedDialWindow(*commander->GetGadget()->GetClass()))
		RETURN_IF_ERROR(CreateSpeedDialExtensionWindow(commander));
	else
		RETURN_IF_ERROR(CreateDummyExtensionWindow(commander));

	return OpStatus::OK;
}

OP_STATUS DesktopExtensionsManager::CreateExtensionOptionsWindow(OpWindowCommander* commander, const OpStringC& url)
{
	if (m_extension_prefs_controller)
	{
		RETURN_IF_ERROR(m_extension_prefs_controller->ShowDialog(
					g_application->GetActiveDocumentDesktopWindow()));
		return m_extension_prefs_controller->LoadPrefs(commander, url);
	}
	else
	{
		BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
		OP_ASSERT(browser_window != NULL || !"How did you manage to request the options page when you have no browser window?");
		if (browser_window == NULL)
			return OpStatus::ERR_NULL_POINTER;
		DocumentDesktopWindow* new_window = g_application->CreateDocumentDesktopWindow(
				browser_window->GetWorkspace(), commander);
		if (new_window == NULL)
			return OpStatus::ERR;
		new_window->GoToPage(url.CStr());
	}

	return OpStatus::OK;
		
}

OP_STATUS DesktopExtensionsManager::CreateExtensionPopupWindow(OpWindowCommander* commander, const OpStringC& url)
{
	ExtensionButtonComposite *button = 	m_extensions_model.GetButton(m_popup_composite_id);
	if (!button)
		return OpStatus::ERR;

	UINT32 width, height;
	button->GetPopupSize(width, height);

	RETURN_IF_ERROR(m_last_popup_url.Set(button->GetPopupURL()));
	if (m_popup_controller != NULL)
	{
		if (OpStatus::IsError(ShowDialog(
			m_popup_controller,g_global_ui_context,g_application->GetActiveDocumentDesktopWindow())))
		{
			m_popup_controller = NULL;
			return OpStatus::ERR;
		}
		
		SetPopupSize(m_popup_composite_id,width, height);

		m_popup_controller->SetListener(this);
		RETURN_IF_ERROR(m_popup_controller->LoadDocument(commander, button->GetPopupURL()));
		RETURN_IF_ERROR(m_popup_controller->AddPageListener(this));
		
		commander->SetExtensionUIListener(&m_extension_ui_listener);
		commander->SetCanBeClosedByScript(TRUE);
		
	}
	else if(m_popup_window)
	{
		if (OpStatus::IsError(m_popup_window->Construct("Extension Popup", OpWindow::STYLE_EXTENSION_POPUP)))
		{
			OP_DELETE(m_popup_window);
			m_popup_window = NULL;
			return OpStatus::ERR;
		}
		m_popup_window->SetSkin("Extension Popup Menu Skin");

		OpBrowserView* view = GetBrowserView(m_popup_window);
		
		if (!view)
		{
			OP_DELETE(m_popup_window);
			m_popup_window = NULL;
			return OpStatus::ERR;
		}
		
		OpAutoPtr<OpPage> page(OP_NEW(OpPage, (commander)));
		RETURN_OOM_IF_NULL(page.get());
		RETURN_IF_ERROR(page->Init());
		RETURN_IF_ERROR(view->SetPage(page.release()));
		view->AddListener(this);
		SetPopupSize(m_popup_composite_id,width, height);
		
		commander->DisableGlobalHistory();
		commander->SetExtensionUIListener(&m_extension_ui_listener);
		commander->SetCanBeClosedByScript(TRUE);
		
		/// would like to use that line below.. but unfortunately core does not support that.
		// RETURN_IF_ERROR(commander->OnUiWindowCreated(view->GetOpWindow()));
		//  so we need to open url manually : CORE-40839 
		if(commander->OpenURL(button->GetPopupURL(), FALSE))
		{
			m_popup_window->Show(*this);
			m_popup_window->SetFocus(FOCUS_REASON_OTHER);
			m_popup_active = TRUE;
		}
		else
		{
			m_popup_window->CloseMenu();
		}
	}
	return OpStatus::OK;
}

OP_STATUS DesktopExtensionsManager::CreateSpeedDialExtensionWindow(OpWindowCommander* commander)
{
	OpAutoPtr<BitmapOpWindow> bitmap_window(OP_NEW(BitmapOpWindow, ()));
	RETURN_OOM_IF_NULL(bitmap_window.get());
	RETURN_IF_ERROR(bitmap_window->Init(
				THUMBNAIL_WIDTH * SpeedDialManager::MaximumZoomLevel,
				THUMBNAIL_HEIGHT * SpeedDialManager::MaximumZoomLevel));

	RETURN_IF_ERROR(commander->OnUiWindowCreated(bitmap_window.get()));

	commander->SetExtensionUIListener(&m_extension_ui_listener);
	commander->SetDocumentListener(bitmap_window.get());
	commander->ForcePluginsDisabled(TRUE);

	bitmap_window->Show(TRUE);
	bitmap_window.release();

	return OpStatus::OK;
}

OP_STATUS DesktopExtensionsManager::CreateDummyExtensionWindow(OpWindowCommander* commander)
{
	// all gadgets (including extensions) still require OpWindow to run
	// properly (even if it doesn't represent anything to user);
	//
	// This opwindow belongs to quick, even if we don't use it.
	// It is deleted in ExtensionsModel.
	//
	// [pobara 2010-09-20]
	OpWindow* dummy_window = NULL;
	RETURN_IF_ERROR(OpWindow::Create(&dummy_window));
	OpAutoPtr<OpWindow> window_holder(dummy_window);

	RETURN_IF_ERROR(dummy_window->Init(OpWindow::STYLE_HIDDEN_GADGET, OpTypedObject::WINDOW_TYPE_GADGET));
	OP_STATUS status = commander->OnUiWindowCreated(dummy_window);
	if (OpStatus::IsError(status))
	{
		commander->OnUiWindowClosing();
		return status;
	}

	commander->SetExtensionUIListener(&m_extension_ui_listener);
	window_holder.release();

	return OpStatus::OK;
}

ExtensionButtonComposite* DesktopExtensionsManager::GetButtonFromModel(INT32 id)
{
	return m_extensions_model.GetButton(id);
}

OP_STATUS DesktopExtensionsManager::CreateButtonInModel(INT32 id)
{
	return m_extensions_model.CreateButton(id);
}

OP_STATUS DesktopExtensionsManager::DeleteButtonInModel(INT32 id)
{
	return m_extensions_model.DeleteButton(id);
}

void DesktopExtensionsManager::OnButtonRemovedFromComposite(INT32 id)
{
	m_extensions_model.OnButtonRemovedFromComposite(id);
}

OP_STATUS DesktopExtensionsManager::CheckForDelayedUpdates()
{

	BOOL updated;
	UINT32 num_updated = 0;
	OpGadget* last_updated = NULL;
	OpGadget* next_to_last_updated = NULL;
	for (UINT i = 0; i < g_gadget_manager->NumGadgets(); ++i)
	{
		OpGadget* gadget = g_gadget_manager->GetGadget(i);
		OP_ASSERT(gadget);
		if (!gadget)
			continue;

		OpGadgetClass* gadget_class = gadget->GetClass(); 
		OP_ASSERT(gadget_class);

		if (gadget_class->IsExtension())
		{
			CheckForDelayedUpdate(gadget,updated);
			if (updated)
			{
				num_updated++;
				next_to_last_updated = last_updated;
				last_updated = gadget;
			}
		}
	}

	if (num_updated > 0)
	{
		ShowUpdateNotification(last_updated, next_to_last_updated, num_updated);
	}
	return OpStatus::OK;
}


OP_STATUS DesktopExtensionsManager::CheckForDelayedUpdate(OpGadget* gadget, BOOL &updated)
{
	RETURN_IF_ERROR(m_extension_installer.InstallDelayedUpdate(gadget,updated));
	return OpStatus::OK;
}

void DesktopExtensionsManager::ShowUpdateNotification()
{
	if (m_number_of_updates_made <= 0)
		return;

	ShowUpdateNotification(m_last_updated,m_next_to_last_updated,m_number_of_updates_made);
	m_number_of_updates_made = 0;
	m_last_updated = NULL;
}

void DesktopExtensionsManager::ShowUpdateNotification(OpGadget * last_updated, OpGadget* next_to_last_updated, int number_of_updates_made)
{
	OpString title_text;
	OpString body_text;
	Image notification_image;

	if (number_of_updates_made == 1)
	{
		OpStatus::Ignore(ExtensionUtils::GetExtensionIcon(notification_image, last_updated->GetClass()));
	}
	if (notification_image.IsEmpty())
	{
		OpSkinElement* element = g_skin_manager->GetSkinElement("Extension 64");
		if (element)
			notification_image = element->GetImage(0);
	}

	OpStatus::Ignore(
		g_languageManager->GetString(Str::D_EXTENSIONS_UPDATE_NOTIFICATION_TITILE,title_text));

	if (number_of_updates_made == 1)
	{
		OpString unite_name;
		last_updated->GetClass()->GetGadgetName(unite_name);
		OpString temp;
		g_languageManager->GetString(Str::D_EXTENSIONS_UPDATE_NOTIFICATION_SINGLE_APP_TEXT,temp);
		OpStatus::Ignore(body_text.AppendFormat(temp.CStr(),unite_name.CStr()));
	}
	else
	{
		OpString temp;
		OpString few;
		OpString title;
		last_updated->GetClass()->GetGadgetName(title);
		OpStatus::Ignore(few.Append(title));
		OpStatus::Ignore(few.Append(", "));
		next_to_last_updated->GetClass()->GetGadgetName(title);
		OpStatus::Ignore(few.Append(title));

		if (number_of_updates_made > 2)
			g_languageManager->GetString(Str::D_EXTENSIONS_UPDATE_NOTIFICATION_MULTIPLE_APP_TEXT,temp);
		else
			g_languageManager->GetString(Str::D_EXTENSIONS_UPDATE_NOTIFICATION_FEW_APP_TEXT,temp);

		OpStatus::Ignore(body_text.AppendFormat(temp.CStr(), few.CStr(), number_of_updates_made-2));
	}

	OpInputAction* notification_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_MANAGE));
	if (!notification_action)
		return;
	notification_action->SetActionDataString(UNI_L("Extensions"));
	g_notification_manager->ShowNotification(DesktopNotifier::AUTOUPDATE_NOTIFICATION_EXTENSIONS,
		title_text,body_text,notification_image,notification_action,NULL,TRUE);

	m_extensions_model.Refresh();
}

void DesktopExtensionsManager::CheckForUpdate()
{
	OpString params;
	const char* uuid = NULL;
	unsigned long uuid_length;
	if (g_autoupdate_manager->GetUpdateStorage()->GetData(UUID_KEY, uuid, uuid_length) != StatusCode::OK)
	{
		m_uid_retrieval_retry_count++;
		if (m_uid_retrieval_retry_count <= MAX_UUID_RETRIEVAL_RETRIES)
		{
			m_uid_retrieval_retry_timer.Start(3000); // Retry in 3s if not retried too many times already.
			return;
		}
		m_uid_retrieval_retry_count = 0;
	}
	else
		m_uid_retrieval_retry_count = 0;

	OpString uni_uuid;
	if (uuid)
	{
		TempBuffer8 url_param_uuid;
		// UUID may contain chars which needs URL encoding.
		url_param_uuid.AppendURLEncoded(uuid);
		OP_DELETEA(uuid);
		uni_uuid.SetFromUTF8(url_param_uuid.GetStorage());
	}

	OpVector<uni_char> unique_ids;
	for (int i  = g_gadget_manager->NumGadgets();i > 0 ; i--)
	{
		OpGadget* gadget = g_gadget_manager->GetGadget(i-1);
		if (!gadget)
			continue;

		OpGadgetClass* gadget_class = gadget->GetClass();

		if (!gadget_class || !gadget_class->IsExtension())
			continue;

		OpStringC gadget_id(gadget_class->GetGadgetId());
		if (gadget_id.IsEmpty())
			continue;

		BOOL developer_mode = FALSE;
		if (OpStatus::IsError(ExtensionUtils::IsDevModeExtension(*gadget_class, developer_mode)) || developer_mode)
			continue;

		ExtensionsModelItem* item = m_extensions_model.FindExtensionById(gadget->GetIdentifier());
		if (item && item->IsDeleted())
			continue;

		bool already_checked = false;
		for (UINT32 z = 0; z< unique_ids.GetCount(); z++)
		{
			if(gadget_id.Compare(unique_ids.Get(z))== 0)
			{
				already_checked = true;
				break;
			}
		}

		if (already_checked)
			continue;

		unique_ids.Add(const_cast<uni_char*>(gadget_id.CStr()));

		params.Empty();

		const uni_char* update_url_str = gadget_class->GetGadgetUpdateUrl();
		if (update_url_str && *update_url_str)
		{
			URL update_url = g_url_api->GetURL(update_url_str);
			ServerName *server = update_url.GetServerName();
			if (server && server->GetName().CompareI(ALLOWED_UPDATE_SERVER_NAME) == 0)
			{
				OpStatus::Ignore(params.AppendFormat(UNI_L("uid=%s&state=%s"),
																						uni_uuid.CStr() ? uni_uuid.CStr() : UNI_L(""),
																						gadget->IsRunning() ? RUNNING_GADGET : STOPPED_GADGET));
			}
		}

		gadget->Update(params.CStr());
	}
}

OP_STATUS DesktopExtensionsManager::RemoveDuplicatedSDExtensions(OpGadget* source_gadget)
{
	RETURN_VALUE_IF_NULL(source_gadget,OpStatus::ERR);

	UINT starting_pos = 0;
	OpGadget* gadget = NULL;
	
	while (g_gadget_manager->NumGadgets() > starting_pos)
	{		
		RETURN_IF_ERROR(ExtensionUtils::FindNormalExtension(source_gadget->GetClass()->GetGadgetId(),&gadget,starting_pos));
	
		if (gadget && gadget!=source_gadget)
		{
			UninstallExtension(gadget->GetIdentifier());
		}
	}
	return OpStatus::OK;
}

OP_STATUS DesktopExtensionsManager::UpdateExtension(GadgetUpdateInfo* data,OpStringC& update_source)
{
	if (!update_source.HasContent())
		return OpStatus::ERR;

	if (!(data && data->gadget_class && data->gadget_class->IsExtension()))
		return OpStatus::ERR;

	OpString gadget_id;
	if (OpStatus::IsError(data->gadget_class->GetGadgetId(gadget_id)) || gadget_id.IsEmpty())
		return OpStatus::ERR;

	BOOL delayed = FALSE;
	OpGadget *updated_gadget;
	OpGadget* sd_checker = NULL;
	UINT dummy = 0;

	RETURN_IF_ERROR(ExtensionUtils::FindNormalExtension(gadget_id,&sd_checker,dummy));
	RETURN_VALUE_IF_NULL(sd_checker,OpStatus::ERR);
	BOOL old_ext_sd = ExtensionUtils::RequestsSpeedDialWindow(*sd_checker->GetClass());
	
	OpGadgetClass *gclass_new;
	RETURN_IF_ERROR(ExtensionUtils::CreateExtensionClass(update_source,&gclass_new));
	BOOL new_ext_sd = ExtensionUtils::RequestsSpeedDialWindow(*gclass_new);

	UINT starting_pos = 0;
	OP_STATUS err = OpStatus::OK;
	do {
		RETURN_IF_ERROR(ExtensionUtils::FindNormalExtension(gadget_id,
			&updated_gadget,starting_pos));

		if (!updated_gadget)
			break;

		ExtensionsModelItem* item = m_extensions_model.FindExtensionById(updated_gadget->GetIdentifier());

		if (!item)
			return OpStatus::ERR;

		if (item->IsDeleted())
			continue;

		err =  m_extension_installer.ReplaceExtension(&updated_gadget,update_source,delayed);

		if (OpStatus::IsSuccess(err))
		{
			if (!delayed)
			{
				m_next_to_last_updated = m_last_updated;
				m_last_updated = updated_gadget;
				m_number_of_updates_made++;
				if (!new_ext_sd && old_ext_sd)
				{
					break;
				}
			}
			else
			{
				ShowUpdateAvailable(updated_gadget->GetIdentifier());
			}
		}
	}
	while (g_gadget_manager->NumGadgets() > starting_pos && OpStatus::IsSuccess(err));

	if (!delayed && !new_ext_sd && old_ext_sd)
	{
		RETURN_IF_ERROR(RemoveDuplicatedSDExtensions(updated_gadget));
	}

	return err;
}

void DesktopExtensionsManager::StartListeningTo(OpWorkspace* workspace)
{
	workspace->AddListener(&m_tab_api_listener);
}

void DesktopExtensionsManager::StopListeningTo(OpWorkspace* workspace)
{
	workspace->RemoveListener(&m_tab_api_listener);
}

void DesktopExtensionsManager::CloseDevExtensionPopup()
{
	if (m_popup_controller)
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_OK, 0, NULL, m_popup_controller);
		m_popup_controller = NULL;
	}
	m_popup_active = FALSE;
}

void DesktopExtensionsManager::CloseRegExtensionPopup()
{
	if(m_popup_window)
		m_popup_window->CloseMenu();
}

void DesktopExtensionsManager::OnPageClose(OpWindowCommander* commander)
{
	if (m_popup_window)
		m_popup_window->CloseMenu();
	m_popup_active = FALSE;
}

void DesktopExtensionsManager::ShowPopup(OpWidget* opener, INT32 composite_id)
{
	if (m_popup_controller)
	{
		CloseDevExtensionPopup();	
		return;
	}

	ExtensionButtonComposite *button = 	m_extensions_model.GetButton(composite_id);
	if (!button || !opener)
	{
		return;
	}

	if (button->GetPopupURL() == 0)
		return;

	ExtensionsModelItem* item = m_extensions_model.FindExtensionById(button->GetGadgetId());
	if (!item || !item->GetGadget())	
	{
		return;
	}

	if (opener->GetType() == OpTypedObject::WIDGET_TYPE_EXTENSION_BUTTON)
		m_extension_popup_btn = static_cast<OpExtensionButton*>(opener);

	m_popup_composite_id = composite_id;

	BOOL developer_mode = FALSE;
	if (OpStatus::IsError(ExtensionUtils::IsDevModeExtension(button->GetGadgetId(), developer_mode)))
		return;

	if (developer_mode)
	{
		m_popup_controller = OP_NEW(ExtensionPopupController,(*opener));
		if (!m_popup_controller)
			return;
	}
	else
	{
		m_popup_window = OP_NEW(OpRichMenuWindow,
		(opener, g_application->GetActiveDesktopWindow()));

		if (!m_popup_window)
			return;
	}
	
	OpUiWindowListener::CreateUiWindowArgs args;
	args.type = OpUiWindowListener::WINDOWTYPE_EXTENSION;
	args.extension.type = OpUiWindowListener::EXTENSIONTYPE_POPUP;
	args.extension.data = NULL;

	if (OpStatus::IsError(item->GetGadget()->CreateNewUiExtensionWindow(args, button->GetPopupURL())))
	{
		OP_DELETE(m_popup_window);
		m_popup_window = NULL;
		OP_DELETE(m_popup_controller);
		m_popup_controller = NULL;
	}
}

void DesktopExtensionsManager::OnMenuClosed()
{ 
	if (m_extension_popup_btn)
		m_extension_popup_btn->MenuClosed();

	m_extension_popup_btn = NULL;
	m_popup_active = FALSE;
	m_popup_window = NULL;

	m_last_popup_url.Empty();
}



void DesktopExtensionsManager::SetPopupURL(INT32 composite_id, const OpStringC& url)
{
	if (m_popup_composite_id != composite_id)
		return;

	if (m_last_popup_url == url)
		return;

	if (OpStatus::IsError(m_last_popup_url.Set(url.CStr())))
		return;

	ExtensionButtonComposite *button = 	m_extensions_model.GetButton(m_popup_composite_id);
	if (!button)
		return;
	ExtensionsModelItem *item = m_extensions_model.FindExtensionById(button->GetGadgetId());
	if (!item)
		return;

	if (m_popup_controller != NULL)
	{
		m_popup_controller->ReloadDocument(item->GetGadget(),url);
	}
	else if(m_popup_window)
	{
		OpBrowserView* view = GetBrowserView(m_popup_window);
		if (view && view->GetWindowCommander())
		{
			view->GetWindowCommander()->OpenURL(url,FALSE);
		}
	}
}

void DesktopExtensionsManager::SetPopupSize(INT32 composite_id,UINT32 width, UINT32 height)
{
	if (m_popup_composite_id != composite_id)
		return;

	if (width == 0)
		width = 300;
	if (height == 0)
		height = 300;
	if (width< 18)
		width = 18;
	if (height < 18)
		height = 18;

	if(m_popup_controller)
	{
		m_popup_controller->SetContentSize(width, height);
	}
	else if(m_popup_window) 
	{		
		OpBrowserView* view = GetBrowserView(m_popup_window);
		if(WindowCommanderProxy::HasWindow(view))
		{
			INT32 current_width, current_height;
			view->GetPreferedSize(&current_width, &current_height);

			if (static_cast<UINT32>(current_width) != width || static_cast<UINT32>(current_height) != height)
			{
				view->ResetRequiredSize();
				view->SetPreferedSize(width, height);

				if (m_popup_window->IsShowed())
					m_popup_window->Show(*this);
			}
		}
	}
}

BOOL DesktopExtensionsManager::HandlePopupAction(OpInputAction* action)
{
	if(m_popup_controller)
	{
		return m_popup_controller->HandlePopupAction(action);
	}
	else if(m_popup_window) 
	{
		OpBrowserView* view = GetBrowserView(m_popup_window);
		if (view && view->OnInputAction(action))
		{
			return TRUE;
		}
	}
	return FALSE;
}

void DesktopExtensionsManager::OnDialogClosing(DialogContext* context)
{
	m_popup_controller = NULL;
}

OP_STATUS DesktopExtensionsManager::GetURL(const OpStringC& extension_id, OpString& url) const
{
	OpGadget* gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, extension_id);
	RETURN_VALUE_IF_NULL(gadget, OpStatus::ERR);
	return GetURL(*gadget, url);
}

OP_STATUS DesktopExtensionsManager::GetURL(OpGadget& gadget, OpString& url) const
{
	OpString path;
	RETURN_IF_ERROR(g_desktop_gadget_manager->GetUpdateUrlFileName(gadget, path));
	OpFile file;
	RETURN_IF_ERROR(file.Construct(path));

	BOOL file_exists = FALSE;
	RETURN_IF_ERROR(file.Exists(file_exists));
	if (file_exists == FALSE)
	{
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	RETURN_IF_ERROR(file.Open(OPFILE_READ));
	RETURN_IF_ERROR(file.ReadUTF8Line(url));

	return OpStatus::OK;
}

