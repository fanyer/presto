/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"

#include "adjunct/quick/Application.h"

#include "adjunct/quick/extensions/ExtensionInstaller.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/controller/ExtensionInstallController.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/models/ServerWhiteList.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/ToolbarManager.h"
#include "adjunct/quick/widgets/DownloadExtensionBar.h"
#include "adjunct/quick/widgets/OpExtensionButton.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinUtils.h"
#include "modules/util/filefun.h"
#include "modules/util/path.h"

namespace
{
	enum
	{
		UI_TIMEOUT = 1000,
		SHOW_TICK = 100
	};
};

ExtensionInstaller::ExtensionInstaller() :
		m_simple_download_item(NULL)
{
	m_button_show_timer.SetTimerListener(this);
	m_button_firmness_timer.SetTimerListener(this);
	Reset();
}


void ExtensionInstaller::Reset()
{
	m_gclass = NULL;
	OP_DELETE(m_simple_download_item);
	m_simple_download_item = NULL;
	m_url_name.Empty();
	m_dev_mode_install = FALSE;
	m_busy = FALSE;
	m_silent_installation = FALSE;
	m_temp_mode = FALSE;
}

ExtensionInstaller::~ExtensionInstaller()
{
	OP_DELETE(m_simple_download_item);
	if (m_busy)
	{
		NotifyExtensionInstallFailed();
	}
}

OP_STATUS ExtensionInstaller::InstallRemote(const OpStringC& url_name,BOOL silent_installation, BOOL temp_mode)
{
	if (m_busy)
	{
		return OpStatus::ERR;
	}	
	Reset();
	m_silent_installation = silent_installation;
	m_temp_mode = temp_mode;
	m_busy = TRUE;
	RETURN_IF_ERROR(m_url_name.Set(url_name));

	return InstallInternal();
}


OP_STATUS ExtensionInstaller::InstallLocal(const OpStringC& extension_path, BOOL silent_installation)
{
	if (m_busy)
	{
		return OpStatus::ERR;
	}

	Reset();
	m_busy = TRUE;
	m_silent_installation = silent_installation;

	if (OpStringC(GADGET_CONFIGURATION_FILE) == OpPathGetFileName(extension_path))
	{
		// We're here, so extension_path ends with 'config.xml'.
		int idx = extension_path.Find(GADGET_CONFIGURATION_FILE);
		if (idx > 0)
			RETURN_IF_ERROR(m_extension_path.Set(extension_path.SubString(0, idx - 1).CStr()));

		m_dev_mode_install = TRUE;
	}
	else
	{
		RETURN_IF_ERROR(g_desktop_gadget_manager->PlaceFileInWidgetsFolder(
				extension_path, m_extension_path,FALSE));
	}

	if (OpStatus::IsError(LoadGadgetClass()) || OpStatus::IsError(InstallInternal()))
	{
		CleanUp();
		InstallFailed();
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS ExtensionInstaller::InstallExtensionsFromDirectory(const OpStringC& directory)
{
	OP_STATUS status = OpStatus::OK;
	OpString pattern;
	RETURN_IF_ERROR(pattern.Set("*."));
	RETURN_IF_ERROR(pattern.Append(ExtensionUtils::GetExtensionsExtension()));

	OpAutoPtr<OpFolderLister> lister(OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, pattern.CStr(), directory.CStr()));
	RETURN_OOM_IF_NULL(lister.get());

	while (lister->Next())
	{
		OpStringC extension_path(lister->GetFullPath());
		BOOL extension_present_in_package = FALSE;
		RETURN_IF_ERROR(ExtensionUtils::IsExtensionInPackage(extension_path, extension_present_in_package));
		if (extension_present_in_package)
		{
			BOOL extension_exists = FALSE;
			RETURN_IF_ERROR(ExtensionUtils::IsExtensionInstalled(extension_path, extension_exists));
			if (!extension_exists)
			{
				OP_STATUS s = InstallLocal(extension_path, TRUE);
				if (OpStatus::IsError(s))
				{
					status = s;
				}
			}
		}

		OpFile file;
		RETURN_IF_ERROR(file.Construct(extension_path.CStr()));
		OpStatus::Ignore(file.Delete());
	}
	return status;
}

OP_STATUS ExtensionInstaller::InstallOrAskIfNeeded()
{
	if (!m_gclass)
		return OpStatus::ERR;

	BOOL sd_extension =  ExtensionUtils::RequestsSpeedDialWindow(*m_gclass);
	BOOL has_access_list = FALSE, has_userjs = FALSE;
	OpString access_list;
	RETURN_IF_ERROR(ExtensionUtils::GetAccessLevel(m_gclass,has_access_list,access_list,has_userjs));

	if (m_silent_installation || (sd_extension && !has_access_list && !has_userjs))
	{
		InstallAllowed(TRUE, FALSE);
	}
	else
	{
		RETURN_IF_ERROR(ShowInstallerDialog());
	}
	return OpStatus::OK;
}

OP_STATUS ExtensionInstaller::InstallInternal()
{
	if (m_dev_mode_install)
	{
		return InstallDeveloperMode();
	}

	if (!m_gclass && m_url_name.HasContent())
	{
		BOOL allowed = FALSE;
		RETURN_IF_ERROR(CheckIfTrusted(allowed));
		if (allowed)
		{
			RETURN_IF_ERROR(InstallInternalPart2());
		}
		else
		{
			NotifyExtensionInstallAborted();
			CleanUp();
			FinalizeInstallation();
			g_desktop_extensions_manager->ShowUntrustedRepositoryWarningDialog(m_url_name);
			return OpStatus::ERR;
		}
	}
	else
	{
		RETURN_IF_ERROR(InstallOrAskIfNeeded());
	}

	return OpStatus::OK;
}

OP_STATUS ExtensionInstaller::InstallInternalPart2()
{
	OP_STATUS err = FetchExtensionFromURL();
	if (OpStatus::IsError(err))
	{
		CleanUp();
		if (err != OpStatus::ERR_NO_ACCESS)
			InstallFailed(true);
	}		
	return err;
}

OP_STATUS ExtensionInstaller::InstallDeveloperMode()
{
	OpGadget* gadget = NULL;

	if (!m_gclass)
	{
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(g_gadget_manager->CreateGadget(&gadget, m_gclass->GetGadgetPath(),
				URL_EXTENSION_INSTALL_CONTENT));

	OP_STATUS status = InitializeExtension(gadget->GetIdentifier(), TRUE, FALSE);
	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(g_gadget_manager->DestroyGadget(gadget));
		OpStatus::Ignore(g_desktop_extensions_manager->RefreshModel());
		return status;
	}

	FinalizeInstallation();

	g_application->GetPanelDesktopWindow(Hotlist::GetPanelTypeByName(UNI_L("Extensions")), TRUE);  //create if needed
	//other defaults: don't create if exists, open it in foreground, doesn't have additional param

	return OpStatus::OK;
}


OP_STATUS ExtensionInstaller::LoadGadgetClass()
{
	if (OpStatus::IsError(g_gadget_manager->CreateGadgetClass(
			&m_gclass, m_extension_path, URL_EXTENSION_INSTALL_CONTENT)))
	{
		m_gclass = NULL;
		return OpStatus::ERR;
	}

	if (!m_gclass->IsExtension())
	{
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS ExtensionInstaller::CheckIfTrusted(BOOL& trusted)
{
	// just in case if caller does not check the return result and we fail to determine if host is trusted
	trusted = FALSE;

	if (m_url_name.IsEmpty())
		return OpStatus::ERR;

	OpString url;
	RETURN_IF_ERROR(g_server_whitelist->GetRepositoryFromAddress(url, m_url_name));
	trusted = url.FindFirstOf(UNI_L("file://")) == 0 || g_server_whitelist->FindMatch(url);

	return OpStatus::OK;
}

OP_STATUS ExtensionInstaller::FetchExtensionFromURL()
{
	DocumentDesktopWindow* document_desktop_window = g_application->GetActiveDocumentDesktopWindow();
	if (document_desktop_window)
	{
		DownloadExtensionBar* download_bar = document_desktop_window->GetDownloadExtensionBar(TRUE);
		OpStatus::Ignore(AddListener(download_bar));
	}
	OP_DELETE(m_simple_download_item);

	m_simple_download_item = OP_NEW(SimpleDownloadItem,	(*g_main_message_handler, *this));
	RETURN_OOM_IF_NULL(m_simple_download_item);
	if (OpStatus::IsError(m_simple_download_item->Init(m_url_name.CStr())))
	{
		OP_DELETE(m_simple_download_item);
		m_simple_download_item = NULL;
		return OpStatus::ERR;
	}
	NotifyDownloadStarted();
	return OpStatus::OK;
}

void ExtensionInstaller::AbortDownload(const OpStringC& download_url)
{
	if (!m_simple_download_item)
		return;

	if (m_url_name != download_url)
		return;

	OP_DELETE(m_simple_download_item);
	m_simple_download_item = NULL;

	NotifyDownloadFinished();
	InstallFailed();
}

OP_STATUS ExtensionInstaller::Install(BOOL ssl_access, BOOL private_access, OpGadget** gadget)
{
	VersionType installed_version = NoVersionFound;

	// for SD-extensions, always add next instance
	if (!ExtensionUtils::RequestsSpeedDialWindow(*m_gclass))
	{
		RETURN_IF_ERROR(CheckIfTheSameExist(m_gclass, installed_version, gadget));

		if (installed_version == SameVersion)
		{
			return g_desktop_extensions_manager->EnableExtension((*gadget)->GetIdentifier());
		}
	}

	bool has_created_extension = false;
	*gadget = NULL;
	if (installed_version != NoVersionFound)
	{
		RETURN_IF_ERROR(ReplaceExtension(m_gclass, gadget));

		// gadget class is invalidated during update, refresh it
		m_gclass = (*gadget)->GetClass();
	}
	else
	{
		if (OpStatus::IsError(g_gadget_manager->CreateGadget(gadget, m_extension_path,
					URL_EXTENSION_INSTALL_CONTENT)))
		{
			OpFile dest_file;
			RETURN_IF_ERROR(dest_file.Construct(m_extension_path));
			dest_file.Delete();

			return OpStatus::ERR;
		}
		has_created_extension = true;
	}

	OP_STATUS status = InitializeExtension((*gadget)->GetIdentifier(), ssl_access, private_access);
	if (OpStatus::IsError(status) && has_created_extension)
	{
		OpStatus::Ignore(g_gadget_manager->DestroyGadget(*gadget));
		OpStatus::Ignore(g_desktop_extensions_manager->RefreshModel());
	}

	return status;
}

OP_STATUS ExtensionInstaller::ShowInstallerDialog()
{
	OP_ASSERT(!m_silent_installation);
	OP_ASSERT(m_gclass);
	if (!m_gclass || m_silent_installation)
		return OpStatus::ERR;

	ExtensionInstallController* controller = OP_NEW(ExtensionInstallController, (this,*m_gclass));

	if (OpStatus::IsError(ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow())))
	{
		// on error dialog will call onCancel 
		// which triggers cancel installation, that cleans gadget class
		m_gclass = NULL;
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

void ExtensionInstaller::InstallAllowed(BOOL ssl_access, BOOL private_access)
{
	OpGadget* gadget = NULL;
	if (OpStatus::IsSuccess(Install(ssl_access, private_access, &gadget)))
	{
		OP_ASSERT(gadget);
		FinalizeInstallation();
		ShowInstallNotificationSuccess(gadget);
		NotifyExtensionInstalled(*gadget);
		Reset();
	}
	else
	{
		CleanUp();
		
		InstallFailed();
	}
}

class FallbackInstallerIconProvider : public GadgetUtils::FallbackIconProvider
{
public:
	OP_STATUS GetIcon(Image& icon)
	{
		OpSkinElement *element = g_skin_manager->GetSkinElement("Extension 64");

		if (element)
		{
			icon = element->GetImage(SKINPART_TILE_CENTER);
			return OpStatus::OK;
		}
		return OpStatus::ERR;
	}
};

void ExtensionInstaller::ShowInstallNotificationSuccess(OpGadget* gadget)
{
	OP_ASSERT(m_gclass);
	if (!m_gclass || m_silent_installation)
		return;

	if (ExtensionUtils::RequestsSpeedDialWindow(*m_gclass))
	{
		//allow ExtensionSpeedDialHandler to carry on with the UX discovery
		return;
	}
	
	// Clean previous request, just in case we have some
	m_button_firmness_timer.Stop();
	m_button_show_timer.Stop();

	if (m_delayed_notification.gadget_id.HasContent())
	{
		// At this point we are processing another extension installation, 
		// while intallation popup for previous extension was not yet shown. 
		// Show regular notification now and proceed with next one.. 
		InternalShowInstallNotificationSuccess(NULL); 
	}
	
	m_delayed_notification.follow = NULL;
	
	OpStatus::Ignore(m_gclass->GetGadgetName(m_delayed_notification.title));
	OpStatus::Ignore(gadget->GetIdentifier(m_delayed_notification.gadget_id));

	FallbackInstallerIconProvider fallback_provider;
	// here icons are also scaled up if necessary, because smaller icons are not centered
	// in notification window
	OpStatus::Ignore(GadgetUtils::GetGadgetIcon(m_delayed_notification.icon, m_gclass, 64, 64, 0, 0, TRUE, TRUE, fallback_provider));

	m_button_show_timer.Start(UI_TIMEOUT);
	m_waiting_for_button = true;
}

void ExtensionInstaller::InternalShowInstallNotificationSuccess(OpWidget* button)
{
	OpString text,temp,info;

	RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_EXTENSIONS_INSTALL_NOTIFICATION_TEXT, temp));
	RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::D_EXTENSIONS_INSTALL_NOTIFICATION_TEXT_INFO, info));

	OpStatus::Ignore(text.AppendFormat(temp.CStr(), m_delayed_notification.title.CStr()));

	OpString nTitle;
	RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_EXTENSIONS_NOTIFICATION, nTitle));

	OpInputAction* notification_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_MANAGE));
	if (!notification_action)
		return;
	notification_action->SetActionDataString(nTitle.CStr());

	if (button)
	{
		g_notification_manager->ShowNotification(
			DesktopNotifier::EXTENSION_NOTIFICATION,
			text,
			info,
			m_delayed_notification.icon,
			notification_action,
			NULL,
			TRUE,
			button);
	}
	else
	{
		g_notification_manager->ShowNotification(
			DesktopNotifier::EXTENSION_NOTIFICATION,
			text,
			info,
			m_delayed_notification.icon,
			notification_action,
			NULL,
			TRUE);
	}

	m_delayed_notification.icon = Image();
	m_delayed_notification.gadget_id.Empty();
	m_delayed_notification.title.Empty();
	m_delayed_notification.follow = NULL;
}

/*
The order below is chosen to minimize the time spent in this function.
Most of the time, the delay timer is not running, so it will exit as soon
as op_nan exits (itself - flag checker inside a double).

If, however, there actually is an installation to be reported, the parent vs active
should be more restricting and quickier than OpStringC::Compare on 20-something characters:
if there is a UI button created, then it's copy is created for every tab and every copy would
hold the same gadget id...

So, let's check if the button is on the active document and if yes, check if this is
button from the extension we look for.
*/
void ExtensionInstaller::CheckUIButtonForInstallNotification(OpExtensionButton* button, BOOL show)
{
	if (!m_waiting_for_button)
		return;

	if ((m_delayed_notification.follow == button) && !show) 
	{
		m_delayed_notification.follow = NULL;
		return;
	}

	DesktopWindow* parent_window = button->GetParentDesktopWindow();
	if (!parent_window)
		return;

	OpWorkspace* workspace = parent_window->GetWorkspace();
	if (!workspace)
		return;

	DesktopWindow* active_window = workspace->GetActiveDesktopWindow();
	if (parent_window != active_window)
		return;

	if (m_delayed_notification.gadget_id.Compare(button->GetGadgetId()) != 0)
		return;

	m_button_show_timer.Stop();

	m_delayed_notification.follow = button;

	m_button_firmness_timer.Start(SHOW_TICK); //wait the button is both visible and sane (e.g. size is non-zero)
}

void ExtensionInstaller::InstallCanceled()
{
	NotifyExtensionInstallAborted();
	CleanUp();
	FinalizeInstallation();
}

void ExtensionInstaller::InstallFailed(bool reason_network)
{
	NotifyExtensionInstallFailed();
	FinalizeInstallation();
	if (m_silent_installation)
	{
		return;
	}
	OpString title;
	OpString message;
	
	g_languageManager->GetString(Str::S_ERROR_EXTENSION_FAILURE_TITLE, title);

	if (reason_network)
	{
		g_languageManager->GetString(Str::SI_ERR_NETWORK_PROBLEM, message);
	}
	else
	{
		g_languageManager->GetString(Str::S_ERROR_EXTENSION_INVALID, message);
	}
	

	SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
	if (dialog)
	{
		if(OpStatus::IsError(dialog->Init(WINDOW_NAME_SERVICE_INSTALL_FAILED, title, message,
			g_application->GetActiveDesktopWindow(), Dialog::TYPE_OK,Dialog::IMAGE_ERROR)))
			OP_DELETE(dialog);
	}
}

OP_STATUS ExtensionInstaller::CheckIfTheSameExist(OpGadgetClass* gclass, VersionType& version, OpGadget** gadget)
{
	version = NoVersionFound;

	OP_ASSERT(gclass);
	if (!gclass)
	{
		return OpStatus::ERR;
	}

	OpStringC widg_id(gclass->GetAttribute(WIDGET_ATTR_ID));
	UINT dummy = 0;
	if (widg_id.HasContent())
		RETURN_IF_ERROR(ExtensionUtils::FindNormalExtension(widg_id, gadget, dummy));
	else
		return OpStatus::OK;

	if (!*gadget)
	{
		return OpStatus::OK;
	}

	version = OtherVersion;		
	if (OpStringC((*gadget)->GetClass()->GetGadgetVersion()) == gclass->GetGadgetVersion())
	{
		version = SameVersion;
	}

	return OpStatus::OK;
}

OP_STATUS ExtensionInstaller::ReplaceExtension(OpGadgetClass* gclass,OpGadget** gadget)
{
	OP_ASSERT(gclass);
	if (!gclass)
	{
		return OpStatus::ERR;
	}

	const uni_char* gid = gclass->GetGadgetId();
	OP_ASSERT(gid);
	UINT dummy = 0;
	RETURN_IF_ERROR(
			ExtensionUtils::FindNormalExtension(gid, gadget,dummy)); 
	if (!gadget)
	{
		return OpStatus::ERR;
	}

	OpString widgets_path; // widgets location in profile 
	OpString gadget_path;  // old gadget path 

	RETURN_IF_ERROR(gadget_path.Set((*gadget)->GetGadgetPath()));

	// we need to check if upgradable gadget is installed in widgets folder
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_GADGET_FOLDER, widgets_path));

	BOOL external_gadget = FALSE;

	if (gadget_path.Length()<widgets_path.Length())
	{
		external_gadget = TRUE;
	}
	else
	{
		if ( 0 != widgets_path.Compare(gadget_path,widgets_path.Length()))
		{
			external_gadget = TRUE;
		}
	}

	RETURN_IF_ERROR(g_desktop_extensions_manager->DisableExtension((*gadget)->GetIdentifier()));

	OpString gadget_path_new;
	if (!external_gadget)
	{
		RETURN_IF_ERROR(gclass->GetGadgetPath(gadget_path_new));
		RETURN_IF_ERROR(ReplaceOexFile(gadget_path, gadget_path_new, TRUE));
		RETURN_IF_ERROR(gadget_path_new.Set(gadget_path));
	}
	else
	{
		RETURN_IF_ERROR(gadget_path_new.Set(gclass->GetGadgetPath()));
	}

	RETURN_IF_ERROR(g_gadget_manager->UpgradeGadget(*gadget,
			gadget_path_new, URL_EXTENSION_INSTALL_CONTENT));

	(*gadget)->Reload();

	return OpStatus::OK;
}

void ExtensionInstaller::DownloadSucceeded(const OpStringC& tmp_path)
{
	NotifyDownloadFinished();
	if (OpStatus::IsSuccess(g_desktop_gadget_manager->PlaceFileInWidgetsFolder(tmp_path, m_extension_path)))
	{
		if (OpStatus::IsSuccess(LoadGadgetClass()))
		{
			if (OpStatus::IsSuccess(InstallOrAskIfNeeded()))
				return;
		}
	}
	CleanUp();
	InstallFailed();
}

void ExtensionInstaller::DownloadFailed()
{
	NotifyDownloadFinished();
	InstallFailed(true);
}

void ExtensionInstaller::DownloadInProgress(const URL& url)
{
	NotifyDownloadProgress(url.ContentLoaded(), url.GetContentSize());
}

void ExtensionInstaller::CleanUp()
{
	if (m_gclass && !m_dev_mode_install)
	{
		OpStatus::Ignore(g_gadget_manager->RemoveGadget(
				m_gclass->GetGadgetPath()));
	}
}


BOOL ExtensionInstaller::HasDifferentAccess(OpGadget* gadget, const OpStringC& file_path)
{
	BOOL different = TRUE;
	OpString error_reason;
	OpGadgetClass *gclass_new = g_gadget_manager->CreateClassWithPath(file_path.CStr(),URL_EXTENSION_INSTALL_CONTENT, NULL, error_reason);
	
	if (!gclass_new)
	{
		return FALSE;
	}
	
	if (ExtensionUtils::RequestsSpeedDialWindow(*gadget->GetClass()) ^ 
		ExtensionUtils::RequestsSpeedDialWindow(*gclass_new))
	{
		return TRUE;
	}

	//
	// Check access list first
	//
	OpGadgetAccess *a_orig = gadget->GetClass()->GetFirstAccess();
	OpGadgetAccess *a_new  = gclass_new->GetFirstAccess();
		
	while (a_new && a_orig && uni_strcmp(a_new->Name(), a_orig->Name()) == 0 
			&& a_new->Subdomains() == a_orig->Subdomains())
	{
		a_new =  static_cast<OpGadgetAccess*>(a_new->Suc());
		a_orig = static_cast<OpGadgetAccess*>(a_orig->Suc());
	}

	if (a_new || a_orig)
		return TRUE;
	else
	{
		different = FALSE;
		// And now - it might happen that one has access to userJS and other - 
		// don't, so last check here
		BOOL orig_has_userjs_includes = FALSE;
		BOOL new_has_userjs_includes = FALSE;
		RETURN_IF_ERROR(ExtensionUtils::HasExtensionUserJSIncludes(gadget->GetClass(), orig_has_userjs_includes));
		RETURN_IF_ERROR(ExtensionUtils::HasExtensionUserJSIncludes(gclass_new, new_has_userjs_includes));
		
		if (orig_has_userjs_includes != new_has_userjs_includes)
		{
			different = TRUE;
		}

	}		
	return different;
}

OP_STATUS ExtensionInstaller::InstallDelayedUpdate(OpGadget* gadget, BOOL& updated, BOOL force)
{
	updated = FALSE;
	OP_ASSERT(gadget);
	if (!gadget)
		return OpStatus::ERR;

	OpString update_file_path;
	g_desktop_gadget_manager->GetUpdateFileName(*gadget,update_file_path);

	OpFile upd_file;
	RETURN_IF_ERROR(upd_file.Construct(update_file_path));

	BOOL exist;
	if (OpStatus::IsSuccess(upd_file.Exists(exist))&& exist)
	{
		if (!force && HasDifferentAccess(gadget, update_file_path))
			return OpStatus::ERR;

		if (!g_desktop_extensions_manager->IsUpdateFromTrustedHost(*gadget))
		{
			return OpStatus::ERR;
		}

		RETURN_IF_ERROR(ReplaceOexFile(gadget->GetGadgetPath(),update_file_path,TRUE));

		RETURN_IF_ERROR(g_gadget_manager->UpgradeGadget(gadget,gadget->GetGadgetPath(),
			URL_EXTENSION_INSTALL_CONTENT));
		gadget->Reload();

		updated = TRUE;
	}
	return OpStatus::OK;
}


OP_STATUS ExtensionInstaller::ReplaceExtension(OpGadget** for_update,const OpStringC& update_source,
											  BOOL& delayed)
{
	delayed = FALSE;
	
	if (!(*for_update))
	{
		return OpStatus::ERR;
	}

	BOOL is_ext_running = FALSE;
	Window* wnd = (*for_update)->GetWindow();
	if (wnd)
	{
		is_ext_running = TRUE;
	}

	if (is_ext_running || HasDifferentAccess(*for_update, update_source.CStr()))
	{
		// so this will be delayed update
		delayed = TRUE;
		OpFile destination, source;

		OpString update_file_path;
		RETURN_IF_ERROR(g_desktop_gadget_manager->GetUpdateFileName(**for_update,update_file_path));

		RETURN_IF_ERROR(destination.Construct(update_file_path));
		RETURN_IF_ERROR(source.Construct(update_source));
		
		RETURN_IF_ERROR(destination.CopyContents(&source, FALSE));
	}
	else
	{
		OpString gadget_path;
		(*for_update)->GetClass()->GetGadgetPath(gadget_path);

		RETURN_IF_ERROR(ReplaceOexFile(gadget_path.CStr(),update_source.CStr()));
		
		RETURN_IF_ERROR(g_gadget_manager->UpgradeGadget((*for_update),
				gadget_path, URL_EXTENSION_INSTALL_CONTENT));
		
		RETURN_IF_ERROR(
				g_desktop_extensions_manager->RefreshModel());
	}

	return OpStatus::OK;
}


OP_STATUS ExtensionInstaller::ReplaceOexFile(const OpStringC& destination, const OpStringC& source, 
		BOOL delete_source)
{

		OpFile orig_service, new_service;
		RETURN_IF_ERROR(orig_service.Construct(destination));
		orig_service.Delete(); // delete old extension

		RETURN_IF_ERROR(new_service.Construct(source));
		RETURN_IF_ERROR(orig_service.CopyContents(&new_service, FALSE));

		if (delete_source)
			new_service.Delete();

		return OpStatus::OK;
}

void ExtensionInstaller::CancelDownload()
{
	OP_DELETE(m_simple_download_item);
	m_simple_download_item = NULL;
	NotifyDownloadFinished();
	InstallCanceled();
}

void ExtensionInstaller::FinalizeInstallation()
{
	m_busy = FALSE;
}

OP_STATUS ExtensionInstaller::AddListener(Listener* listener)
{
	if (!listener)
		return OpStatus::ERR;

	return m_listeners.Add(listener);
}

OP_STATUS ExtensionInstaller::RemoveListener(Listener* listener)
{
	return m_listeners.Remove(listener);
}

void ExtensionInstaller::NotifyExtensionInstalled(const OpGadget& gadget)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnExtensionInstalled(gadget,m_url_name);
}

void ExtensionInstaller::NotifyExtensionInstallAborted()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnExtensionInstallAborted();
}

void ExtensionInstaller::NotifyExtensionInstallFailed()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnExtensionInstallFailed();
}

void ExtensionInstaller::OnTimeOut(OpTimer* timer)
{
	m_waiting_for_button = false;
	if (timer == &m_button_show_timer)
	{
		InternalShowInstallNotificationSuccess(NULL);
	}
	else if (timer == &m_button_firmness_timer)
	{
		if (!m_delayed_notification.follow)
		{
			InternalShowInstallNotificationSuccess(NULL);
			return;
		}

		OpRect rect = m_delayed_notification.follow->GetRect(TRUE);
		if (rect.width && rect.height)
		{
			InternalShowInstallNotificationSuccess(m_delayed_notification.follow);
		}
	}
}

void ExtensionInstaller::NotifyDownloadStarted()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnExtensionDownloadStarted(*this);
}

void ExtensionInstaller::NotifyDownloadProgress(OpFileLength loaded, OpFileLength total)
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnExtensionDownloadProgress(loaded, total);
}

void ExtensionInstaller::NotifyDownloadFinished()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnExtensionDownloadFinished();
}

OP_STATUS ExtensionInstaller::InitializeExtension(const uni_char* id, BOOL ssl_access, BOOL private_access)
{
	RETURN_IF_ERROR(g_desktop_extensions_manager->RefreshModel());
	RETURN_IF_ERROR(g_desktop_extensions_manager->SetExtensionTemporary(id, m_temp_mode));
	RETURN_IF_ERROR(g_desktop_extensions_manager->SetExtensionSecurity(id, ssl_access, private_access));
	return g_desktop_extensions_manager->EnableExtension(id);
}
