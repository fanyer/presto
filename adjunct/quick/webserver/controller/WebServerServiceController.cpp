/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef GADGET_SUPPORT
#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/controller/WebServerServiceController.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/models/ServerWhiteList.h"
#include "adjunct/quick/panels/UniteServicesPanel.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/webserver/controller/WebServerServiceDownloadContext.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/viewers/viewers.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/util/filefun.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "modules/util/zipload.h"

#ifdef FORMATS_URI_ESCAPE_SUPPORT
	#include "modules/formats/uri_escape.h"
#endif // FORMATS_URI_ESCAPE_SUPPORT

#include "modules/regexp/include/regexp_api.h" 

/***********************************************************************************
**  WebServerServiceController::WebServerServiceController
************************************************************************************/
WebServerServiceController::WebServerServiceController()
{
}


/***********************************************************************************
**  WebServerServiceController::~WebServerServiceController
************************************************************************************/
WebServerServiceController::~WebServerServiceController()
{
	// If this hits we have unhandled pending installs!
	OP_ASSERT(m_pending_installs.GetCount() == 0);
}


/***********************************************************************************
**  WebServerServiceController::CreateServiceContextFromPath
************************************************************************************/
WebServerServiceSettingsContext*
WebServerServiceController::CreateServiceContextFromPath(const OpStringC & file_path, const OpStringC & remote_location)
{
	OpString svc_path, error_reason;
	svc_path.Set(file_path);
	OpGadgetClass * service_class = g_gadget_manager->CreateClassWithPath(svc_path, URL_UNITESERVICE_INSTALL_CONTENT, NULL, error_reason);

	if (service_class)
	{
		WebServerServiceSettingsContext* service_context = WebServerServiceSettingsContext::CreateContextFromServiceClass(service_class);
	
		service_context->SetLocalDownloadPath(file_path);
		service_context->SetRemoteDownloadLocation(remote_location);

		return service_context; 
	}
	return NULL;
}	


/***********************************************************************************
**  WebServerServiceController::CreateServiceContextFromHotlistItem
************************************************************************************/
WebServerServiceSettingsContext*
WebServerServiceController::CreateServiceContextFromHotlistItem(UniteServiceModelItem * item)
{
	if(item)
	{
		OpGadget* service = const_cast<UniteServiceModelItem*>(item)->GetOpGadget();
		if (service)
		{
			WebServerServiceSettingsContext* service_context = WebServerServiceSettingsContext::CreateContextFromService(service);
			
			HotlistManager::ItemData item_data;
			
			g_hotlist_manager->GetItemValue( item, item_data );
			service_context->SetFriendlyName(item_data.name);
			service_context->SetHotlistID(item->GetID());

			return service_context;
		}
	}

	return NULL;
}


/***********************************************************************************
 **  WebServerServiceController::ConfigurePreinstalledService
 ************************************************************************************/
OP_STATUS
WebServerServiceController::ConfigurePreinstalledService(UniteServiceModelItem * item)
{
	WebServerServiceSettingsContext* service_context = CreateServiceContextFromHotlistItem(item);
	
	OP_ASSERT(service_context);
	if (service_context)
	{
#ifdef STUB_UNITE_SERVICES_SUPPORT
	OpString escaped_id;
#ifdef FORMATS_URI_ESCAPE_SUPPORT
	UriEscape::AppendEscaped(escaped_id, service_context->GetWidgetID().CStr(), UriEscape::AllUnsafe);
#else
	escaped_id.Set(service_context->GetWidgetID().CStr());
#endif // FORMATS_URI_ESCAPE_SUPPORT

	OpString download_url;

	download_url.AppendFormat(UNI_L("http://unite.opera.com/service/download/force/0/?serviceid=%s"), escaped_id.CStr());
	g_application->OpenURL(download_url, NO, NO, YES);
	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif // STUB_UNITE_SERVICES_SUPPORT

	}
	return OpStatus::ERR;
}


/***********************************************************************************
 **  WebServerServiceController::ChangeServiceSettings
 ************************************************************************************/
OP_STATUS
WebServerServiceController::ChangeServiceSettings(const WebServerServiceSettingsContext & service_context)
{
	OP_STATUS status = OpStatus::OK;
	BOOL need_restart = FALSE;

	OpGadget* service = g_gadget_manager->FindGadget(GADGET_FIND_BY_GADGET_ID, service_context.GetWidgetID());
	if (service)
	{
		if (service_context.GetServiceNameInURL().HasContent())
		{
			//OP_ASSERT(DesktopGadgetManager::GetInstance().IsUniqueServerName(service_context.GetServiceName(), gmi));
			OpString ui_data_servicename;
			ui_data_servicename.Set(service->GetUIData(UNI_L("serviceName")));
			if (ui_data_servicename.IsEmpty() || ui_data_servicename.Compare(service_context.GetServiceNameInURL().CStr()) != 0)
			{
				service->SetUIData(UNI_L("serviceName"), service_context.GetServiceNameInURL().CStr());
				need_restart = TRUE;
			}
		}
		if (service_context.GetFriendlyName().HasContent())
		{
			service->SetGadgetName(service_context.GetFriendlyName().CStr());
		}

#ifdef WEBSERVER_CAP_SET_VISIBILITY
		service->SetVisibleASD(service_context.IsVisibleASD());
		service->SetVisibleUPNP(service_context.IsVisibleUPNP());
		service->SetVisibleRobots(service_context.IsVisibleRobots());
#endif // WEBSERVER_CAP_SET_VISIBILITY

#ifdef GADGETS_CAP_HAS_SHAREDFOLDER
		const uni_char *old_folder = service->GetSharedFolder();
		if(!old_folder || (old_folder && service_context.GetSharedFolderPath().Compare(old_folder)))
		{
			// rudimentary check for if the folder has changed
			need_restart = TRUE;
		}
		service->SetSharedFolder(service_context.GetSharedFolderPath().CStr());
#endif // GADGETS_CAP_HAS_SHAREDFOLDER

		HotlistModel *model = g_hotlist_manager->GetUniteServicesModel();
		if(model)
		{
			UniteServiceModelItem *item = static_cast<UniteServiceModelItem *>(model->GetItemByID(service_context.GetHotlistID()));
			if(item)
			{
				if (item->NeedsConfiguration()) // service is now configured
				{
					item->SetNeedsConfiguration(FALSE);
				}

				HotlistManager::ItemData item_data;

				g_hotlist_manager->GetItemValue( item, item_data );

				if (item_data.name.Compare(service_context.GetFriendlyName()) != 0)
				{
					item_data.name.Set(service_context.GetFriendlyName());
					need_restart = TRUE;
				}

				g_hotlist_manager->SetItemValue( item, item_data, TRUE, HotlistModel::FLAG_NAME);
				// select the item in the Unite panel
				OpTreeView * tree_view = GetUniteItemsTreeView();
				if (tree_view)
				{
					tree_view->SetSelectedItemByID(item->GetID());
				}
				if(need_restart)
				{
					status = item->ShowGadget(FALSE, FALSE);
					if(OpStatus::IsSuccess(status))
					{
						status = item->ShowGadget(TRUE, TRUE);
					}
				}
			}
		}
	}

	return status;
}


/***********************************************************************************
**  WebServerServiceController::IsValidServiceNameInURL
************************************************************************************/
BOOL
WebServerServiceController::IsValidServiceNameInURL(const OpStringC & service_address)
{
	if (!service_address.HasContent() || (service_address.Compare(UNI_L(".")) == 0 || service_address.Compare(UNI_L("..")) == 0))
	{
		return FALSE;
	}

	OpStringC pattern(UNI_L("[:/\\?#@!\\$&'()\\*\\+,;=<>\\[\\]\\\\]")); // maching these chars (see [DSK-255524]): :/?#[]@!$&'()*+,;=\\<>
	OpRegExp *regex;

	OpREFlags flags;
	flags.multi_line = FALSE;
	flags.case_insensitive = FALSE;
	flags.ignore_whitespace = FALSE;

	if (OpStatus::IsError(OpRegExp::CreateRegExp(&regex, pattern.CStr(), &flags)))
	{
		OP_ASSERT(FALSE); // faulty regex?
		return FALSE;
	}
	
	OpREMatchLoc match_pos;
	OP_STATUS status = regex->Match(service_address.CStr(), &match_pos);
	OP_DELETE(regex);

	if (OpStatus::IsError(status))
	{
		OP_ASSERT(FALSE);
		return FALSE;
	}

	if (match_pos.matchloc == REGEXP_NO_MATCH)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/***********************************************************************************
 **  WebServerServiceController::IsValidTrustedServer
 ************************************************************************************/
BOOL
WebServerServiceController::IsValidTrustedServer(const OpStringC & server_url)
{
	return g_server_whitelist->IsValidEntry(server_url);
}

/***********************************************************************************
 **  WebServerServiceController::AddTrustedServer
 ************************************************************************************/
void
WebServerServiceController::AddTrustedServer(const OpStringC & server_url)
{
	g_server_whitelist->AddServer(server_url);
}

/***********************************************************************************
 **  WebServerServiceController::IsServiceInstalled
 ************************************************************************************/
WebServerServiceController::VersionType
WebServerServiceController::IsServiceInstalled(const WebServerServiceSettingsContext& service_context)
{
	OpString service_identifier;

	if (service_context.GetWidgetID().HasContent())
		service_identifier.Set(service_context.GetWidgetID().CStr());
	else if (service_context.GetRemoteDownloadLocation().HasContent())
		service_identifier.Set(service_context.GetRemoteDownloadLocation().CStr());
	else
		return NoVersionFound;

	VersionType type = NoVersionFound;
	HotlistModel * model = g_hotlist_manager->GetUniteServicesModel();
	for (INT32 i = 0; i < model->GetCount(); i++)
	{
		HotlistModelItem* item = model->GetItemByIndex(i);
		if (item->IsUniteService() && !item->GetIsInsideTrashFolder())
		{
			UniteServiceModelItem* unite_item = static_cast<UniteServiceModelItem*>(item);
			if (unite_item && unite_item->EqualsID(service_identifier))
			{
				OpGadget * gadget = unite_item->GetOpGadget();
				OpString version_str;
				GadgetVersion version;
				if (gadget 
					&& OpStatus::IsSuccess(gadget->GetClass()->GetGadgetVersion(version_str))
					&& OpStatus::IsSuccess(version.Set(version_str)))
				{
					if (version < service_context.GetVersion())
					{
						if (unite_item->NeedsConfiguration())
						{
							return OlderVersionUnconfigured;
						}
						else
						{
							return OlderVersion;
						}
					}
					else if (version == service_context.GetVersion())
					{
						type = SameVersion;
					}
				}
				if (type == NoVersionFound)
				{
					type = NewerVersion;
				}
			}
		}
	}

	return type;
}

/***********************************************************************************
 **  WebServerServiceController::IsServicePreInstalled
 ************************************************************************************/
BOOL
WebServerServiceController::IsServicePreInstalled(const WebServerServiceSettingsContext& service_context)
{
	OpString service_identifier;

	if (service_context.GetWidgetID().HasContent())
		service_identifier.Set(service_context.GetWidgetID().CStr());
	else if (service_context.GetRemoteDownloadLocation().HasContent())
		service_identifier.Set(service_context.GetRemoteDownloadLocation().CStr());
	else
	{
		return FALSE;
	}

	HotlistModel * model = g_hotlist_manager->GetUniteServicesModel();

	for (INT32 i = 0; i < model->GetCount(); i++)
	{
		HotlistModelItem* item = model->GetItemByIndex(i);
		if (item->IsUniteService() && !item->GetIsInsideTrashFolder())
		{
			UniteServiceModelItem* svc = static_cast<UniteServiceModelItem*>(item);
			if (svc && svc->EqualsID(service_identifier))
			{
				if (svc->NeedsConfiguration())
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/***********************************************************************************
 **  WebServerServiceController::GetUniteItemsTreeView
 ************************************************************************************/
OpTreeView *
WebServerServiceController::GetUniteItemsTreeView()
{
	OpTreeView * tree_view = NULL;
	BrowserDesktopWindow * bdw = g_application->GetActiveBrowserDesktopWindow();
	Hotlist* hotlist = NULL;
	if (bdw)
	{
		hotlist = bdw->GetHotlist();
	}
	if (hotlist)
	{
		UniteServicesPanel * panel = static_cast<UniteServicesPanel *>(hotlist->GetPanelByType(Hotlist::PANEL_TYPE_UNITE_SERVICES));
		if (panel)
		{
			OpHotlistView * hotlist_view = panel->GetHotlistView();
			if (hotlist_view)
			{
				tree_view = hotlist_view->GetItemView();
			}
		}
	}
	return tree_view;
}

/***********************************************************************************
 **  WebServerServiceController::GetDowloadLocationForURL
 ************************************************************************************/
OP_STATUS
WebServerServiceController::GetDowloadLocationForURL(const URL & url, OpString & location)
{
	// Retrieve suggested file name
	OpString suggested_filename;
	RETURN_IF_LEAVE(url.GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));

	if (suggested_filename.IsEmpty())
	{
		RETURN_IF_ERROR(suggested_filename.Set("default"));

		OpString ext;
		TRAPD(op_err, url.GetAttributeL(URL::KSuggestedFileNameExtension_L, ext, TRUE));
		RETURN_IF_ERROR(op_err);
		if (ext.HasContent())
		{
			RETURN_IF_ERROR(suggested_filename.Append("."));
			RETURN_IF_ERROR(suggested_filename.Append(ext));
		}
	}

	// Build unique file name
	OpString filename;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TEMPDOWNLOAD_FOLDER, filename));
	if( filename.HasContent() && filename[filename.Length()-1] != PATHSEPCHAR )
	{
		RETURN_IF_ERROR(filename.Append(PATHSEP));
	}

	RETURN_IF_ERROR(filename.Append(suggested_filename.CStr()));

	BOOL exists = FALSE;

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Exists(exists));

	if (exists)
	{
		RETURN_IF_ERROR(CreateUniqueFilename(filename));
	}

	location.Set(filename);
	return OpStatus::OK;
}

/***********************************************************************************
 **  WebServerServiceController::MoveServiceToDestination
 ************************************************************************************/
OP_STATUS
WebServerServiceController::MoveServiceToDestination(WebServerServiceSettingsContext & service_context, BOOL copy_only)
{
	OpString service_path;
	RETURN_IF_ERROR(g_desktop_gadget_manager->PlaceFileInWidgetsFolder(
		service_context.GetLocalDownloadPath(),service_path,!copy_only));
	service_context.SetLocalDownloadPath(service_path);

	return OpStatus::OK;
}

/***********************************************************************************
 **  WebServerServiceController::GetUpgradableGadgets
 ************************************************************************************/
void
WebServerServiceController::GetUpgradableGadgets(OpVector<GadgetModelItem>& upgradable_gadgets, const WebServerServiceSettingsContext & service_context)
{
	HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();

	for (INT32 i = 0; i < model->GetCount(); i++)
	{
		HotlistModelItem* item = model->GetItemByIndex(i);
		if (item->IsUniteService() && !item->GetIsInsideTrashFolder()) // TODO: Use getdescendant inst.
		{
			GadgetModelItem* gdg = static_cast<GadgetModelItem*>(item);
			if (gdg)
			{
				if (gdg->EqualsID(service_context.GetWidgetID(), service_context.GetRemoteDownloadLocation()))
				{
					OpGadget * gadget = gdg->GetOpGadget();
					OpString version_str;
					GadgetVersion version;
					if (gadget 
						&& OpStatus::IsSuccess(gadget->GetClass()->GetGadgetVersion(version_str))
						&& OpStatus::IsSuccess(version.Set(version_str)))
					{
						if (version < service_context.GetVersion())
						{
							upgradable_gadgets.Add(gdg);
						}
					}
				}
			}
		}
	}
}

BOOL WebServerServiceController::IsServiceNameUnique(const OpStringC& current_service_identifier, const OpStringC& service_name_in_url)
{
	HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();

	for (INT32 i = 0; i < model->GetCount(); i++)
	{
		HotlistModelItem* item = model->GetItemByIndex(i);
		if (item->IsUniteService() && !item->GetIsInsideTrashFolder()) // TODO: Use getdescendant inst.
		{
			GadgetModelItem* gdg = static_cast<GadgetModelItem*>(item);
			if (gdg)
			{
				OpGadget* gadget = gdg->GetOpGadget();
				OpString service_name;

				if (gadget && OpStatus::IsSuccess(service_name.Set(gadget->GetUIData(UNI_L("serviceName")))))
				{
					if(!service_name.Compare(service_name_in_url))
					{
						if(current_service_identifier.HasContent())
						{
							if(current_service_identifier.Compare(gadget->GetIdentifier()))
							{
								// only report duplicate on difference services than ourself
								return FALSE;
							}
							/* else continue as it's our current service */
						}
						else
						{
							return FALSE;
						}
					}
				}
			}
		}
	}
	return TRUE;
}

#endif // WEBSERVER_SUPPORT
#endif // GADGET_SUPPORT
