// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Huib Kleinhout
//
#include "core/pch.h"

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"

#include "adjunct/quick/Application.h"

#include "adjunct/quick/dialogs/SetupApplyDialog.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"

#include "adjunct/quick/windows/DesktopGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadget.h"

#include "modules/pi/OpPainter.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/util/zipload.h"
#include "modules/util/path.h"

#ifdef WEBSERVER_SUPPORT
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/models/ServerWhiteList.h"
#include "adjunct/quick/webserver/view/WebServerServiceSettingsDialog.h"
#include "adjunct/quick/webserver/controller/WebServerServiceDownloadContext.h"
#include "adjunct/quick/webserver/controller/WebServerServiceSettingsContext.h"
#endif // WEBSERVER_SUPPORT

#include "modules/windowcommander/src/TransferManagerDownload.h"
#include "modules/util/filefun.h"

#include "adjunct/quick_toolkit/widgets/OpPage.h"
#ifdef WIDGET_RUNTIME_SUPPORT
#include "adjunct/widgetruntime/pi/PlatformGadgetUtils.h"
#include "adjunct/widgetruntime/GadgetUtils.h"
#endif  // WIDGET_RUNTIME_SUPPORT

#ifdef GADGET_UPDATE_SUPPORT
#include "adjunct/quick/managers/NotificationManager.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

#define UNITE_UPDATE_NOTIFICATION_DELAY 32000
#define UNITE_UPDATE_RESTART_NOTIFICATION_DELAY 2000
#endif //GADGET_UPDATE_SUPPORT


#ifdef GADGET_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

const uni_char DesktopGadgetManager::UPDATE_FILENAME[] = UNI_L("update.zip");
const uni_char DesktopGadgetManager::UPDATE_URL_FILENAME[] = UNI_L("update_url.dat");


DesktopGadgetManager::DesktopGadgetManager() :
	m_root_service_window(NULL)
{
#ifdef WEBSERVER_SUPPORT
	g_main_message_handler->SetCallBack(this, MSG_CLOSE_ALL_UNITE_SERVICES, (MH_PARAM_1)this);
#endif // WEBSERVER_SUPPORT
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DesktopGadgetManager::~DesktopGadgetManager()
{
#ifdef WEBSERVER_SUPPORT
	//// if the home service was installed correctly, the root service is already closed
	//// otherwise, calling the function below crashes, as hotlist manager isn't valid anymore
	//CloseRootServiceWindow();
	
	if (g_main_message_handler)
		g_main_message_handler->UnsetCallBacks(this);
#endif // WEBSERVER_SUPPORT
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WEBSERVER_SUPPORT
BOOL DesktopGadgetManager::IsUniteServicePath(const OpStringC& address) 
{ 
	return g_gadget_manager->IsThisAnAlienGadgetPath(address); 
}


///////////////////////////////////////////////////////////////////////////////////////////////////


INT32
DesktopGadgetManager::GetRunningServicesCount()
{
	HotlistGenericModel* model = g_hotlist_manager->GetUniteServicesModel();

	if (!model)
		return 0;

    unsigned int number_of_gadgets = model->GetCount();
	INT32 services_count = 0;

	for (UINT32 i = 0; i < number_of_gadgets; i++)
	{
		HotlistModelItem* item = model->GetItemByIndex(i);
		if (!item || !item->IsUniteService() || item->IsRootService() || item->GetIsInsideTrashFolder())
			continue;

		if (static_cast<GadgetModelItem*>(item)->IsGadgetRunning())
		{
			services_count++;
		}
	}
	return services_count;
}


#endif // WEBSERVER_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef WEBSERVER_SUPPORT
/*virtual*/ void
DesktopGadgetManager::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if(desktop_window == m_root_service_window)
	{
		m_root_service_window = NULL;
		return;
	}
}
#endif // WEBSERVER_SUPPORT

GadgetInstallationType 
DesktopGadgetManager::GetGadgetInstallationTypeForPath(const OpStringC & gadget_path)
{
	/* None of the return values from the calls to GetFolderPath below
	 * needs to live beyond the enclosing Find() call, so it should be
	 * safe to reuse the same storage object for all of them.
	 */
	OpString tmp_storage;
	// if inside gadget folder
	if (gadget_path.Find(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_GADGET_FOLDER, tmp_storage)) == 0)
	{
		return UserSpaceGadget;
	}
	// if inside system folder
#ifdef WEBSERVER_SUPPORT
	if (gadget_path.Find(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_UNITE_PACKAGE_FOLDER, tmp_storage)) == 0)
	{
		return SystemSpaceGadget;
	}
#endif // WEBSERVER_SUPPORT
	// if config.xml or path to folder
	if (gadget_path.SubString(gadget_path.Length() - 5).Compare(".xml", 4) == 0)
	{
		return LocalGadgetConfigXML;
	}
	OpFile file;
	OpFileInfo::Mode mode;
	if (OpStatus::IsSuccess(file.Construct(gadget_path.CStr())) 
		&& OpStatus::IsSuccess(file.GetMode(mode))
		&& mode == OpFileInfo::DIRECTORY)
	{
		return LocalGadgetConfigXML;
	}

	return LocalPackagedGadget;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DesktopGadgetManager::IsUniqueServiceName(const OpStringC& service_name, GadgetModelItem* gmi)
{
	HotlistGenericModel* model = g_hotlist_manager->GetUniteServicesModel();

	if (!model)
		return TRUE;

    unsigned int number_of_gadgets = model->GetCount();

	for (UINT32 i = 0; i < number_of_gadgets; i++)
	{
		HotlistModelItem* item = model->GetItemByIndex(i);
		if (!item || !item->IsUniteService() || (gmi && item == gmi))
			continue;

		OpGadget* gadget = static_cast<GadgetModelItem*>(item)->GetOpGadget();

		if (gadget)
		{
			const uni_char *serviceName = gadget->GetUIData(UNI_L("serviceName"));
			
			if (serviceName && uni_str_eq(serviceName, service_name.CStr()))
			{
				return FALSE;
			}
		}
    }
    
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/***********************************************************************************
 *
 * OpenIfGadgetFolder
 *
 * @param OpStringC& address   -
 * 
 * Checks with OpGadgetManager if this is a gadget (path), and if so tries
 * to open (if same gadget already installed) or install the gadget (if not installed
 * already).
 *
 *
 * If WEBSERVER_SUPPORT is on, a wizard for the installation is shown.
 *
 * @return TRUE (if !WEBSERVER_SUPPORT) if installation failed (else TRUE).
 * 
 **********************************************************************************/

BOOL DesktopGadgetManager::OpenIfGadgetFolder(const OpStringC &address, BOOL open_service_page)
{
	const uni_char* tail = address.CStr() +
		MAX(address.Length() - GADGET_CONFIGURATION_FILE_LEN, 0);
	const BOOL is_config_file = OpStringC(tail) == GADGET_CONFIGURATION_FILE;

	if (g_gadget_manager->IsThisAGadgetPath(address))
	{
		//IsThisAnExtensionGadgetPath requires path to folder or zip file, path to config.xml is not supported
		int found_config_pos = -1;
		OpString path;
		if ((found_config_pos = address.Find(GADGET_CONFIGURATION_FILE))>0)
		{
			RETURN_VALUE_IF_ERROR(path.Set(address.CStr(),found_config_pos),FALSE);
		}

		if (g_gadget_manager->IsThisAnExtensionGadgetPath(path))
		{
			if (!is_config_file)
			{
				g_desktop_extensions_manager->InstallFromLocalPath(path);
			}
			// BLOCKING 
			return TRUE;
		}
	}
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

GadgetModelItem* DesktopGadgetManager::FindGadgetInstallation(const OpStringC &address, BOOL unite_service)
{
	OP_ASSERT(address.HasContent());
	if (address.HasContent())
	{
		// for drag&drop of config.xml: if present, strip the "config.xml" from the string
		const INT32 config_strlen = ARRAY_SIZE(GADGET_CONFIGURATION_FILE);
		OpString temp_address;
		if (address.Length() >= config_strlen && address.SubString(address.Length() - config_strlen).Compare(GADGET_CONFIGURATION_FILE) == 0)
		{
			temp_address.Set(address.CStr(), address.Length() - config_strlen - 1);
		}
		else
		{
			temp_address.Set(address);
		}
		
		OpVector<HotlistModelItem> items;

		HotlistModel* model = 0;
		if (unite_service)
		{
			model = g_hotlist_manager->GetUniteServicesModel();
		}

		if (!model) 
			return NULL;

		for( INT32 i = model->GetCount() - 1; i >= 0; i--)
		{
			HotlistModelItem *hmi = model->GetItemByIndex(i);

			if (hmi && ((unite_service && hmi->IsUniteService()) || hmi->IsGadget()))
			{
				OpGadget* op_gadget = static_cast<GadgetModelItem*>(hmi)->GetOpGadget();
				OP_ASSERT(op_gadget);	//This shouldn't happen, contact huibk
				if (op_gadget)
				{				
					OpString path;
					path.Set(op_gadget->GetGadgetPath()); //OOM doesn't matter, path.Compare just fails

					BOOL equal;
					g_op_system_info->PathsEqual(path.CStr(), temp_address.CStr(), &equal);

					if (equal)
					{
						return static_cast<GadgetModelItem*>(hmi);
					}
				}
			}	
		}
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopGadgetManager::PlaceFileInWidgetsFolder(const OpStringC& src_path,OpString& dest_file,BOOL copy_only)
{
	if (src_path.IsEmpty()) 
		return OpStatus::ERR;

	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_GADGET_FOLDER, dest_file));
	
	OpFile src_service, dest_service;
	RETURN_IF_ERROR(src_service.Construct(src_path.CStr()));

	dest_file.Append(src_service.GetName());
	CreateUniqueFilename(dest_file);
	
	RETURN_IF_ERROR(dest_service.Construct(dest_file.CStr()));
	RETURN_IF_ERROR(dest_service.CopyContents(&src_service, FALSE));

	if (copy_only)
	{
		// if OpGadgetClass has been loaded, remove it now. 
		OpStatus::Ignore(g_gadget_manager->RemoveGadget(src_path));
		
		src_service.Delete();
	}
	
	src_service.Close();
	dest_service.Close();
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WEBSERVER_SUPPORT
OP_STATUS DesktopGadgetManager::ShowUniteServiceSettings(HotlistModelItem* item)
{
	OP_ASSERT(item && item->IsUniteService());
	if (!(item && item->IsUniteService()))
	{
		return OpStatus::ERR;
	}

	WebServerServiceSettingsContext* service_context = WebServerServiceController::CreateServiceContextFromHotlistItem(static_cast<UniteServiceModelItem*>(item));
	OP_ASSERT(service_context);
	if (service_context)
	{
		WebServerServiceSettingsDialog* settings_dialog = OP_NEW(WebServerServiceSettingsDialog, (&m_service_controller, service_context));

		if (settings_dialog)
		{
			settings_dialog->Init(g_application->GetActiveDesktopWindow());
			return OpStatus::OK;
		}
		else
		{
			OP_DELETE(service_context);
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
		return OpStatus::ERR;
}
#endif // WEBSERVER_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef WEBSERVER_SUPPORT
/***********************************************************************************
 *
 * AddAutoStartService
 *
 * Adds a unite service to m_running_services list.
 * The items added to this list will all be started when StartAutoStartServices
 * is called. Makes it possible to postpone starting those service.
 * Used to start all services after AutoLogin has succeeded.
 *
 * @param OpStringC& gadget_id - Gadget Identifier of item to be added
 * 
 * @return TRUE if the item was added to the list, else return FALSE
 *

 **********************************************************************************/

OP_STATUS DesktopGadgetManager::AddAutoStartService(const OpStringC& gadget_id)
{
	HotlistGenericModel* model = g_hotlist_manager->GetUniteServicesModel();
	if (model)
	{
		HotlistModelItem* hmi = model->GetByGadgetIdentifier( gadget_id );
		if (hmi && hmi->IsUniteService()) 
		{
			if(!static_cast<UniteServiceModelItem*>(hmi)->IsRootService())
			{
				m_running_services.Add(hmi->GetID());
				return OpStatus::OK;
			}
		}
	}

	// Return FALSE if it wasn't a service / added
	return OpStatus::ERR;
}

OP_STATUS
DesktopGadgetManager::RemoveAutoStartService(const OpStringC& gadget_id)
{
	HotlistGenericModel* model = g_hotlist_manager->GetUniteServicesModel();
	if (model)
	{
		HotlistModelItem* hmi = model->GetByGadgetIdentifier( gadget_id );
		if (hmi && hmi->IsUniteService()) 
		{
			RETURN_VALUE_IF_ERROR(m_running_services.Remove(hmi->GetID()), FALSE);
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

///////////////////////////////////////////////////////////////////////////////////////////////////


/***********************************************************************************
 *
 * StartAutoStartServices
 *
 * Starts all Unite Service stored in the list m_running_services.
 *
 **********************************************************************************/

void DesktopGadgetManager::StartAutoStartServices()
{
	HotlistGenericModel* services_model = g_hotlist_manager->GetUniteServicesModel();

	if (services_model)
	{
		// Loop the list of gadgets and launch them all
		for (INT32SetIterator it(m_running_services); it; it++)
		{
			HotlistModelItem* item = services_model->GetItemByID(it.GetData());
			if (item)
			{
				// Open the gadget
				if(OpStatus::IsError(static_cast<UniteServiceModelItem*>(item)->ShowGadget(TRUE, FALSE)))
				{
					break;
				}
			}
		}							
	}

	// Kill all the items in the list
	m_running_services.RemoveAll();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopGadgetManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_CLOSE_ALL_UNITE_SERVICES)
	{
		HotlistGenericModel* model = g_hotlist_manager->GetUniteServicesModel();

		if (!model)
			return;

		unsigned int number_of_gadgets = model->GetCount();

		for (UINT32 i = 0; i < number_of_gadgets; i++)
		{
			GadgetModelItem* item = static_cast<GadgetModelItem*>(model->GetItemByIndex(i));
			if (!item || !item->IsUniteService())
				continue;

			if (item->IsGadgetRunning())
			{
				OP_ASSERT(item->GetOpGadget());
				AddAutoStartService(item->GetGadgetIdentifier());

				// Stop the service
				item->ShowGadget(FALSE, FALSE);
			}
		}

		// refresh UI, since the status of selected Gadgets may have changed, the "Start/Stop" button has to be refreshed,fix DSK-253870
		g_input_manager->UpdateAllInputStates();
	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
DesktopGadgetManager::SetRootServiceWindow(DesktopGadget *gadget)
{
	OP_ASSERT(NULL == m_root_service_window);
	m_root_service_window = gadget;
	m_root_service_window->AddListener(this);
}


OP_STATUS DesktopGadgetManager::GetRootServiceAddress(OpString& address)
{
	// Build the path to the root widget
	OpString root_service_address;

	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_GADGET_FOLDER, root_service_address));
	root_service_address.Append(HOME_APP);
	OpFile verify_exist;
	RETURN_IF_ERROR(verify_exist.Construct(root_service_address));
	BOOL exist;
	RETURN_IF_ERROR(verify_exist.Exists(exist));
	if(!exist)
	{
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_UNITE_PACKAGE_FOLDER, root_service_address));
		root_service_address.Append(HOME_APP);
	}
	return address.Set(root_service_address);

}


/**********************************************************************
 *
 * InstallRootService - unite master service
 *
 *
 **********************************************************************/
OP_STATUS DesktopGadgetManager::InstallRootService(BOOL try_force_update)
{
	OP_ASSERT(NULL == m_root_service_window);

	OpString root_service_address;
	RETURN_IF_ERROR(GetRootServiceAddress(root_service_address));
	OpGadget* root_service = GetRootGadget();
	RETURN_IF_ERROR(g_gadget_manager->CreateGadget(&root_service,root_service_address,URL_UNITESERVICE_INSTALL_CONTENT));

	OP_ASSERT(NULL != root_service);
	
	g_hotlist_manager->NewRootService(*root_service);

	return OpStatus::OK;
}

/**********************************************************************
 *
 * OpenRootService
 *
 *
 **********************************************************************/


OP_STATUS DesktopGadgetManager::OpenRootService(BOOL open_home_page)
{
	OP_ASSERT(NULL == m_root_service_window);

	OpString root_service_address;
	RETURN_IF_ERROR(GetRootServiceAddress(root_service_address));
	OpGadget* gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_PATH,
			root_service_address);

	if (gadget == NULL)
	{
		RETURN_IF_ERROR(InstallRootService());
	}

	gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_PATH,
		root_service_address);
	OP_ASSERT(NULL != gadget);

	RETURN_IF_ERROR(g_gadget_manager->OpenRootService(gadget));

	// OpGadgetManager should have ended up calling SetRootServiceWindow()
	// on us.
	OP_ASSERT(NULL != m_root_service_window);

	RETURN_IF_ERROR(m_root_service_window->Init());

	HotlistGenericModel* model = g_hotlist_manager->GetUniteServicesModel();
	
	if (model)
	{
		HotlistModelItem* item = model->GetRootService();
		
		if (item && item->IsUniteService())
		{
			UniteServiceModelItem* root = static_cast<UniteServiceModelItem*>(item);

			root->SetGadgetWindow(*m_root_service_window);
			root->SetRootServiceName(); // upgrade from old Unite buids: overriding old item name (now: static text, as opposed to the username)
			root->SetOpenServicePage(open_home_page);
			root->Change();
		}
	}
	model->SetDirty( TRUE );

	return OpStatus::OK;
}

OP_STATUS DesktopGadgetManager::GetUpdateFileName(OpGadget& gadget, OpString& file_name)
{
	OpString gadgets_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_GADGET_FOLDER, gadgets_folder));
	OpString gadget_profile_dir;
	RETURN_IF_ERROR(OpPathDirFileCombine(gadget_profile_dir, gadgets_folder, OpStringC(gadget.GetIdentifier())));
	return OpPathDirFileCombine(file_name, gadget_profile_dir, OpStringC(UPDATE_FILENAME));
}

OP_STATUS DesktopGadgetManager::GetUpdateUrlFileName(OpGadget& gadget, OpString& result)
{
	OpString gadgets_folder;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_GADGET_FOLDER, gadgets_folder));
	OpString gadget_profile_dir;
	RETURN_IF_ERROR(OpPathDirFileCombine(gadget_profile_dir, gadgets_folder, OpStringC(gadget.GetIdentifier())));
	return OpPathDirFileCombine(result, gadget_profile_dir, OpStringC(UPDATE_URL_FILENAME));
}

OpGadget* DesktopGadgetManager::GetRootGadget()
{
	OpGadget* gadget = NULL;
	HotlistGenericModel* model = g_hotlist_manager->GetUniteServicesModel();
	if (model )
	{
		HotlistModelItem* item = model->GetRootService();
		if (item && item->IsUniteService())
		{
			UniteServiceModelItem* root = static_cast<UniteServiceModelItem*>(item);
			gadget = root->GetOpGadget();
		}
	}
	return gadget;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DesktopGadgetManager::CloseRootServiceWindow(BOOL immediately, BOOL user_initiated, BOOL force)
{
	if (m_root_service_window)
	{
		m_root_service_window->Close(immediately, user_initiated, force);
		HotlistGenericModel* model = g_hotlist_manager->GetUniteServicesModel();
		if (model)
		{
			HotlistModelItem* item = model->GetRootService();
			if (item && item->IsUniteService())
			{
				UniteServiceModelItem* root = static_cast<UniteServiceModelItem*>(item);
				root->Change();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS 
DesktopGadgetManager::ConfigurePreinstalledService(UniteServiceModelItem * item)
{
	return m_service_controller.ConfigurePreinstalledService(item);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

UniteServiceModelItem * DesktopGadgetManager::FindUniteModelItem(const OpStringC & widget_id)
{
	HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();

	if (!model)
		return NULL;

	unsigned int number_of_gadgets = model->GetCount();

	for (UINT32 i = 0; i < number_of_gadgets; i++)
	{
		HotlistModelItem* item = model->GetItemByIndex(i);
		if (!item || !item->IsUniteService())
			{ continue; }
		
		UniteServiceModelItem * us_item = static_cast<UniteServiceModelItem *>(item);
		OpGadget * gadget = us_item->GetOpGadget();

		if (gadget && widget_id.Compare(gadget->GetGadgetId()) == 0)
		{	
			return us_item;		
		}

	}
	return NULL;
}


UniteServiceModelItem *
DesktopGadgetManager::FindHigherOrEqualUnconfiguredVersion(const OpStringC & widget_id, const GadgetVersion & version)
{
	HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();

	if (!model)
		return NULL;

	unsigned int number_of_gadgets = model->GetCount();

	for (UINT32 i = 0; i < number_of_gadgets; i++)
	{
		HotlistModelItem* item = model->GetItemByIndex(i);
		if (!item || !item->IsUniteService())
			{ continue; }
		
		UniteServiceModelItem * us_item = static_cast<UniteServiceModelItem *>(item);
		OpGadget * gadget = us_item->GetOpGadget();

		if (gadget && widget_id.Compare(gadget->GetGadgetId()) == 0)
		{	
			GadgetVersion curr_version;
			OpString version_str;
			if (   OpStatus::IsSuccess(gadget->GetClass()->GetGadgetVersion(version_str)) 
				&& OpStatus::IsSuccess(curr_version.Set(version_str)))
			{
				if (curr_version >= version && us_item->NeedsConfiguration())
				{
					return us_item;
				}
			}
		}

	}
	return NULL;
}

#endif // WEBSERVER_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopGadgetManager::GetGadgetIcon(OpGadgetClass* op_gadget, Image &image, UINT32 max_size)
{
	if (op_gadget)
	{
		// Loop all the icons in a gadget and find the largest one
		UINT32 icon_count = op_gadget->GetGadgetIconCount();
		UINT32 max_w = 0;
		OpBitmap *largest_icon = NULL;
		OpBitmap *largest_icon_toobig = NULL;

		for(UINT32 i=0; i<icon_count; i++)
		{
			OpBitmap *icon = NULL;
			if(OpStatus::IsSuccess(op_gadget->GetGadgetIcon(&icon, i)) && icon)
			{
				UINT32 w = icon->Width();
				if(w > max_w)
				{
					// only take icon if it's the allowed size
					if (w <= max_size)
					{
						max_w = w;
						OP_DELETE(largest_icon);
						largest_icon = icon;
						continue;
					}
					// remember that there's a bigger one, just in case
					else if (!largest_icon_toobig)
					{
						largest_icon_toobig = icon;
						continue;
					}
				}
				OP_DELETE(icon);
			}
		}
		// if there's no icon with the allowed max size, take the bigger one instead
		if (!largest_icon && largest_icon_toobig)
		{
			largest_icon = largest_icon_toobig;
			largest_icon_toobig = NULL;
		}

		if(!largest_icon)
		{
			// use a default skin image as the widget has no image

		}
		else
		{
			if(largest_icon->Width() >= max_size)
			{
				// Larger than required the size so just return it an it will scale down automatically
				image.DecVisible(null_image_listener); 
				image = imgManager->GetImage(largest_icon);
				if (image.IsEmpty())     // OOM
					OP_DELETE(largest_icon);
				image.IncVisible(null_image_listener); 

				OP_DELETE(largest_icon_toobig);
				return OpStatus::OK;
			}
			else
			{
				// Smaller than the required size so scale it up!
				OpBitmap *scaled_bitmap;
				if(OpStatus::IsSuccess(OpBitmap::Create(&scaled_bitmap, max_size, max_size, FALSE, TRUE, 0, 0, TRUE)))
				{
					OpPainter *painter = scaled_bitmap->GetPainter();
					if(painter)
					{
						OpBitmap* iconBitmap = NULL;
						Image iconImage;

# ifdef _UNIX_DESKTOP_			
						// Clear bitmap
						painter->SetColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND));
						painter->FillRect(OpRect(0,0,max_size,max_size));
# endif

						if(largest_icon)
						{
							iconBitmap = largest_icon;
						}
						else
						{
							OpSkinElement* element = NULL;

#ifdef WEBSERVER_SUPPORT
							if (op_gadget && op_gadget->IsSubserver())
								element = g_skin_manager->GetSkinElement("Unite Icon");
							else
#endif // WEBSERVER_SUPPORT
								element = g_skin_manager->GetSkinElement("Widget Icon");

							if (element) 
							{
								iconImage = element->GetImage(0);
								iconBitmap = iconImage.GetBitmap(NULL);
							}
						}
						if(iconBitmap)
						{
							painter->DrawBitmapScaled(iconBitmap, OpRect(0,0,iconBitmap->Width(),iconBitmap->Height()), OpRect(0,0,max_size,max_size));
						}
						if(largest_icon)
						{
							OP_DELETE(largest_icon);
						}
						else
						{
							if(iconBitmap)
							{
								iconImage.ReleaseBitmap();
							}
						}
						scaled_bitmap->ReleasePainter();
						image.DecVisible(null_image_listener); 
						image = imgManager->GetImage(scaled_bitmap);
						if (image.IsEmpty())     // OOM
							OP_DELETE(scaled_bitmap);
						image.IncVisible(null_image_listener);

						return OpStatus::OK;
					}
				}
			}
		}
	}

	return OpStatus::ERR;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OpGadget * DesktopGadgetManager::GetInstalledVersionOfGadget(const OpStringC &service_path)
{
	OpString svc_path, error_reason;
	svc_path.Set(service_path);
	OpGadgetClass * service_class = g_gadget_manager->CreateClassWithPath(svc_path, URL_UNITESERVICE_INSTALL_CONTENT, NULL, error_reason);
	if (service_class)
	{
		OpString id;
		id.Set(service_class->GetGadgetId());

		if (id.HasContent())
		{
#ifdef GADGETS_CAP_HAS_FIND_BY_GADGET_ID
			return g_gadget_manager->FindGadget(GADGET_FIND_BY_GADGET_ID, id);
#else
			OP_ASSERT(FALSE);
#endif //GADGETS_CAP_HAS_FIND_BY_GADGET_ID
		}
		else
		{
			service_class->GetGadgetDownloadUrl(id);

			if (id.HasContent())
			{
				return g_gadget_manager->FindGadget(GADGET_FIND_BY_DOWNLOAD_URL, id);
			}
			else
			{
				OP_ASSERT(!"service doesn't have either download URL or gadget ID");
			}
		}
	}
	
	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*virtual*/ BOOL
DesktopGadgetManager::IsGadgetInstalledAtSameLocation(const OpStringC &service_path, BOOL unite_service)
{
	return (FindGadgetInstallation(service_path, unite_service) != NULL);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


OP_STATUS DesktopGadgetManager::UninstallGadget(OpGadget* op_gadget)
{
	if (op_gadget)
	{
		RETURN_IF_ERROR(g_gadget_manager->DestroyGadget(op_gadget));
		// AJMTODO: Fix the file cleanup from disk
		//Let the gadget manager save the changed list of installed gadgets
		g_gadget_manager->SaveGadgets();
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

#endif // GADGET_SUPPORT
