/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Michal Zajaczkowski (mzajaczkowski)
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef PLUGIN_AUTO_INSTALL

#include "adjunct/quick/managers/AutoUpdateManager.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/desktop_util/file_utils/FileUtils.h"
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/libssl/updaters.h"
#include "modules/viewers/viewers.h"
#include "adjunct/quick/controller/PluginInstallController.h"
#include "adjunct/quick/Application.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "modules/skin/OpSkinManager.h"

// URL of the opera:plugins page that will be opened when a disabled plugin placeholder is clicked
static const uni_char* pim_opera_plugins_url = UNI_L("opera:plugins");
// Prefs file holding the information about mimetypes for which the user has suppressed the Plugin Missing Toolbar by clicking
// "Never for this plugin"
static const OpFileFolder pim_prefs_folder = OPFILE_HOME_FOLDER;
static const uni_char* pim_prefs_file = UNI_L("missing_plugins.ini");
static const uni_char* pim_prefs_section = UNI_L("Suppress Toolbar");
static const uni_char* pim_prefs_value = UNI_L("1");
// The folder where the plugin installers will be downloaded
static const OpFileFolder pim_download_folder = OPFILE_TEMPDOWNLOAD_FOLDER;

static const uni_char* PIM_ADOBE_SERVER_NAME = UNI_L("Adobe Flash Player");
// Due to Adobe legal stuff we need to show the text "Flash Player" in the checkbox instead of "Adobe Flash Player".
static const uni_char* PIM_ADOBE_CHECKBOX_NAME = UNI_L("Flash Player");

PluginInstallManager::PluginInstallManager():
	m_prefs_pfile(PREFS_STD),
	m_prefs_file_inited(FALSE),
	m_pref_enabled(FALSE),
	m_pref_resolver_max_items(0),
	m_ignore_target_pref(FALSE)
{
}

PluginInstallManager::~PluginInstallManager()
{
	m_prefs_file_inited = FALSE;

	if (g_pcui)
		g_pcui->UnregisterListener(this);

	if (g_autoupdate_manager)
		g_autoupdate_manager->RemoveListener(this);

	m_items.DeleteAll();
}

OP_STATUS PluginInstallManager::Init()
{
	ReadPrefs();
	TRAPD(err, g_pcui->RegisterListenerL(this));

	RETURN_IF_ERROR(g_autoupdate_manager->AddListener(this));

	// This is non-fatal if we fail to save the toolbar show settings
	OpStatus::Ignore(InitPrefsFile());

	return OpStatus::OK;
}
 
OP_STATUS PluginInstallManager::Notify(PIM_ManagerNotification pim_action, const OpStringC& mime_type, DesktopWindow* window, UpdatableResource* resource, const PluginInstallInformation* information, const OpStringC& plugin_content_url)
{
	OP_STATUS res = OpStatus::OK;
	switch(pim_action)
	{
	case PIM_PLUGIN_MISSING:
		res = ProcessPluginMissing(mime_type, window);
		break;
	case PIM_PLUGIN_RESOLVED:
		res = ProcessPluginResolved(mime_type, resource);
		break;
	case PIM_PLUGIN_DETECTED:
		res = ProcessPluginDetected(mime_type, window);
		break;
	case PIM_INSTALL_DIALOG_REQUESTED:
		res = ProcessInstallDialogRequested(mime_type, plugin_content_url);
		break;
	case PIM_MANUAL_INSTALL_REQUESTED:
		res = ProcessPluginInstallRequested(mime_type, FALSE);
		break;
	case PIM_AUTO_INSTALL_REQUESTED:
		res = ProcessPluginInstallRequested(mime_type, TRUE);
		break;
	case PIM_INSTALL_CANCEL_REQUESTED:
		res = ProcessInstallCancelRequested(mime_type);
		break;
	case PIM_INSTALL_OK:
		res = ProcessPluginInstallOK(mime_type);
		break;
	case PIM_INSTALL_FAILED:
		res = ProcessPluginInstallFailed(mime_type);
		break;
	case PIM_PLUGIN_DOWNLOAD_REQUESTED:
		res = ProcessPluginDownloadRequested(mime_type);
		break;
	case PIM_PLUGIN_DOWNLOAD_CANCEL_REQUESTED:
		res = ProcessPluginDownloadCancelRequested(mime_type);
		break;
	case PIM_PLUGIN_DOWNLOAD_STARTED:
		res = ProcessPluginDownloadStarted(mime_type);
		break;
	case PIM_PLUGIN_DOWNLOADED:
		res = ProcessPluginDownloaded(mime_type);
		break;
	case PIM_REFRESH_PLUGINS_REQUESTED:
		res = ProcessRefreshPlugins(mime_type);
		break;
	case PIM_REMOVE_WINDOW:
		res = ProcessRemoveWindow(window);
		break;
	case PIM_INSTALL_DIALOG_CLOSED:
		res = ProcessInstallDialogClosed(mime_type);
		break;
	case PIM_SET_DONT_SHOW:
		res = ProcessSetDontShow(mime_type);
		break;
	case PIM_ENABLE_PLUGIN_REQUESTED:
		res = ProcessEnablePluginRequested(mime_type);
		break;
	case PIM_RELOAD_ACTIVE_WINDOW:
		res = ProcessReloadActiveWindow();
		break;
	case PIM_PLUGIN_PLACEHOLDER_CLICKED:
		res = ProcessPluginPlaceholderClicked(mime_type, information);
		break;
	default:
		OP_ASSERT(!"PluginInstallManager::Notify: Unknown manager action");
		res = OpStatus::ERR;
	};
	return res;
}

void PluginInstallManager::ReadPrefs()
{
	m_pref_resolver_max_items = g_pcui->GetIntegerPref(PrefsCollectionUI::PluginAutoInstallMaxItems);
	m_pref_enabled = g_pcui->GetIntegerPref(PrefsCollectionUI::PluginAutoInstallEnabled);
}

OP_STATUS PluginInstallManager::ShowInstallPluginDialog(PIM_DialogMode dialog_mode, const OpStringC& mime_type, const OpStringC& plugin_content_url)
{
	PluginInstallDialog* install_dialog = OP_NEW(PluginInstallDialog, ());
	RETURN_OOM_IF_NULL(install_dialog);
	g_global_ui_context->AddChildContext(install_dialog);
	RETURN_IF_ERROR(install_dialog->ShowDialog(g_application->GetActiveDesktopWindow()));
	// This needs to be called *after* the dialog is shown
	RETURN_IF_ERROR(install_dialog->ConfigureDialogContent(dialog_mode, mime_type, plugin_content_url));

	SetIgnoreTarget();
	return OpStatus::OK;
}

PIM_PluginItem* PluginInstallManager::GetItemForMimeType(const OpStringC& a_mime_type)
{
	if (!m_pref_enabled)
		return NULL;

	if (a_mime_type.Length() == 0)
		return NULL;

	for (UINT32 i=0; i<m_items.GetCount(); i++)
		if (0 == m_items.Get(i)->GetMimeType().CompareI(a_mime_type))
			return m_items.Get(i);
	return NULL;
}

BOOL PluginInstallManager::IsPluginViewerPresent(const OpStringC& a_mime_type, BOOL a_reload_viewers)
{
	if (a_reload_viewers)
		OpStatus::Ignore(Notify(PIM_REFRESH_PLUGINS_REQUESTED, a_mime_type));

	Viewer* v = g_viewers->FindViewerByMimeType(a_mime_type);
	return (v && 
			(v->GetAction() == VIEWER_PLUGIN) && 
			(v->GetDefaultPluginViewerPath() != NULL));
}

OP_STATUS PluginInstallManager::EnablePluginViewer(const OpStringC& a_mime_type)
{
	int how_many = 0;
	Viewer* v = NULL;
	PluginViewer* pv = NULL;

	v = g_viewers->FindViewerByMimeType(a_mime_type);
	how_many = v->GetPluginViewerCount();

	for(int i=0;i<how_many;i++)
	{
		pv = v->GetPluginViewer(i);
		if(!pv->IsEnabled())
		{
			pv->SetEnabledPluginViewer(TRUE);
			g_plugin_viewers->SaveDisabledPluginsPref();
		}
	}
	return OpStatus::OK;
}

BOOL PluginInstallManager::IsPluginsEnabledGlobally()
{
	// Check if the plugins are disabled globally
	// Warning: core uses the document URL also when checking if the plugin should be
	// enabled for a particular content, however plugins are disabled/enabled
	// globally and we don't have the URL here anyway.
	return g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled);
}

BOOL PluginInstallManager::IsPluginViewerDisabled(const OpStringC& a_mime_type)
{
	Viewer* v = NULL;
	PluginViewer* enabled_pv = NULL;
	PluginViewer* default_pv = NULL;

	v = g_viewers->FindViewerByMimeType(a_mime_type);
	if (v)
	{
		default_pv = v->GetDefaultPluginViewer(FALSE);
		// Get plugin viewer, skip disabled ones
		enabled_pv = v->GetDefaultPluginViewer(TRUE);
	}

	// If there is a viewer but it's disabled
	if (v && default_pv && !enabled_pv)
		return TRUE;

	// The viewer is not disabled OR it's not there
	return FALSE;
}

OP_STATUS PluginInstallManager::GetPluginInfo(const OpStringC& mime_type, OpString& plugin_name, BOOL& plugin_available)
{
	plugin_available = FALSE;

	PIM_PluginItem* item = GetItemForMimeType(mime_type);
    if (!item)
		return OpStatus::ERR;

	if (!item->GetResource())
		return OpStatus::ERR;

	RETURN_IF_ERROR(item->GetResource()->GetAttrValue(URA_PLUGIN_NAME, plugin_name));
	plugin_available = item->GetIsAvailable();

	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginMissing(const OpStringC& mime_type, DesktopWindow* window)
{
/*
    The document in a_win sent a notification about missing plugin for a_mime_type.
    Now it's all ours responsibility to resolve (or fail to resolve) the a_mime_type for a plugin,
    show notifications to user and install the plugin if needed.
*/

	// Don't trigger anything if the prefence for this feature is off
	// Make this the first check since it's the fastest one
	if (FALSE == m_pref_enabled)
		return OpStatus::ERR;

	// No empty mimetypes, can't do much with them anyways
	if (mime_type.IsEmpty())
		return OpStatus::ERR;

	// DSK-347776: core >220 seems to be sending the missing plugin notification for OBJECT tags even despite
	// the plugin is already there. Make sure the viewer is NOT present in the system for the given mime-type,
	// that was what we relied on the core to do up to now.
	if (TRUE == IsPluginViewerPresent(mime_type, FALSE))
		return OpStatus::ERR;

	// Don't trigger anything if the plugins are disabled globally
	if (FALSE == IsPluginsEnabledGlobally())
		return OpStatus::ERR;

	// Don't trigger anything if the plugin for a given mimetype is disabled
	if (TRUE == IsPluginViewerDisabled(mime_type))
		return OpStatus::ERR;

    // Check if a_mime_type was already seen at all:
	PIM_PluginItem* item = GetItemForMimeType(mime_type);

    if (NULL == item)
    {
		// None found
		// Don't try to manage too many mimetypes at once, the usecase for having many different missing plugins is unlikely
		// and it wouldn't be wise to have too many items here as we iterate through them quite often.
		if (m_items.GetCount() >= GetPrefResolverMaxItems())
		{
			OP_ASSERT(!"ProcessPluginMissing: Too many items already, not adding another!");
			return OpStatus::ERR;
		}
		// Add new PIM_PluginItem to m_items
		// Add the calling window to the item's window collection
		item = OP_NEW(PIM_PluginItem, ());
		RETURN_OOM_IF_NULL(item);

		RETURN_IF_ERROR(item->SetMimeType(mime_type));
		RETURN_IF_ERROR(m_items.Add(item));
		RETURN_IF_ERROR(item->AddDesktopWindow(window));

		// The newly created item has the m_was_resolved flag set to false.
		// From now on we just wait for a response to come back then.
		RETURN_IF_ERROR(g_autoupdate_manager->CheckForAddition(UpdatableResource::RTPlugin, mime_type));
    }
    else
    {
        // The currently requested mime type was already requested
        // AddDocumentDesktopWindow will filter out multiple windows on its own
        RETURN_IF_ERROR(item->AddDesktopWindow(window));
		// Check if we got an answer from autoupdate about this mimetype
		BOOL plugin_is_resolved		= item->GetIsResolved();
        // Check if a plugin for this mimetype is available on the autoupdate server
		BOOL plugin_is_available	= item->GetIsAvailable();
        // Check if a plugin for this mimetype has been already installed during this Opera session
		BOOL plugin_was_installed	= item->GetIsInstalled();
		// Check if the installer was run in non-silent mode, we don't know if it's installed then
		BOOL plugin_maybe_installed	= (item->GetInstallMethod() == PIM_IM_MANUAL);
        // Decide what to do
        if (plugin_is_resolved)
        {
			if (plugin_is_available)
			{
				// Core will also notify about missing plugin when plugin is installed
				// but it failed to initalize due to 404 error when loading inline data
				// for example. We don't care about this case.
				if (!plugin_was_installed)
				{
					BOOL is_now_installed = FALSE;

					if (plugin_maybe_installed)
						// Just to be sure - reread the plugin store and decide what to do
						is_now_installed = IsPluginViewerPresent(mime_type, TRUE);

					if (is_now_installed)
					{
						// Hey, it was installed.
						if (window)
							window->OnPluginInstalled(mime_type);
					}
					else
					{
						// Plugin is available but was not installed yet, user needs to click the
						// "Install Plugin" button in the InfoBar, go and show it to the user.
						if (window)
							window->OnPluginAvailable(mime_type);
					}
				}
			}
			else
			{
				// Plugin is resolved but not available, this will not change. Do nothing.
			}
        }
        else
        {
			// Plugin is not resolved yet. Request the addition check again so that autoupdate request is made
			// right away since user just loaded the page and would like to see the placeholders updated.
			RETURN_IF_ERROR(g_autoupdate_manager->CheckForAddition(UpdatableResource::RTPlugin, mime_type));
        }
    }
    return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginDetected(const OpStringC& mime_type, DesktopWindow* window)
{
	PIM_PluginItem* item = GetItemForMimeType(mime_type);
	return !item ? OpStatus::OK : item->RemoveDesktopWindow(window);
}

OP_STATUS PluginInstallManager::ProcessRefreshPlugins(const OpStringC& a_mime_type)
{
	TRAPD(err, g_viewers->ReadViewersL());
	if (OpStatus::IsError(err) || OpStatus::IsError(g_plugin_viewers->RefreshPluginViewers(FALSE)))
	{
		OP_ASSERT(!"ProcessRefreshPlugins: Plugin refresh failed!");
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessRemoveWindow(DesktopWindow* window)
{
	for (UINT32 i=0; i<m_items.GetCount(); i++)
	{
		PIM_PluginItem* item = m_items.Get(i);
		if (item)
			// The window might not even be there in case if plugin auto installation is disabled
			// for the widget runtime, and that is perfectly valid.
			OpStatus::Ignore(item->RemoveDesktopWindow(window));
	}
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessInstallDialogRequested(const OpStringC& a_mime_type, const OpStringC& plugin_content_url)
{
	// Are plugins disabled globally? Or is the particular plugin disabled?
	if ((FALSE == IsPluginsEnabledGlobally()) || (TRUE == IsPluginViewerDisabled(a_mime_type)))
	{
		OpString opera_plugins_url;
		RETURN_IF_ERROR(opera_plugins_url.Set(pim_opera_plugins_url));
		BOOL open_success = g_application->OpenURL(opera_plugins_url, NO, YES, NO);
		if (!open_success)
			return OpStatus::ERR;

		return OpStatus::OK;
	}

	// If the preference is disabled we still show the redirect dialog
	if (!m_pref_enabled || a_mime_type.IsEmpty())
		if (a_mime_type.IsEmpty())
			return ShowInstallPluginDialog(PIM_DM_REDIRECT, NULL, plugin_content_url);
		else
			return ShowInstallPluginDialog(PIM_DM_REDIRECT, a_mime_type, plugin_content_url);

	// Perhaps the plugin is already installed, either from a different browser/copy of Opera/outside/earlier installalation
	// In this case a request to install will simply reload the viewers and the page so that the user sees the plugin content
	// immediately.
	OpStatus::Ignore(Notify(PIM_REFRESH_PLUGINS_REQUESTED, a_mime_type));

	if (IsPluginViewerPresent(a_mime_type))
		return Notify(PIM_RELOAD_ACTIVE_WINDOW, a_mime_type);

	PIM_PluginItem* item = GetItemForMimeType(a_mime_type);

	if (NULL == item)
	{
		// User clicked a placeholder with a mimetype that is not among items. If the plugin was installed earlier, we would
		// detect it after refreshing plugins above.
		// This means that this mimetype is not tracked by the manager because it could not be inserted to the items collection,
		// perhaps because we ran out of room for it (max item count exceeded).
		// We know nothing about this plugin, so show a redirect dialog.

		return ShowInstallPluginDialog(PIM_DM_REDIRECT, NULL, plugin_content_url);
	}

	UpdatablePlugin* resource = item->GetResource();
	PIM_ItemState state = item->GetCurrentState();

	switch (state)
	{
	case PIMIS_NotAvailable:
		// The item is not resolved at all. Maybe it will be resolved by the retry timer, but it isn't now, show the
		// redirect dialog.
		return ShowInstallPluginDialog(PIM_DM_REDIRECT, a_mime_type, plugin_content_url);
	case PIMIS_Available:
		// The item needs to be downloaded before installation, ask the user if he wants to start the download now

		// Check if the user requesting installation of plugin that is resolved but has no resource. 
		// Should not occur.
		if (resource)
			return ShowInstallPluginDialog(PIM_DM_NEEDS_DOWNLOADING, a_mime_type, plugin_content_url);
		else
			return OpStatus::ERR;
	case PIMIS_Downloading:
		if (resource)
			return ShowInstallPluginDialog(PIM_DM_IS_DOWNLOADING, a_mime_type, plugin_content_url);
		else
			return OpStatus::ERR;
	case PIMIS_Downloaded:
		if (item->GetInstallMethod() != PIM_IM_NONE)
			// Well, something is fishy here, the installation was perhaps cancelled last time.
			// Check if the plugin is available.
			OpStatus::Ignore(Notify(PIM_REFRESH_PLUGINS_REQUESTED, a_mime_type));
		if (item->GetIsInstalled())
			// Plugin is installed, refresh the page.
			return Notify(PIM_RELOAD_ACTIVE_WINDOW, a_mime_type);
		else
		{
			// Still not available, time to show the dialog.

			// Reset the install method since we're installing from scratch.
			item->SetInstallMethod(PIM_IM_NONE);
			RETURN_IF_ERROR(ShowInstallPluginDialog(PIM_DM_INSTALL, a_mime_type, plugin_content_url));
		}
		break;
	default:
		OP_ASSERT(!"Logic missing?");
		return OpStatus::ERR;
		break;
	}

	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginResolved(const OpStringC& a_mime_type, UpdatableResource* a_plugin)
{
	PIM_PluginItem* item = GetItemForMimeType(a_mime_type);
	if (NULL == item) 
	{
		OP_ASSERT(!"ProcessPluginResolved: Server sent us plugin that was never requested.");
		return OpStatus::ERR;
	}
	if (item->GetIsResolved())
	{
		OP_ASSERT(!"ProcessPluginResolved: Server sent us same plugin again.");
		return OpStatus::ERR;
	}
	if (NULL == a_plugin)
	{
		/*
		This means that the autoupdate server didn't send us anything in response to our request. This shouldn't really happen and means a server
		misconfiguration or server logic error.
		Don't mark the plugin as resolved therefore. The request for resolving the addition will be repeated the next time a missing plugin with
		this mime-type is found.
		*/
		item->SetResource(NULL);
	}
	else
	{
		item->SetIsResolved();
		GenerateOnPluginResolved(a_mime_type);

		// Found a plugin
		if (a_plugin->GetType() == UpdatableResource::RTPlugin)
			item->SetResource(static_cast<UpdatablePlugin*>(a_plugin));
		else
		{
			OP_ASSERT(!"ProcessPluginResolved: We have a non-NULL resource that is not a plugin. Very bad.");
			item->SetResource(NULL);
			return OpStatus::ERR;
		}
		// From now on this method has to return OpStatus::OK, or else autoupdater will delete the resource object
		// to which we keep a reference.
		if (item && item->GetResource())
		{
			// Check if we have an installer for this plugin and if there was a platform runner created for it
			bool has_installer;
			RETURN_IF_ERROR(item->GetResource()->GetAttrValue(URA_HAS_INSTALLER, has_installer));
			if (has_installer && item->GetResource()->GetAsyncProcessRunner())
			{
				item->OnPluginAvailable();
				GenerateOnPluginAvailable(a_mime_type);
			}
			else
			{
				// Plugin installer is not available or we can't install it
				return OpStatus::OK;
			}
		}
		else
		{
			OP_ASSERT(!"ProcessPluginResolved: NULL resource even it was set non-NULL!");
			// Return error since we have a NULL reference anyway
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginDownloadRequested(const OpStringC& a_mime_type)
{
	PIM_PluginItem* item = GetItemForMimeType(a_mime_type);
	if(NULL == item)
	{
		OP_ASSERT(!"ProcessPluginDownloadRequested: Willnot download a plugin that wasn't requested.");
		return OpStatus::ERR;
	}
	if(!(item->GetIsResolved() && item->GetIsAvailable()))
	{
		OP_ASSERT(!"ProcessPluginDownloadRequested: Willnot download a plugin that is not resolved or available.");
		return OpStatus::ERR;
	}

	OP_STATUS is_down_res = item->CheckAlreadyDownloadedAndVerified();
	if(is_down_res == OpStatus::OK)
	{
		// The plugin installer is already downloaded and passed checksum verification, signal it's ready to run
		RETURN_IF_ERROR(Notify(PIM_PLUGIN_DOWNLOADED, a_mime_type));
	}
	else
	{
		OP_STATUS res;
		// Start plugin installer download
		res = item->StartInstallerDownload();
		if(OpStatus::IsError(res))
		{
			// TODO: Some kind of retry mechanism if we need one (any form of automatic/unattended download?)
		}
		return res;
	}
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginDownloadCancelRequested(const OpStringC& a_mime_type)
{
	PIM_PluginItem* item = GetItemForMimeType(a_mime_type);
	if (NULL == item)
	{
		OP_ASSERT(!"ProcessPluginDownloadCancelRequested: Willnot cancel plugin download for plugin that wasn't requested");
		return OpStatus::ERR;
	}
	if (!(item->GetIsResolved() && item->GetIsAvailable()))
	{
		OP_ASSERT(!"ProcessPluginDownloadCancelRequested: Willnot cancel plugin download since its state is weird.");
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(item->CancelInstallerDownload());
	GenerateOnPluginDownloadCancelled(a_mime_type);
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginDownloadStarted(const OpStringC& mime_type)
{
	GenerateOnPluginDownloadStarted(mime_type);
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginDownloaded(const OpStringC& a_mime_type)
{
	// Either the plugin installer was downloaded just a moment ago or it was already present in 
	// the filesystem in which case it was checksum-verified.
	PIM_PluginItem* item = GetItemForMimeType(a_mime_type);
	if (NULL == item)
	{
		OP_ASSERT(!"ProcessPluginDownloaded: Can't find item for mimetype.");
		return OpStatus::ERR;
	}
	OP_STATUS check_res = item->CheckAlreadyDownloadedAndVerified();
	if (OpStatus::IsSuccess(check_res))
	{
		OP_ASSERT(item && item->GetResource());
		OpString notification_string, plugin_name;
		RETURN_IF_ERROR(item->GetResource()->GetAttrValue(URA_PLUGIN_NAME, plugin_name));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_NOTIFICATION_DOWNLOAD_FINISHDED, notification_string));

		OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_PLUGIN_INSTALL_SHOW_DIALOG));
		RETURN_OOM_IF_NULL(action);
		action->SetActionDataString(a_mime_type.CStr());

		Image plugin_notification_image;
		OpSkinElement* plugin_skin_element = g_skin_manager->GetSkinElement("Install Plugin Dialog");
		if (plugin_skin_element)
			plugin_notification_image = plugin_skin_element->GetImage(0);

		g_notification_manager->ShowNotification(DesktopNotifier::PLUGIN_DOWNLOADED_NOTIFICATION,
										plugin_name,
										notification_string,
										plugin_notification_image,
										action,
										NULL,
										TRUE);

		GenerateOnPluginDownloaded(a_mime_type);
	}
	else
		GenerateOnFileDownloadFailed(a_mime_type);

	return check_res;
}

OP_STATUS PluginInstallManager::ProcessPluginInstallOK(const OpStringC& a_mime_type)
{
	// The installer process returned 0 eventually, it might be automatic or manual install.
	// The original manual installer process might as well have spawned a number of new processes,
	// so we don't really know where we stand now.

	PIM_PluginItem* item = GetItemForMimeType(a_mime_type);
	if(NULL == item)
	{
		OP_ASSERT(!"ProcessPluginInstallOK: Can't find item for mimetype.");
		return OpStatus::ERR;
	}
	// Refresh the plugin store, will set plugin installed flag if a plugin is available
	OP_STATUS refresh_status = Notify(PIM_REFRESH_PLUGINS_REQUESTED, a_mime_type);
	if (!OpStatus::IsSuccess(refresh_status))
		OP_ASSERT(!"ProcessPluginInstallOK(): Error refreshin plugins!");

	BOOL is_installed = item->GetIsInstalled();

	if(is_installed)
	{
		GenerateOnPluginInstalledOK(a_mime_type);
		// Switch the infobars for each window containing a document with an element with the given a_mime_type
		item->OnPluginInstalled();
		// Reload the active window
		Notify(PIM_RELOAD_ACTIVE_WINDOW, a_mime_type);
		
		// Delete the downloaded installer
		if(item->GetResource())
			item->GetResource()->Cleanup();

		// Normally we would expect here to remove the item that was installed from m_items so that more plugin mime-types could be 
		// recognized between browser restarts. This would base on an assumption that core won't call us back with a mime-type for
		// an installed plug-in, and this seemed to work fine, until DSK-347141.
		// It seems that core will notify us about a "missing" plugin even thou it is able to play it, perhaps due to the mess 
		// in the layout/docxs code that makes it almost impossible to determine betwen OBJECT/EMBED fallbacks, especially if both have
		// different mime-types.
		// Therefore, we DON'T remove the newly installed item from within the m_items collection. This should not be a problem anyway
		// since we don't support a great number of plugins deliverable via the plugin auto install.
	}
	else
	{
		GenerateOnPluginInstallFailed(a_mime_type);
		OP_ASSERT(!"ProcessPluginInstallOK: Can't find the plugin viewer even thou it should be installed");
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginInstallFailed(const OpStringC& a_mime_type)
{
	GenerateOnPluginInstallFailed(a_mime_type);

	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessPluginInstallRequested(const OpStringC& a_mime_type, BOOL a_silent)
{
	PIM_PluginItem* item = GetItemForMimeType(a_mime_type);
	if(NULL == item)
	{
		OP_ASSERT(!"ProcessPluginInstallRequested: Can't find item for mimetype.");
		return OpStatus::ERR;
	}
	OP_STATUS res = item->InstallPlugin(a_silent);
	if(!OpStatus::IsSuccess(res))
		RETURN_IF_ERROR(Notify(PIM_INSTALL_FAILED, a_mime_type));

	return res;
}

OP_STATUS PluginInstallManager::ProcessInstallCancelRequested(const OpStringC& a_mime_type)
{
	PIM_PluginItem* item = GetItemForMimeType(a_mime_type);
	if(NULL == item)
	{
		OP_ASSERT(!"ProcessInstallCancelRequested: Can't find item for mimetype.");
		return OpStatus::ERR;
	}
	return item->CancelPluginInstallation();
}

OP_STATUS PluginInstallManager::ProcessInstallDialogClosed(const OpStringC& a_mime_type)
{
	RestoreIgnoreTarget();
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessSetDontShow(const OpStringC& a_mime_type)
{
	return SetDontShowToolbar(a_mime_type);
}

OP_STATUS PluginInstallManager::ProcessEnablePluginRequested(const OpStringC& a_mime_type)
{
	if (!IsPluginsEnabledGlobally())
		RETURN_IF_ERROR(EnablePluginsGlobally());

	if(IsPluginViewerDisabled(a_mime_type))
		RETURN_IF_ERROR(EnablePluginViewer(a_mime_type));
	
	RETURN_IF_ERROR(Notify(PIM_RELOAD_ACTIVE_WINDOW, a_mime_type));
	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::ProcessReloadActiveWindow()
{
	OpInputAction a(OpInputAction::ACTION_RELOAD);
	DesktopWindow* win = g_application->GetActiveDesktopWindow();

	if (win && win->OnInputAction(&a))
		return OpStatus::OK;
	else
	{
		OP_ASSERT(!"ProcessReloadActiveWindow(): Reload window failed");
		return OpStatus::ERR;
	}
}

OP_STATUS PluginInstallManager::ProcessPluginPlaceholderClicked(const OpStringC& mime_type, const PluginInstallInformation* information)
{
	OP_ASSERT(information);
	OpString plugin_content_url;

	if (information->IsURLValidForDialog())
		RETURN_IF_ERROR(information->GetURLString(plugin_content_url, FALSE));

	PIM_PluginItem* item = GetItemForMimeType(mime_type);
	if (NULL == item)
		return Notify(PIM_INSTALL_DIALOG_REQUESTED, mime_type);

	PIM_ItemState state = item->GetCurrentState();

	switch (state)
	{
	case PIMIS_NotAvailable:
	case PIMIS_Available:
	case PIMIS_Downloading:
	case PIMIS_Downloaded:
		return Notify(PIM_INSTALL_DIALOG_REQUESTED, mime_type, NULL, NULL, NULL, plugin_content_url);
		break;
	case PIMIS_Installed:
		return Notify(PIM_RELOAD_ACTIVE_WINDOW, mime_type);
		break;
	default:
		OP_ASSERT(!"Shouldn't be here or logic missing");
		break;
	};

	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::InitPrefsFile()
{
	if (m_prefs_file_inited)
		return OpStatus::OK;

	RETURN_IF_ERROR(m_prefs_file.Construct(pim_prefs_file, pim_prefs_folder));
	RETURN_IF_LEAVE(m_prefs_pfile.ConstructL());
	RETURN_IF_LEAVE(m_prefs_pfile.SetFileL(&m_prefs_file));
	m_prefs_file_inited = TRUE;

	return OpStatus::OK;
}

OP_STATUS PluginInstallManager::EnablePluginsGlobally()
{
	RETURN_IF_LEAVE(g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::PluginsEnabled, 1));
	return OpStatus::OK;
}

BOOL PluginInstallManager::GetDontShowToolbar(const OpStringC& a_mime_type)
{
	OP_STATUS st = OpStatus::OK;
	OpString key, val;
	BOOL retval = FALSE;
	PrefsSection* prefs_section = NULL;

	if (!m_prefs_file_inited)
		RETURN_VALUE_IF_ERROR(InitPrefsFile(), FALSE);

	TRAP_AND_RETURN_VALUE_IF_ERROR(st, m_prefs_pfile.LoadAllL(), FALSE);
	TRAP_AND_RETURN_VALUE_IF_ERROR(st, prefs_section = m_prefs_pfile.ReadSectionL(pim_prefs_section), FALSE);

	if (!prefs_section)
		return FALSE;

	for (const PrefsEntry* entry = prefs_section->Entries(); entry != NULL; entry = static_cast<const PrefsEntry*>(entry->Suc()))
	{
		st = key.Set(entry->Key());
		if (OpStatus::IsError(st))
			break;

		if (a_mime_type.CompareI(key) == 0)
		{
			st = val.Set(entry->Value());
			if (OpStatus::IsError(st))
				break;
			if (val.CompareI(pim_prefs_value) == 0)
				retval = TRUE;
		}
	}

		OP_DELETE(prefs_section);
	return retval;
}

OP_STATUS PluginInstallManager::SetDontShowToolbar(const OpStringC& a_mime_type)
{
	if (a_mime_type.IsEmpty())
		return OpStatus::ERR;

	if (!m_prefs_file_inited)
		RETURN_IF_ERROR(InitPrefsFile());

	RETURN_IF_LEAVE(m_prefs_pfile.WriteStringL(pim_prefs_section, a_mime_type, pim_prefs_value));
	RETURN_IF_LEAVE(m_prefs_pfile.CommitL());

	return OpStatus::OK;
}

void PluginInstallManager::OnAdditionResolved(UpdatableResource::UpdatableResourceType type, const OpStringC& key, UpdatableResource* resource)
{
	if (UpdatableResource::RTPlugin != type)
		return;

	Notify(PIM_PLUGIN_RESOLVED, key, NULL, resource);
}

void PluginInstallManager::OnAdditionResolveFailed(UpdatableResource::UpdatableResourceType type, const OpStringC& key)
{
	if (UpdatableResource::RTPlugin != type)
		return;

	Notify(PIM_PLUGIN_RESOLVED, key, NULL, NULL);
}

/**
 * DocumentWindowListener
 */
void PluginInstallManager::OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url)
{
}

void PluginInstallManager::OnStartLoading(DocumentDesktopWindow* document_window)
{
}

void PluginInstallManager::OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user)
{
}

/**
 * OpPrefsListener
 */
void PluginInstallManager::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if(OpPrefsCollection::UI != id)
		return;

	switch(PrefsCollectionUI::integerpref(pref))
	{
		case PrefsCollectionUI::PluginAutoInstallMaxItems:
			m_pref_resolver_max_items = newvalue;
			break;
		case PrefsCollectionUI::PluginAutoInstallEnabled:
			m_pref_enabled = newvalue;
			break;
		default:
			break;
	}
}

void PluginInstallManager::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue)
{
// No strings prefs so far
}

void PluginInstallManager::SetIgnoreTarget()
{
	m_ignore_target_pref = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget);
	g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, TRUE);
}

void PluginInstallManager::RestoreIgnoreTarget()
{
	g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, m_ignore_target_pref);
}


void PluginInstallManager::GenerateOnPluginResolved(const OpStringC& a_mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPluginResolved(a_mime_type);
	}
}

void PluginInstallManager::GenerateOnPluginAvailable(const OpStringC& a_mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPluginAvailable(a_mime_type);
	}
}

void PluginInstallManager::GenerateOnPluginDownloaded(const OpStringC& a_mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPluginDownloaded(a_mime_type);
	}
}

void PluginInstallManager::GenerateOnPluginDownloadStarted(const OpStringC& mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPluginDownloadStarted(mime_type);
	}
}

void PluginInstallManager::GenerateOnPluginDownloadCancelled(const OpStringC& mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPluginDownloadCancelled(mime_type);
	}
}

void PluginInstallManager::GenerateOnPluginInstalledOK(const OpStringC& a_mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPluginInstalledOK(a_mime_type);
	}
}

void PluginInstallManager::GenerateOnPluginInstallFailed(const OpStringC& a_mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPluginInstallFailed(a_mime_type);
	}
}

void PluginInstallManager::GenerateOnPluginsRefreshed()
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPluginsRefreshed();
	}
}

void PluginInstallManager::GenerateOnFileDownloadProgress(const OpStringC& a_mime_type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long estimate)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFileDownloadProgress(a_mime_type, total_size, downloaded_size, kbps, estimate);
	}
}

void PluginInstallManager::GenerateOnFileDownloadDone(const OpStringC& a_mime_type, OpFileLength total_size)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFileDownloadDone(a_mime_type, total_size);
	}
}

void PluginInstallManager::GenerateOnFileDownloadFailed(const OpStringC& a_mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFileDownloadFailed(a_mime_type);
	}
}

void PluginInstallManager::GenerateOnFileDownloadAborted(const OpStringC& a_mime_type)
{
	for(OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnFileDownloadAborted(a_mime_type);
	}
}

void PluginInstallManager::OnFileDownloadProgress(const OpStringC& a_mime_type, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate)
{
	GenerateOnFileDownloadProgress(a_mime_type, total_size, downloaded_size, kbps, time_estimate);
}

void PluginInstallManager::OnFileDownloadDone(const OpStringC& a_mime_type, OpFileLength total_size)
{
	GenerateOnFileDownloadDone(a_mime_type, total_size);

	OpStatus::Ignore(Notify(PIM_PLUGIN_DOWNLOADED, a_mime_type));
}

void PluginInstallManager::OnFileDownloadFailed(const OpStringC& a_mime_type)
{
	GenerateOnFileDownloadFailed(a_mime_type);
}

void PluginInstallManager::OnFileDownloadAborted(const OpStringC& a_mime_type)
{
	GenerateOnFileDownloadAborted(a_mime_type);
}

/******************************************
	PIM_PluginItem
*******************************************/

PIM_PluginItem::~PIM_PluginItem()
{
	if(m_resource)
	{
		m_resource->StopDownload();
		m_resource->SetFileDownloadListener(NULL);
		m_resource->Cleanup();
		OP_DELETE(m_resource);
		m_resource = NULL;
	}
}

void PIM_PluginItem::SetResource(UpdatablePlugin* resource)
{
	OP_DELETE(m_resource);
	m_resource = resource;

	m_is_it_flash = FALSE;
	if (NULL == resource)
		return;

	OpString plugin_name;
	RETURN_VOID_IF_ERROR(m_resource->GetAttrValue(URA_PLUGIN_NAME, plugin_name));
	if (plugin_name.CompareI(PIM_ADOBE_SERVER_NAME) == 0)
			m_is_it_flash = TRUE;
}

void PIM_PluginItem::OnFileDownloadDone(FileDownloader* file_downloader, OpFileLength total_size)
{
	m_downloading = FALSE;
	g_plugin_install_manager->OnFileDownloadDone(m_mime_type, total_size);
}

void PIM_PluginItem::OnFileDownloadFailed(FileDownloader* file_downloader)
{
	m_downloading = FALSE;
	g_plugin_install_manager->OnFileDownloadFailed(m_mime_type);
}

void PIM_PluginItem::OnFileDownloadAborted(FileDownloader* file_downloader)
{
	m_downloading = FALSE;
	g_plugin_install_manager->OnFileDownloadAborted(m_mime_type);
}

void PIM_PluginItem::OnFileDownloadProgress(FileDownloader* file_downloader, OpFileLength total_size, OpFileLength downloaded_size, double kbps, unsigned long time_estimate)
{
	g_plugin_install_manager->OnFileDownloadProgress(m_mime_type, total_size, downloaded_size, kbps, time_estimate);
}

OP_STATUS PIM_PluginItem::CheckAlreadyDownloadedAndVerified()
{
	if(m_resource && m_resource->CheckResource())
		return OpStatus::OK;
	else
		return OpStatus::ERR;
}

OP_STATUS PIM_PluginItem::StartInstallerDownload()
{
	if (NULL == m_resource)
	{
		OP_ASSERT(!"StartInstallerDownload: Trying to download a NULL resource, very bad.");
		return OpStatus::ERR;
	}

	if (GetIsDownloaded())
		return OpStatus::ERR;

	// Ignore the retcode since the plugin installer file we're trying to delete might not even be there.
	OpStatus::Ignore(m_resource->Cleanup());
	RETURN_IF_ERROR(m_resource->StartDownloading(this));

	m_downloading = TRUE;
	RETURN_IF_ERROR(g_plugin_install_manager->Notify(PIM_PLUGIN_DOWNLOAD_STARTED, m_mime_type));

	// Notify all windows that are registered for this mimetype
	for(UINT32 i=0;i<m_windows.GetCount();i++)
	{
		DesktopWindow* win = m_windows.Get(i);
		OP_ASSERT(win);

		if(win)
			win->OnPluginDownloadStarted(m_mime_type);
	}

	return OpStatus::OK;
}

BOOL PIM_PluginItem::GetIsDownloaded()
{
	if (OpStatus::IsSuccess(CheckAlreadyDownloadedAndVerified()))
	{
		m_downloading = FALSE;
		return TRUE;
	}
	return FALSE;
}

BOOL PIM_PluginItem::GetIsDownloading()
{
	return m_downloading;
}

OP_STATUS PIM_PluginItem::CancelInstallerDownload()
{
	if (NULL == m_resource)
	{
		OP_ASSERT(!"CancelInstallerDownload: Trying to cancel download with a NULL resource.");
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(m_resource->StopDownload());
	// Ignore the retcode since the file might not even be there.
	OpStatus::Ignore(m_resource->Cleanup());

	m_downloading = FALSE;

	return OpStatus::OK;
}

OP_STATUS PIM_PluginItem::AddDesktopWindow(DesktopWindow* window)
{
	if (NULL == window)
		return OpStatus::OK;

	// Only add the window if it is not already there
	for (UINT i=0; i<m_windows.GetCount(); i++)
		if (m_windows.Get(i) == window)
			return OpStatus::ERR;

	// Okay, add it
	return m_windows.Add(window);
}

OP_STATUS PIM_PluginItem::RemoveDesktopWindow(DesktopWindow* window)
{
	return m_windows.RemoveByItem(window);
}

void PIM_PluginItem::OnPluginAvailable()
{
	m_available = TRUE;

	// Notify all windows that are registered for this mimetype
	for(UINT32 i=0;i<m_windows.GetCount();i++)
	{
		DesktopWindow* win = m_windows.Get(i);
		if(win)
		{
			win->OnPluginAvailable(m_mime_type);

			if (win->GetWindowCommander())
				win->GetWindowCommander()->OnPluginAvailable(m_mime_type.CStr());
		}
	}
}

void PIM_PluginItem::OnPluginInstalled()
{
	for(UINT32 i=0;i<m_windows.GetCount();i++)
	{
		DesktopWindow* win = m_windows.Get(i);
		if(win)
			win->OnPluginInstalled(m_mime_type);
	}
}

BOOL PIM_PluginItem::GetIsInstalled()
{
	return g_plugin_install_manager->IsPluginViewerPresent(m_mime_type);
}

OP_STATUS PIM_PluginItem::InstallPlugin(BOOL silent)
{
	if (NULL == m_resource)
		return OpStatus::ERR_NULL_POINTER;

	if (silent)
	{
		int installs_silently;
		RETURN_IF_ERROR(m_resource->GetAttrValue(URA_INSTALLS_SILENTLY, installs_silently));
		if (!installs_silently)
			return OpStatus::ERR;
	}

	if (silent)
		SetInstallMethod(PIM_IM_AUTO);
	else
		SetInstallMethod(PIM_IM_MANUAL);

	return m_resource->InstallPlugin(silent);
}

BOOL PIM_PluginItem::GetIsItFlash()
{
	return m_is_it_flash;
}

BOOL PIM_PluginItem::GetIsReadyForInstall()
{
	return GetIsAvailable() && GetIsResolved() && GetResource();
}

OP_STATUS PIM_PluginItem::CancelPluginInstallation()
{
	if (NULL == m_resource)
		return OpStatus::ERR_NULL_POINTER;

	return m_resource->CancelPluginInstallation();
}

PIM_ItemState PIM_PluginItem::GetCurrentState()
{
	OP_ASSERT(m_mime_type.HasContent());

	BOOL is_available = GetIsAvailable();
	BOOL is_downloading = GetIsDownloading();
	BOOL is_downloaded = GetIsDownloaded();
	BOOL is_installed = GetIsInstalled();

	if (is_installed)
	{
		OP_ASSERT(is_available && !is_downloading);
		return PIMIS_Installed;
	}

	if (is_downloaded)
	{
		OP_ASSERT(is_available && !is_downloading);
		return PIMIS_Downloaded;
	}

	if (is_downloading)
	{
		OP_ASSERT(is_available && !is_downloaded && !is_installed);
		return PIMIS_Downloading;
	}

	if (is_available)
	{
		return PIMIS_Available;
	}

	return PIMIS_NotAvailable;
}

OP_STATUS PIM_PluginItem::GetStringWithPluginName(Str::LocaleString string_id, OpString& ret_string)
{
	OpString plugin_name;
	OpString translated_string;

	if (NULL == m_resource)
		return OpStatus::ERR;

	if (Str::S_PLUGIN_AUTO_INSTALL_I_AGREE_TO_TOS == string_id && GetIsItFlash())
		RETURN_IF_ERROR(plugin_name.Set(PIM_ADOBE_CHECKBOX_NAME));
	else
		RETURN_IF_ERROR(m_resource->GetAttrValue(URA_PLUGIN_NAME, plugin_name));
	
	if (plugin_name.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(g_languageManager->GetString(string_id, translated_string));
	RETURN_IF_ERROR(ret_string.AppendFormat(translated_string.CStr(), plugin_name.CStr()));
	return OpStatus::OK;
}


/**
 * Platform implementation(s) of PIM_AsyncProcessRunner
 */

#ifdef MSWIN
PIM_WindowsAsyncProcessRunner::PIM_WindowsAsyncProcessRunner():
	m_process_handle(0)
{
	m_poll_timer.SetTimerListener(this);
}

void PIM_WindowsAsyncProcessRunner::OnProcessFinished(DWORD exit_code)
{
	m_is_running = FALSE;
	m_shell_exit_code = exit_code;
	m_process_handle = 0;
	if(m_listener)
		m_listener->OnProcessFinished(m_shell_exit_code);
}

void PIM_WindowsAsyncProcessRunner::OnTimeOut(OpTimer* timer)
{
	if(timer != &m_poll_timer)
	{
		OP_ASSERT(!"PIM_WindowsAsyncProcessRunner: Timer from outer space just fired.");
		return;
	}

	if(!m_is_running)
	{
		OP_ASSERT(!"PIM_WindowsAsyncProcessRunner: Timer fired while m_is_running is false.");
		return;
	}

	DWORD	wait_result;
	BOOL	get_exit_code_result;
	DWORD	exit_code;

	wait_result = ::WaitForSingleObject(m_process_handle, 0);

	switch(wait_result) 
	{
	case WAIT_OBJECT_0:
		get_exit_code_result = ::GetExitCodeProcess(m_process_handle, &exit_code);
		if(FALSE == get_exit_code_result)
		{
			OnProcessFinished(ProcessFailExitCode());
			return;
		}
		if(STILL_ACTIVE == exit_code)
		{
			// Still running...
			KickTimer();
			return;
		}
		OnProcessFinished(exit_code);
		return;
	case WAIT_TIMEOUT:
		// Still running...
		KickTimer();
		return;
	case WAIT_ABANDONED:
	case WAIT_FAILED:
		OP_ASSERT(!"PIM_WindowsAsyncProcessRunner: Something wicked happened while checking if installer is alive.");
		OnProcessFinished(ProcessFailExitCode());
		return;
	default:
		OP_ASSERT(!"PIM_WindowsAsyncProcessRunner: Now, THAT is weird.");
		OnProcessFinished(ProcessFailExitCode());
		return;
	}
}

void PIM_WindowsAsyncProcessRunner::KickTimer()
{
	m_poll_timer.Start(ProcessPollInterval());
}

OP_STATUS PIM_WindowsAsyncProcessRunner::RunProcess(const OpStringC& a_app_name, const OpStringC& a_app_params)
{
	if (m_is_running)
		return OpStatus::ERR_YIELD;

	RETURN_IF_ERROR(m_app_name.Set(a_app_name));
	RETURN_IF_ERROR(m_app_params.Set(a_app_params));

	m_shell_exit_code = PluginInstallManager::PIM_PROCESS_DEFAULT_EXIT_CODE;
	m_is_running = FALSE;

	if (m_app_name.Length() == 0)
		return OpStatus::ERR;

	SHELLEXECUTEINFO shell_info;

	memset(&shell_info, 0, sizeof(shell_info));
	shell_info.cbSize = sizeof(shell_info);
	// This will ensure that UAC dialog dims the screen
	shell_info.hwnd = GetForegroundWindow();
	shell_info.lpVerb = UNI_L("open");
	shell_info.lpFile = m_app_name.CStr();
	shell_info.lpParameters = m_app_params?m_app_params.CStr():UNI_L("");
	shell_info.nShow = SW_NORMAL;
	shell_info.fMask = SEE_MASK_NOCLOSEPROCESS;
	BOOL res = ::ShellExecuteEx(&shell_info);

	if (FALSE == res)
	{
		// ShellExecuteEx "not successful".
		return OpStatus::ERR;
	}

	if (shell_info.hInstApp <= (HINSTANCE)32)
	{
		// Could not run the process
		return OpStatus::ERR;
	}

	m_process_handle = shell_info.hProcess;
	m_is_running = TRUE;

	KickTimer();

	return OpStatus::OK;
}

OP_STATUS PIM_WindowsAsyncProcessRunner::KillProcess()
{
	if (!m_is_running)
	{
		OP_ASSERT(!"PIM_WindowsAsyncProcessRunner: Trying to kill a process that is not running");
		return OpStatus::ERR;
	}
	if (0 == m_process_handle)
	{
		OP_ASSERT(!"PIM_WindowsAsyncProcessRunner: Zero process handle");
		return OpStatus::ERR;
	}
	BOOL terminate_retcode = ::TerminateProcess(m_process_handle, 69);
	if (0 == terminate_retcode)
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}
#endif // MSWIN

#endif // PLUGIN_AUTO_INSTALL
#endif // AUTO_UPDATE_SUPPORT
