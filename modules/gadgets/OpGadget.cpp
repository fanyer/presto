/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/documentreferrer.h"
#include "modules/dochand/win.h"
#include "modules/formats/uri_escape.h"
#include "modules/url/url_man.h"
#include "modules/util/filefun.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opguid.h"
#include "modules/util/zipload.h"
#include "modules/prefs/prefsmanager/opprefscollection.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/dochand/winman.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/pi/system/OpPlatformViewers.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/style/cssmanager.h"
#include "modules/database/opdatabasemanager.h" // because of DATABASE_MODULE_ADD_CONTEXT

#ifdef WEBSERVER_SUPPORT
# include "modules/webserver/webserver-api.h"
# include "modules/webserver/webserver_resources.h"
#endif /* WEBSERVER_SUPPORT */

#if defined(GADGET_ENCRYPT_PREFERENCES)
# include "modules/libcrypto/include/OpCryptoKeyManager.h"
#endif // GADGET_ENCRYPT_PREFERENCES

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
# include "modules/database/opstorage.h"
# include "modules/database/webstorage_data_abstraction.h"
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT

OpGadget::OpGadget(OpGadgetClass* gadget_class)
	: m_class(gadget_class)
	, m_window(NULL)
	, m_prefsfile(NULL)
	, m_mp_section(NULL)
	, m_mode(gadget_class->GetDefaultMode())
	, m_width(gadget_class->InitialWidth())
	, m_height(gadget_class->InitialHeight())
#ifdef WEBSERVER_SUPPORT
	, m_is_root_service(FALSE)
#endif // WEBSERVER_SUPPORT
	, m_write_msg_posted(FALSE)
	, m_is_closing(FALSE)
	, m_is_active(FALSE)
	, m_gadget_url_loaded(FALSE)
	, m_default_prefs_applied(FALSE)
	, m_is_opening(FALSE)
	, m_security_policy(NULL)
#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	, m_global_security_policy(NULL)
	, m_allow_global_security_policy(FALSE)
#endif // GADGETS_MUTABLE_POLICY_SUPPORT
#ifdef EXTENSION_SUPPORT
	, m_is_running(FALSE)
	, m_extension_flags(AllowUserJSHTTPS)
#endif // EXTENSION_SUPPORT
{
}

OpGadget::~OpGadget()
{
	Out();

	g_main_message_handler->UnsetCallBack(this, MSG_GADGET_DELAYED_PREFS_WRITE);

	if (m_write_msg_posted)
	{
		TRAPD(err, m_prefsfile->CommitL(TRUE, FALSE));
	}

	DeleteCacheContext();
	m_aso_objects.Clear();

	OP_DELETE(m_prefsfile);
	OP_DELETE(m_security_policy);
	OP_DELETE(m_mp_section);
#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
	OP_DELETE(m_global_security_policy);
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

#ifdef EXTENSION_SUPPORT
	if (m_class->IsExtension())
		StopExtension();
#endif // EXTENSION_SUPPORT
}

void
OpGadget::ApplyNewStyles()
{
	HLDocProfile* hld_profile = m_window && m_window->GetCurrentDoc() ? m_window->GetCurrentDoc()->GetHLDocProfile() : NULL;
	if (hld_profile)
		hld_profile->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_ALL);
}

OP_STATUS
OpGadget::ApplyModeChanged()
{
	if (!m_window)
		return OpStatus::OK;

	FramesDocument* frm_doc = (FramesDocument*)m_window->GetCurrentDoc();
	if (!frm_doc)
		return OpStatus::OK;

	DOM_Environment* dom_env = frm_doc->GetDOMEnvironment();
	if (!dom_env)
		return OpStatus::OK;

	OpString strMode;
	RETURN_IF_ERROR(GetMode(strMode));

	// Create event and dispatch it.
	DOM_Environment::GadgetEventData evt;
	evt.mode = strMode.CStr();

	return dom_env->HandleGadgetEvent(DOM_Environment::GADGET_EVENT_ONMODECHANGE, &evt);
}

OP_STATUS
OpGadget::GetMode(OpString& mode)
{
	switch(m_mode)
	{
	case WINDOW_VIEW_MODE_WIDGET:
		return mode.Set("widget");
	case WINDOW_VIEW_MODE_FLOATING:
		return mode.Set("floating");
	case WINDOW_VIEW_MODE_DOCKED:
		return mode.Set("docked");
	case WINDOW_VIEW_MODE_APPLICATION:
		return mode.Set("application");
	case WINDOW_VIEW_MODE_FULLSCREEN:
		return mode.Set("fullscreen");
	case WINDOW_VIEW_MODE_WINDOWED:
		return mode.Set("windowed");
	case WINDOW_VIEW_MODE_MAXIMIZED:
		return mode.Set("maximized");
	case WINDOW_VIEW_MODE_MINIMIZED:
		return mode.Set("minimized");
	default:
		return OpStatus::ERR;
	}
}

BOOL OpGadget::CanSupportMode(WindowViewMode mode)
{
	if (m_class->SupportsNamespace(GADGETNS_W3C_1_0))
	{
		switch (mode)
		{
			case WINDOW_VIEW_MODE_FLOATING:
			case WINDOW_VIEW_MODE_FULLSCREEN:
			case WINDOW_VIEW_MODE_WINDOWED:
			case WINDOW_VIEW_MODE_MAXIMIZED:
			case WINDOW_VIEW_MODE_MINIMIZED:
				return TRUE;
			default:
				return FALSE;
		}
	}
	else
	{
		switch (mode)
		{
			case WINDOW_VIEW_MODE_WIDGET:
			case WINDOW_VIEW_MODE_APPLICATION:
			case WINDOW_VIEW_MODE_FULLSCREEN:
				return TRUE;
			case WINDOW_VIEW_MODE_DOCKED:
				return IsDockable();
			default:
				return FALSE;
		}
	}
}

OP_STATUS
OpGadget::SetMode(WindowViewMode mode)
{
	if (!CanSupportMode(mode))
		return OpStatus::ERR_NOT_SUPPORTED;
	m_mode = mode;
	ApplyNewStyles();
	return ApplyModeChanged();
}

OP_STATUS
OpGadget::SetMode(const OpStringC& mode)
 {
	WindowViewMode new_mode; 
	 if (mode.Compare("widget") == 0)
		 new_mode = WINDOW_VIEW_MODE_WIDGET;
	 else if (mode.Compare("floating") == 0)
		 new_mode = WINDOW_VIEW_MODE_FLOATING;
	 else if (mode.Compare("docked") == 0)
		 new_mode = WINDOW_VIEW_MODE_DOCKED;
	 else if (mode.Compare("application") == 0)
		 new_mode = WINDOW_VIEW_MODE_APPLICATION;
	 else if (mode.Compare("fullscreen") == 0)
		 new_mode = WINDOW_VIEW_MODE_FULLSCREEN;
	 else if (mode.Compare("windowed") == 0)
		 new_mode = WINDOW_VIEW_MODE_WINDOWED;
	 else if (mode.Compare("maximized") == 0)
		 new_mode = WINDOW_VIEW_MODE_MAXIMIZED;
	 else if (mode.Compare("minimized") == 0)
		 new_mode = WINDOW_VIEW_MODE_MINIMIZED;
	 else
		 return OpStatus::ERR;

	 return SetMode(new_mode);
 }

OP_STATUS
OpGadget::SetGadgetName(const uni_char *name)
{
	if (!name || !*name)
		return OpStatus::ERR; // Widgets _must_ have a name

	return SetUIData(UNI_L("name"), name);
}

OP_STATUS
OpGadget::GetGadgetName(OpString& name)
{
	//  Use saved name if a name has been saved.
	const uni_char* ui_name = GetUIData(UNI_L("name"));

	if (ui_name)
		return name.Set(ui_name);

	// fallback to default name.
	return m_class->GetGadgetName(name);
}

#ifdef WEBSERVER_SUPPORT
OP_STATUS
OpGadget::SetupWebSubServer()
{
	if (g_webserver == NULL || m_class->IsSubserver() == FALSE)
		return OpStatus::OK;

	RETURN_IF_ERROR(g_webserver->RemoveSubServers(m_window->Id()));

	OpString gadgetStoragePath;
	OpString gadgetName;
	OpString gadgetAuthor;
	OpString gadgetDescription;
	OpString uniqueServiceName;
	OpString gadgetDownloadUrl;

	RETURN_IF_ERROR(GetGadgetName(gadgetName));

	OpString serviceNameCandidate;
	RETURN_IF_ERROR(GetGadgetSubserverUri(serviceNameCandidate));
	RETURN_IF_ERROR(g_gadget_manager->GetUniqueSubServerPath(this, serviceNameCandidate, uniqueServiceName));

	RETURN_IF_ERROR(GetGadgetDownloadUrl(gadgetDownloadUrl));
	RETURN_IF_ERROR(m_class->GetGadgetAuthor(gadgetAuthor));
	RETURN_IF_ERROR(m_class->GetGadgetDescription(gadgetDescription));

	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_GADGET_FOLDER, gadgetStoragePath));
	RETURN_IF_ERROR(gadgetStoragePath.Append("/"));

	OpString identifier;
	RETURN_IF_ERROR(identifier.Set(GetIdentifier()));
	RETURN_IF_ERROR(gadgetStoragePath.Append(identifier));

	RETURN_IF_ERROR(gadgetStoragePath.Append("/webserver/"));

	WebSubServer *subserver;
	RETURN_IF_ERROR(WebSubServer::Make(subserver, g_webserver, m_window->Id(), gadgetStoragePath, gadgetName, uniqueServiceName, gadgetAuthor, gadgetDescription, gadgetDownloadUrl, TRUE));
	OP_STATUS err = OpStatus::OK;

	if (m_is_root_service)
		err = g_webserver->SetRootSubServer(subserver);
	else
		err = g_webserver->AddSubServer(subserver);

	if (OpStatus::IsError(err))
	{
		OP_DELETE(subserver);
		return err;
	}

	// retrieve visibility from storage, or set default
	const uni_char * visibility;

	visibility = GetPersistentData(UNI_L("visibility_asd"));
	if (visibility)
	{
		subserver->SetVisibleASD(uni_atoi(visibility) != 0);
	}
	else
	{
		SetPersistentData(UNI_L("visibility_asd"), subserver->IsVisibleASD());
	}

	visibility = GetPersistentData(UNI_L("visibility_robots"));
	if (visibility)
	{
		subserver->SetVisibleRobots(uni_atoi(visibility) != 0);
	}
	else
	{
		SetPersistentData(UNI_L("visibility_robots"), subserver->IsVisibleRobots());
	}

	visibility = GetPersistentData(UNI_L("visibility_upnp"));
	if (visibility)
	{
		subserver->SetVisibleUPNP(uni_atoi(visibility) != 0);
	}
	else
	{
		SetPersistentData(UNI_L("visibility_upnp"), subserver->IsVisibleUPNP());
	}

	OpString webserverPath;
	RETURN_IF_ERROR(webserverPath.Set(GetGadgetPath()));
	RETURN_IF_ERROR(webserverPath.Append("/public_html/"));

	WebserverResourceDescriptor_Static *staticResource = WebserverResourceDescriptor_Static::Make(webserverPath);

	if (staticResource == NULL || OpStatus::IsError(subserver->AddSubserverResource(staticResource)))
	{
		OP_DELETE(staticResource);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS
OpGadget::GetGadgetSubserverUri(OpString& subserver_uri)
{
	//  Use saved name if a name has been saved.
	const uni_char* ui_subserver_uri = GetUIData(UNI_L("subserver_uri"));

	if (ui_subserver_uri != NULL)
		return subserver_uri.Set(ui_subserver_uri);

	//  Fall back to using the default name.
	return m_class->GetGadgetSubserverUri(subserver_uri);
}

BOOL
OpGadget::IsVisibleASD() const
{
	OpStringC service_path(GetUIData(UNI_L("serviceName")));
	WebSubServer * subserver = g_webserver->GetSubServer(service_path);
	if (subserver)
	{
		return subserver->IsVisibleASD();
	}
	else
	{
		const uni_char * visibility = GetPersistentData(UNI_L("visibility_asd"));
		if (visibility)
		{
			return uni_atoi(visibility) != 0;
		}
		else
		{
			return TRUE; // TODO: get default value from somewhere
		}
	}
}

BOOL
OpGadget::IsVisibleUPNP() const
{
	OpStringC service_path(GetUIData(UNI_L("serviceName")));
	WebSubServer * subserver = g_webserver->GetSubServer(service_path);
	if (subserver)
	{
		return subserver->IsVisibleUPNP();
	}
	else
	{
		const uni_char * visibility = GetPersistentData(UNI_L("visibility_upnp"));
		if (visibility)
		{
			return uni_atoi(visibility) != 0;
		}
		else
		{
			return TRUE; // TODO: get default value from somewhere
		}
	}
}

BOOL
OpGadget::IsVisibleRobots() const
{
	OpStringC service_path(GetUIData(UNI_L("serviceName")));
	WebSubServer * subserver = g_webserver->GetSubServer(service_path);
	if (subserver)
	{
		return subserver->IsVisibleRobots();
	}
	else
	{
		const uni_char * visibility = GetPersistentData(UNI_L("visibility_robots"));
		if (visibility)
		{
			return uni_atoi(visibility) != 0;
		}
		else
		{
			return FALSE; // TODO: get default value from somewhere
		}
	}
}

void
OpGadget::SetVisibleASD(BOOL asd_visibility)
{
	OpStringC service_path(GetUIData(UNI_L("serviceName")));
	WebSubServer * subserver = g_webserver->GetSubServer(service_path);
	if (subserver)
	{
		subserver->SetVisibleASD(asd_visibility);
	}

	SetPersistentData(UNI_L("visibility_asd"), asd_visibility);
}

void
OpGadget::SetVisibleUPNP(BOOL upnp_visibility)
{
	OpStringC service_path(GetUIData(UNI_L("serviceName")));
	WebSubServer * subserver = g_webserver->GetSubServer(service_path);
	if (subserver)
	{
		subserver->SetVisibleUPNP(upnp_visibility);
	}

	SetPersistentData(UNI_L("visibility_upnp"), upnp_visibility);
}

void
OpGadget::SetVisibleRobots(BOOL robots_visibility)
{
	OpStringC service_path(GetUIData(UNI_L("serviceName")));
	WebSubServer * subserver = g_webserver->GetSubServer(service_path);
	if (subserver)
	{
		subserver->SetVisibleRobots(robots_visibility);
	}

	SetPersistentData(UNI_L("visibility_robots"), robots_visibility);
}

#endif // WEBSERVER_SUPPORT

OP_STATUS
OpGadget::SetSharedFolder(const uni_char *folder)
{
	RETURN_IF_ERROR(m_shared_folder.Set(folder));

	return SetUIData(UNI_L("sharedFolder"), folder);
}

const uni_char *
OpGadget::GetSharedFolder()
{
	if (m_shared_folder.IsEmpty())
		OpStatus::Ignore(m_shared_folder.Set(GetUIData(UNI_L("sharedFolder"))));

	return m_shared_folder.CStr();
}

OP_STATUS
OpGadget::CreateCacheContext()
{
	m_context_id = urlManager->GetNewContextID();

	OpFileFolder cache_folder;
	RETURN_IF_ERROR(g_folder_manager->AddFolder(m_gadget_root, UNI_L("cache"), &cache_folder ));

	DeleteCacheContext();

	RETURN_IF_LEAVE(urlManager->AddContextL(UrlContextId(), m_gadget_root, m_gadget_root, m_gadget_root, cache_folder, !!GetAttribute(WIDGET_EXTENSION_SHARE_URL_CONTEXT)));

#ifdef EXTENSION_SUPPORT
	if (IsExtension())
		RETURN_IF_ERROR(DATABASE_MODULE_ADD_EXTENSION_CONTEXT(UrlContextId(), m_gadget_root));
	else
#endif // EXTENSION_SUPPORT
		RETURN_IF_ERROR(DATABASE_MODULE_ADD_CONTEXT(UrlContextId(), m_gadget_root));

	return OpStatus::OK;
}

void
OpGadget::DeleteCacheContext()
{
	if (urlManager->ContextExists(UrlContextId()))
		urlManager->RemoveContext(UrlContextId(), TRUE);
}

OP_STATUS
OpGadget::Construct(OpString& gadget_id)
{
	if (gadget_id.IsEmpty())
	{
		OpGuid wuid;
		RETURN_IF_ERROR(g_opguidManager->GenerateGuid(wuid));
		char uuid_string[37]; /* ARRAY OK 2011-11-14 peter */
		g_opguidManager->ToString(wuid, uuid_string, ARRAY_SIZE(uuid_string));
		RETURN_IF_ERROR(gadget_id.Set("wuid-"));
		RETURN_IF_ERROR(gadget_id.Append(uuid_string));
	}

	RETURN_IF_ERROR(m_identifier.Set(gadget_id));

	OpString gadget_root;
	RETURN_IF_ERROR(gadget_root.Set(gadget_id));
	if (g_gadget_manager->GetSeparateStorageKey() && *g_gadget_manager->GetSeparateStorageKey())
	{
		RETURN_IF_ERROR(gadget_root.Append(PATHSEP));
		RETURN_IF_ERROR(gadget_root.Append(g_gadget_manager->GetSeparateStorageKey()));
	}
	RETURN_IF_ERROR(g_folder_manager->AddFolder(OPFILE_GADGET_FOLDER, gadget_root.CStr(), &m_gadget_root));

	RETURN_IF_ERROR(CreateCacheContext());

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GADGET_DELAYED_PREFS_WRITE, reinterpret_cast<MH_PARAM_1>(this)));

	OpFile file;
#ifdef GADGET_ENCRYPT_PREFERENCES
	byte key[16]; /* ARRAY OK 2010-08-24 peter */
	OpCryptoKeyManager::GetObfuscatedDeviceKey(key, 16);
	RETURN_IF_ERROR(file.ConstructEncrypted(UNI_L("prefs.dat"), key, 16, m_gadget_root));
#else
	RETURN_IF_ERROR(file.Construct(UNI_L("prefs.dat"), m_gadget_root));
#endif // GADGET_ENCRYPT_PREFERENCES

	m_prefsfile = OP_NEW(PrefsFile, (PREFS_XML));
	if (!m_prefsfile)
		return OpStatus::ERR_NO_MEMORY;

	TRAP_AND_RETURN(err, m_prefsfile->ConstructL(); m_prefsfile->SetFileL(&file); m_prefsfile->LoadAllL());

	m_shared_folder.Set(GetUIData(UNI_L("sharedFolder")));

	return OpStatus::OK;
}

OP_GADGET_STATUS
OpGadget::Upgrade(OpGadgetClass* gadget_class)
{
	OpGadgetClass* original_class = m_class;
	m_class = gadget_class;

	RETURN_IF_ERROR(SetUIData(UNI_L("name"), m_class->GetAttribute(WIDGET_NAME_TEXT)));

	// Notify listeners that the gadget instance has been upgraded
	OpGadgetListener::OpGadgetStartStopUpgradeData data;
	data.gadget = this;
	data.gadget_class = original_class;
	g_gadget_manager->NotifyGadgetUpgraded(data);

	OP_STATUS status = urlManager->SetShareCookiesWithMainContext(UrlContextId(), !!GetAttribute(WIDGET_EXTENSION_SHARE_URL_CONTEXT));
	OpStatus::Ignore(status);
	OP_ASSERT(OpStatus::IsSuccess(status));

	return OpStatus::OK;
}

OP_STATUS
OpGadget::OnDragStart()
{
	if (!m_window)
		return OpStatus::OK;

	FramesDocument* frm_doc = (FramesDocument*)m_window->GetCurrentDoc();
	if (!frm_doc)
		return OpStatus::OK;

	DOM_Environment* dom_env = frm_doc->GetDOMEnvironment();
	if (!dom_env)
		return OpStatus::OK;

	return dom_env->HandleGadgetEvent(DOM_Environment::GADGET_EVENT_ONDRAGSTART);
}

OP_STATUS
OpGadget::OnDragStop()
{
	if (!m_window)
		return OpStatus::OK;

	FramesDocument* frm_doc = (FramesDocument*)m_window->GetCurrentDoc();
	if (!frm_doc)
		return OpStatus::OK;

	DOM_Environment* dom_env = frm_doc->GetDOMEnvironment();
	if (!dom_env)
		return OpStatus::OK;

	return dom_env->HandleGadgetEvent(DOM_Environment::GADGET_EVENT_ONDRAGSTOP);
}

#ifdef DOM_JIL_API_SUPPORT
OP_STATUS
OpGadget::OnFocus()
{
	if(!m_window)
		return OpStatus::OK;

	FramesDocument* frm_doc = (FramesDocument*)m_window->GetCurrentDoc();
	if (!frm_doc)
		return OpStatus::OK;

	DOM_Environment* dom_env = frm_doc->GetDOMEnvironment();
	if (!dom_env)
		return OpStatus::OK;

	return dom_env->HandleGadgetEvent(DOM_Environment::GADGET_EVENT_ONFOCUS);
}
#endif // DOM_JIL_API_SUPPORT


OP_STATUS
OpGadget::OnShow()
{
	if (!m_window)
		return OpStatus::OK;

	FramesDocument* frm_doc = (FramesDocument*)m_window->GetCurrentDoc();
	if (!frm_doc)
		return OpStatus::OK;

	DOM_Environment* dom_env = frm_doc->GetDOMEnvironment();
	if (!dom_env)
		return OpStatus::OK;

	return dom_env->HandleGadgetEvent(DOM_Environment::GADGET_EVENT_ONSHOW);
}

OP_STATUS
OpGadget::OnHide()
{
	if (!m_window)
		return OpStatus::OK;

	FramesDocument* frm_doc = (FramesDocument*)m_window->GetCurrentDoc();
	if (!frm_doc)
		return OpStatus::OK;

	DOM_Environment* dom_env = frm_doc->GetDOMEnvironment();
	if (!dom_env)
		return OpStatus::OK;

	return dom_env->HandleGadgetEvent(DOM_Environment::GADGET_EVENT_ONHIDE);
}

void
OpGadget::OnShowNotificationFinished(Reply reply)
{
	if (m_window)
	{
		FramesDocument* frm_doc = (FramesDocument*)m_window->GetCurrentDoc();
		if (frm_doc)
		{
			DOM_Environment* dom_env = frm_doc->GetDOMEnvironment();
			if (dom_env)
				dom_env->HandleGadgetEvent(DOM_Environment::GADGET_EVENT_ONSHOWNOTIFICATIONFINISHED);
		}
	}
}

OP_GADGET_STATUS
OpGadget::Update(const uni_char* additional_params)
{
	return g_gadget_manager->Update(m_class, additional_params);
}

#ifdef GADGETS_MUTABLE_POLICY_SUPPORT
OP_STATUS
OpGadget::SetGlobalSecurityPolicy(GadgetSecurityPolicy *policy)
{
	if (!GetAllowGlobalPolicy())
		return OpStatus::ERR;

	OP_DELETE(m_global_security_policy);
	m_global_security_policy = policy;
	return OpStatus::OK;
}

const GadgetSecurityPolicy*
OpGadget::GetGlobalSecurityPolicy() const
{
	if (GetAllowGlobalPolicy())
		return m_global_security_policy;

	return NULL;
}

void
OpGadget::SetAllowGlobalPolicy(BOOL allow)
{
	m_allow_global_security_policy = allow;
}

BOOL
OpGadget::GetAllowGlobalPolicy() const
{
	return m_allow_global_security_policy;
}
#endif // GADGETS_MUTABLE_POLICY_SUPPORT

#ifdef EXTENSION_SUPPORT
OP_STATUS
OpGadget::OpenExtensionOptionsPage()
{
	OpString full_url;
	RETURN_IF_ERROR(GetGadgetUrl(full_url, FALSE));
	RETURN_IF_ERROR(full_url.Append("/")); // GetGadgetUrl() doesn't append a final slash.
	RETURN_IF_ERROR(full_url.Append(EXTENSIONS_OPTIONS_PAGE_FILENAME));

	OpUiWindowListener::CreateUiWindowArgs args;
	args.type = OpUiWindowListener::WINDOWTYPE_EXTENSION;
	args.extension.type = OpUiWindowListener::EXTENSIONTYPE_OPTIONS;
	return CreateNewUiExtensionWindow(args, full_url.CStr());
}

BOOL
OpGadget::HasOptionsPage()
{
	OpString i18n_path, options_path;
	BOOL found = FALSE;
	RETURN_VALUE_IF_ERROR(options_path.Set(EXTENSIONS_OPTIONS_PAGE_FILENAME), FALSE);
	RETURN_VALUE_IF_ERROR(g_gadget_manager->Findi18nPath(GetClass(), options_path, m_class->HasLocalesFolder(), i18n_path, found, NULL), FALSE);
	return found;
}

OP_STATUS
OpGadget::CreateNewUiExtensionWindow(OpUiWindowListener::CreateUiWindowArgs args, const uni_char *url, OpWindowCommander **new_wc)
{
	WindowCommander *wc = OP_NEW(WindowCommander, ());

	if (!wc)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<WindowCommander> wc_anchor(wc);

	RETURN_IF_ERROR(wc->Init());

	BOOL set_gadget = FALSE;

	if (url)
	{
		URLType type = g_url_api->GetURL(url).Type();
		set_gadget = type == URL_WIDGET;
	}

	if (set_gadget)
		wc->SetGadget(this);

	RETURN_IF_ERROR(g_windowCommanderManager->GetUiWindowListener()->CreateUiWindow(wc, args));

	if (new_wc)
		*new_wc = wc;

	wc->OpenURL(url, FALSE, this->UrlContextId());

	wc_anchor.release();

	return OpStatus::OK;
}

#endif // EXTENSION_SUPPORT


OP_STATUS
OpGadget::OnUiWindowCreated(OpWindow *opWindow, Window *window)
{
#ifdef EXTENSION_SUPPORT
	if (!IsExtension() || !m_window)
#endif // EXTENSION_SUPPORT
	{
		/* It's not an extension, or it's the first widget window of an
		 * extension (a.k.a. the background process). Otherwise it's a regular
		 * window loading resources from an extension. */
		m_window = window;
		window->SetType(WIN_TYPE_GADGET);
		window->SetShowScrollbars(FALSE);

		// load gadget
		OpString url;
		RETURN_IF_ERROR(GetGadgetUrl(url));
		RETURN_IF_ERROR(window->OpenURL(url, TRUE, TRUE, FALSE, FALSE, FALSE, WasEnteredByUser, FALSE, m_context_id));

		m_gadget_url_loaded = TRUE;
	}

#ifdef EXTENSION_SUPPORT
	if (IsExtension())
		RETURN_IF_ERROR(DATABASE_MODULE_ADD_EXTENSION_CONTEXT(m_context_id, m_gadget_root));
	else
#endif // EXTENSION_SUPPORT
		RETURN_IF_ERROR(DATABASE_MODULE_ADD_CONTEXT(m_context_id, m_gadget_root));

	if (m_window == window)
	{
#ifdef WEBSERVER_SUPPORT
		if (g_webserver && m_class->IsSubserver() && m_class->GetWebserverType() == GADGET_WEBSERVER_TYPE_SERVICE)
			RETURN_IF_ERROR(SetupWebSubServer());
#endif //WEBSERVER_SUPPORT

		OpGadgetListener::OpGadgetStartStopUpgradeData data;
		data.gadget = this;
		g_gadget_manager->NotifyGadgetStarted(data);
	}

	SetIsActive(TRUE);

#ifdef EXTENSION_SUPPORT
	// Start extension
	if (m_window == window && IsExtension())
	{
		// Set up location of includes folder
		OpString includes_folder;
		RETURN_IF_ERROR(GetGadgetIncludesPath(includes_folder));

		// Load User JavaScript
		// Locate all *.js files in the includes folder
		OpFolderLister *jslister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*.js"), includes_folder.CStr());
		if (jslister)
		{
			OpStackAutoPtr<OpFolderLister> jslister_anchor(jslister);
			while (jslister->Next())
			{
				// Get path relative to archive top
				const uni_char *userjs_path = FindRelativePath(jslister->GetFullPath(), GetGadgetRootPath());

				OpStackAutoPtr<OpGadgetExtensionUserJS> userjs(OP_NEW(OpGadgetExtensionUserJS, ()));
				if (!userjs.get())
					return OpStatus::ERR_NO_MEMORY;

				OpString i18n_path;
				BOOL found = FALSE;
				RETURN_IF_ERROR(g_gadget_manager->Findi18nPath(GetClass(), userjs_path, m_class->HasLocalesFolder(), i18n_path, found, NULL));
				if (found)
				{
					// Add the localized version of the User JS.
					RETURN_IF_ERROR(userjs->userjs_filename.Set(i18n_path));
					// Add ourselves as the owner of this script
					userjs->owner = this;
					g_gadget_manager->AddUserJavaScript(userjs.release());
				}
			}
		}

// CORE-33669: temporarily disabled until 'UserJS-like' domain matching is possible for CSS.
# if 0
		// Load User style sheets
		// Locate all *.css files in the includes folder
		OpFolderLister *csslister = OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*.css"), includes_folder.CStr());
		if (csslister)
		{
			OpStackAutoPtr<OpFolderLister> csslister_anchor(csslister);
			while (csslister->Next())
			{
				// Get path relative to archive top
				const uni_char *usercss_path = FindRelativePath(csslister->GetFullPath(), GetGadgetRootPath());

				OpString i18n_path;
				BOOL found = FALSE;
				RETURN_IF_ERROR(g_gadget_manager->Findi18nPath(GetGadgetRootPath(), usercss_path, m_class->HasLocalesFolder(), i18n_path, found, NULL));
				if (found)
				{
					// Add the localized version of the User CSS.
					RETURN_IF_ERROR(g_cssManager->AddExtensionUserCSS(i18n_path.CStr(), reinterpret_cast<UINTPTR>(this)));
				}
			}
		}
# endif // 0
	}
#endif

	return OpStatus::OK;
}

void
OpGadget::OnUiWindowClosing(Window *window)
{
#ifdef EXTENSION_SUPPORT
	if (IsExtension() && m_window != window)
		return; // It's not the main background window, so don't do anything.
#endif // EXTENSION_SUPPORT

	m_window = NULL;
	SetIsActive(FALSE);

	OpGadgetListener::OpGadgetStartStopUpgradeData data;
	data.gadget = this;
	g_gadget_manager->NotifyGadgetStopped(data);

#ifdef EXTENSION_SUPPORT
	if (m_class->IsExtension())
		StopExtension();
#endif // 
}

#ifdef EXTENSION_SUPPORT
void
OpGadget::StopExtension()
{
	OP_ASSERT(m_class->IsExtension());
	// Unload User JavaScript
	g_gadget_manager->DeleteUserJavaScripts(this);

// CORE-33669: temporarily disabled until 'UserJS-like' domain matching is possible for CSS.
# if 0
	// Unload user style sheets
	g_cssManager->RemoveExtensionUserCSSes(reinterpret_cast<UINTPTR>(this));
# endif // 0

	// Notify listeners to cease using gadget.
	OpGadgetExtensionListener *l = m_extensions_listeners.First();
	while(l)
	{
		OpGadgetExtensionListener *next = l->Suc();
		l->OnGadgetTerminated();
		l = next;
	}

	m_extensions_listeners.RemoveAll();

	m_is_running = FALSE;
	m_gadget_url_loaded = FALSE;
}

void
OpGadget::SetIsRunning(BOOL r)
{
	m_is_running = r;

#ifdef UTIL_ZIP_CACHE
	/* Try to evict now-unused zip files as part of stopping (=> unlock them for windows users.) */
	if (!r)
		g_opera->util_module.m_zipcache->FlushUnused();
#endif // UTIL_ZIP_CACHE
}

void
OpGadget::ConnectGadgetListeners()
{
	for (OpGadgetExtensionListener * l = m_extensions_listeners.First(); l; l = l->Suc())
	{
		if (!l->HasGadgetConnected())
			l->OnGadgetRunning();
	}
}

void
OpGadget::SetExtensionFlag(ExtensionFlag flag, BOOL value)
{
	if (value)
		m_extension_flags |= flag;
	else
		m_extension_flags &= ~flag;
}

BOOL
OpGadget::GetExtensionFlag(ExtensionFlag flag)
{
	return ((m_extension_flags & flag) != 0);
}
#endif // EXTENSION_SUPPORT


OP_STATUS
OpGadget::SetPersistentData(const uni_char* section, const uni_char* key, const uni_char* data)
{
	TRAP_AND_RETURN(err, m_prefsfile->WriteStringL(section, key, data));
	RETURN_IF_ERROR(CommitPrefs());
	// Section must be re-read after committing changes.
	if (uni_stri_eq_lower(section, "mountpoints"))
	{
		OP_DELETE(m_mp_section);
		m_mp_section = NULL;
	}
	return OpStatus::OK;
}

const uni_char*
OpGadget::GetPersistentData(const uni_char* section, const uni_char* key)
{
	OpStringC result;
	OP_STATUS err;
	TRAP_AND_RETURN_VALUE_IF_ERROR(err, result = m_prefsfile->ReadStringL(section, key), NULL);
	return result.CStr();
}

OP_STATUS
OpGadget::GetPersistentDataItem(UINT32 index, OpString& keyA, OpString& dataA)
{
	OP_MEMORY_VAR UINT32 idx = index;
	if (!m_mp_section)
		RETURN_IF_LEAVE(m_mp_section = m_prefsfile->ReadSectionL(UNI_L("mountpoints")));

	if ((UINT32) m_mp_section->Number() > idx)
	{
		const PrefsEntry *entry = m_mp_section->Entries();
		while (idx--) { entry = entry->Suc(); }

		RETURN_IF_ERROR(keyA.Set(entry->Key()));
		RETURN_IF_ERROR(dataA.Set(entry->Value()));

		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
OpGadget::DeletePersistentData(const uni_char* section, const uni_char* key)
{
	RETURN_IF_LEAVE(m_prefsfile->DeleteKeyL(section, key));
	RETURN_IF_ERROR(CommitPrefs());

	return OpStatus::OK;
}

OP_STATUS
OpGadget::GetStoragePath(OpString& storage_path)
{
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(m_gadget_root, storage_path));

	return OpStatus::OK;
}

OP_STATUS
OpGadget::GetApplicationPath(OpString& path)
{
	return path.Set(GetGadgetPath());
}

OP_STATUS
OpGadget::SetPersistentData(const uni_char *key, const uni_char *data)
{
	if (data)
	{
		RETURN_IF_LEAVE(m_prefsfile->WriteStringL(UNI_L("user"), key, data));
	}
	else
	{
		RETURN_IF_LEAVE(m_prefsfile->DeleteKeyL(UNI_L("user"), key));
	}
	RETURN_IF_ERROR(CommitPrefs());
	return OpStatus::OK;
}

OP_STATUS
OpGadget::SetPersistentData(const uni_char *key, int data)
{
	RETURN_IF_LEAVE(m_prefsfile->WriteIntL(UNI_L("user"), key, data));
	RETURN_IF_ERROR(CommitPrefs());
	return OpStatus::OK;
}

OP_STATUS
OpGadget::SetUIData(const uni_char *key, const uni_char *data)
{
	RETURN_IF_LEAVE(m_prefsfile->WriteStringL(UNI_L("ui"), key, data));
	RETURN_IF_ERROR(CommitPrefs());
	return OpStatus::OK;
}

const uni_char *
OpGadget::GetPersistentData(const uni_char *key) const
{
	OpStringC result;
	OP_STATUS err;
	TRAP_AND_RETURN_VALUE_IF_ERROR(err, result = m_prefsfile->ReadStringL(UNI_L("user"), key), NULL);
	return result.CStr();
}

const uni_char *
OpGadget::GetUIData(const uni_char *key) const
{
	OpStringC result;
	OP_STATUS err;
	TRAP_AND_RETURN_VALUE_IF_ERROR(err, result = m_prefsfile->ReadStringL(UNI_L("ui"), key), NULL);
	return result.CStr();
}

OP_STATUS
OpGadget::SetGlobalData(const uni_char *key, const uni_char *data)
{
	return g_gadget_manager->SetGlobalData(key, data);
}

const uni_char*
OpGadget::GetGlobalData(const uni_char *key)
{
	return g_gadget_manager->GetGlobalData(key);
}

OP_STATUS OpGadget::SetSessionData(const uni_char *key, const uni_char *data)
{
	SessionDataItem *data_item = 0;
	if (OpStatus::IsSuccess(m_session_hash.GetData(key, &data_item)))
	{
		// already in the session hash
		uni_char *data_copy = uni_strdup(data);
		if(!data_copy)
			return OpStatus::ERR_NO_MEMORY;
		op_free(data_item->data);
		data_item->data = data_copy;
		return OpStatus::OK;
	}

	data_item = OP_NEW(SessionDataItem, ());
	if (!data_item)
		return OpStatus::ERR_NO_MEMORY;

	data_item->key = uni_strdup(key);
	data_item->data = uni_strdup(data);
	if (!data_item->key || !data_item->data)
	{
		OP_DELETE(data_item);
		return OpStatus::ERR_NO_MEMORY;
	}
	OP_STATUS retval = m_session_hash.Add(data_item->key, data_item);
	if (OpStatus::IsError(retval))
		OP_DELETE(data_item);
	return retval;
}

const uni_char *OpGadget::GetSessionData(const uni_char *key)
{
	SessionDataItem *data_item = 0;
	if (OpStatus::IsError(m_session_hash.GetData(key, &data_item)))
		return NULL;
	OP_ASSERT(data_item);
	return data_item->data;
}

#ifdef EXTENSION_SUPPORT
URL OpGadget::ResolveExtensionURL(const uni_char *url_s)
{
	OpString gadget_url_s;

	if (OpStatus::IsError(GetGadgetUrl(gadget_url_s, FALSE)))
		return g_url_api->GetURL(url_s);

	URL gadget_url = g_url_api->GetURL(gadget_url_s, m_context_id);
	return g_url_api->GetURL(gadget_url, url_s, FALSE, m_context_id);
}
#endif // EXTENSION_SUPPORT

void
OpGadget::OpenURLInNewWindow(const uni_char* url, Window* opener_window)
{
	if (!opener_window)
		opener_window = m_window;

	URL url_to_open;

#ifdef EXTENSION_SUPPORT
	if (IsExtension())
		url_to_open = ResolveExtensionURL(url);
	else
#endif // EXTENSION_SUPPORT
		url_to_open = g_url_api->GetURL(url, m_context_id);

	OpenURLInNewWindow(url_to_open, opener_window);
}

void
OpGadget::OpenURLInNewWindow(const URL& url, Window* opener_window)
{
	if (!opener_window)
		opener_window = m_window;

	BOOL permitted;
	OpSecurityState state;
	OpSecurityManager::Operation op(OpSecurityManager::GADGET_EXTERNAL_URL);

	URL url_to_open = url;
#ifdef EXTENSION_SUPPORT
	if(IsExtension())
	{
		if (url.Type() == URL_WIDGET)
			op = OpSecurityManager::DOM_LOADSAVE;
		else if (url.GetContextId() == UrlContextId())
		{
			/* Hackish, but windows that pop out of extensions and are not
			 * internal should share the main browsing context so as not to mix
			 * "identical" looking tabs using different contexts and confuse
			 * the user. Hardcoding 0 as the context since it can't be a
			 * private/turbo/etc context since it was previously in the gadget's
			 * context */
			url_to_open = g_url_api->GetURL(url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr(), static_cast<URL_CONTEXT_ID>(0));
		}
	}
#endif // EXTENSION_SUPPORT

	OpSecurityManager::CheckSecurity(op, OpSecurityContext(this, opener_window), OpSecurityContext(url_to_open), permitted, state);

	if (state.suspended)
	{
		OpGadgetAsyncObject* aso = OP_NEW(OpGadgetAsyncObject, (this));
		if (aso == NULL)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}
		aso->Into(&m_aso_objects);

		aso->m_url = url_to_open;
		aso->m_open_in_new_window = TRUE;
		aso->m_secstate = state;

		state.host_resolving_comm = NULL; // aso object takes ownership

		Comm* comm = aso->m_secstate.host_resolving_comm;
		g_main_message_handler->SetCallBack(aso, MSG_COMM_NAMERESOLVED, comm->Id());
		g_main_message_handler->SetCallBack(aso, MSG_COMM_LOADING_FAILED, comm->Id());
		return;
	}

	if (permitted)
	{
#ifdef USE_EXTERNAL_BROWSER
		OP_STATUS err = OpStatus::ERR;
# ifdef EXTENSION_SUPPORT
		if (!IsExtension())
# endif // EXTENSION_SUPPORT
		{
			OpString url_name;
			RETURN_VOID_IF_ERROR(url_to_open.GetAttribute(URL::KUniName_Username, url_name));
			err = g_op_platform_viewers->OpenInDefaultBrowser(url_name);
		}
		if (OpStatus::IsSuccess(err))
			return;
		else
#endif
		{
			// Fallback on opening URL in Opera
			Window* window = g_windowManager->SignalNewWindow(opener_window);
			URL tmp_url = url_to_open;
			URL dummy = URL();
			if (window)
			{
#ifdef EXTENSION_SUPPORT
				if(IsExtension())
				{
					if (url_to_open.Type() == URL_WIDGET)
						window->SetGadget(this);

					OP_STATUS status = window->OpenURL(tmp_url, DocumentReferrer(dummy), FALSE, FALSE, -1);

					if (OpStatus::IsError(status))
						window->SetGadget(NULL);
				}
				else
#endif //EXTENSION_SUPPORT
				{
					window->OpenURL(tmp_url, DocumentReferrer(dummy), FALSE, FALSE, -1);
				}
			}
		}
	}
}

void
OpGadget::OpenURL(const URL& url)
{
	BOOL permitted;
	OpSecurityState state;
	OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_LOADSAVE, OpSecurityContext(this), OpSecurityContext(url), permitted, state);

	if (state.suspended)
	{
		OpGadgetAsyncObject* aso = OP_NEW(OpGadgetAsyncObject, (this));
		if (aso == NULL)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}
		aso->Into(&m_aso_objects);

		aso->m_url = url;
		aso->m_open_in_new_window = FALSE;
		aso->m_secstate = state;

		state.host_resolving_comm = NULL; // aso object takes ownership

		Comm* comm = aso->m_secstate.host_resolving_comm;
		g_main_message_handler->SetCallBack(aso, MSG_COMM_NAMERESOLVED, comm->Id());
		g_main_message_handler->SetCallBack(aso, MSG_COMM_LOADING_FAILED, comm->Id());
		return;
	}

	if (permitted)
	{
		URL tmp_url = url;
		URL dummy = URL();
		if (m_window)
			OpStatus::Ignore(m_window->OpenURL(tmp_url, DocumentReferrer(dummy), FALSE, FALSE, -1));
	}
}

void
OpGadget::OpenURL(const uni_char* url, BOOL is_dom_call)
{
	URL url_to_open = g_url_api->GetURL(url, m_context_id);
	OpenURL(url_to_open);
}

void
OpGadget::Reload()
{
	OpStatus::Ignore(m_class->Reload());
#ifdef WEBSERVER_SUPPORT
	OpStatus::Ignore(SetupWebSubServer());
#endif // WEBSERVER_SUPPORT
	TRAPD(error, m_prefsfile->WriteIntL(UNI_L("ui"), UNI_L("default-prefs-applied"), FALSE));
	OpStatus::Ignore(error);
	m_default_prefs_applied = FALSE;
}

void
OpGadget::GetAttention()
{
	WindowCommander* wc = m_window ? m_window->GetWindowCommander() : NULL;
	if (wc)
		wc->GetDocumentListener()->OnGadgetGetAttention(wc);
}

OP_STATUS
OpGadget::ShowNotification(const uni_char* message)
{
	WindowCommander* wc = m_window ? m_window->GetWindowCommander() : NULL;
	if (wc)
		wc->GetDocumentListener()->OnGadgetShowNotification(wc, message, this);

	return OpStatus::OK;
}

OP_STATUS
OpGadget::GetGadgetUrl(OpString& url, BOOL include_index_file, BOOL include_protocol)
{
	url.Empty();

	if (include_protocol)
		RETURN_IF_ERROR(url.Append(UNI_L("widget://")));

	RETURN_IF_ERROR(url.Append(m_identifier));

	if (include_index_file && m_class->GetGadgetFile() && *m_class->GetGadgetFile())
	{
		// GetGadgetFile() returns a path with local file separators. We need to
		// reverse the file separators and escape it for use in a URL.
		RETURN_IF_ERROR(url.Append(UNI_L("/")));
#if PATHSEPCHAR != '/'
		OpString start_file;

		RETURN_IF_ERROR(m_class->GetGadgetFile(start_file));
		for (uni_char *p = start_file.CStr(); *p; ++ p)
		{
			if (PATHSEPCHAR == *p)
			{
				*p = '/';
			}
		}
		UriEscape::AppendEscaped(url, start_file, UriEscape::StandardUnsafe | UriEscape::Percent | UriEscape::Hash);
#else
		UriEscape::AppendEscaped(url, m_class->GetGadgetFile(), UriEscape::StandardUnsafe | UriEscape::Percent | UriEscape::Hash);
#endif
	}

	uni_char *purl = url.CStr();
	for (int i = 0; i < url.Length(); i++)
	{
		if(purl[i] == '\\')
			purl[i] = '/';
	}

	return OpStatus::OK;
}

void
OpGadget::SetIcon(const uni_char *icon)
{
}

OP_STATUS
OpGadget::InitializePreferences(InitPrefsReason reason)
{
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	// Should be called from outside initcore. Currently called from DOM_Widget just before we create the webstorage object.

	if (!m_class->SupportsNamespace(GADGETNS_W3C_1_0))
		return OpStatus::OK;

	OpGadgetPreference *pref = m_class->GetFirstPref();
	if (pref == NULL)
		return OpStatus::OK;

	if (!m_default_prefs_applied)
		RETURN_IF_LEAVE(m_default_prefs_applied = m_prefsfile->ReadBoolL(UNI_L("ui"), UNI_L("default-prefs-applied")));

	if (reason == PREFS_APPLY_DATA_CLEARED || reason == PREFS_APPLY_DATA_LOST)
	{
		/* If the prefs have not been applied, they are not applied again.
		 * When reapply = true, it is during private data, so this resets
		 * widget.prefs to default values.
		 * When reapply = false it is during the first access to widget.prefs,
		 * so the values from config.xml are prefilled, if it is the first
		 * time it runs.
		 * This heuristic avoids prefilling the values during delete private
		 * data if they have never been before (widget.prefs never accessed)
		 * which is a small optimization. */
		if (reason == PREFS_APPLY_DATA_CLEARED && !m_default_prefs_applied)
			return OpStatus::OK;
		TRAPD(status, status = m_prefsfile->WriteIntL(UNI_L("ui"), UNI_L("default-prefs-applied"), FALSE));
		OpStatus::Ignore(status);
		m_default_prefs_applied = FALSE;
	}

	if (!m_default_prefs_applied)
	{
		// Initialize default preferences
		OpString origin;
		RETURN_IF_ERROR(GetGadgetUrl(origin, FALSE, TRUE));
		OpStorage *storage;
		RETURN_IF_ERROR(g_webstorage_manager->GetStorage(WEB_STORAGE_WGT_PREFS, UrlContextId(), origin.CStr(), TRUE, &storage));
		AutoReleaseOpStoragePtr storage_anchor(storage);

		while (pref)
		{
			WebStorageValueTemp key(pref->Name(), pref->NameLength());
			WebStorageValueTemp value(pref->Value(), pref->ValueLength());

			RETURN_IF_ERROR(storage->SetNewItemReadOnly(&key, &value, pref->IsReadOnly(), NULL));
			pref = static_cast<OpGadgetPreference*>(pref->Suc());
		}

		storage->OnDefaultWidgetPrefsSet();

		RETURN_IF_LEAVE(m_prefsfile->WriteIntL(UNI_L("ui"), UNI_L("default-prefs-applied"), TRUE));
		m_default_prefs_applied = TRUE;
	}
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT

	return OpStatus::OK;
}

void
OpGadget::MigrateHashL(PrefsFile* from_prefsfile, const uni_char* from_section, const uni_char* from_hashname, const uni_char *to_section)
{
	OpString num_val_fmt; ANCHOR(OpString, num_val_fmt);
	LEAVE_IF_ERROR(num_val_fmt.AppendFormat(UNI_L("number of saved %svalues"), from_hashname));

	OP_MEMORY_VAR int nr_of_values = 0;
	nr_of_values = from_prefsfile->ReadIntL(from_section, num_val_fmt, 0);
	while (nr_of_values)
	{
		nr_of_values--;

		OpString key; ANCHOR(OpString, key);
		LEAVE_IF_ERROR(key.AppendFormat(UNI_L("%skey #%d"), from_hashname, nr_of_values));
		OpStringC key_value;
		key_value = from_prefsfile->ReadStringL(from_section, key);

		OpString value; ANCHOR(OpString, value);
		LEAVE_IF_ERROR(value.AppendFormat(UNI_L("%sval #%d"), from_hashname, nr_of_values));
		OpStringC value_value;
		value_value = from_prefsfile->ReadStringL(from_section, value);

		m_prefsfile->WriteStringL(to_section, key_value, value_value);

		from_prefsfile->DeleteKeyL(from_section, key.CStr());
		from_prefsfile->DeleteKeyL(from_section, value.CStr());
	};

	from_prefsfile->DeleteKeyL(from_section, num_val_fmt.CStr());
}

void
OpGadget::LoadOldPrefsL(PrefsFile* prefsfile, const OpStringC& section)
{
	// intranet/internet bit
	OP_MEMORY_VAR BOOL3 intranet = (BOOL3) prefsfile->ReadIntL(section, UNI_L("intranet"), MAYBE);
	if (intranet != MAYBE)
	{
		m_prefsfile->WriteIntL(section, UNI_L("network_access"), intranet == YES ? GadgetPrivateNetwork::INTRANET : GadgetPrivateNetwork::INTERNET);
		prefsfile->DeleteKeyL(section.CStr(), UNI_L("intranet"));
	}

	OP_MEMORY_VAR int network_access = prefsfile->ReadIntL(section, UNI_L("network_access"), 0);
	if (network_access != 0)
	{
		m_prefsfile->WriteIntL(section, UNI_L("network_access"), network_access);
		prefsfile->DeleteKeyL(section.CStr(), UNI_L("network_access"));
	}

	// ui data
	MigrateHashL(prefsfile, section, UNI_L("ui "), UNI_L("ui"));

	// global data
	MigrateHashL(prefsfile, section, UNI_L(""), UNI_L("user"));

	// persistent data
	MigrateHashL(prefsfile, section, UNI_L("mountpoints "), UNI_L("mountpoints"));

	// Commit data
	m_prefsfile->CommitL(TRUE, FALSE);
	prefsfile->CommitL(TRUE, FALSE);
}

void
OpGadget::LoadFromFileL(PrefsFile* global_prefsfile, const OpStringC& section)
{
	// migrate old preferences if we have any.
	LoadOldPrefsL(global_prefsfile, section);

	// Load widget specific settings.
	OP_MEMORY_VAR INT access = (INT) m_prefsfile->ReadIntL(section, UNI_L("network_access"), GadgetPrivateNetwork::UNKNOWN);
	if (GetClass()->GetSecurity() && access != GadgetPrivateNetwork::UNKNOWN)
		GetClass()->GetSecurity()->private_network->allow = access;

	OP_MEMORY_VAR BOOL persistent = global_prefsfile->ReadBoolL(section, UNI_L("persistent"));
	m_class->SetPersistent(persistent);
}

void
OpGadget::SaveToFileL(PrefsFile* /*global_prefsfile*/, const OpStringC& section)
{
	// Save widget specific settings.
	OP_MEMORY_VAR INT access = GadgetPrivateNetwork::NONE;
	if (GetClass()->GetSecurity())
		access = GetClass()->GetSecurity()->private_network->allow;
	m_prefsfile->WriteIntL(section, UNI_L("network_access"), access);

	m_prefsfile->CommitL(TRUE, FALSE);
}

OP_STATUS
OpGadget::CommitPrefs()
{
	if (!m_write_msg_posted)
	{
		if (!g_main_message_handler->PostDelayedMessage(MSG_GADGET_DELAYED_PREFS_WRITE, reinterpret_cast<MH_PARAM_1>(this), 0, GADGETS_WRITE_PREFS_DELAY))
			return OpStatus::ERR;

		m_write_msg_posted = TRUE;
	}

	return OpStatus::OK;
}

void
OpGadget::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_GADGET_DELAYED_PREFS_WRITE)
	{
		m_write_msg_posted = FALSE;
		TRAPD(err, m_prefsfile->CommitL(TRUE, FALSE));
		RAISE_AND_RETURN_VOID_IF_ERROR(err);
	}
}

void
OpGadget::SetInstanceSecurity(GadgetSecurityPolicy* p)
{
	if (m_security_policy != p)
	{
		if (m_security_policy)
		{
			if (p->private_network && m_security_policy->private_network)
				p->private_network->allow = m_security_policy->private_network->allow;	// Do not throw away old intranet/internet sticky thingy
			OP_DELETE(m_security_policy);
		}
		m_security_policy = p;
	}
}

//////////////////////////////////////////////////////////////////////////

OpGadgetAsyncObject::OpGadgetAsyncObject(OpGadget* gadget)
: m_gadget(gadget)
, m_open_in_new_window(FALSE)
{
	OP_ASSERT(gadget);
}

OpGadgetAsyncObject::~OpGadgetAsyncObject()
{
	Out();
	g_main_message_handler->UnsetCallBacks(this);
}

void
OpGadgetAsyncObject::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	g_main_message_handler->UnsetCallBacks(this);

	if (msg == MSG_COMM_NAMERESOLVED)
	{
		if (m_open_in_new_window)
			m_gadget->OpenURLInNewWindow(m_url);
		else
			m_gadget->OpenURL(m_url);
	}

	OP_DELETE(this);
}

#endif // GADGET_SUPPORT

