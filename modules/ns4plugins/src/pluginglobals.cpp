/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/pi/OpSystemInfo.h"
#include "modules/auth/auth_basic.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/uri_escape.h"
#include "modules/forms/form.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/memtools/stacktraceguard.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/ns4plugins/src/pluginglobals.h"
#include "modules/ns4plugins/src/pluginhandler.h"
#include "modules/ns4plugins/src/pluginmemoryhandler.h"
#include "modules/ns4plugins/src/pluginscript.h"
#include "modules/ns4plugins/src/pluginstream.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/url/uamanager/ua.h"
#include "modules/upload/upload.h"
#include "modules/url/tools/url_util.h"
#include "modules/url/url_man.h"
#include "modules/viewers/viewers.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef WINGOGI
#include "platforms/wingogi/pi_impl/wingogipluginwindow.h"
#endif // WINGOGI

#ifdef _MACINTOSH_
#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#endif

#ifdef WINGOGI
#include "platforms/wingogi/pi_impl/wingogipluginutils.h"
#endif // WINGOGI

#define FUNC_ENTRY(func_name) NPN_##func_name
#define VERSION_ENTRY (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR
#ifdef HAS_COMPLEX_GLOBALS
# define FUNC_MAP_START() 	const NPNetscapeFuncs g_operafuncs = {
# define FUNC_SIZE_ENTRY()	sizeof(NPNetscapeFuncs)
# define FUNC_VERSION_ENTRY()	VERSION_ENTRY
# define FUNC_MAP_ENTRY(field, func_name) FUNC_ENTRY(func_name)
# define FUNC_MAP_ENTRY_NULL(field) NULL
# define FUNC_MAP_END()		};
#else // HAS_COMPLEX_GLOBALS
# define FUNC_MAP_START()	void init_operafuncs() {\
                            NPNetscapeFuncs *tmp_map = g_opera->ns4plugins_module.m_operafuncs;
# define FUNC_SIZE_ENTRY() tmp_map->size = sizeof(NPNetscapeFuncs)
# define FUNC_VERSION_ENTRY()  tmp_map->version = VERSION_ENTRY
# define FUNC_MAP_ENTRY(field, func_name) tmp_map->field = FUNC_ENTRY(func_name)
# define FUNC_MAP_ENTRY_NULL(field) tmp_map->field = NULL
# define FUNC_MAP_END()		;}
#endif // HAS_COMPLEX_GLOBALS



FUNC_MAP_START()
	FUNC_SIZE_ENTRY(),
	FUNC_VERSION_ENTRY(),
	FUNC_MAP_ENTRY(geturl,					GetURL),
	FUNC_MAP_ENTRY(posturl,					PostURL),
	FUNC_MAP_ENTRY(requestread,				RequestRead),
	FUNC_MAP_ENTRY(newstream,				NewStream),
	FUNC_MAP_ENTRY(write,					Write),
	FUNC_MAP_ENTRY(destroystream,			DestroyStream),
	FUNC_MAP_ENTRY(status,					Status),
	FUNC_MAP_ENTRY(uagent,					UserAgent),
	FUNC_MAP_ENTRY(memalloc,				MemAlloc),
	FUNC_MAP_ENTRY(memfree,					MemFree),
	FUNC_MAP_ENTRY(memflush,				MemFlush),
	FUNC_MAP_ENTRY(reloadplugins,			ReloadPlugins),
	FUNC_MAP_ENTRY(getJavaEnv,				GetJavaEnv),
	FUNC_MAP_ENTRY(getJavaPeer,				GetJavaPeer),
	FUNC_MAP_ENTRY(geturlnotify,			GetURLNotify),
	FUNC_MAP_ENTRY(posturlnotify,			PostURLNotify),
	FUNC_MAP_ENTRY(getvalue,				GetValue),		// Some new functions introduced in NS4.0
	FUNC_MAP_ENTRY(setvalue,				SetValue),
	FUNC_MAP_ENTRY(invalidaterect,			InvalidateRect),
	FUNC_MAP_ENTRY(invalidateregion,		InvalidateRegion),
	FUNC_MAP_ENTRY(forceredraw,				ForceRedraw),
	FUNC_MAP_ENTRY(getstringidentifier,		GetStringIdentifier),
	FUNC_MAP_ENTRY(getstringidentifiers,	GetStringIdentifiers),
	FUNC_MAP_ENTRY(getintidentifier,		GetIntIdentifier),
	FUNC_MAP_ENTRY(identifierisstring,		IdentifierIsString),
	FUNC_MAP_ENTRY(utf8fromidentifier,		UTF8FromIdentifier),
	FUNC_MAP_ENTRY(intfromidentifier,		IntFromIdentifier),
	FUNC_MAP_ENTRY(createobject,			CreateObject),
	FUNC_MAP_ENTRY(retainobject,			RetainObject),
	FUNC_MAP_ENTRY(releaseobject,			ReleaseObject),
	FUNC_MAP_ENTRY(invoke,					Invoke),
	FUNC_MAP_ENTRY(invokeDefault,			InvokeDefault),
	FUNC_MAP_ENTRY(evaluate,				Evaluate),
	FUNC_MAP_ENTRY(getproperty,				GetProperty),
	FUNC_MAP_ENTRY(setproperty,				SetProperty),
	FUNC_MAP_ENTRY(removeproperty,			RemoveProperty),
	FUNC_MAP_ENTRY(hasproperty,				HasProperty),
	FUNC_MAP_ENTRY(hasmethod,				HasMethod),
	FUNC_MAP_ENTRY(releasevariantvalue,		ReleaseVariantValue),
	FUNC_MAP_ENTRY(setexception,			SetException),
	FUNC_MAP_ENTRY(pushpopupsenabledstate,	PushPopupsEnabledState),
	FUNC_MAP_ENTRY(poppopupsenabledstate,	PopPopupsEnabledState),
	FUNC_MAP_ENTRY(enumerate,				Enumerate),
	FUNC_MAP_ENTRY(pluginthreadasynccall,	PluginThreadAsyncCall),
	FUNC_MAP_ENTRY(construct,				Construct),
	FUNC_MAP_ENTRY(getvalueforurl,			GetValueForURL),
	FUNC_MAP_ENTRY(setvalueforurl,			SetValueForURL),
	FUNC_MAP_ENTRY(getauthenticationinfo,	GetAuthenticationInfo),
	FUNC_MAP_ENTRY(scheduletimer,			ScheduleTimer),
	FUNC_MAP_ENTRY(unscheduletimer,			UnscheduleTimer),
	FUNC_MAP_ENTRY(popupcontextmenu,		PopUpContextMenu),
	FUNC_MAP_ENTRY(convertpoint,			ConvertPoint),
	FUNC_MAP_ENTRY(handleevent,				HandleEvent),
	FUNC_MAP_ENTRY(unfocusinstance,			UnfocusInstance),

	// Opera currently doesn't have this function implemented. For discussion on
	// the matter, please see: http://critic.oslo.osa/showcomment?chain=3639
	FUNC_MAP_ENTRY_NULL(urlredirectresponse)

FUNC_MAP_END()


#ifdef HAS_SET_HTTP_DATA
OP_STATUS PreparePostURL(URL& url, const char* buf, const uint32 len, const NPBool file)
{
	OP_STATUS stat = OpStatus::OK;
	url.SetHTTP_Method(HTTP_METHOD_POST);
#if defined(NS4P_DISABLE_HTTP_COMPRESS) && defined(URL_ALLOW_DISABLE_COMPRESS)
	stat = url.SetAttribute(URL::KDisableCompress,TRUE);
#endif // NS4P_DISABLE_HTTP_COMPRESS && URL_ALLOW_DISABLE_COMPRESS

	if (OpStatus::IsSuccess(stat) && file)
	{
		OpAutoPtr<Upload_TemporaryURL> upload_url(OP_NEW(Upload_TemporaryURL, ()));
		RETURN_OOM_IF_NULL(upload_url.get());
		RETURN_IF_LEAVE(upload_url->InitL(make_doublebyte_in_tempbuffer(buf, len), NULL, TRUE));
		// disable content-disposition
		RETURN_IF_LEAVE(upload_url->AccessHeaders().HeaderSetEnabledL("Content-Disposition", FALSE));
		RETURN_IF_LEAVE(upload_url->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));
		url.SetHTTP_Data(upload_url.release(), TRUE);
	}
	else
	{
		OpAutoPtr<Upload_BinaryBuffer> upload_buf(OP_NEW(Upload_BinaryBuffer, ()));
		RETURN_OOM_IF_NULL(upload_buf.get());
		RETURN_IF_LEAVE(upload_buf->InitL((unsigned char *) buf, len, UPLOAD_COPY_EXTRACT_HEADERS));
		RETURN_IF_LEAVE(upload_buf->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION));
		url.SetHTTP_Data(upload_buf.release(), TRUE);
		stat = OpStatus::OK;
	}
	// Probably want to call the equivalent of url.SetRequestedExternally(TRUE); ???
	return stat;
}

#endif // HAS_SET_HTTP_DATA

/* PrepareGetURL() extracts the http request header name and value pairs from the src buffer received
   from the plugin and convert it into the URL_Custom_Header required by the URL module.
   The logic is mainly fetched from Header_List::ExtractHeadersL()
*/
OP_STATUS PrepareGetURL(URL& url, const char* src)
{
	OP_ASSERT(src);
	int len = op_strlen(src);
	const char *pos = src;
	char token;
	int i;

	for(i=0; i<len; i++)
	{
		token = *(pos++);
		if(token < 0x20)
		{
			if(token == '\n')
			{
				if(i == len-1 || *pos == '\n')
					break;

				if(*pos != '\r')
					continue;

				i++;
				pos++;
				if(i == len-1 || *pos == '\n')
					break;
			}
			else if(token != '\t' && token != '\r')
				return OpStatus::OK;
		}
	}

	if(i < len)
	{
		HeaderList headers;
		ANCHOR(HeaderList, headers);

#if defined(NS4P_DISABLE_HTTP_COMPRESS) && defined(URL_ALLOW_DISABLE_COMPRESS)
		RETURN_IF_ERROR(url.SetAttribute(URL::KDisableCompress,TRUE));
#endif // NS4P_DISABLE_HTTP_COMPRESS && URL_ALLOW_DISABLE_COMPRESS
		((unsigned char *)src)[i+1] = '\0';

		RETURN_IF_ERROR(headers.SetValue((char *) src));

		HeaderEntry *item = headers.First();
		while(item)
		{
			URL_Custom_Header header_item;
			RETURN_IF_ERROR(header_item.name.Set(item->Name()));
			RETURN_IF_ERROR(header_item.value.Set(item->Value()));
			RETURN_IF_ERROR(url.SetAttribute(URL::KAddHTTPHeader, &header_item));
			item = item->Suc();
		}
	}
	return OpStatus::OK;
}

/* Rule for resolving URLs:
   a URL should always be resolved in the context of the URL of the plugin that
   issued the GetURL call.
   */
NPError PluginGetURL(BYTE type, NPP instance, const char* url_name, const char* url_target, uint32 len, const char* buf, NPBool file, void* notifyData, NPBool unique, const char* headers)
{
	BOOL only_notification = FALSE; // url is loaded in another window, no streaming, notification is sent to the plugin

	OP_ASSERT((!(type & PLUGIN_POST) || unique) && "Post urls must be retrieved from url API with unique = TRUE");

	Plugin* plugin = NULL;
	if (OpNS4PluginHandler::GetHandler())
		plugin = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance);

	if (!plugin)
		return NPERR_NO_ERROR;

	if (!url_name || !*url_name)
		return NPERR_INVALID_URL;

	FramesDocument* frames_doc = plugin->GetDocument();
	if (!frames_doc)
		return NPERR_GENERIC_ERROR;

	Window* window = frames_doc->GetWindow();
	if (!window)
		return NPERR_GENERIC_ERROR;

#ifdef SUPPORT_VISUAL_ADBLOCK
	if (url_target && frames_doc->GetHtmlDocument() && frames_doc->GetHtmlDocument()->GetWindow()->GetContentBlockEditMode())
	{
		HTML_Element* hlm = plugin->GetHtmlElement();
		URL* img_url = hlm->GetEMBED_URL();
		URL url;

		if (!img_url)
		{
			url = hlm->GetImageURL(FALSE);

			if (!url.IsEmpty())
				img_url = &url;
		}

		if (img_url)
		{
			if (!frames_doc->GetIsURLBlocked(*img_url))
			{
				OpString s;
				RETURN_NPERR_IF_ERROR(img_url->GetAttribute(URL::KUniName_Username_Password_Hidden, s, FALSE));

				if (WindowCommander *win_com = frames_doc->GetWindow()->GetWindowCommander())
					win_com->GetDocumentListener()->OnContentBlocked(win_com, s.CStr());

				frames_doc->AddToURLBlockedList(*img_url);
				hlm->MarkDirty(frames_doc, TRUE, TRUE);
				if (plugin->GetPluginWindow())
					frames_doc->GetVisualDevice()->ShowPluginWindow(plugin->GetPluginWindow(), FALSE);
			}
		}

		return NPERR_NO_ERROR;
	}
#endif // SUPPORT_VISUAL_ADBLOCK

	/* Determine the url */

	URL url = plugin->DetermineURL(type, url_name, !!unique);
	if (url.IsEmpty())
		return OpStatus::ERR;

#if MAX_PLUGIN_CACHE_LEN>0
	url.SetAttribute(URL::KUseStreamCache, 1);
#endif

	/* Prepare for loading */
	if (type & PLUGIN_POST)
	{
		RETURN_NPERR_IF_ERROR(PreparePostURL(url, buf, len, file));
	}
	else if (headers)
	{
		RETURN_NPERR_IF_ERROR(PrepareGetURL(url, headers));
	}

	/* Determine the target window, if any, and load the data */
	if (url_target)
	{
#ifdef GADGET_SUPPORT
		if (window->GetGadget() && url.Type() == URL_FILE) // security restriction
			return NPERR_GENERIC_ERROR;
#endif // GADGET_SUPPORT

		const uni_char* win_name = NULL;
		BOOL open_in_other_window = FALSE;
		BOOL user_activated = FALSE;

		if (op_stricmp("_new", url_target) == 0 ||
			op_stricmp("_blank", url_target) == 0)
		{
			win_name = UNI_L("_blank");
			open_in_other_window = TRUE;
		}
		else if (op_stricmp("_current", url_target) == 0 ||
			op_stricmp("_self", url_target) == 0)
		{
			win_name = UNI_L("_self");
		}
		else if (op_stricmp("_parent", url_target) == 0 ||
			op_stricmp("_top", url_target) == 0)
		{
			win_name = make_doublebyte_in_tempbuffer(url_target, op_strlen(url_target));
		}
		else
		{
			win_name = make_doublebyte_in_tempbuffer(url_target, op_strlen(url_target));
			user_activated = TRUE; // always open named window, regardless of user interaction with the plugin
		}

		if (win_name && url.Type() != URL_JAVASCRIPT)
		{
			BOOL plugin_unrequested_popup = TRUE;

			if (plugin->GetPluginfuncs()->version >= NPVERS_HAS_POPUPS_ENABLED_STATE && plugin->GetPopupEnabledState()) // allow pop-ups according to user settings specified through plugin
			{
				user_activated = TRUE;
			}
			else if (plugin->IsPopupsEnabled())
			{
				user_activated = TRUE;
				plugin->SetPopupsEnabled(FALSE);
			}
			if (user_activated && g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups))
			{
				plugin_unrequested_popup = FALSE;
			}

			int sub_win_id = -1;
			RETURN_NPERR_IF_ERROR(g_windowManager->OpenURLNamedWindow(url, window, frames_doc, sub_win_id, win_name, user_activated, open_in_other_window, FALSE, FALSE, FALSE, NULL, plugin_unrequested_popup));
		}

		if (win_name)
			only_notification = (type & PLUGIN_NOTIFY) != 0; // url is loaded in another named window
	}

	/* continue loading and notifying */

	if (url_target && url.Type() != URL_JAVASCRIPT)
	{
		if (only_notification) // url is loaded in another window without streaming, notification is sent to the plugin when finished loading
		{
			PluginStream* dummy;
			RETURN_NPERR_IF_ERROR(plugin->AddStream(dummy, url, (type & PLUGIN_NOTIFY) != 0, notifyData, only_notification));
		}
	}
	else
	{
		if (url.Type() == URL_JAVASCRIPT)
		{
			RETURN_NPERR_IF_ERROR(frames_doc->ConstructDOMEnvironment());

			if (!frames_doc->GetDOMEnvironment()->IsEnabled())
				return NPERR_GENERIC_ERROR;

			OpString stream_url;
			RETURN_NPERR_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_Hidden, stream_url));

			// 'script' must be copied because the URL module may use its temporary
			// storage space for something else after GetUniName is called.  But note
			// that the same operation is repeated in the code below for the sync case.
			// It's possible that it would be helpful to avoid doing the operation twice.

			URL tmpurl;
			if (plugin->GetEmbedSrcUrl())
				tmpurl = plugin->GetEmbedSrcUrl()->GetURL();
			OpSecurityContext source(tmpurl);
			OpSecurityContext target(frames_doc);

			target.AddText(stream_url.CStr());

			BOOL allowed = FALSE;
			RETURN_NPERR_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::PLUGIN_RUNSCRIPT, source, target, allowed));

			if (!allowed)
				return NPERR_GENERIC_ERROR;

			if (!url_target) // stream creation only for JS URL without window, then the result is streamed to the plugin
			{
				PluginStream* new_stream;
				RETURN_NPERR_IF_ERROR(plugin->AddStream(new_stream, url, (type & PLUGIN_NOTIFY) != 0, notifyData, FALSE));

				if (plugin->GetPluginfuncs()->version >= NP_VERSION_MINOR_14)
				{ // NP_VERSION_MINOR_14 is the first version supporting the new scripting api.
					OpString decoded_stream_url(stream_url);
					UriUnescape::ReplaceChars(decoded_stream_url.CStr(), UriUnescape::All);

					NPVariant result;
					BOOL eval_success = SynchronouslyEvaluateJavascriptURL(plugin, frames_doc, decoded_stream_url.CStr(), result);

					// Result is guaranteed to be of a string type if evaluation succeeded.
					if (eval_success)
						RETURN_NPERR_IF_ERROR(new_stream->New(plugin, NULL, result.value.stringValue.UTF8Characters, result.value.stringValue.UTF8Length));

					PluginReleaseExternalValue(result);

					return eval_success ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR;
				}
			}

			BOOL user_activated = FALSE;
			if (plugin->GetPluginfuncs()->version >= NPVERS_HAS_POPUPS_ENABLED_STATE && plugin->GetPopupEnabledState())
			{
				if (g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups)) // allow pop-ups according to user settings specified through plugin
					user_activated = TRUE;
			}
			else if (plugin->IsPopupsEnabled())
			{
				if (g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups))
					user_activated = TRUE;
				plugin->SetPopupsEnabled(FALSE);
			}

			RETURN_NPERR_IF_ERROR(frames_doc->ESOpenJavascriptURL(url, FALSE, TRUE, FALSE, TRUE, NULL, user_activated));

			return NPERR_NO_ERROR;
		}

		PluginStream* new_stream = NULL;
		RETURN_NPERR_IF_ERROR(plugin->AddStream(new_stream, url, (type & PLUGIN_NOTIFY) != 0, notifyData, FALSE));
#ifdef WEB_TURBO_MODE
		RETURN_NPERR_IF_ERROR(url.SetAttribute(URL::KTurboBypassed, TRUE));
#endif // WEB_TURBO_MODE
#if defined(NS4P_DISABLE_HTTP_COMPRESS) && defined(URL_ALLOW_DISABLE_COMPRESS)
		RETURN_NPERR_IF_ERROR(url.SetAttribute(URL::KDisableCompress,TRUE));
#endif // NS4P_DISABLE_HTTP_COMPRESS && URL_ALLOW_DISABLE_COMPRESS

		// Send the URL of the plugin as the referrer
		URL ref_url;
		if (plugin->GetHtmlElement()->Type() == HE_EMBED) // Possible values are HE_EMBED, HE_OBJECT and HE_APPLET
		{
			URL *embed_url = plugin->GetHtmlElement()->GetEMBED_URL(frames_doc->GetLogicalDocument());
			if (embed_url)
				ref_url = *embed_url;
		}
		else if (plugin->GetHtmlElement()->Type() == HE_OBJECT)
		{
			URL *object_url = plugin->GetHtmlElement()->GetUrlAttr(ATTR_DATA, NS_IDX_HTML, frames_doc->GetLogicalDocument());
			if (object_url)
				ref_url = *object_url;
		}
		else
		{
			OP_ASSERT(plugin->GetHtmlElement()->Type() == HE_APPLET);
			URL *applet_url = plugin->GetHtmlElement()->GetUrlAttr(ATTR_CODE, NS_IDX_HTML, frames_doc->GetLogicalDocument());
			if (applet_url)
				ref_url = *applet_url;
		}

		if (!ref_url.IsEmpty())
		{
			URL redirected_url = ref_url.GetAttribute(URL::KMovedToURL, URL::KFollowRedirect);
			if (!redirected_url.IsEmpty())
				ref_url = redirected_url;
		}
		else
			ref_url = plugin->GetBaseURL();

# ifdef _DEBUG
		OP_NEW_DBG("PluginGetURL", "pluginloading");
		plugin->m_loading_streams++;
		OP_DBG((" (%d/%d) Starting loading of %s", plugin->m_loading_streams, plugin->m_loaded_streams, url.GetName(FALSE, PASSWORD_SHOW)));
# endif // _DEBUG
		URL_NormalLoad load_policy;

#ifdef URL_FILTER
		// Checks if content_filter rules allow this URL
		if (url.Type() != URL_JAVASCRIPT)
			RETURN_NPERR_IF_ERROR(frames_doc->ConstructDOMEnvironment());

		ServerName *doc_sn = url.GetServerName();
		URLFilterDOMListenerOverride lsn_over(frames_doc->GetDOMEnvironment(), NULL);
		HTMLLoadContext load_ctx(RESOURCE_OBJECT_SUBREQUEST, doc_sn, &lsn_over, window->GetType() == WIN_TYPE_GADGET);
		BOOL allowed = FALSE;

		RETURN_VALUE_IF_ERROR(g_urlfilter->CheckURL(&url, allowed, NULL,  &load_ctx), NPERR_GENERIC_ERROR);

		if (!allowed)
			return NPERR_GENERIC_ERROR;
#endif // URL_FILTER

		RETURN_NPERR_IF_ERROR(plugin->LoadDocument(url, ref_url, load_policy, new_stream));
	}

	return NPERR_NO_ERROR;
}

NPError __export NPN_GetURL(NPP instance, const char* url_name, const char* window)
{
	OP_NEW_DBG("NPN_GetURL", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}

	BOOL unique = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) == 0;

#if MAX_PLUGIN_CACHE_LEN>0
	unique = TRUE;
#endif // MAX_PLUGIN_CACHE_LEN>0

	NPError ret = PluginGetURL(PLUGIN_URL, instance, url_name, window, 0, NULL, FALSE, NULL, unique, NULL);
    return ret;
}

NPError __export NPN_PostURL(NPP instance, const char* url_name, const char* window, uint32_t len, const char* buf, NPBool file)
{
	OP_NEW_DBG("NPN_PostURL", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}

    Plugin* plugin = NULL;
    NPError ret = NPERR_NO_ERROR;

    if (!url_name
#ifdef _MACINTOSH_
        || *url_name == 0
#endif
        )
        return NPERR_INVALID_URL;

    if (OpNS4PluginHandler::GetHandler())
        plugin = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance);

    if (plugin && plugin->GetDocument())
    {
        int security_status = plugin->GetDocument()->GetURL().GetAttribute(URL::KSecurityStatus);
        if (security_status != SECURITY_STATE_NONE && security_status != SECURITY_STATE_UNKNOWN
#ifdef SELFTEST
			&& !plugin->GetDocument()->GetURL().GetAttribute(URL::KIsGeneratedBySelfTests)
#endif // SELFTEST
			)
        {
            if (OpStatus::IsError(plugin->HandlePostRequest(PLUGIN_POST, url_name, window, len, buf, file)))
                ret = NPERR_GENERIC_ERROR;
        }
        else
            ret = PluginGetURL(PLUGIN_POST, instance, url_name, window, len, buf, file, NULL, TRUE, NULL);
    }
    else
        ret = NPERR_GENERIC_ERROR;
    return ret;
}

/**
 * Request byte range(s) or seek from active stream. Invoked by plug-in.
 *
 * Our current implementation, though it may at first glance appear partially
 * complete, is in reality spotty at best.
 *
 * 1. The API requires that we support fetching separate byte ranges,
 * which we clearly do not (see PluginStream::RequestRead.)
 *
 * 2. What has been partially implemented, is seeking by way of receiving one
 * zero-interval byte range indicating the seek offset. This was added by
 * ohrn@opera.com for basic Flash Lite support. The implementation was
 * supported by a set of network patches (pre-PPP), but not all of these were
 * accepted. It is quite likely that this implementation is completely non-
 * functional and can be dropped.
 */
NPError	__export NPN_RequestRead(NPStream* stream, NPByteRange* rangeList)
{
	OP_NEW_DBG("NPN_RequestRead", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}
	if (stream && rangeList)
		return static_cast<PluginStream *>(stream->ndata)->RequestRead(rangeList);
	else
		return NPERR_INVALID_PARAM;
}

NPError	__export NPN_NewStream(NPP instance, NPMIMEType type, const char* window, NPStream** stream)
{
	OP_NEW_DBG("NPN_NewStream", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}

	return NPERR_GENERIC_ERROR;
}

int32_t __export NPN_Write(NPP instance, NPStream* stream, int32_t len, void* buffer)
{
	OP_NEW_DBG("NPN_Write", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return 0;
	}

	return NPERR_GENERIC_ERROR;
}

NPError __export NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
	OP_NEW_DBG("NPN_DestroyStream", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}

	if (stream)
	{
		Plugin* plugin = NULL;
		if (OpNS4PluginHandler::GetHandler())
			plugin = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance);
		if (plugin)
		{
			if (OpStatus::IsError(plugin->InterruptStream(stream, reason)))
				return NPERR_GENERIC_ERROR;
		}
	    else
			return NPERR_INVALID_PLUGIN_ERROR;
	}
	else
		return NPERR_GENERIC_ERROR;
	return NPERR_NO_ERROR;
}

void __export NPN_Status(NPP instance, const char* message)
{
	OP_NEW_DBG("NPN_Status", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return;
	}

	Plugin* plugin = NULL;
	if (OpNS4PluginHandler::GetHandler())
		plugin = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance);

	if (plugin && plugin->GetDocument() && plugin->GetDocument()->GetWindow())
	{
		OpString tmp_str;
		OP_STATUS stat = tmp_str.Set(message);
		if (OpStatus::IsError(stat))
		{ // The plug-in interface does not define a return value for the NPN_Status call.
			if (OpStatus::IsMemoryError(stat))
				g_memory_manager->RaiseCondition(stat);
			else
				OpStatus::Ignore(stat);
		}
		else
			plugin->GetDocument()->GetWindow()->SetMessage(tmp_str.CStr());
	}

}

const char* __export NPN_UserAgent(NPP instance)
{
	OP_NEW_DBG("NPN_UserAgent", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NULL;
	}

	int len = 0;

#ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	Plugin* plug = g_pluginhandler && instance ? g_pluginhandler->FindPlugin(instance) : NULL;

	if ((plug && plug->SpoofUA()) || (!plug && g_pluginhandler && g_pluginhandler->SpoofUA()))
		len = g_uaManager->GetUserAgentStr(g_agent, sizeof(g_agent), NULL, NULL, UA_MozillaOnly);
	else
#endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
		len = g_uaManager->GetUserAgentStr(g_agent, sizeof(g_agent), NULL, NULL, UA_Opera);

	if (len > 0)
	{
		return g_agent;
	}

	return NULL;
}

void* __export NPN_MemAlloc(uint32_t size)
{
	OP_NEW_DBG("NPN_MemAlloc", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	return PluginMemoryHandler::GetHandler()->Malloc(size);
}

void __export NPN_MemFree(void* ptr)
{
	OP_NEW_DBG("NPN_MemFree", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	PluginMemoryHandler::GetHandler()->Free(ptr);
}

uint32_t __export NPN_MemFlush(uint32_t size)
{
	OP_NEW_DBG("NPN_MemFlush", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	return 0;
}

void __export NPN_ReloadPlugins(NPBool reloadPages)
{
	OP_NEW_DBG("NPN_ReloadPlugins", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return;
	}

	OpStatus::Ignore(g_plugin_viewers->RefreshPluginViewers(FALSE));

	return;
}

// *** When JAVA is READY

/**
 * NPN_GetJavaEnv() and NPN_GetJavaPeer() was a parts of the now deprecated FEATURE_NETSCAPE4_LIVECONNECT
 * which has been replaced by the FEATURE_JAVA_PLUGIN + the Oracle Java NPAPI plugin. We retain these stubs
 * only because they are listed in the upstream npapi headers. */
void* __export NPN_GetJavaEnv(void)
{
	OP_NEW_DBG("NPN_GetJavaEnv", "ns4p");
	IN_CALL_FROM_PLUGIN
	return 0;
}

/**
 * See note on NPN_GetJavaEnv().
 */
void* __export NPN_GetJavaPeer(NPP instance)
{
	OP_NEW_DBG("NPN_GetJavaPeer", "ns4p");
	IN_CALL_FROM_PLUGIN
	return 0;
}

// *** When JAVA is READY END

NPError	__export NPN_GetURLNotify(NPP instance, const char* url_name, const char* window, void* notifyData)
{
	OP_NEW_DBG("NPN_GetURLNotify", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}

	BOOL unique = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DiskCacheSize) == 0;

#if MAX_PLUGIN_CACHE_LEN>0
	unique = TRUE;
#endif // MAX_PLUGIN_CACHE_LEN>0

	return PluginGetURL(PLUGIN_URL|PLUGIN_NOTIFY, instance, url_name, window, 0, NULL, FALSE, notifyData, unique, NULL);
}

NPError __export NPN_PostURLNotify(NPP instance, const char* url_name, const char* window, uint32_t len, const char* buf, NPBool file, void* notifyData)
{
	OP_NEW_DBG("NPN_PostURLNotify", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}

    Plugin* plugin = NULL;
    NPError ret = NPERR_NO_ERROR;
    if (!url_name
#ifdef _MACINTOSH_
        || *url_name == 0
#endif
        )
        return NPERR_INVALID_URL;

    if (OpNS4PluginHandler::GetHandler())
        plugin = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance);

    if (plugin && plugin->GetDocument())
    {
        int security_status = plugin->GetDocument()->GetURL().GetAttribute(URL::KSecurityStatus);
        if (security_status != SECURITY_STATE_NONE && security_status != SECURITY_STATE_UNKNOWN
#ifdef SELFTEST
			&& !plugin->GetDocument()->GetURL().GetAttribute(URL::KIsGeneratedBySelfTests)
#endif // SELFTEST
			)
        {
            if (OpStatus::IsError(plugin->HandlePostRequest(PLUGIN_POST|PLUGIN_NOTIFY, url_name, window, len, buf, file, notifyData)))
                ret = NPERR_GENERIC_ERROR;
        }
        else
            ret = PluginGetURL(PLUGIN_POST|PLUGIN_NOTIFY, instance, url_name, window, len, buf, file, notifyData, TRUE, NULL);
    }
    else
        ret = NPERR_GENERIC_ERROR;
    return ret;
}


NPError __export NPN_GetValue(NPP instance, NPNVariable variable, void *value)
{
	OP_NEW_DBG("NPN_GetValue", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}

	NPError result = NPERR_NO_ERROR;

#ifdef USE_PLUGIN_EVENT_API
	if (!instance && OpNS4PluginAdapter::GetValueStatic(variable, value, &result))
		return result;
#endif // USE_PLUGIN_EVENT_API

	Plugin* plug = NULL;
	if (OpNS4PluginHandler::GetHandler())
		plug = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance);
	if (plug)
	{
		FramesDocument* frames_doc = plug->GetDocument();
		if (!frames_doc)
			return NPERR_GENERIC_ERROR;

#ifdef USE_PLUGIN_EVENT_API
		if (plug->GetPluginAdapter()->GetValue(variable, value, &result))
			return result;
#endif // USE_PLUGIN_EVENT_API

		switch (variable)
		{
#ifndef USE_PLUGIN_EVENT_API
		case NPNVnetscapeWindow:
#ifdef MSWIN
			if (plug->GetWindow())
				*((void**)value) = (void*)::GetParent((HWND)plug->GetWindow());
			else
			{
#ifdef VEGA_OPPAINTER_SUPPORT
				WindowsOpWindow* tw = ((WindowsOpView*)frames_doc->GetVisualDevice()->view->GetOpView())->GetNativeWindow();
				HWND hwnd = tw?tw->m_hwnd:NULL;
#else
				HWND hwnd = ((WindowsOpView*)frames_doc->GetVisualDevice()->view->GetOpView())->hwnd;
#endif
				*((void**)value) = hwnd;
			}
#elif defined(WINGOGI)
			*((void**)value) = GetParentWindowHandle();
#else
			*((void**)value) = 0;
#endif // MSWIN
			break;
#endif // !USE_PLUGIN_EVENT_API

		case NPNVjavascriptEnabledBool:
			*((BOOL*)value) = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, plug->GetHostName());
			break;

		case NPNVasdEnabledBool:
			*((BOOL*)value) = FALSE;
			break;

		case NPNVisOfflineBool:
			*((BOOL*)value) = !!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode);
			break;

		case NPNVWindowNPObject:
			*((void**)value) = plug->GetWindowNPObject();
			break;

		case NPNVPluginElementNPObject:
			*((void**)value) = plug->GetPluginElementNPObject();
			break;

		case NPNVSupportsXEmbedBool:
			*((BOOL*)value) = FALSE;
			break;

		case NPNVSupportsWindowless:
			*(static_cast<NPBool *>(value)) = TRUE;
			break;

		case NPNVprivateModeBool:
			*(static_cast<NPBool *>(value)) = plug->GetDocument()->GetWindow()->GetPrivacyMode();
			break;

		case NPNVsupportsAdvancedKeyHandling:
			*(static_cast<NPBool *>(value)) = FALSE;
			break;

		case NPNVdocumentOrigin:
			if (OpStatus::IsError(plug->GetOrigin(static_cast<char**>(value))))
				result = NPERR_GENERIC_ERROR;
		break;

		default:
			result = NPERR_GENERIC_ERROR; // Follow Firefox when handling unknown variable value
			break;
		}
	}
	else if (variable == NPNVserviceManager)
		// Obsolete - not supported.
		result = NPERR_GENERIC_ERROR;
	else
		result = NPERR_INVALID_PLUGIN_ERROR;

	return result;
}


NPError __export NPN_SetValue(NPP instance, NPPVariable variable, void *value)
{
	OP_NEW_DBG("NPN_SetValue", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return NPERR_INVALID_PARAM;
	}

	Plugin* plugin = NULL;
	if (OpNS4PluginHandler::GetHandler())
		plugin = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance);
	if (!plugin)
	    return NPERR_INVALID_PLUGIN_ERROR;

#ifdef USE_PLUGIN_EVENT_API
	NPError result;

	if (plugin->GetPluginAdapter()->SetValue(variable, value, &result))
		return result;
#endif // USE_PLUGIN_EVENT_API

	switch (variable)
	{
#ifndef NS4P_ALL_PLUGINS_ARE_WINDOWLESS
	case NPPVpluginWindowBool:
		plugin->SetWindowless(!value);
		// Assume that plugin wants to be transparent when setting windowless mode.
		// Required for Silverlight which does not set NPPVpluginTransparentBool.
		plugin->SetTransparent(!value);
		break;
#endif // !NS4P_ALL_PLUGINS_ARE_WINDOWLESS
	case NPPVpluginTransparentBool:
		plugin->SetTransparent(!!value);
		break;
	case NPPVpluginUrlRequestsDisplayedBool:
		plugin->SetPluginUrlRequestsDisplayed(!!value);
		break;
#ifdef _MACINTOSH_
	case NPPVpluginDrawingModel:
	{
		NPDrawingModel model = static_cast<NPDrawingModel>(reinterpret_cast<intptr_t>(value));
		plugin->SetDrawingModel(model);
		if (model == NPDrawingModelCoreAnimation || model == NPDrawingModelInvalidatingCoreAnimation)
			plugin->SetCoreAnimation(TRUE);
		break;
	}
#endif // _MACINTOSH_
	default:
		return NPERR_GENERIC_ERROR; // Follow Firefox when handling unknown variable value

	}
	return NPERR_NO_ERROR;
}

void __export NPN_InvalidateRect(NPP instance, NPRect *invalidRect)
{
	OP_NEW_DBG("NPN_InvalidateRect", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return;
	}

	Plugin* plugin = g_pluginhandler ? g_pluginhandler->FindPlugin(instance) : NULL;

	if (plugin)
	{
		plugin->Invalidate(invalidRect);
	}
}

void __export NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion)
{
	OP_NEW_DBG("NPN_InvalidateRegion", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return;
	}

#ifdef USE_PLUGIN_EVENT_API
	Plugin* plugin = g_pluginhandler ? g_pluginhandler->FindPlugin(instance) : NULL;

	if (plugin)
	{
		NPRect invalidRect;
		if (plugin->GetPluginAdapter()->ConvertPlatformRegionToRect(invalidRegion, invalidRect))
			plugin->Invalidate(&invalidRect);
	}
#endif // USE_PLUGIN_EVENT_API
}

void __export NPN_ForceRedraw(NPP instance)
{
	OP_NEW_DBG("NPN_ForceRedraw", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return;
	}

	Plugin* plugin = g_pluginhandler ? g_pluginhandler->FindPlugin(instance) : NULL;

	if (plugin && plugin->GetDocument() && plugin->GetDocument()->GetVisualDevice())
		plugin->GetDocument()->GetVisualDevice()->GetView()->Sync();
}

void __export NPN_PushPopupsEnabledState(NPP instance, NPBool enabled)
{
	OP_NEW_DBG("NPN_PushPopupsEnabledState", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return;
	}

	Plugin* plugin = OpNS4PluginHandler::GetHandler() ? ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance) : NULL;
	if (plugin)
		plugin->PushPopupsEnabledState(enabled);
	return;
}

void __export NPN_PopPopupsEnabledState(NPP instance)
{
	OP_NEW_DBG("NPN_PopPopupsEnabledState", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		return;
	}

	Plugin* plugin = OpNS4PluginHandler::GetHandler() ? ((PluginHandler*)OpNS4PluginHandler::GetHandler())->FindPlugin(instance) : NULL;
	if (plugin)
		plugin->PopPopupsEnabledState();
	return;
}

NPError AllocateString(const char* buf, char **value, uint32_t *len)
{
	unsigned buf_len = 0;
	if (buf)
	{
		buf_len = op_strlen(buf);
		*value = static_cast<char *>(PluginMemoryHandler::GetHandler()->Malloc(sizeof(char)*(buf_len + 1)));
		if (!*value)
			return NPERR_OUT_OF_MEMORY_ERROR;

		op_strncpy(*value, buf, buf_len);
	}
	(*value)[buf_len] = '\0';
	*len = buf_len;

	return NPERR_NO_ERROR;
}

NPError __export NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char *url_name, char **value, uint32_t *len)
{
	OP_NEW_DBG("NPN_GetValueForURL", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	NPError np_stat = NPERR_NO_ERROR;
	OP_STATUS op_stat = OpStatus::OK;

	Plugin* plug = g_pluginhandler && instance ? g_pluginhandler->FindPlugin(instance) : NULL;
	if (plug)
	{
		if ((variable == NPNURLVCookie || variable == NPNURLVProxy) && value && len)
		{
			*len = 0;

			if (url_name)
			{
				URL url = g_url_api->GetURL(url_name);
				if (variable == NPNURLVCookie)
				{ // read cookie string into buffer
					const char *OP_MEMORY_VAR buf = NULL;
					int version, max_version;
					BOOL have_password = FALSE;
					BOOL have_authentication = FALSE;
					TRAP(op_stat, buf = g_url_api->GetCookiesL(	url, version, max_version,
																!!url.GetAttribute(URL::KHavePassword),
																!!url.GetAttribute(URL::KHaveAuthentication),
																have_password, have_authentication));
					if (OpStatus::IsError(op_stat))
						np_stat = OpStatus::IsMemoryError(op_stat) ? NPERR_OUT_OF_MEMORY_ERROR : NPERR_GENERIC_ERROR;
					else if (buf)
						np_stat = AllocateString(buf, value, len);
				}
				else
				{ // NPNURLVProxy
					OpString8 proxy;
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
					/* temporary disable fetching of automatic proxy configuration, as it requires asynchronous
					   fetching of information. Disapproved during code review.
					if (g_pcnet->IsAutomaticProxyOn() &&
						g_pcnet->GetStringPref(PrefsCollectionNetwork::AutomaticProxyConfigURL).CStr() &&
						plug->GetDocument())
					{ // use the automatic proxy configuration setting.
					  // Note that determining proxy information is url data storage dependant code, so the url must be loaded
						op_stat = plug->GetDocument()->LoadInline(&url, plug->GetHtmlElement(), EMBED_INLINE, TRUE, TRUE);
						if (OpStatus::IsSuccess(op_stat))
							op_stat = url.GetAttribute(URL::KHTTPProxy, proxy);
						np_stat = OpStatus::IsError(op_stat) ? (OpStatus::IsMemoryError(op_stat) ? NPERR_OUT_OF_MEMORY_ERROR : NPERR_GENERIC_ERROR) : NPERR_NO_ERROR;
					}*/
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
					if (np_stat == NPERR_NO_ERROR)
					{
						if (proxy.IsEmpty())
						{ // check if any other proxy information is available from Prefs (no need to load the url)
							if (ServerName *sn = (ServerName*)url.GetAttribute(URL::KServerName, NULL))
							{
								const uni_char* proxy_url = urlManager->GetProxy(sn, url.Type());
								int proxy_len = proxy_url ? uni_strlen(proxy_url) : 0;
								if (proxy_len > 0)
								{
									OP_STATUS conversion_stat = proxy.SetUTF8FromUTF16(proxy_url, proxy_len);
									np_stat = OpStatus::IsError(conversion_stat) ? (OpStatus::IsMemoryError(conversion_stat) ? NPERR_OUT_OF_MEMORY_ERROR : NPERR_GENERIC_ERROR) : NPERR_NO_ERROR;
								}
							}
							else
								np_stat = NPERR_INVALID_URL;
						}
						if (np_stat == NPERR_NO_ERROR)
						{
							if (proxy.IsEmpty() || op_stricmp(proxy.CStr(), "DIRECT") == 0)
							{ // default return value is "DIRECT"
								np_stat = AllocateString("DIRECT", value, len);
							}
							else
							{ // add "PROXY " before the proxy string
								OpString8 add_proxy;
								if (OpStatus::IsSuccess(op_stat = add_proxy.Set("PROXY ")) &&
									OpStatus::IsSuccess(op_stat = add_proxy.Append(proxy.CStr())))
									np_stat = AllocateString(add_proxy.CStr(), value, len);
								else
									np_stat = OpStatus::IsMemoryError(op_stat) ? NPERR_OUT_OF_MEMORY_ERROR : NPERR_GENERIC_ERROR;
							}
						}
					}
				}
			}
			else
				np_stat = NPERR_INVALID_URL;
		}
		else
			np_stat = NPERR_INVALID_PARAM;
	}
	else
		np_stat = NPERR_INVALID_INSTANCE_ERROR;

	return np_stat;
}

NPError __export NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char *url_name, const char *value, uint32_t len)
{
	OP_NEW_DBG("NPN_SetValueForURL", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	/* Require valid plug-in instance parameter. */
	if (!g_pluginhandler || !instance || !g_pluginhandler->FindPlugin(instance))
		return NPERR_INVALID_INSTANCE_ERROR;

	/* Require non-empty cookie. */
	if (variable != NPNURLVCookie || !value || !len)
		return NPERR_INVALID_PARAM;

	/* Require non-empty, known and valid URL. */
	if (!url_name || !*url_name)
		return NPERR_INVALID_URL;

	URL url = g_url_api->GetURL(url_name);
	if (url.GetAttribute(URL::KHostName).IsEmpty())
		return NPERR_INVALID_URL;

	/* Make a null-terminated copy of the argument. */
	char* val = OP_NEWA(char, len + 1);
	if (!val)
		return NPERR_OUT_OF_MEMORY_ERROR;

	op_memcpy(val, value, len);
	val[len] = 0;

	// Build a list of cookie parameters, separated by ";", and check if the cookie contains the
	// "httponly" parameter, which means that the cookie should be ignored, following Mozilla.
	OP_STATUS op_stat = OpStatus::OK;
	ParameterList p_list; ANCHOR(ParameterList, p_list);

	TRAP(op_stat, p_list.SetValueL(val, PARAM_SEP_SEMICOLON | PARAM_ONLY_SEP));
	if (OpStatus::IsSuccess(op_stat))
	{
		BOOL p_httponly = FALSE;
		Parameters* p = p_list.First();

		while (p && !p_httponly)
		{
			p_httponly = op_stricmp(p->Name(), "httponly") == 0;
			p = p->Suc();
		}

		if (!p_httponly)
		{ // Store the cookie value as received from the plug-in.
			OpString8 cookie;
			op_stat = cookie.Set(value, len);
			if (OpStatus::IsSuccess(op_stat))
				TRAP(op_stat, g_url_api->HandleSingleCookieL(url, cookie, cookie, 0));
		}
	}

	OP_DELETEA(val);

	if (OpStatus::IsMemoryError(op_stat))
		return NPERR_OUT_OF_MEMORY_ERROR;

	return OpStatus::IsError(op_stat) ? NPERR_GENERIC_ERROR : NPERR_NO_ERROR;
}

NPError __export NPN_GetAuthenticationInfo(NPP instance, const char *protocol, const char *host, int32_t port, const char *scheme, const char *realm, char **username, uint32_t *ulen, char **password, uint32_t *plen)
{
	OP_NEW_DBG("NPN_GetAuthenticationInfo", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
		// It seems to be semi-allowed to run this in the wrong thread
		// so until we know more, we just log it.
	}

	NPError np_stat = NPERR_NO_ERROR;

	Plugin* plug = g_pluginhandler && instance ? g_pluginhandler->FindPlugin(instance) : NULL;
	if (plug)
	{
		if (instance && protocol && host && scheme && realm && username && ulen && password && plen)
		{
			const OpStringC8 sname(host);
			if (ServerName *sn = static_cast<ServerName *>(g_url_api->GetServerName(sname)))
			{
				URLType url_type = g_opera->url_module.g_urlManager->MapUrlType(protocol);
				if (url_type == URL_HTTP || url_type == URL_HTTPS)
				{
					AuthElm* auth = sn->Get_Auth(realm, port, NULL, url_type, AUTH_SCHEME_HTTP_UNKNOWN, 0);
					if (!auth) // different input authentication scheme parameter for proxy authentication
						auth = sn->Get_Auth(realm, port, NULL, url_type, AUTH_SCHEME_HTTP_PROXY, 0);
					if (auth && auth->GetUser() && auth->GetPassword())
					{
						if ((np_stat = AllocateString(auth->GetUser(), username, ulen)) == NPERR_NO_ERROR)
							np_stat = AllocateString(auth->GetPassword(), password, plen);
					}
				}
				else
					np_stat = NPERR_GENERIC_ERROR;
			}
			else
				np_stat = NPERR_GENERIC_ERROR;
		}
		else
			np_stat = NPERR_INVALID_PARAM;
	}
	else
		np_stat = NPERR_INVALID_INSTANCE_ERROR;

	return np_stat;
}

uint32_t __export NPN_ScheduleTimer(NPP instance, uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
	OP_NEW_DBG("NPN_ScheduleTimer", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
	}

	uint32_t timerID = 0;
	if (g_pluginhandler && instance)
	{
		timerID = g_pluginhandler->ScheduleTimer(instance, interval, !!repeat, timerFunc);
	}

	return timerID;
}

void __export NPN_UnscheduleTimer(NPP instance, uint32_t timerID)
{
	OP_NEW_DBG("NPN_UnscheduleTimer", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
	}

	if (g_pluginhandler)
	{
		g_pluginhandler->UnscheduleTimer(timerID);
	}
}

NPError __export NPN_PopUpContextMenu(NPP instance, NPMenu* menu)
{
	OP_NEW_DBG("NPN_PopUpContextMenu", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
	}

	NPError np_stat = NPERR_GENERIC_ERROR;
	Plugin* plugin = g_pluginhandler ? g_pluginhandler->FindPlugin(instance) : NULL;
#ifdef USE_PLUGIN_EVENT_API
	if (plugin && plugin->GetPluginWindow())
		if (OpStatus::IsSuccess(plugin->GetPluginWindow()->PopUpContextMenu(menu)))
			np_stat = NPERR_NO_ERROR;
#endif
	return np_stat;
}

NPBool __export NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double* destX, double* destY, NPCoordinateSpace destSpace)
{
	OP_NEW_DBG("NPN_ConvertPoint", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
	{
		OP_DBG(("Function call from a plugin on the wrong thread"));
	}

	Plugin* plugin = g_pluginhandler ? g_pluginhandler->FindPlugin(instance) : NULL;
#ifdef USE_PLUGIN_EVENT_API
	if (plugin && plugin->GetPluginWindow())
		if (OpStatus::IsSuccess(plugin->GetPluginWindow()->ConvertPoint(sourceX, sourceY, sourceSpace, destX, destY, destSpace)))
			return TRUE;
#endif
	return FALSE;
}

NPBool __export NPN_HandleEvent(NPP instance, void *event, NPBool handled)
{
	OP_NEW_DBG("NPN_HandleEvent", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
		OP_DBG(("Function call from a plugin on the wrong thread"));

	// Stub
	return FALSE;
}

NPBool __export NPN_UnfocusInstance(NPP instance, NPFocusDirection direction)
{
	OP_NEW_DBG("NPN_UnfocusInstance", "ns4p");
	IN_CALL_FROM_PLUGIN
	if (!g_op_system_info->IsInMainThread())
		OP_DBG(("Function call from a plugin on the wrong thread"));

	// Stub
	return FALSE;
}

#endif // _PLUGIN_SUPPORT_
