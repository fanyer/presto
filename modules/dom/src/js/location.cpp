/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/js/location.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/domutils.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/url/url_man.h"
#include "modules/url/url_sn.h"
#include "modules/util/tempbuf.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#ifdef GADGET_SUPPORT
# include "modules/dochand/win.h"
# include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT
#include "modules/security_manager/include/security_manager.h"

#ifdef DOM_WEBWORKERS_SUPPORT
#include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT

class ES_Thread;

URL
GetEncodedURL(FramesDocument *origining_frames_doc, FramesDocument* target_frames_doc, const uni_char *url
#ifdef SELFTEST
             , URL *override_base_url
#endif // SELFTEST
             )
{
	const uni_char *url_ptr = url;

#if defined (MSWIN) || defined (EPOC) // Really: Support for DOS-style drive designator, eg "C:"
	if( url[1] == ':' && uni_isalpha(url[0]) )
	{
		OpString resolved_url;
		OP_STATUS result;

		TRAP(result, g_url_api->ResolveUrlNameL(url, resolved_url));

		if (OpStatus::IsSuccess(result))
			url_ptr = resolved_url.DataPtr();
	}
#endif // MSWIN || EPOC

	URL base_url;
	URL *tmp_url = NULL;

#ifdef SELFTEST
	if (override_base_url)
		tmp_url = CreateUrlFromString(url_ptr, uni_strlen(url_ptr), override_base_url, NULL, FALSE, TRUE);
	else
#endif // SELFTEST
	if (origining_frames_doc)
	{
		base_url = origining_frames_doc->GetURL();
		if (HLDocProfile *hld_profile = origining_frames_doc->GetHLDocProfile())
		{
			if (hld_profile->BaseURL())
				base_url = *hld_profile->BaseURL();

			tmp_url = CreateUrlFromString(url_ptr, uni_strlen(url_ptr), &base_url, hld_profile, FALSE, TRUE);
		}
	}

	URL ret_url;
	if (tmp_url)
	{
		ret_url = *tmp_url;
		OP_DELETE(tmp_url);
	}
	else
		ret_url = g_url_api->GetURL(base_url, url_ptr);

	if (target_frames_doc && target_frames_doc != origining_frames_doc)
	{
		// In case of unique urls (not uncommon since
		// FramesDocument::ESOpen creates them to be able to set a custom
		// content type) we need to move the url to the
		// right context so that it's possible for the current
		// url and the new url to compare equal (if they are equal).

		const uni_char* url_str = ret_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr();
		tmp_url = CreateUrlFromString(url_str, uni_strlen(url_str), &target_frames_doc->GetURL(), target_frames_doc->GetHLDocProfile(), FALSE, FALSE);
		if (tmp_url)
		{
			ret_url = *tmp_url;
			OP_DELETE(tmp_url);
		}
	}

	return ret_url;
}

URL
JS_Location::GetEncodedURL(FramesDocument *origining_frames_doc, const uni_char *url)
{
	return ::GetEncodedURL(origining_frames_doc, GetFramesDocument(), url
#ifdef SELFTEST
	                      , !do_navigation ? &current_url : NULL
#endif // SELFTEST
	                      );
}

/* virtual */ BOOL
JS_Location::SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (!DOM_Object::SecurityCheck(property_name, value, origining_runtime))
		if (FramesDocument *frames_doc = GetFramesDocument())
			if (value == NULL)
				/* FIXME: The security check will probably have succeeded for about:blank
				   in the first place, so this ought to be pointless. */
				return IsAboutBlankURL(frames_doc->GetURL());
			else if (property_name == OP_ATOM_href)
				/* Checking in JS_Location::PutName. */
				return TRUE;
			else
				return FALSE;

	/* Allow access if there is no FramesDocument.  In this case, there is no
	   sensitive information to be returned, and PutName can't do anything.
	   No point risking throwing security errors we wouldn't throw if there
	   was a FramesDocument. */
	return TRUE;
}

/* virtual */ ES_GetState
JS_Location::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	TempBuffer *buffer = GetEmptyTempBuf();
	URL url;

	if (fakewindow)
		url = fakewindow->GetURL();
#ifdef SELFTEST
	else if (!do_navigation)
		url = current_url;
#endif // SELFTEST
	else if (FramesDocument *frames_doc = GetFramesDocument())
	{
		url = frames_doc->GetURL();

		// The anchors (hash) might be better in DocumentManager
		URL doc_man_url = frames_doc->GetDocManager()->GetCurrentURL();
		if (doc_man_url == url) // Doesn't compare anchors
			url = doc_man_url;
	}
#ifdef DOM_WEBWORKERS_SUPPORT
	/* No FramesDocument to query, so consult the origin DocumentManager for the Worker */
	if (!GetFramesDocument())
	{
		DOM_WebWorkerController *web_workers = GetEnvironment()->GetWorkerController();
		if (DOM_WebWorker *ww = web_workers->GetWorkerObject())
			url = ww->GetLocationURL();
		else if (DocumentManager *doc = web_workers->GetWorkerDocManager())
			url = doc->GetCurrentURL();
		OP_ASSERT(!url.IsEmpty());
	}
#endif // DOM_WEBWORKERS_SUPPORT

	switch (property_name)
	{
	case OP_ATOM_href:
		DOMSetString(value, url.GetAttribute(URL::KUniName_With_Fragment_Escaped).CStr());
		return GET_SUCCESS;

	case OP_ATOM_protocol:
		if (value)
		{
			const char *protocol = url.GetAttribute(URL::KProtocolName).CStr();
			if (protocol)
			{
				GET_FAILED_IF_ERROR(buffer->Append(protocol));
				GET_FAILED_IF_ERROR(buffer->Append(":"));
			}

			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_host:
	case OP_ATOM_hostname:
		if (value)
		{
			const uni_char *name = url.GetServerName() ? url.GetServerName()->UniName() : NULL;

			if (property_name == OP_ATOM_host)
			{
				unsigned short port = url.GetServerPort();
				if (port)
				{
					GET_FAILED_IF_ERROR(buffer->Append(name));
					GET_FAILED_IF_ERROR(buffer->Append(":"));
					GET_FAILED_IF_ERROR(buffer->AppendUnsignedLong(port));
					name = buffer->GetStorage();
				}
			}

			DOMSetString(value, name);
		}
		return GET_SUCCESS;

	case OP_ATOM_port:
		if (value)
		{
			unsigned short port = url.GetServerPort();
			if (port)
				GET_FAILED_IF_ERROR(buffer->AppendUnsignedLong(port));

			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_pathname:
		if (value)
		{
			const uni_char *path = url.GetAttribute(URL::KUniPath).CStr();

			if (path)
			{
				GET_FAILED_IF_ERROR(buffer->Append(path));
				uni_char *path_tmp = buffer->GetStorage();

				/* It isn't obvious from the JS spec and the relevant RFC, but in
				   Javascript the 'pathname' excludes any arguments passed to the page. */
				if (uni_char *query_start = uni_strchr(path_tmp, '?'))
				{
					path = path_tmp;
					*query_start = 0;
				}
			}

			DOMSetString(value, path);
		}
		return GET_SUCCESS;

	case OP_ATOM_search:
		if (value)
		{
			const uni_char *name = url.GetAttribute(URL::KUniName).CStr();

			if (name)
				name = uni_strchr(name, '?');

			DOMSetString(value, name);
		}
		return GET_SUCCESS;

	case OP_ATOM_hash:
		if (value)
		{
			const uni_char *fragment = url.UniRelName();

			// MSIE emits "#" for the empty fragment (as in http://www.opera.com/# ) but no other
			// browser does that and neither will we.
			if (fragment && *fragment)
			{
				GET_FAILED_IF_ERROR(buffer->Append('#'));
				GET_FAILED_IF_ERROR(buffer->Append(fragment));
				fragment = buffer->GetStorage();
			}

			DOMSetString(value, fragment);
		}
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

ES_PutState
JS_Location::SetTheURL(FramesDocument *frames_doc, DocumentReferrer &ref_url, URL &url, ES_Thread *origin_thread, BOOL only_hash)
{
	BOOL is_reload = do_reload && !only_hash;
	BOOL is_replace = do_replace;
	do_reload = FALSE;
	do_replace = FALSE;

	URL &old_url = GetFramesDocument()->GetURL();

	/* If the URLs are identical, pretend it's a reload */
	if (!is_reload && !only_hash && old_url == url)
	{
		const uni_char *rel1, *rel2;
		rel1 = old_url.UniRelName();
		rel2 = url.UniRelName();

		is_reload = (!rel1 && !rel2) || (rel1 && rel2 && uni_str_eq(rel1, rel2));
	}

	if (only_hash)
	{
		BOOL from_onload = FALSE;

		ES_Thread *thread = origin_thread;
		while (thread)
		{
			if (thread->Type() == ES_THREAD_EVENT)
			{
				DOM_Event *event = ((DOM_EventThread *) thread)->GetEvent();
				if (!event->GetSynthetic() && event->GetKnownType() == ONLOAD)
				{
					from_onload = TRUE;
					break;
				}
			}
			thread = thread->GetInterruptedThread();
		}

		OP_BOOLEAN result = frames_doc->GetDocManager()->JumpToRelativePosition(url, from_onload);

		PUT_FAILED_IF_MEMORY_ERROR(result);

		if (result == OpBoolean::IS_TRUE)
			return PUT_SUCCESS;
	}

	if (old_url == url && url.RelName())
		is_reload = FALSE;

	return OpenURL(url, ref_url, is_reload, is_replace, origin_thread) ? PUT_SUCCESS : PUT_NO_MEMORY;
}

DocumentReferrer JS_Location::GetStandardRefURL(FramesDocument *frames_doc, ES_Runtime *origining_runtime)
{
	if (origining_runtime->GetFramesDocument())
		return origining_runtime->GetFramesDocument();

	URL ref_url = frames_doc->GetURL();

	if (!ref_url.IsEmpty())
		return frames_doc;

	ES_Thread *thread = GetCurrentThread(origining_runtime);
	while (thread)
		if (thread->Type() == ES_THREAD_JAVASCRIPT_URL)
			return DocumentReferrer(static_cast<ES_JavascriptURLThread *>(thread)->GetURL());
		else
			thread = thread->GetInterruptedThread();
	return DocumentReferrer();
}

/* virtual */ ES_PutState
JS_Location::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (GetName(property_name, NULL, origining_runtime) != GET_SUCCESS)
		return PUT_FAILED;

	FramesDocument *frames_doc = GetFramesDocument();
	if (!frames_doc)
		return PUT_SUCCESS;

	if (value->type != VALUE_STRING)
		return PUT_NEEDS_STRING;

	const uni_char *value_string = value->value.string;

	while (value_string[0] == ' ')
		++value_string;

	if (property_name == OP_ATOM_href)
		if (value_string[0] == '#')
			property_name = OP_ATOM_hash;
		else if (value_string[0] == '?')
			property_name = OP_ATOM_search;

	URL url;
	DocumentReferrer ref_url(GetStandardRefURL(frames_doc, origining_runtime));
	TempBuffer buffer;

	URL current_url = ref_url.url;
#ifdef SELFTEST
	if (!do_navigation)
		current_url = this->current_url;
#endif // SELFTEST

	switch (property_name)
	{
	case OP_ATOM_href:
	case OP_ATOM_protocol:
	case OP_ATOM_host:
	case OP_ATOM_hostname:
	case OP_ATOM_port:
	case OP_ATOM_pathname:
		BOOL allowed;
		if (OpStatus::IsError(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_ALLOWED_TO_NAVIGATE, static_cast<DOM_Runtime *>(origining_runtime), GetRuntime(), allowed)) ||
			!allowed)
			return PUT_SECURITY_VIOLATION;
	}

	switch (property_name)
	{
	case OP_ATOM_protocol:
	{
		unsigned length = uni_strlen(value_string);
		while (length > 0 && value_string[length - 1] == ':')
			length--;
		if (length > 0)
		{
			const uni_char *current_url_string = current_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
			const uni_char *current_scheme_end = uni_strchr(current_url_string, ':');
			if (!current_scheme_end)
				return PUT_SUCCESS;

			PUT_FAILED_IF_ERROR(buffer.Append(value_string, length));
			PUT_FAILED_IF_ERROR(buffer.Append(current_scheme_end));

			url = GetEncodedURL(origining_runtime->GetFramesDocument(), buffer.GetStorage());

			BOOL allowed;
			if (url.Type() == URL_JAVASCRIPT)
				if (OpStatus::IsError(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, static_cast<DOM_Runtime *>(origining_runtime), GetRuntime(), allowed)) ||
				    !allowed)
					return PUT_SUCCESS;
		}
		break;
	}
	case OP_ATOM_host:
	{
		const uni_char *current_url_string = current_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
		const uni_char *current_scheme_end = uni_strchr(current_url_string, ':');

		// URL must be an "authority-based URL"
		if (current_scheme_end && current_scheme_end[1] == '/' && current_scheme_end[2] == '/')
		{
			OpString hostname;
			PUT_FAILED_IF_ERROR(current_url.GetAttribute(URL::KUniHostName, hostname));
			/* Just bail if the URL doesn't have a hostname after all. */
			if (!hostname.CStr())
				return PUT_SUCCESS;

			uni_char *hostname_start = uni_strstr(current_url_string, hostname.CStr());
			OP_ASSERT(hostname_start);
			uni_char *hostname_end = hostname_start + hostname.Length();

			unsigned short port = current_url.GetAttribute(URL::KServerPort);
			if (port > 0 && *hostname_end == ':')
			{
				hostname_end++;
				while (uni_isdigit(*hostname_end))
					hostname_end++;
			}

			PUT_FAILED_IF_ERROR(buffer.Append(current_url_string, hostname_start - current_url_string));
			PUT_FAILED_IF_ERROR(buffer.Append(value_string));
			PUT_FAILED_IF_ERROR(buffer.Append(hostname_end));

			url = GetEncodedURL(origining_runtime->GetFramesDocument(), buffer.GetStorage());
		}
		break;
	}
	case OP_ATOM_hostname:
	{
		while (*value_string == '/')
			value_string++;

		const uni_char *current_url_string = current_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
		const uni_char *current_scheme_end = uni_strchr(current_url_string, ':');

		// URL must be an "authority-based URL"
		if (*value_string && current_scheme_end && current_scheme_end[1] == '/' && current_scheme_end[2] == '/')
		{
			OpString hostname;
			PUT_FAILED_IF_ERROR(current_url.GetAttribute(URL::KUniHostName, hostname));
			/* Just bail if the URL doesn't have a hostname after all. */
			if (!hostname.CStr())
				return PUT_SUCCESS;

			uni_char *hostname_start = uni_strstr(current_url_string, hostname.CStr());
			OP_ASSERT(hostname_start);
			uni_char *hostname_end = hostname_start + hostname.Length();

			PUT_FAILED_IF_ERROR(buffer.Append(current_url_string, hostname_start - current_url_string));
			PUT_FAILED_IF_ERROR(buffer.Append(value_string));
			PUT_FAILED_IF_ERROR(buffer.Append(hostname_end));

			url = GetEncodedURL(origining_runtime->GetFramesDocument(), buffer.GetStorage());
		}
		break;
	}
	case OP_ATOM_port:
	{
		const uni_char *current_url_string = current_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
		const uni_char *current_scheme_end = uni_strchr(current_url_string, ':');
		// URL must be an "authority-based URL"
		if (current_scheme_end && current_scheme_end[1] == '/' && current_scheme_end[2] == '/')
		{
			while (*value_string == '0')
				value_string++;

			int port = 0;
			if (uni_isdigit(*value_string))
				port = uni_atoi(value_string);

			if (port <= 0 || port > 65535)
				break;

			OpString hostname;
			PUT_FAILED_IF_ERROR(current_url.GetAttribute(URL::KUniHostName, hostname));
			/* Just bail if the URL doesn't have a hostname after all. */
			if (!hostname.CStr())
				return PUT_SUCCESS;

			uni_char *hostname_start = uni_strstr(current_scheme_end, hostname.CStr());
			OP_ASSERT(hostname_start);
			uni_char *hostname_end = hostname_start + hostname.Length();

			PUT_FAILED_IF_ERROR(buffer.Append(current_url_string, hostname_end - current_url_string));
			PUT_FAILED_IF_ERROR(buffer.Append(":"));
			if (*hostname_end == ':')
			{
				hostname_end++;
				while (uni_isdigit(*hostname_end))
					hostname_end++;
			}
			PUT_FAILED_IF_ERROR(buffer.AppendLong(port));
			PUT_FAILED_IF_ERROR(buffer.Append(hostname_end));

			url = GetEncodedURL(origining_runtime->GetFramesDocument(), buffer.GetStorage());
		}
		break;
	}
	case OP_ATOM_href:
	case OP_ATOM_pathname:
	{
		url = GetEncodedURL(origining_runtime->GetFramesDocument(), value_string);

		BOOL allowed;
		// Stricter security for javascript urls. It's possible this check should move into DocumentManager in the future.
		if (url.Type() == URL_JAVASCRIPT)
			if (OpStatus::IsError(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, static_cast<DOM_Runtime *>(origining_runtime), GetRuntime(), allowed)) ||
			    !allowed)
				return PUT_SUCCESS;

		break;
	}
	case OP_ATOM_search:
	{
		const uni_char *current_url_string = current_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
		int current_len;

		const uni_char *current_search_start = uni_strchr(current_url_string, '?');
		if (current_search_start)
			current_len = current_search_start - current_url_string;
		else
			current_len = uni_strlen(current_url_string);

		if (value_string[0] == '?')
			++value_string;

		PUT_FAILED_IF_ERROR(buffer.Expand(current_len + uni_strlen(value_string) + 2));

		OpStatus::Ignore(buffer.Append(current_url_string, current_len)); // buffer is successfully expanded above
		OpStatus::Ignore(buffer.Append("?"));
		OpStatus::Ignore(buffer.Append(value_string));

		url = GetEncodedURL(origining_runtime->GetFramesDocument(), buffer.GetStorage());
		break;
	}
	case OP_ATOM_hash:
		if (value_string[0] == '#')
			++value_string;

		// Strip trailing whitespace
		if (unsigned length = uni_strlen(value_string))
		{
			if (value_string[length - 1] == ' ')
			{
				PUT_FAILED_IF_ERROR(buffer.Append(value_string));

				uni_char *string = buffer.GetStorage();

				while (length > 0 && string[length - 1] == ' ')
					--length;

				string[length] = 0;
				value_string = string;
			}
		}

#ifdef SELFTEST
		url = URL(!do_navigation ? current_url : frames_doc->GetURL(), value_string);
#else
		url = URL(frames_doc->GetURL(), value_string);
#endif // SELFTEST
		break;
	}

	if (url.Type() != URL_NULL_TYPE)
	{
#ifdef GADGET_SUPPORT
		switch (property_name)
		{
		case OP_ATOM_href:
		case OP_ATOM_protocol:
		case OP_ATOM_host:
		case OP_ATOM_hostname:
		case OP_ATOM_port:
		case OP_ATOM_pathname:
		{
			BOOL allowed;
			if (frames_doc->GetWindow()->GetGadget())
				if (OpStatus::IsError(OpSecurityManager::CheckSecurity(OpSecurityManager::GADGET_ALLOWED_TO_NAVIGATE, OpSecurityContext(frames_doc), url, allowed)) || !allowed)
					return PUT_SECURITY_VIOLATION;
		}
		}
#endif // GADGET_SUPPORT
		return SetTheURL(frames_doc, ref_url, url, GetCurrentThread(origining_runtime), property_name == OP_ATOM_hash);
	}
	else
		return PUT_SUCCESS;
}

/* virtual */ ES_PutState
JS_Location::PutName(const uni_char *property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (uni_str_eq(property_name, "valueOf") || uni_str_eq(property_name, "toString"))
		return PUT_SUCCESS;
	else
		return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

BOOL
JS_Location::OpenURL(URL &url, DocumentReferrer ref_url, BOOL is_reload, BOOL is_replace, ES_Thread* origin_thread)
{
	if (fakewindow)
	{
		fakewindow->SetURL(url, ref_url);
		return TRUE;
	}
#ifdef SELFTEST
	if (!do_navigation)
	{
		current_url = url;
		return TRUE;
	}
#endif // SELFTEST

	if (FramesDocument *frames_doc = GetFramesDocument())
	{
		if (frames_doc->IsRestoringFormState(origin_thread))
			/* We really do not want this to submit the form as a side-effect. */
			return TRUE;

		frames_doc->SignalFormChangeSideEffect(origin_thread);

		EnteredByUser entered_by_user;

		if (url.Type() == URL_JAVASCRIPT)
		{
			/* ESOpenURL will do nothing when a javascript URL is reloaded.
		       If is_replace is TRUE however, this was a call to location.replace,
		       and is_reload is set there because that works a little better, the
		       user/script didn't actually request a reload. */
			if (is_replace)
				is_reload = FALSE;

			/* Note: Using WasEnteredByUser here to bypass incompetent security
			   check in ESOpenURL; we've already performed a more competent
			   security check and concluded the operation is permissable. */
			entered_by_user = WasEnteredByUser;
		}
		else
			entered_by_user = NotEnteredByUser;

		// Sandboxed documents may not navigate anywhere.
		if (ref_url.origin && ref_url.origin->IsUniqueOrigin())
			return TRUE;

		if (JS_Window::IsUnrequestedPopup(origin_thread))
			switch (url.Type())
			{
			case URL_MAILTO:
				return TRUE;
			}

		ES_ThreadInfo info = origin_thread->GetOriginInfo();
		BOOL user_initiated = info.is_user_requested && !info.has_opened_url;
		origin_thread->SetHasOpenedURL(); // It has now consumed its chance to be bad

		/* Automatically replace if an inline script or window.onload event handler
		 * redirects via location.href/location.assign(), but only if it comes from the same thread
		 * as the origin, to prevent things like an iframe redirecting the parent and losing the
		 * original history position. */
		if (!is_reload && !is_replace && origin_thread == frames_doc->GetESScheduler()->GetCurrentThread())
		{
			// Reuse the current history position if this is done during the load phase.
			if (DOM_Utils::IsInlineScriptOrWindowOnLoad(origin_thread))
				is_replace = TRUE;
		}

		DocumentManager::OpenURLOptions options;
		options.user_initiated = user_initiated;
		options.entered_by_user = entered_by_user;
		options.is_walking_in_history = FALSE;
		options.origin_thread = origin_thread;
		if (OpStatus::IsMemoryError(frames_doc->ESOpenURL(url, ref_url, TRUE, is_reload, is_replace, options)))
			return FALSE;
	}

	return TRUE;
}

/* virtual */ BOOL
JS_Location::AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op)
{
	if (uni_str_eq(property_name, "href") ||
	    uni_str_eq(property_name, "toString") ||
	    uni_str_eq(property_name, "valueOf"))
		return FALSE;
	else
		return TRUE;
}

/* virtual */ void
JS_Location::GCTrace()
{
	GCMark(fakewindow);
}

void JS_Location::DoReload()
{
	do_reload = TRUE;
}

void JS_Location::DoReplace()
{
	do_replace = TRUE;
}

#ifdef SELFTEST
void
JS_Location::DisableNavigation()
{
	OP_ASSERT(do_navigation);
	do_navigation = FALSE;

	if (fakewindow)
		current_url = fakewindow->GetURL();
	else if (FramesDocument *frames_doc = GetFramesDocument())
		current_url = frames_doc->GetURL();
}
#endif // SELFTEST

/* static */ void
JS_Location::PreparePrototype(ES_Object *object, DOM_Runtime *runtime)
{
	DOM_Runtime::SetIsCrossDomainAccessible(object);
}

int
JS_Location::reload(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(location, DOM_TYPE_LOCATION, JS_Location);
	DOM_CHECK_ARGUMENTS("|b");

	if (!location->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	FramesDocument* frames_doc = location->GetFramesDocument();
	if (!frames_doc)
		return ES_FAILED;

	if (JS_Window::IsUnrequestedThread(GetCurrentThread(origining_runtime)))
		if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableClientRefresh, DOM_EnvironmentImpl::GetHostName(origining_runtime->GetFramesDocument())) ||
			(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ClientRefreshToSame) && frames_doc->DocumentIsReloaded()))
			return ES_FAILED;

	if (argc > 0 && argv[0].value.boolean)
		frames_doc->GetDocManager()->Reload(NotEnteredByUser, FALSE, FALSE, FALSE);
	else
		if (!location->OpenURL(frames_doc->GetURL(), frames_doc->GetRefURL(), TRUE, FALSE, GetCurrentThread(origining_runtime)))
			return ES_NO_MEMORY;

	return ES_FAILED;
}

int
JS_Location::replaceOrAssign(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	JS_Location *location;

	if (data == 0 || data == 1)
		DOM_THIS_OBJECT_EXISTING(location, DOM_TYPE_LOCATION, JS_Location);
	else
	{
		DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);

		ES_Value value;
		if (window->GetName(OP_ATOM_location, &value, origining_runtime) == GET_SUCCESS)
			location = DOM_VALUE2OBJECT(value, JS_Location);
		else
			return ES_FAILED;
	}

	DOM_CHECK_ARGUMENTS("s");

	if (data == 0)
		location->DoReplace();

	ES_PutState status = location->PutName(OP_ATOM_href, &argv[0], origining_runtime);

	if (status == PUT_NO_MEMORY)
		return ES_NO_MEMORY;
	else if (status == PUT_SECURITY_VIOLATION)
		return ES_EXCEPT_SECURITY;
	else
		return ES_FAILED;
}

int
JS_Location::toString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(location, DOM_TYPE_LOCATION, JS_Location);

	if (!location->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	return ConvertGetNameToCall(location->GetName(OP_ATOM_href, return_value, origining_runtime), return_value);
}

#ifdef DOM_WEBWORKERS_SUPPORT

/* virtual */ ES_PutState
JS_Location_Worker::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_href:
	case OP_ATOM_protocol:
	case OP_ATOM_host:
	case OP_ATOM_hostname:
	case OP_ATOM_port:
	case OP_ATOM_pathname:
	case OP_ATOM_search:
	case OP_ATOM_hash:
		return PUT_READ_ONLY;
	default:
		return PUT_FAILED;
	}
}

#endif // DOM_WEBWORKERS_SUPPORT

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(JS_Location)
	DOM_FUNCTIONS_FUNCTION(JS_Location, JS_Location::reload, "reload", "b-")
	DOM_FUNCTIONS_FUNCTION(JS_Location, JS_Location::toString, "toString", 0)
	DOM_FUNCTIONS_FUNCTION(JS_Location, JS_Location::toString, "valueOf", 0)
DOM_FUNCTIONS_END(JS_Location)

DOM_FUNCTIONS_WITH_DATA_START(JS_Location)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Location, JS_Location::replaceOrAssign, 0, "!replace", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_Location, JS_Location::replaceOrAssign, 1, "!assign", "s-")
DOM_FUNCTIONS_WITH_DATA_END(JS_Location)

#ifdef DOM_WEBWORKERS_SUPPORT
DOM_FUNCTIONS_START(JS_Location_Worker)
	DOM_FUNCTIONS_FUNCTION(JS_Location_Worker, JS_Location::toString, "toString", 0)
	DOM_FUNCTIONS_FUNCTION(JS_Location_Worker, JS_Location::toString, "valueOf", 0)
DOM_FUNCTIONS_END(JS_Location_Worker)
#endif // DOM_WEBWORKERS_SUPPORT
