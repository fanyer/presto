/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/js/navigat.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/uamanager/ua.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT

#if defined _APPLET_2_EMBED_ && defined _PLUGIN_SUPPORT_
# include "modules/viewers/viewers.h"
#endif // _APPLET_2_EMBED_ && _PLUGIN_SUPPORT_

#if defined(DOM_GEOLOCATION_SUPPORT) && defined(PI_GEOLOCATION)
# include "modules/dom/src/js/geoloc.h"
#endif // DOM_GEOLOCATION_SUPPORT && PI_GEOLOCATION

#ifdef DOM_STREAM_API_SUPPORT
# include "modules/dom/src/media/domstream.h"
# include "modules/media/camerarecorder.h"
#endif // DOM_STREAM_API_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
#define SUPPORT_NAVIGATOR_ID_ONLINE_ONLY 1
#define IS_NAV_PROPERTY_SUPPORTED(prop) (GetEnvironment()->GetWorkerController()->GetWorkerObject() == NULL)
#endif // DOM_WEBWORKERS_SUPPORT

#if !defined(SUPPORT_NAVIGATOR_ID_ONLINE_ONLY)
#define IS_NAV_PROPERTY_SUPPORTED(prop) (1)
#endif

#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT

#ifdef WEB_HANDLERS_SUPPORT
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dom/src/js/location.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/domutils.h"
#ifdef WEBFEEDS_EXTERNAL_READERS
#include "modules/webfeeds/webfeeds_api.h"
#endif // WEBFEEDS_EXTERNAL_READERS
#include "modules/dom/src/domrestartobject.h"
#endif // WEB_HANDLERS_SUPPORT

static UA_BaseStringId
DOM_GetUAStringId(DOM_Object *object, BOOL resolveNotOverridden = FALSE)
{
	ServerName *hostname = object->GetHostName();

	UA_BaseStringId id;

	id = (UA_BaseStringId) g_pcjs->GetIntegerPref(PrefsCollectionJS::ScriptSpoof, hostname);
	if (id != UA_NotOverridden)
		return id;

	if (FramesDocument* frames_doc = object->GetFramesDocument())
	{
		id = (UA_BaseStringId) frames_doc->GetURL().GetAttribute(URL::KOverRideUserAgentId);
		if (id != UA_NotOverridden)
			return id;
	}

	if (resolveNotOverridden)
		if (hostname)
			return (UA_BaseStringId) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UABaseId, hostname);
		else
			return g_uaManager->GetUABaseId();

	return UA_NotOverridden;
}

/* static */ void
JS_Navigator::MakeL(DOM_Object *&object, DOM_Runtime *runtime)
{
	JS_Navigator *navigator = OP_NEW(JS_Navigator, ());
	DOMSetObjectRuntimeL(navigator, runtime, runtime->GetPrototype(DOM_Runtime::NAVIGATOR_PROTOTYPE), "Navigator");
	object = navigator;

#ifdef DOM_STREAM_API_SUPPORT
#ifdef GADGET_SUPPORT
	OpGadget *gadget = NULL;
	if (FramesDocument *doc = runtime->GetFramesDocument())
		gadget = doc->GetWindow()->GetGadget();

	if (!gadget || gadget->HasGetUserMediaAccess())
#endif // GADGET_SUPPORT
		navigator->AddFunctionL(JS_Navigator::getUserMedia, "getUserMedia", "(s|?{video:b})-");
#endif // DOM_STREAM_API_SUPPORT
}

/* virtual */ ES_GetState
JS_Navigator::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState state;
	ServerName *hostname = GetHostName();

	switch(property_name)
	{
	case OP_ATOM_appVersion:
	case OP_ATOM_userAgent:
		if (value)
		{
			const uni_char *host;
			if (hostname)
				host = hostname->UniName();
			else
				host = NULL;

			Window *window;
			if (FramesDocument *doc = GetFramesDocument())
				window = doc->GetWindow();
			else
				window = NULL;

			TempBuffer *buf = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(buf->Expand(256)); /* 256 characters should be enough */
			char *char_buffer = reinterpret_cast<char *>(buf->GetStorage());
			int ualen = 0;

			if (property_name == OP_ATOM_appVersion)
			{
				ualen = g_uaManager->GetUserAgentVersionStr(char_buffer, buf->GetCapacity(), host, window, DOM_GetUAStringId(this));
#ifdef _MACINTOSH_
				// Mac spoof fix: On Macintosh, "Mac_PowerPC" should be reported in navigator.userAgent (when spoofing IE), and
				// "Macintosh" should be used in navigator.appVersion. Otherwise certain Acive-X capability detectors will fail.
				// (that is, serve us Active X content). Most noticably shockwave.com.
				char *change = op_strstr(char_buffer, "Mac_PowerPC");
				if (change)
					op_strcpy(change, "Macintosh)");
#endif // _MACINTOSH_
			}
			else
			{
				ualen = g_uaManager->GetUserAgentStr(char_buffer, buf->GetCapacity(), host, window, DOM_GetUAStringId(this));
			}
			make_doublebyte_in_place(buf->GetStorage(), ualen);
			DOMSetString(value, buf);
		}
		return GET_SUCCESS;

	case OP_ATOM_appCodeName:
		if (!IS_NAV_PROPERTY_SUPPORTED(OP_ATOM_appCodeName))
			return GET_FAILED;
		if (value)
		{
			OpStringC data = g_pcjs->GetStringPref(PrefsCollectionJS::ES_AppCodeName, hostname);

			uni_char *tmpbuf = (uni_char *) g_memory_manager->GetTempBufUni();
			uni_strcpy(tmpbuf, data.CStr());
			DOMSetString(value, tmpbuf);
		}
		return GET_SUCCESS;

	case OP_ATOM_appMinorVersion:
		if (!IS_NAV_PROPERTY_SUPPORTED(OP_ATOM_appMinorVersion))
			return GET_FAILED;
		// MSIE 5.01 windows returns ";SP1;" (???)
		// Undefined in Netscape 4.7
		// For the time being we return "" because it works better than undefined.
		DOMSetString(value);
		return GET_SUCCESS;

	case OP_ATOM_browserLanguage:
	case OP_ATOM_language:
	case OP_ATOM_userLanguage:
		if (!IS_NAV_PROPERTY_SUPPORTED(OP_ATOM_browserLanguage))
			return GET_FAILED;
		if (value)
		{
			const uni_char* lang = g_languageManager->GetLanguage().CStr();
			DOMSetString(value, lang ? lang : UNI_L(""));
		}
		return GET_SUCCESS;

	case OP_ATOM_platform:
		DOMSetString(value, g_op_system_info->GetPlatformStr());
		return GET_SUCCESS;

	case OP_ATOM_appName:
		if (value)
		{
			OpStringC data;

			switch (DOM_GetUAStringId(this, TRUE))
			{
			case UA_MSIE:
			case UA_MSIE_Only:
				data = g_pcjs->GetStringPref(PrefsCollectionJS::ES_IEAppName, hostname);
				break;

			case UA_Mozilla:
			case UA_MozillaOnly:
				data = g_pcjs->GetStringPref(PrefsCollectionJS::ES_NSAppName, hostname);
				break;

			default:
				data = g_pcjs->GetStringPref(PrefsCollectionJS::ES_OperaAppName, hostname);
			}

			uni_char *tmpbuf = (uni_char *) g_memory_manager->GetTempBufUni();
			uni_strcpy(tmpbuf, data.CStr());

			DOMSetString(value, tmpbuf);
		}
		return GET_SUCCESS;

	case OP_ATOM_cookieEnabled:
		if (!IS_NAV_PROPERTY_SUPPORTED(OP_ATOM_cookieEnabled))
			return GET_FAILED;
		DOMSetBoolean(value, (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled, hostname) != COOKIE_NONE);
		return GET_SUCCESS;

	case OP_ATOM_plugins:
		if (!IS_NAV_PROPERTY_SUPPORTED(OP_ATOM_plugins))
			return GET_FAILED;
		state = DOMSetPrivate(value, DOM_PRIVATE_plugins);

		if (state == GET_FAILED)
		{
			JS_PluginArray *plugins;

			GET_FAILED_IF_ERROR(JS_PluginArray::Make(plugins, GetEnvironment()));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_plugins, *plugins));

			DOMSetObject(value, plugins);
			return GET_SUCCESS;
		}
		else
			return state;

	case OP_ATOM_mimeTypes:
		if (!IS_NAV_PROPERTY_SUPPORTED(OP_ATOM_mimeTypes))
			return GET_FAILED;
		state = DOMSetPrivate(value, DOM_PRIVATE_mimeTypes);

		if (state == GET_FAILED)
		{
			JS_MimeTypes *mimeTypes;

			GET_FAILED_IF_ERROR(JS_MimeTypes::Make(mimeTypes, GetEnvironment()));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_mimeTypes, *mimeTypes));

			DOMSetObject(value, mimeTypes);
			return GET_SUCCESS;
		}
		else
			return state;

	case OP_ATOM_onLine:
		DOMSetBoolean(value, FALSE);
		if ( GetFramesDocument() && GetFramesDocument()->GetWindow()->GetOnlineMode() == Window::ONLINE
#ifdef DOM_WEBWORKERS_SUPPORT
			|| (GetEnvironment()->GetWorkerController()->GetWorkerDocManager() &&
				GetEnvironment()->GetWorkerController()->GetWorkerDocManager()->GetWindow()->GetOnlineMode() == Window::ONLINE)
#endif // DOM_WEBWORKERS_SUPPORT
			)
		{
			BOOL online = TRUE;
#ifdef PI_NETWORK_INTERFACE_MANAGER
			online = g_url_api->IsNetworkAvailable();
#elif defined _DEBUG
			if (!g_opera->dom_module.debug_has_warned_about_navigator_online)
			{
				OP_ASSERT(!"the platform can't answer this question truthfully");
				g_opera->dom_module.debug_has_warned_about_navigator_online = TRUE;
			}
#endif // PI_NETWORK_INTERFACE_MANAGER
			DOMSetBoolean(value, online);
		}
		return GET_SUCCESS;

#if defined(DOM_GEOLOCATION_SUPPORT) && defined(PI_GEOLOCATION)
	case OP_ATOM_geolocation:
#ifdef GADGET_SUPPORT
		if (FramesDocument *doc = GetFramesDocument())
		{
			Window *window = doc->GetWindow();
			if (window->GetGadget() && !window->GetGadget()->HasGeolocationAccess())
				return GET_FAILED;
		}
#endif // GADGET_SUPPORT

		if (!IS_NAV_PROPERTY_SUPPORTED(OP_ATOM_geolocation))
			return GET_FAILED;
		state = DOMSetPrivate(value, DOM_PRIVATE_geolocation);

		if (state == GET_FAILED)
		{
			DOM_Geolocation *geolocation;

			GET_FAILED_IF_ERROR(DOM_Geolocation::Make(geolocation, GetRuntime()));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_geolocation, *geolocation));

			DOMSetObject(value, geolocation);
			return GET_SUCCESS;
		}
		else
			return state;
#endif // DOM_GEOLOCATION_SUPPORT && PI_GEOLOCATION

	case OP_ATOM_doNotTrack:
		// doNotTrack DOM interface as described in http://www.w3.org/TR/2011/WD-tracking-dnt-20111114/#js-dom
		if (value)
		{
			BOOL is_global = static_cast<BOOL>(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DoNotTrack));
			BOOL is_site_specific = static_cast<BOOL>(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DoNotTrack, hostname));

			if (is_global || is_site_specific)
			{
				// According to the specification, if Do-Not_track is "on",
				// but overridden to off for specific site, send "0"
				if (!is_site_specific)
					DOMSetString(value, UNI_L("0"));
				else
					DOMSetString(value, UNI_L("1"));
			}
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

 /* virtual */ int
JS_Navigator::javaEnabled(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_NAVIGATOR);

	BOOL java_enabled = FALSE;
#ifdef _PLUGIN_SUPPORT_
	if (Viewer* viewer = g_viewers->FindViewerByMimeType(UNI_L("application/x-java-applet")))
		java_enabled = viewer->GetAction() == VIEWER_PLUGIN && viewer->GetDefaultPluginViewer();
#endif // _PLUGIN_SUPPORT_

	DOMSetBoolean(return_value, java_enabled);

	return ES_VALUE;
}

/* virtual */ int
JS_Navigator::taintEnabled(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOMSetBoolean(return_value, FALSE);
	return ES_VALUE;
}

#ifdef DOM_STREAM_API_SUPPORT

/* static */ OP_STATUS
JS_Navigator::CreateUserMediaErrorObject(ES_Object** new_object, int error_code, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(runtime->CreateNativeObjectObject(new_object));
	ES_Value error_value;
	DOMSetNumber(&error_value, error_code);
	RETURN_IF_ERROR(runtime->PutName(*new_object, UNI_L("code"), error_value, PROP_READ_ONLY));
	RETURN_IF_ERROR(PutNumericConstant(*new_object, UNI_L("PERMISSION_DENIED"), GETUSERMEDIA_PERMISSION_DENIED, runtime));
	return OpStatus::OK;
}

/* static */ OP_STATUS
JS_Navigator::CallUserMediaErrorCallback(ES_Object* error_callback, int error_code, DOM_Runtime* runtime)
{
	BOOL is_thumbnail = FALSE;
	if (runtime->GetFramesDocument())
		is_thumbnail = runtime->GetFramesDocument()->GetWindow()->IsThumbnailWindow();

	OP_STATUS status = OpStatus::OK;
	if (!is_thumbnail)
	{
		ES_Object* error_object;
		status = CreateUserMediaErrorObject(&error_object, error_code, runtime);
		if (OpStatus::IsSuccess(status))
		{
			ES_Value argument;
			ES_AsyncInterface* asyncif = runtime->GetEnvironment()->GetAsyncInterface();
			OP_ASSERT(asyncif);

			DOM_Object::DOMSetObject(&argument, error_object);
			status = asyncif->CallFunction(error_callback, NULL, 1, &argument, NULL, NULL);
		}
	}
	return status;
}

class GetUserMediaSecurityCallback
	: public OpSecurityCheckCallback
	, public CameraRemovedListener
{
public:
	GetUserMediaSecurityCallback(DOM_Runtime* runtime)
		: m_runtime(runtime)
		, m_security_check_cancel(NULL)
		, m_camera_recorder(NULL)
	{
	}

	~GetUserMediaSecurityCallback()
	{
		if (m_camera_recorder)
			m_camera_recorder->DetachCameraRemovedListener(this);
	}

	OP_STATUS Construct(ES_Object* success_callback, ES_Object* error_callback)
	{
		RETURN_IF_ERROR(m_success_callback_ref.Protect(m_runtime, success_callback));
		RETURN_IF_ERROR(m_error_callback_ref.Protect(m_runtime, error_callback));
		return OpStatus::OK;
	}

	OP_STATUS SetCancel(OpSecurityCheckCancel* cancel, CameraRecorder* camera_recorder)
	{
		m_security_check_cancel = cancel;
		RETURN_IF_ERROR(camera_recorder->AttachCameraRemovedListener(this));

		// This code assumes that the camera_recorder is a global object
		// that lives as long as Opera.
		m_camera_recorder = camera_recorder;
		return OpStatus::OK;
	}

	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type = PERSISTENCE_NONE)
	{
		ES_Value argument;
		ES_AsyncInterface* asyncif = m_runtime->GetEnvironment()->GetAsyncInterface();
		OP_ASSERT(asyncif);

		OP_STATUS status = OpStatus::OK;
		if (allowed)
		{
			DOM_LocalMediaStream* stream;
			status = DOM_LocalMediaStream::Make(stream, m_runtime);
			if (OpStatus::IsSuccess(status))
			{
				DOM_Object::DOMSetObject(&argument, stream);
				status = asyncif->CallFunction(m_success_callback_ref.GetObject(), NULL, 1, &argument, NULL, NULL);
			}
		}
		else
			status = CallErrorCallback();

		if (OpStatus::IsMemoryError(status)) // Ignore non-OOM errors
			g_memory_manager->RaiseCondition(status);
		OP_DELETE(this);
	}

	virtual void OnSecurityCheckError(OP_STATUS error)
	{
		// In case of OOM don't do anything, it would probably fail anyway.
		// There should not be any other errors. If there are, the getUserMedia
		// specification doesn't say how to report them.
		OP_ASSERT(OpStatus::IsMemoryError(error));
		if (OpStatus::IsMemoryError(error)) // Ignore non-OOM errors
			g_memory_manager->RaiseCondition(error);

		OP_DELETE(this);
	}

	virtual void OnCameraRemoved()
	{
		m_security_check_cancel->CancelSecurityCheck();
		OP_STATUS status = CallErrorCallback();
		if (OpStatus::IsMemoryError(status)) // Ignore non-OOM errors
			g_memory_manager->RaiseCondition(status);
		OP_DELETE(this);
	}

private:
	OP_STATUS CallErrorCallback()
	{
		if (m_error_callback_ref.GetObject())
			return JS_Navigator::CallUserMediaErrorCallback(m_error_callback_ref.GetObject(), JS_Navigator::GETUSERMEDIA_PERMISSION_DENIED, m_runtime);

		return OpStatus::OK;
	}

	DOM_Runtime* m_runtime;
	ES_ObjectReference m_success_callback_ref;
	ES_ObjectReference m_error_callback_ref;
	OpSecurityCheckCancel* m_security_check_cancel;
	CameraRecorder* m_camera_recorder;
};

/* static */ int
JS_Navigator::getUserMedia(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("-O|O");

	BOOL has_video = FALSE;

	switch (argv[0].type)
	{
	case VALUE_STRING:
		{
			const uni_char* options = argv[0].value.string;

			// Check that there is "video" on the list of options.
			// Any additional suboptions to the "video" option are ignored.
			size_t delimiter_index = 0;
			const uni_char* p = options;
			do {
				p += uni_strspn(p, UNI_L(" "));

				if (uni_strncmp("video", p, 5) == 0)
				{
					uni_char after_video = *(p + 5);
					if (after_video == '\0' || after_video == ' ' || after_video == ',')
						has_video = TRUE;
				}

				delimiter_index = uni_strcspn(p, UNI_L(","));
				p += delimiter_index + 1;
			} while (!has_video && *p != '\0');
		}
		break;
	case VALUE_OBJECT:
		has_video = this_object->DOMGetDictionaryBoolean(argv[0].value.object, UNI_L("video"), FALSE);
		break;
	}

	if (argv[1].type != VALUE_OBJECT || !ES_Runtime::IsCallable(argv[1].value.object))
		return ES_FAILED;

	ES_Object* successCallback = argv[1].value.object;
	ES_Object* errorCallback = (argc > 2) && argv[2].type == VALUE_OBJECT ? argv[2].value.object : NULL;

	// We only support video currently.
	if (!has_video)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	CameraRecorder* camera_recorder;
	OP_STATUS camera_recorder_status = g_media_module.GetCameraRecorder(camera_recorder);
	if (OpStatus::IsMemoryError(camera_recorder_status))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(camera_recorder_status) || !camera_recorder->HasCamera())
	{
		// The specification says that the error callback should be called,
		// among other things, "due to platform limitations" (as I understand it: no camera).
		// Point 3.1.1, step 9 (also see step 7):
		// http://dev.w3.org/2011/webrtc/editor/getusermedia.html#local-content
		if (errorCallback)
			CALL_FAILED_IF_ERROR(CallUserMediaErrorCallback(errorCallback, JS_Navigator::GETUSERMEDIA_PERMISSION_DENIED, origining_runtime));

		return ES_FAILED;
	}

	if (this_object->GetFramesDocument())
	{
		GetUserMediaSecurityCallback* callback = OP_NEW(GetUserMediaSecurityCallback, (origining_runtime));
		if (!callback)
			return ES_NO_MEMORY;
		OpAutoPtr<GetUserMediaSecurityCallback> callback_anchor(callback);
		CALL_FAILED_IF_ERROR(callback->Construct(successCallback, errorCallback));

		OpSecurityContext source(origining_runtime);
		OpSecurityCheckCancel* security_check_cancel;
		CALL_FAILED_IF_ERROR(g_secman_instance->CheckSecurity(OpSecurityManager::GET_USER_MEDIA, source, OpSecurityContext(), callback, &security_check_cancel));
		callback_anchor.release();
		if (security_check_cancel)
		{
			OP_STATUS status = callback->SetCancel(security_check_cancel, camera_recorder);
			if (OpStatus::IsError(status))
			{
				security_check_cancel->CancelSecurityCheck();
				CALL_FAILED_IF_ERROR(status);
			}
		}
	}

	return ES_FAILED;
}
#endif // DOM_STREAM_API_SUPPORT
#ifdef WEB_HANDLERS_SUPPORT
#ifdef WEBFEEDS_EXTERNAL_READERS
#define IS_WEB_FEED_HANDLER(type) (uni_stri_eq(type, "application/rss+xml") || uni_stri_eq(type, "application/atom+xml"))
#endif // WEBFEEDS_EXTERNAL_READERS


static OP_STATUS GetTrustedApplication(int &index, const OpStringC &protocol, OpString &web_handler, OpString &description, BOOL &in_terminal, BOOL &user_defined)
{
	TrustedProtocolData data;
	index = g_pcdoc->GetTrustedProtocolInfo(protocol, data);
	if (index != -1)
	{
		RETURN_IF_ERROR(web_handler.Set(data.web_handler));
		RETURN_IF_ERROR(description.Set(data.description));
		in_terminal = data.in_terminal;
		user_defined = data.user_defined;
	}
	return OpStatus::OK;
}

static OP_STATUS SetTrustedProtocolInfo(int index,
	const OpStringC &protocol, const OpStringC &web_handler,
	const OpStringC &description, ViewerMode handler_mode, BOOL in_terminal, BOOL user_defined = FALSE)
{
	TrustedProtocolData data;
	data.flags = TrustedProtocolData::TP_Protocol | TrustedProtocolData::TP_WebHandler | TrustedProtocolData::TP_Description |
		TrustedProtocolData::TP_ViewerMode | TrustedProtocolData::TP_InTerminal | TrustedProtocolData::TP_UserDefined;
	data.protocol     = protocol;
	data.web_handler  = web_handler;
	data.description  = description;
	data.viewer_mode  = handler_mode;
	data.in_terminal  = in_terminal;
	data.user_defined = user_defined;

	BOOL set = FALSE;
	RETURN_IF_LEAVE(set = g_pcdoc->SetTrustedProtocolInfoL(index, data));
	if (!set)
		return OpStatus::ERR;

	return OpStatus::OK;
}

class RegisterHandlerPermissionCallback : public OpPermissionListener::WebHandlerPermission, public ListElement<RegisterHandlerPermissionCallback>
{
public:
	RegisterHandlerPermissionCallback(BOOL protocol_handler)
	: is_protocol_handler(protocol_handler)
	, docman(NULL)
	, cancel(TRUE)
	{}

	~RegisterHandlerPermissionCallback()
	{
		if (cancel)
			docman->GetWindow()->GetWindowCommander()->GetPermissionListener()->OnAskForPermissionCancelled(docman->GetWindow()->GetWindowCommander(), this);
	}

	OP_STATUS Construct(DocumentManager* docman, URL& handler_url, URL& origin_url, const uni_char* type, const uni_char* title);

	void OnAllowed(PersistenceType persistence = PERSISTENCE_TYPE_NONE);
	void OnDisallowed(PersistenceType persistence = PERSISTENCE_TYPE_NONE);
	virtual PermissionType Type() { return OpPermissionListener::PermissionCallback::PERMISSION_TYPE_WEB_HANDLER_REGISTRATION; }
	virtual const uni_char* GetURL() { return url.CStr(); }
	virtual int AffirmativeChoices() { return PERSISTENCE_TYPE_ALWAYS; }
	virtual int NegativeChoices() { return PERSISTENCE_TYPE_NONE | PERSISTENCE_TYPE_ALWAYS; }
	const uni_char* GetHandlerURL_NOT_FOR_UI() { return handler_url_NOT_FOR_UI.CStr(); }
	virtual BOOL IsProtocolHandler() { return is_protocol_handler; }
	virtual const uni_char* GetName() { return type.CStr(); }
	virtual const uni_char* GetHandlerURL() { return handler_url.CStr(); }
	virtual const uni_char* GetDescription() { return title.CStr(); }

private:
	URL origin_url;
	BOOL is_protocol_handler;
	OpString url;
	OpString handler_url;
	OpString handler_url_NOT_FOR_UI;
	OpString url_string;
	OpString type;
	OpString title;
	DocumentManager* docman;
	BOOL cancel;
};

OP_STATUS
RegisterHandlerPermissionCallback::Construct(DocumentManager* docman, URL& handler_url, URL& origin_url, const uni_char* type, const uni_char* title)
{
	OP_ASSERT(docman);
	this->docman = docman;
	RETURN_IF_ERROR(origin_url.GetAttribute(URL::KUniHostName, this->url));
	if (this->url.IsEmpty())
	{
		RETURN_IF_ERROR(origin_url.GetAttribute(URL::KProtocolName, this->url));
		if (origin_url.Type() == URL_OPERA && origin_url.GetAttribute(URL::KIsGeneratedByOpera))
			RETURN_IF_ERROR(this->url.AppendFormat(UNI_L(":%s"), origin_url.GetAttribute(URL::KUniPath).CStr()));
		else
			RETURN_IF_ERROR(this->url.Append(UNI_L(":...")));
	}
	RETURN_IF_ERROR(handler_url.GetAttribute(URL::KUniName_With_Fragment_Escaped, this->handler_url));
	RETURN_IF_ERROR(handler_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI, this->handler_url_NOT_FOR_UI));
	RETURN_IF_ERROR(this->type.Set(type));
	RETURN_IF_ERROR(this->title.Set(title));
	this->origin_url = origin_url;
	return OpStatus::OK;
}

void
RegisterHandlerPermissionCallback::OnDisallowed(PersistenceType persistence)
{
	OP_ASSERT(persistence == PERSISTENCE_TYPE_ALWAYS || persistence == PERSISTENCE_TYPE_NONE || !"No other persistence type is supported. Pay attention to what NegativeChoices() returns!");
	OpAutoPtr<RegisterHandlerPermissionCallback> ap_this(this);
	Out();
	cancel = FALSE;
	if (persistence == PERSISTENCE_TYPE_ALWAYS)
	{
		OpString disallowed;
		if (OpStatus::IsMemoryError(disallowed.Set(g_pcjs->GetStringPref(PrefsCollectionJS::DisallowedWebHandlers, origin_url))))
			docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		OP_STATUS status;
		if (disallowed.IsEmpty())
			status = disallowed.Set(type);
		else
			status = disallowed.AppendFormat(UNI_L(",%s"), type.CStr());

		if (OpStatus::IsMemoryError(status))
			docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);

		TRAPD(exception, g_pcjs->OverridePrefL(origin_url.GetAttribute(URL::KUniHostName).CStr(), PrefsCollectionJS::DisallowedWebHandlers, disallowed, TRUE));
		if (OpStatus::IsMemoryError(exception))
			docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	}
}

/* Checks if we should proceed with a handler registration.
 * Returns:
 * - OpBoolean::IS_TRUE if we should proceed with the registration.
 * - OpBoolean::IS_FALSE if we mustn't proceed with the registration and the SECURITY exception should be thrown.
 * - OpStatus::ERR if we mustn't proceed with the registration but no exception must be thrown.
 */
static OP_BOOLEAN ProceedWithWebHandlerRegistration(BOOL protocol_handler, const uni_char* type, const uni_char* url)
{
	OpString protocol_or_mime;
	OpString filename;
	OpString description;
	BOOL in_terminal;
	BOOL user_defined;
	OpString protocol2;
	int index = -1;

	RETURN_IF_ERROR(protocol_or_mime.Set(type));
	RETURN_IF_ERROR(protocol2.Set(url, uni_strchr(url, ':') - url));
	RETURN_IF_ERROR(GetTrustedApplication(index, protocol2.CStr(), filename, description, in_terminal, user_defined));

	if (protocol_handler)
	{
		// Do not allow to register a handler for protocol the handler itself or any other handler uses.
		if (protocol_or_mime.CompareI(protocol2) == 0)
			return OpBoolean::IS_FALSE;

		// Check if not already registered or a user hasn't explicitly set up a handler for protocol in question.
		RETURN_IF_ERROR(GetTrustedApplication(index, protocol_or_mime, filename, description, in_terminal, user_defined));
		if (index >= 0 && (user_defined || filename.Compare(url) == 0))
			return OpStatus::ERR;
	}
	else
	{
		if (index >= 0) // We have a handler for protocol this handler would like to use -> possible loop.
		{
			TrustedProtocolData data;
			if (g_pcdoc->GetTrustedProtocolInfo(index, data) && data.viewer_mode == UseWebService)
				return OpBoolean::IS_TRUE;
		}

		// Check if not already registered or a user hasn't explicitly set up a handler for mime type in question.
		Viewer* viewer = g_viewers->FindViewerByMimeType(protocol_or_mime);
		if (viewer)
		{
			if (!viewer->GetWebHandlerAllowed())
				return OpBoolean::IS_FALSE;
			else if (viewer->IsUserDefined() || (viewer->GetWebHandlerToOpenWith() && uni_str_eq(viewer->GetWebHandlerToOpenWith(), url)))
				return OpStatus::ERR;
		}
	}

	return OpBoolean::IS_TRUE;
}

void
RegisterHandlerPermissionCallback::OnAllowed(PersistenceType persistence)
{
	OP_ASSERT(persistence == PERSISTENCE_TYPE_ALWAYS || !"No other persistence type is supported. Pay attention to what AffirmativeChoices() returns!");
	OpAutoPtr<RegisterHandlerPermissionCallback> ap_this(this);
	Out();
	cancel = FALSE;

	OP_BOOLEAN go_on = ProceedWithWebHandlerRegistration(is_protocol_handler, type.CStr(), handler_url_NOT_FOR_UI.CStr());
	if (OpStatus::IsError(go_on) || go_on == OpBoolean::IS_FALSE)
	{
		if (OpStatus::IsMemoryError(go_on))
			docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}

	if (!is_protocol_handler) // a content handler
	{
#ifdef WEBFEEDS_EXTERNAL_READERS
		if (IS_WEB_FEED_HANDLER(type.CStr()))
		{
			unsigned id = g_webfeeds_api->GetExternalReaderFreeId();
			OpString feed_title;
			if (OpStatus::IsMemoryError(feed_title.AppendFormat(UNI_L("%s: %s"), origin_url.GetAttribute(URL::KUniHostName).CStr(), title.CStr())) ||
				OpStatus::IsMemoryError(g_webfeeds_api->AddExternalReader(id, handler_url_NOT_FOR_UI.CStr(), feed_title.CStr())))
				docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}
#endif // WEBFEEDS_EXTERNAL_READERS

		Viewer* viewer = g_viewers->FindViewerByMimeType(type);
		if (!viewer) // no viewer registered yet for this mime type
		{
			viewer = OP_NEW(Viewer, ());
			if (!viewer)
			{
				docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return;
			}
			
			OpAutoPtr<Viewer> ap_viewer(viewer);
			if (OpStatus::IsMemoryError(viewer->SetContentType(type)) ||
			    OpStatus::IsMemoryError(g_viewers->AddViewer(viewer)))
			{
				docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return;
			}

			ap_viewer.release();
		}

		viewer->SetAction(VIEWER_WEB_APPLICATION);
		if (OpStatus::IsMemoryError(viewer->SetWebHandlerToOpenWith(handler_url_NOT_FOR_UI)) ||
		    OpStatus::IsMemoryError(viewer->SetDescription(title)))
		{
			docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}

#if defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE
		TRAPD(exception, g_viewers->WriteViewersL());
		if (OpStatus::IsMemoryError(exception))
			docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
#endif // defined PREFS_HAS_PREFSFILE && defined PREFS_WRITE
	}
	else // a protocol handler
	{
		WindowCommander* wincmd = docman->GetWindow()->GetWindowCommander();
		if (!uni_stri_eq(type, "mailto") || !wincmd->GetDocumentListener()->OnMailToWebHandlerRegister(wincmd, handler_url_NOT_FOR_UI.CStr(), title.CStr()))
		{
			OpString desc;
			OpString filename;
			BOOL in_terminal;
			BOOL user_defined;
			int index = -1;
			if (OpStatus::IsMemoryError(GetTrustedApplication(index, type, filename, desc, in_terminal, user_defined)))
			{
				docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return;
			}

			if (index == -1)
				index = g_pcdoc->GetNumberOfTrustedProtocols();

			OP_ASSERT(index >= 0);
			if (OpStatus::IsMemoryError(SetTrustedProtocolInfo(index, type, handler_url_NOT_FOR_UI, title, UseWebService, FALSE)))
			{
				docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				return;
			}
#if defined PREFS_HAS_PREFSFILE && defined PREFSFILE_WRITE
			TRAPD(exception, g_pcdoc->WriteTrustedProtocolsL(g_pcdoc->GetNumberOfTrustedProtocols()));
			if (OpStatus::IsMemoryError(exception))
				docman->RaiseCondition(OpStatus::ERR_NO_MEMORY);
#endif // defined PREFS_HAS_PREFSFILE && defined PREFSFILE_WRITE
		}
	}
}

static int PerfromBasicSecurityChecks(FramesDocument* frames_doc, DOM_Runtime* origining_runtime, const uni_char* url, URL& resolved_url, DOM_Object* this_object, ES_Value* return_value)
{
	resolved_url = GetEncodedURL(origining_runtime->GetFramesDocument(), frames_doc, url);
	if (!resolved_url.IsValid() || DOM_Utils::IsOperaIllegalURL(resolved_url))
		return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

	BOOL allowed = FALSE;
	URL origin_url = origining_runtime->GetOriginURL();
	OpSecurityContext source(origin_url);
	OpSecurityContext target(resolved_url);
	OpStatus::Ignore(g_secman_instance->CheckSecurity(OpSecurityManager::DOM_STANDARD, source, target, allowed));
	if (!allowed)
		return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

#ifdef GADGET_SUPPORT
	// We don't want to allow this for now as widget://uid is not allowed to be opened anyway
	// and in order to do this properly this requires proper design and spec. This will be handled at some later point.
	if (resolved_url.Type() == URL_WIDGET)
		return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);
#endif // GADGET_SUPPORT

	return ES_FAILED;
}

class MailtoHandlerRegistrationChecker : public DOM_RestartObject, public OpDocumentListener::MailtoWebHandlerCheckCallback
{
	OpString m_url;
public:
	BOOL m_completed;
	BOOL m_registered;
	RegisterHandlerPermissionCallback* m_callback;
	MailtoHandlerRegistrationChecker() : m_completed(FALSE), m_registered(FALSE), m_callback(NULL) {}
	static OP_STATUS Make(MailtoHandlerRegistrationChecker*& checker, DOM_Runtime* runtime, const uni_char* url)
	{
		RETURN_IF_ERROR(DOMSetObjectRuntime(checker = OP_NEW(MailtoHandlerRegistrationChecker, ()), runtime));
		RETURN_IF_ERROR(checker->m_url.Set(url));
		return checker->KeepAlive();
	}

	const uni_char* GetURL() { return m_url.CStr(); }
	void OnCompleted(BOOL registered)
	{
		m_registered = registered;
		m_completed = TRUE;
		DOM_RestartObject::Resume();
	}
};

/* virtual */ int
JS_Navigator::registerHandler(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(navigator, DOM_TYPE_NAVIGATOR, JS_Navigator);

	MailtoHandlerRegistrationChecker* checker = NULL;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("sss");
		const uni_char* type = argv[0].value.string;
		const uni_char* url = argv[1].value.string;
		const uni_char* title = argv[2].value.string;

		enum { MAX_TITLE_LENGTH = 256 };
		uni_char tailored_title[MAX_TITLE_LENGTH]; /* ARRAY OK 2011-04-27 pstanek */

		if (!uni_strstr(url, UNI_L("%s")))
			return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);

		int title_len = static_cast<int>(uni_strlen(title));
		int tailored_title_write_pos = 0;
		int iter;
		for (iter = 0; iter < title_len && iter < MAX_TITLE_LENGTH - 1; ++iter)
		{
			if (!uni_iscntrl(title[iter]))
			{
				tailored_title[tailored_title_write_pos] = title[iter];
				++tailored_title_write_pos;
			}
		}
		tailored_title[iter] = 0;

		if (FramesDocument* frames_doc = navigator->GetFramesDocument())
		{
			URL resolved_url;
			int sec_status = PerfromBasicSecurityChecks(frames_doc, origining_runtime, url, resolved_url, this_object, return_value);
			if (sec_status != ES_FAILED)
				return sec_status;

			URL origin_url = origining_runtime->GetOriginURL();

			OpString disallowed;
			CALL_FAILED_IF_ERROR(disallowed.Set(g_pcjs->GetStringPref(PrefsCollectionJS::DisallowedWebHandlers, origin_url)));
			if (disallowed.FindI(type) != KNotFound)
				return ES_FAILED;

			const uni_char* url_str = resolved_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI).CStr();
			OP_BOOLEAN go_on = ProceedWithWebHandlerRegistration(data == 1, type, url_str);
			CALL_FAILED_IF_ERROR(go_on);

			if (go_on == OpBoolean::IS_FALSE)
				return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

			if (data == 1)
			{
				BOOL allowed;
				OpSecurityContext source(origin_url);
				OpSecurityContext target(resolved_url);
				target.AddText(type);
				OpStatus::Ignore(g_secman_instance->CheckSecurity(OpSecurityManager::WEB_HANDLER_REGISTRATION, source, target, allowed));
				if (!allowed)
					return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

				// RFC 3986 allows also "." in protocol name but this leads to evil things...
				size_t length = uni_strspn(type, UNI_L("+-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));
				if (!length || uni_strlen(type) != length)
					return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);
			}
			else
			{
				// List of charset alowed in the mime type string was taken from RFC 2045
				size_t length = uni_strspn(type, UNI_L("!#$%&'*+-./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~"));
				if (!length || uni_strlen(type) != length || uni_isdigit(type[0]))
					return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

#ifdef WEBFEEDS_EXTERNAL_READERS
				if (IS_WEB_FEED_HANDLER(type) && g_webfeeds_api->HasExternalReader(url_str))
					return ES_FAILED;
#endif // WEBFEEDS_EXTERNAL_READERS
			}

			RegisterHandlerPermissionCallback* callback = OP_NEW(RegisterHandlerPermissionCallback, (data == 1));
			if (!callback)
				return ES_NO_MEMORY;
			callback->Into(&navigator->register_handler_permission_callbacks);
			DocumentManager* docman = frames_doc->GetDocManager();
			WindowCommander* wincmd = docman->GetWindow()->GetWindowCommander();
			CALL_FAILED_IF_ERROR(callback->Construct(docman, resolved_url, origin_url, type, tailored_title));
			if (data == 1 && uni_stri_eq(type, "mailto"))
			{
				CALL_FAILED_IF_ERROR(MailtoHandlerRegistrationChecker::Make(checker, origining_runtime, url_str));
				checker->m_callback = callback;
				wincmd->GetDocumentListener()->IsMailToWebHandlerRegistered(wincmd, checker);
			}

			if (!checker || checker->m_completed)
				wincmd->GetPermissionListener()->OnAskForPermission(wincmd, callback);
			else
				return checker->BlockCall(return_value, origining_runtime);
		}
	}
	else
	{
		checker = DOM_VALUE2OBJECT(*return_value, MailtoHandlerRegistrationChecker);
		if (FramesDocument* frames_doc = navigator->GetFramesDocument())
		{
			DocumentManager* docman = frames_doc->GetDocManager();
			WindowCommander* wincmd = docman->GetWindow()->GetWindowCommander();
			OP_ASSERT(checker->m_callback);
			wincmd->GetPermissionListener()->OnAskForPermission(wincmd, checker->m_callback);
		}
	}

	return ES_FAILED;
}

/* virtual */ int
JS_Navigator::unregisterHandler(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(navigator, DOM_TYPE_NAVIGATOR, JS_Navigator);

	DOM_CHECK_ARGUMENTS("ss");
	const uni_char* type = argv[0].value.string;
	const uni_char* url = argv[1].value.string;

	if (!uni_strstr(url, UNI_L("%s")))
		return ES_FAILED;

	if (FramesDocument* frames_doc = navigator->GetFramesDocument())
	{
		URL resolved_url;
		int sec_status = PerfromBasicSecurityChecks(frames_doc, origining_runtime, url, resolved_url, this_object, return_value);
		if (sec_status != ES_FAILED)
			return sec_status;

		const OpStringC url_str = resolved_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI);
		if (data == 1)
		{
			TrustedProtocolData data;
			int idx = g_pcdoc->GetTrustedProtocolInfo(type, data);
			if (idx != -1 && data.viewer_mode == UseWebService && uni_str_eq(url_str.CStr(), data.web_handler.CStr()))
			{
				g_pcdoc->RemoveTrustedProtocolInfo(idx);
				TRAPD(exception, g_pcdoc->WriteTrustedProtocolsL(g_pcdoc->GetNumberOfTrustedProtocols()));
				CALL_FAILED_IF_ERROR(exception);
			}
			else if (uni_stri_eq(type, "mailto"))
			{
				WindowCommander* wincmd = frames_doc->GetDocManager()->GetWindow()->GetWindowCommander();
				wincmd->GetDocumentListener()->OnMailToWebHandlerUnregister(wincmd, url_str.CStr());
			}
		}
		else
		{
#ifdef WEBFEEDS_EXTERNAL_READERS
			if (IS_WEB_FEED_HANDLER(type))
			{
				unsigned reader_id;
				if (g_webfeeds_api->HasExternalReader(url_str.CStr(), &reader_id))
					g_webfeeds_api->DeleteExternalReader(reader_id);
			}
			else
#endif // WEBFEEDS_EXTERNAL_READERS
			{
				Viewer* viewer;
				CALL_FAILED_IF_ERROR(g_viewers->FindViewerByWebApplication(url_str, viewer));
				if (viewer && uni_stri_eq(type, viewer->GetContentTypeString()))
				{
					viewer->ResetAction();
					viewer->SetWebHandlerToOpenWith(UNI_L(""));
					viewer->SetDescription(UNI_L(""));
					ViewActionFlag vaflags = viewer->GetFlags();
					short flags = vaflags & ~ViewActionFlag::USERDEFINED_VALUE;
					viewer->SetFlags(flags);
					TRAPD(exception, g_viewers->WriteViewersL());
					CALL_FAILED_IF_ERROR(exception);
				}
			}
		}
	}

	return ES_FAILED;
}

/* virtual */ int
JS_Navigator::isHandlerRegistered(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(navigator, DOM_TYPE_NAVIGATOR, JS_Navigator);

	MailtoHandlerRegistrationChecker* checker;
	BOOL registered = FALSE;

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("ss");
		const uni_char* type = argv[0].value.string;
		const uni_char* url = argv[1].value.string;

		if (!uni_strstr(url, UNI_L("%s")))
			return ES_FAILED;

		if (FramesDocument* frames_doc = navigator->GetFramesDocument())
		{
			URL resolved_url;
			int sec_status = PerfromBasicSecurityChecks(frames_doc, origining_runtime, url, resolved_url, this_object, return_value);
			if (sec_status != ES_FAILED)
				return sec_status;

			const OpStringC url_str = resolved_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Escaped_NOT_FOR_UI);

			RegisterHandlerPermissionCallback* callback = navigator->register_handler_permission_callbacks.First();
			while (callback)
			{
				if (uni_stri_eq(callback->GetName(), type) && uni_str_eq(url_str, callback->GetHandlerURL_NOT_FOR_UI()))
				{
					DOMSetString(return_value, UNI_L("declined"));
					return ES_VALUE;
				}
				callback = callback->Suc();
			}

			OpString disallowed;
			URL origin_url = origining_runtime->GetOriginURL();
			CALL_FAILED_IF_ERROR(disallowed.Set(g_pcjs->GetStringPref(PrefsCollectionJS::DisallowedWebHandlers, origin_url)));
			if (disallowed.FindI(type) != KNotFound)
				registered = TRUE;
			else
			{
				if (data == 1)
				{
					TrustedProtocolData data;
					int idx = g_pcdoc->GetTrustedProtocolIndex(type);
					if (idx != -1)
					{
						registered = TRUE;
						if (uni_stri_eq(type, "mailto"))
						{
							CALL_FAILED_IF_ERROR(MailtoHandlerRegistrationChecker::Make(checker, origining_runtime, url_str.CStr()));
							WindowCommander* wincmd = frames_doc->GetDocManager()->GetWindow()->GetWindowCommander();
							wincmd->GetDocumentListener()->IsMailToWebHandlerRegistered(wincmd, checker);
							if (!checker->m_completed)
								return checker->BlockCall(return_value, origining_runtime);
						}
					}
				}
				else
				{
	#ifdef WEBFEEDS_EXTERNAL_READERS
					if (IS_WEB_FEED_HANDLER(type))
						registered = g_webfeeds_api->HasExternalReader(url_str.CStr());
					else
	#endif // WEBFEEDS_EXTERNAL_READERS
					{
						Viewer* viewer;
						CALL_FAILED_IF_ERROR(g_viewers->FindViewerByWebApplication(url_str, viewer));
						registered = (viewer && uni_stri_eq(type, viewer->GetContentTypeString()));
					}
				}
			}
		}
	}
	else
	{
		checker = DOM_VALUE2OBJECT(*return_value, MailtoHandlerRegistrationChecker);
		registered = checker->m_registered;
	}

	DOMSetString(return_value, registered ? UNI_L("registered") : UNI_L("new"));
	return ES_VALUE;
}

/* static */
void
JS_Navigator::BeforeUnload(DOM_EnvironmentImpl* env)
{
	JS_Window *window = static_cast<JS_Window*>(env->GetWindow());

	ES_Value value;
	if (window->GetPrivate(DOM_PRIVATE_navigator, &value) == OpBoolean::IS_TRUE)
		static_cast<JS_Navigator*>(DOM_GetHostObject(value.value.object))->register_handler_permission_callbacks.Clear();
}
#endif // WEB_HANDLERS_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT

/*virtual*/ ES_PutState
JS_Navigator_Worker::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_appName:
	case OP_ATOM_appVersion:
	case OP_ATOM_platform:
	case OP_ATOM_userAgent:
	case OP_ATOM_onLine:
		return PUT_SUCCESS; /* Ho-hum, report success to avoid having it be stored on the native object. */
	case OP_ATOM_doNotTrack:
		return PUT_READ_ONLY;
	default:
		return JS_Navigator::PutName(property_name, value, origining_runtime);
	}
}
#endif // DOM_WEBWORKERS_SUPPORT

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(JS_Navigator)
	DOM_FUNCTIONS_FUNCTION(JS_Navigator, JS_Navigator::javaEnabled, "javaEnabled", 0)
	DOM_FUNCTIONS_FUNCTION(JS_Navigator, JS_Navigator::taintEnabled, "taintEnabled", 0)
DOM_FUNCTIONS_END(JS_Navigator)

DOM_FUNCTIONS_WITH_DATA_START(JS_Navigator)
#if defined WEB_HANDLERS_SUPPORT
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Navigator, JS_Navigator::registerHandler, 0, "registerContentHandler", "sss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Navigator, JS_Navigator::registerHandler, 1, "registerProtocolHandler", "sss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Navigator, JS_Navigator::unregisterHandler, 0, "unregisterContentHandler", "ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Navigator, JS_Navigator::unregisterHandler, 1, "unregisterProtocolHandler", "ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Navigator, JS_Navigator::isHandlerRegistered, 0, "isContentHandlerRegistered", "ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Navigator, JS_Navigator::isHandlerRegistered, 1, "isProtocolHandlerRegistered", "ss-")
#endif // defined WEB_HANDLERS_SUPPORT
DOM_FUNCTIONS_WITH_DATA_END(JS_Navigator)

