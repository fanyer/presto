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

#include "adjunct/quick/webserver/controller/WebServerServiceSettingsContext.h"
#include "modules/gadgets/OpGadget.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"

#include "modules/locale/oplanguagemanager.h"


#ifdef GADGETS_CAP_HAS_FEATURES
/***********************************************************************************
**  WebServerServiceFolderHintTranslator::WebServerServiceFolderHintTranslator
************************************************************************************/
WebServerServiceFolderHintTranslator::WebServerServiceFolderHintTranslator(OpFolderManager * folder_manager) : 
	m_folder_manager(folder_manager),
	m_folder_hint(),
	m_folder_hint_type(FolderHintNone)
{
}

/***********************************************************************************
**  WebServerServiceFolderHintTranslator::SetServiceClass
************************************************************************************/
OP_STATUS
WebServerServiceFolderHintTranslator::SetServiceClass(OpGadgetClass * service_class)
{
	if (service_class == NULL)
		return OpStatus::ERR_NULL_POINTER;

	Head * features = service_class->GetFeatures();

	if (features == NULL)
		return OpStatus::ERR_NULL_POINTER;

#ifdef USE_COMMON_RESOURCES
	m_folder_hint.Empty(); // start with a clear folder hint path
	OpGadgetFeature * feature = static_cast<OpGadgetFeature*>(features->First());

	OpStringC feature_uri(UNI_L("http://xmlns.opera.com/fileio"));
	OpStringC param_name(UNI_L("folderhint"));

	BOOL has_folderhint = FALSE;
	while (feature)
	{
		if (feature_uri.Compare(feature->URI()) == 0)
		{
			UINT32 param_count = feature->Count();
			for (UINT32 j = 0; j < param_count; j++)
			{
				OpGadgetFeatureParam * param = feature->GetParam(j);
				if (param_name.Compare(param->Name()) == 0)
				{
					OpString hint;
					hint.Set(param->Value());

					OpString hint_path;

					if (hint.CompareI(UNI_L("pictures")) == 0)
					{
						m_folder_hint_type = FolderHintPictures;
						RETURN_IF_ERROR(SetFolderPath(OPFILE_PICTURES_FOLDER));
						return OpStatus::OK;
					}
					else if (hint.CompareI(UNI_L("music")) == 0)
					{
						m_folder_hint_type = FolderHintMusic;
						RETURN_IF_ERROR(SetFolderPath(OPFILE_MUSIC_FOLDER));
						return OpStatus::OK;
					}
					else if (hint.CompareI(UNI_L("video")) == 0)
					{
						m_folder_hint_type = FolderHintVideo;
						RETURN_IF_ERROR(SetFolderPath(OPFILE_VIDEOS_FOLDER));
						return OpStatus::OK;
					}
					else if (hint.CompareI(UNI_L("documents")) == 0)
					{
						m_folder_hint_type = FolderHintDocuments;
						RETURN_IF_ERROR(SetFolderPath(OPFILE_DOCUMENTS_FOLDER));
						return OpStatus::OK;
					}
					else if (hint.CompareI(UNI_L("downloads")) == 0)
					{
						m_folder_hint_type = FolderHintDownloads;
						RETURN_IF_ERROR(SetFolderPath(OPFILE_DOWNLOAD_FOLDER));
						return OpStatus::OK;
					}
					else if (hint.CompareI(UNI_L("desktop")) == 0)
					{
						m_folder_hint_type = FolderHintDesktop;
						RETURN_IF_ERROR(SetFolderPath(OPFILE_DESKTOP_FOLDER));
						return OpStatus::OK;
					}
					else if (hint.CompareI(UNI_L("public")) == 0)
					{
						m_folder_hint_type = FolderHintPublic;
						RETURN_IF_ERROR(SetFolderPath(OPFILE_PUBLIC_FOLDER));
						return OpStatus::OK;
					}
					else if (hint.HasContent())
					{
						has_folderhint = TRUE;
					}
					break;
				}
			}
		}
		feature = static_cast<OpGadgetFeature*>(feature->Suc());
	}
	if (m_folder_hint.IsEmpty() && has_folderhint) // if the service specified a folder but we don't know it (or it is "home")
	{
		m_folder_hint_type = FolderHintDefault;
		RETURN_IF_ERROR(SetFolderPath(OPFILE_DEFAULT_SHARED_FOLDER));
	}
#endif // USE_COMMON_RESOURCES
	return OpStatus::OK;
}

/***********************************************************************************
**  WebServerServiceFolderHintTranslator::SetFolderPath
************************************************************************************/
OP_STATUS
WebServerServiceFolderHintTranslator::SetFolderPath(OpFileFolder folder)
{
	OpString tmp_storage;
	return m_folder_hint.Set(m_folder_manager->GetFolderPathIgnoreErrors(folder, tmp_storage));
}

/***********************************************************************************
**  WebServerServiceFolderHintTranslator::GetFolderHintPath
************************************************************************************/
const OpString &
WebServerServiceFolderHintTranslator::GetFolderHintPath() const
{
	return m_folder_hint;
}

/***********************************************************************************
**  WebServerServiceFolderHintTranslator::GetFolderHintType
************************************************************************************/
FolderHintType		
WebServerServiceFolderHintTranslator::GetFolderHintType() const
{
	return m_folder_hint_type;
}
#endif //  GADGETS_CAP_HAS_FEATURES

/***********************************************************************************
**  WebServerServiceSettingsContext::CreateContextFromService
************************************************************************************/
WebServerServiceSettingsContext *
WebServerServiceSettingsContext::CreateContextFromService(OpGadget * service)
{
	WebServerServiceSettingsContext * service_context = CreateContextFromServiceClass(service->GetClass());
	if (service_context)
	{
		if (service->GetIdentifier())
		{
			service_context->SetServiceIdentifier(service->GetIdentifier());
		}

		OpString service_name_in_url;
		service_name_in_url.Set(service->GetUIData(UNI_L("serviceName")));
		service_context->SetServiceNameInURL(service_name_in_url);

#ifdef GADGETS_CAP_HAS_SHAREDFOLDER
		OpString shared_folder;
		shared_folder.Set(service->GetSharedFolder());
		service_context->SetSharedFolderPath(shared_folder);
#endif // GADGETS_CAP_HAS_SHAREDFOLDER

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	service_context->SetVisibleASD(service->IsVisibleASD());
	service_context->SetVisibleUPNP(service->IsVisibleUPNP());
	service_context->SetVisibleRobots(service->IsVisibleRobots());
#endif // WEBSERVER_CAP_SET_VISIBILITY
	}
	return service_context;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::CreateContextFromServiceClass
************************************************************************************/
WebServerServiceSettingsContext *
WebServerServiceSettingsContext::CreateContextFromServiceClass(OpGadgetClass * service_class)
{
	if (!service_class)
		return NULL;

	WebServerServiceSettingsContext * service_context = OP_NEW(WebServerServiceSettingsContext, ());
	
	Image service_image;
	g_desktop_gadget_manager->GetGadgetIcon(service_class, service_image);
	if (!service_image.IsEmpty())
	{
		service_context->SetServiceImage(service_image);
	}

	OpString friendly_name;
	service_class->GetGadgetName(friendly_name);
	if (!friendly_name.HasContent())
	{
		g_languageManager->GetString(Str::S_UNNAMED_SERVICE, friendly_name);
	}
	service_context->SetFriendlyName(friendly_name);

	OpString description;
	service_class->GetGadgetDescription(description);
	service_context->SetServiceDescription(description);

	OpString version;
	service_class->GetGadgetVersion(version);
	service_context->SetVersionString(version);

	OpString widget_id;
	widget_id.Set(service_class->GetGadgetId());
	service_context->SetWidgetID(widget_id);

	WebServerServiceFolderHintTranslator translator(g_folder_manager);
	translator.SetServiceClass(service_class);
	service_context->SetSharedFolderHint(translator.GetFolderHintPath());
	service_context->SetSharedFolderHintType(translator.GetFolderHintType());

	// set default visibility settings
#ifdef WEBSERVER_CAP_SET_VISIBILITY
	service_context->SetVisibleASD(TRUE);
	service_context->SetVisibleUPNP(TRUE);
	service_context->SetVisibleRobots(FALSE);
#endif // WEBSERVER_CAP_SET_VISIBILITY

	return service_context;
}


/***********************************************************************************
**  WebServerServiceSettingsContext::WebServerServiceSettingsContext
************************************************************************************/
WebServerServiceSettingsContext::WebServerServiceSettingsContext() :
	m_friendly_name(),
	m_service_name_in_url(),
	m_service_description(),
	m_service_image(),
	m_shared_folder_hint(),
	m_shared_folder_path(),
	m_download_url(),
	m_widget_id(),
	m_local_path(),
	m_service_identifier(),
	m_hotlist_id(-1),
	m_installation_type(RemoteGadgetURL),
	m_is_preinstalled(FALSE)
#ifdef WEBSERVER_CAP_SET_VISIBILITY
	, m_asd_visibility(FALSE)
	, m_upnp_visibility(FALSE)
	, m_robots_visibility(FALSE)
#endif // WEBSERVER_CAP_SET_VISIBILITY
{
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetFriendlyName
************************************************************************************/
const OpStringC & 
WebServerServiceSettingsContext::GetFriendlyName() const
{
	return m_friendly_name;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetServiceNameInURL
************************************************************************************/
const OpStringC & 
WebServerServiceSettingsContext::GetServiceNameInURL() const
{
	return m_service_name_in_url;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetServiceImage
************************************************************************************/
const OpStringC &
WebServerServiceSettingsContext::GetServiceDescription() const
{
	return m_service_description;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetServiceImage
************************************************************************************/
const Image & 
WebServerServiceSettingsContext::GetServiceImage() const
{
	return m_service_image;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetFolderHintType
************************************************************************************/
FolderHintType	
WebServerServiceSettingsContext::GetSharedFolderHintType() const
{
	return m_shared_folder_hint_type;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetSharedFolderHint
************************************************************************************/
const OpStringC & 
WebServerServiceSettingsContext::GetSharedFolderHint() const
{
	return m_shared_folder_hint;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetSharedFolderPath
************************************************************************************/
const OpStringC & 
WebServerServiceSettingsContext::GetSharedFolderPath() const
{
	return m_shared_folder_path;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetRemoteDownloadLocation
************************************************************************************/
const OpStringC &
WebServerServiceSettingsContext::GetRemoteDownloadLocation() const
{
	return m_download_url;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetWidgetID
************************************************************************************/
const OpStringC &
WebServerServiceSettingsContext::GetWidgetID() const
{
	return m_widget_id;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetLocalDownloadPath
************************************************************************************/
const OpStringC & 
WebServerServiceSettingsContext::GetLocalDownloadPath() const
{
	return m_local_path;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetFriendlyName
************************************************************************************/
OP_STATUS 
WebServerServiceSettingsContext::SetFriendlyName(const OpStringC & friendly_name)
{
	return m_friendly_name.Set(friendly_name.CStr());
}

/***********************************************************************************
**  WebServerServiceSettingsContext::GetServiceIdentifer
************************************************************************************/
const OpStringC & 
WebServerServiceSettingsContext::GetServiceIdentifier() const
{
	return m_service_identifier;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetServiceNameInURL
************************************************************************************/
OP_STATUS 
WebServerServiceSettingsContext::SetServiceNameInURL(const OpStringC & service_name_in_url)
{
	return m_service_name_in_url.Set(service_name_in_url.CStr());
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetServiceImage
************************************************************************************/
OP_STATUS
WebServerServiceSettingsContext::SetServiceDescription(const OpStringC & service_description)
{
	return m_service_description.Set(service_description.CStr());
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetServiceImage
************************************************************************************/
OP_STATUS 
WebServerServiceSettingsContext::SetServiceImage(const Image & service_image)
{
	m_service_image = service_image;
	return OpStatus::OK;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetSharedFolderHintType
************************************************************************************/
void
WebServerServiceSettingsContext::SetSharedFolderHintType(FolderHintType shared_folder_hint_type)
{
	m_shared_folder_hint_type = shared_folder_hint_type;
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetSharedFolderHint
************************************************************************************/
OP_STATUS 
WebServerServiceSettingsContext::SetSharedFolderHint(const OpStringC & shared_folder_hint)
{
	return m_shared_folder_hint.Set(shared_folder_hint.CStr());
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetSharedFolderPath
************************************************************************************/
OP_STATUS 
WebServerServiceSettingsContext::SetSharedFolderPath(const OpStringC & shared_folder_path)
{
	return m_shared_folder_path.Set(shared_folder_path.CStr());
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetRemoteDownloadLocation
************************************************************************************/
OP_STATUS		
WebServerServiceSettingsContext::SetRemoteDownloadLocation(const OpStringC & download_url)
{
	return m_download_url.Set(download_url);
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetWidgetID
************************************************************************************/
OP_STATUS
WebServerServiceSettingsContext::SetWidgetID(const OpStringC & widget_id)
{
	return m_widget_id.Set(widget_id);
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetLocalDownloadPath
************************************************************************************/
OP_STATUS
WebServerServiceSettingsContext::SetLocalDownloadPath(const OpStringC & local_path)
{
	return m_local_path.Set(local_path.CStr());
}

/***********************************************************************************
**  WebServerServiceSettingsContext::SetServiceIdentifier
************************************************************************************/
OP_STATUS 
WebServerServiceSettingsContext::SetServiceIdentifier(const OpStringC & service_id)
{
	return m_service_identifier.Set(service_id.CStr());
}


#endif // WEBSERVER_SUPPORT
#endif // GADGET_SUPPORT
