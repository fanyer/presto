/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/doc/loadinlineelm.h"
#include "modules/doc/frm_doc.h"
#include "modules/logdoc/helistelm.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/media/mediatrack.h"
#include "modules/security_manager/include/security_manager.h"

#ifdef CORS_SUPPORT
class LoadInlineElm_SecurityCheckCallback : public OpSecurityCheckCallback
{
	BOOL is_allowed;
	BOOL is_finished;
	LoadInlineElm *lie;
public:
	LoadInlineElm_SecurityCheckCallback(LoadInlineElm *lie) : is_allowed(FALSE), is_finished(FALSE), lie(lie) {}
	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type = PERSISTENCE_NONE)
	{
		is_finished = TRUE;
		is_allowed = allowed;
		if (lie)
			lie->OnSecurityCheckResult(allowed);
		else
			OP_DELETE(this);
	}
	virtual void OnSecurityCheckError(OP_STATUS error)
	{
		is_finished = TRUE;
		if (lie)
			lie->OnSecurityCheckResult(FALSE);
		else
			OP_DELETE(this);
	}
	BOOL IsAllowed() { return is_allowed; }
	BOOL IsFinished() { return is_finished; }
	void Cancel() { lie = NULL; }
};
#endif // CORS_SUPPORT

LoadInlineElm::LoadInlineElm(FramesDocument* doc, const URL& curl)
	: url(curl),
	  url_in_use(url),
	  redirected_url_in_use(),
	  helm_list(this),
	  externallisteners(NULL),
	  provider(NULL),
	  doc(doc),
	  last_loaded_size(0),
	  last_total_size(0)
#ifdef CORS_SUPPORT
    , cross_origin_request(NULL)
	, security_callback(NULL)
#endif // CORS_SUPPORT
{
	info_init = 0;
	info.is_current_doc = doc->IsCurrentDoc();
	info.delay_load = TRUE;
}

LoadInlineElm::~LoadInlineElm()
{
	helm_list.Clear();
	if (externallisteners)
	{
		externallisteners->RemoveAll();
		OP_DELETE(externallisteners);
	}
	if (provider)
	{
		if (info.is_current_doc)
			provider->DecLoadingCounter();
		provider->DecRef();
	}
#ifdef CORS_SUPPORT
	OP_DELETE(cross_origin_request);
	if (security_callback && !security_callback->IsFinished())
		security_callback->Cancel();
	else
		OP_DELETE(security_callback);
#endif // CORS_SUPPORT
}

void LoadInlineElm::CheckDelayLoadState()
{
	HEListElm *helm = helm_list.First();
	BOOL helms_delay_load = TRUE;
	if (helm != NULL)
	{
		// Flag is FALSE if all HEListElms have the flag unset.
		helms_delay_load = FALSE;
		while (helm)
		{
			if (helm->GetDelayLoad())
			{
				helms_delay_load = TRUE;
				break;
			}
			helm = helm->Suc();
		}
	}

	if (static_cast<BOOL>(info.delay_load) != helms_delay_load)
	{
		info.delay_load = helms_delay_load;
		if (helms_delay_load)
		{
			if (GetLoading())
				doc->image_info.total_count += 1;
		}
		else
		{
			if (GetLoading())
				doc->image_info.total_count -= 1;
		}
	}
}

void LoadInlineElm::SetUsed(BOOL used)
{
	if (used)
	{
		if (!(url_in_use.GetURL() == url))
			url_in_use.SetURL(url);
	}
	else
	{
#ifndef RAMCACHE_ONLY
		if (url_in_use.GetURL().Type() != URL_FILE)
#endif // RAMCACHE_ONLY
			url_in_use.UnsetURL();
	}
}

void LoadInlineElm::UpdateDocumentIsCurrent()
{
	if (!!info.is_current_doc == !doc->IsCurrentDoc())
	{
		info.is_current_doc = !!doc->IsCurrentDoc();

		if (provider)
			if (info.is_current_doc)
				provider->IncLoadingCounter();
			else
				provider->DecLoadingCounter();
	}
}

void
LoadInlineElm::ResetImageIfNeeded()
{
	if (info.need_reset_image)
	{
		info.need_reset_image = 0;
		UrlImageContentProvider::ResetImageFromUrl(url);
	}
}

UINT32
LoadInlineElm::GetImageStorageId()
{
	return provider ? provider->GetStorageId() : 0;
}

BOOL
LoadInlineElm::IsElementAdded(HTML_Element* element, InlineResourceType inline_type)
{
	ElementRef *iter = element->GetFirstReference();
	while (iter)
	{
		if (iter->IsA(ElementRef::HELISTELM))
		{
			HEListElm *iter_elm = static_cast<HEListElm*>(iter);
			if (iter_elm->GetInlineType() == inline_type
				&& iter_elm->IsActive()
				&& helm_list.HasLink(iter_elm))
				return TRUE;
		}

		iter = iter->NextRef();
	}

	return FALSE;
}

BOOL
LoadInlineElm::RemoveHEListElm(HTML_Element *element, InlineResourceType inline_type)
{
	BOOL found_something = FALSE;
	for (HEListElm *helm = GetFirstHEListElm(); helm;)
	{
		HEListElm *next = helm->Suc();
		if (helm->HElm() == element && helm->GetInlineType() == inline_type)
		{
			OP_DELETE(helm);
			found_something = TRUE;
		}
		helm = next;
	}

	return found_something;
}

BOOL LoadInlineElm::IsLoadingUrl(URL_ID id)
{
	return url.Id() == id || info.loading && url_reserved.GetURL().Id() == id;
}

void LoadInlineElm::UrlMoved(FramesDocument *document, URL_ID target_id)
{
	URL target_url = url;
	while (!target_url.IsEmpty() && target_url.Id() != target_id)
		target_url = target_url.GetAttribute(URL::KMovedToURL,FALSE);

	if (!target_url.IsEmpty())
	{
		redirected_url_in_use = target_url;

#ifdef CORS_SUPPORT
		if (cross_origin_request)
		{
			OP_ASSERT(!security_callback);
			security_callback = OP_NEW(LoadInlineElm_SecurityCheckCallback, (this));
			if (!security_callback)
			{
				info.access_forbidden = TRUE;
				return;
			}
			cross_origin_request->SetInRedirect();
			OP_STATUS status = OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, OpSecurityContext(url, cross_origin_request), OpSecurityContext(target_url), security_callback);
			if (OpStatus::IsError(status))
			{
				info.access_forbidden = TRUE;
				OP_DELETE(security_callback);
				security_callback = NULL;
			}
			RETURN_VOID_IF_ERROR(status);
		}
#endif // CORS_SUPPORT

		if (info.loading)
			OpStatus::Ignore(url_reserved.SetURL(target_url));

#ifdef MEDIA_HTML_SUPPORT
		for (HEListElm *helm = GetFirstHEListElm(); helm; helm = helm->Suc())
			if (helm->GetInlineType() == TRACK_INLINE)
			{
				BOOL same_origin = FALSE;
				RAISE_IF_MEMORY_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD,
																		OpSecurityContext(document),
																		OpSecurityContext(target_url),
																		same_origin));
				if (!same_origin)
				{
					// FIXME: A same-origin resource redirected to a
					// cross-origin resource. Per spec we should make
					// new a CORS-enabled request, but the new request
					// has already been started by URL. Instead, set
					// access_forbidden to stop the load instead.
					info.access_forbidden = TRUE;
				}
				if (TrackElement* track_element = helm->GetElm()->GetTrackElement())
					track_element->LoadingRedirected(helm);
			}
#endif // MEDIA_HTML_SUPPORT

		if (externallisteners)
		{
			Head listeners;
			Link temporary;

			listeners.Append(externallisteners);
			temporary.Into(externallisteners);

			while (ExternalInlineListener *listener = static_cast<ExternalInlineListener *>(listeners.First()))
			{
				listener->Out();
				listener->Into(externallisteners);
				listener->LoadingRedirected(document, url, target_url);
			}

			temporary.Out();

			if (externallisteners->Empty())
			{
				OP_DELETE(externallisteners);
				externallisteners = NULL;
			}
		}
	}
}

void LoadInlineElm::ResetRedirection()
{
	redirected_url_in_use.UnsetURL();
}

void LoadInlineElm::SetLoading(BOOL val)
{
	if (val)
	{
		info.loading = 1;
		OpStatus::Ignore(url_reserved.SetURL(url));
	}
	else
	{
		info.loading = 0;
		url_reserved.UnsetURL();
	}
}

HEListElm *
LoadInlineElm::AddHEListElm(HTML_Element *element, InlineResourceType inline_type, FramesDocument *doc, URL *img_url, BOOL update_url)
{
	OP_ASSERT(element);

	HEListElm *hele = OP_NEW(HEListElm, (element, inline_type, doc));
	if (hele)
	{
		if (update_url)
		{
			if (!hele->UpdateImageUrl(img_url))
			{
				OP_DELETE(hele);
				hele = NULL;
			}
		}
		if (hele)
			Add(hele);
	}

	return hele;
}

void LoadInlineElm::Add(HEListElm* helm)
{
	helm->Into(&helm_list);
	if (helm->GetDelayLoad() != static_cast<BOOL>(info.delay_load))
		CheckDelayLoadState();
}

void LoadInlineElm::Remove(HEListElm* helm)
{
	OP_ASSERT(helm);
	OP_ASSERT(helm_list.HasLink(helm));
	OP_ASSERT(helm->IsInList(this));

	if (GetLoading() && helm->GetInlineType() == CSS_INLINE)
		doc->DecWaitForStyles();

	helm->Out();
	if (!info.delay_load)
		CheckDelayLoadState();
}

BOOL LoadInlineElm::HasInlineType(InlineResourceType inline_type)
{
	HEListElm *helm = helm_list.First();

	while (helm)
	{
		if (helm->GetInlineType() == inline_type)
			return TRUE;

		helm = helm->Suc();
	}

	return FALSE;
}

OpFileLength
LoadInlineElm::GetLoadedSize()
{
	return last_loaded_size = url.GetContentLoaded();
}

OpFileLength
LoadInlineElm::GetTotalSize()
{
	return last_total_size = url.GetContentSize();
}

OP_STATUS
LoadInlineElm::AddExternalListener(ExternalInlineListener *listener)
{
	if (!externallisteners)
		if (!(externallisteners = OP_NEW(List<ExternalInlineListener>, ())))
			return OpStatus::ERR_NO_MEMORY;

	listener->Out();
	listener->Into(externallisteners);
	return OpStatus::OK;
}

BOOL
LoadInlineElm::RemoveExternalListener(ExternalInlineListener *listener)
{
	if (externallisteners && externallisteners->HasLink(listener))
	{
		listener->Out();
		if (externallisteners->Empty())
		{
			OP_DELETE(externallisteners);
			externallisteners = NULL;
		}
		return TRUE;
	}
	else
		return FALSE;
}

void
LoadInlineElm::SetUrlImageContentProvider(UrlImageContentProvider *new_provider)
{
	if (provider != new_provider)
	{
		if (provider)
		{
			if (info.is_current_doc)
				provider->DecLoadingCounter();

			provider->DecRef();
		}
		provider = new_provider;
		provider->IncRef();

		if (info.is_current_doc)
			provider->IncLoadingCounter();
	}
}

void
LoadInlineElm::CheckAccessForbidden()
{
	OpSecurityState sec_state;
	BOOL access_allowed = !info.access_forbidden;

	// Used mostly for the cross network checks, so using any inline type will do.
	RETURN_VOID_IF_ERROR(g_secman_instance->CheckSecurity(OpSecurityManager::DOC_INLINE_LOAD,
														  OpSecurityContext(doc),
														  OpSecurityContext(*GetRedirectedUrl(), GENERIC_INLINE, FALSE),
														  access_allowed,
														  sec_state));

	OP_ASSERT(!sec_state.suspended || !"This method should only be called after both the document's and the inline's urls are loaded.");
	if (!sec_state.suspended)
		info.access_forbidden = !access_allowed;
}

BOOL
LoadInlineElm::HasOnlyImageInlines()
{
	HEListElm *helm = helm_list.First();

	while (helm)
	{
		if (!HEListElm::IsImageInline(helm->GetInlineType()))
			return FALSE;

		helm = helm->Suc();
	}

	return TRUE;
}

BOOL
LoadInlineElm::HasOnlyInlineType(InlineResourceType inline_type)
{
	HEListElm *helm = helm_list.First();

	if (helm == NULL)
		return FALSE;

	while (helm)
	{
		if (helm->GetInlineType() != inline_type)
			return FALSE;

		helm = helm->Suc();
	}

	return TRUE;
}

BOOL
LoadInlineElm::IsAffectedBy(URL_ID id)
{
	URL chain(url);

	while (!chain.IsEmpty())
		if (chain.Id() == id)
			return TRUE;
		else
			chain = chain.GetAttribute(URL::KMovedToURL, FALSE);

	return FALSE;
}

void
LoadInlineElm::SetSentHandleInlineDataAsyncMsg(BOOL val)
{
	if (val != static_cast<BOOL>(info.sent_handle_inline_data_async_msg))
	{
		if (val)
			OpStatus::Ignore(url_reserved_for_data_async_msg.SetURL(url));
		else
			url_reserved_for_data_async_msg.UnsetURL();

		info.sent_handle_inline_data_async_msg = val;
	}
}

#ifdef CORS_SUPPORT
void
LoadInlineElm::OnSecurityCheckResult(BOOL allowed)
{
	if (!allowed)
		info.access_forbidden = TRUE;
	OP_DELETE(security_callback);
	security_callback = NULL;
}

OP_STATUS
LoadInlineElm::SetCrossOriginRequest(const URL &origin_url, URL *img_url, HTML_Element *html_elm)
{
	OP_ASSERT(!cross_origin_request);
	const uni_char *cors_access = html_elm->GetStringAttr(Markup::HA_CROSSORIGIN);
	BOOL with_cred;
	if (uni_stri_eq(cors_access, "use-credentials"))
		with_cred = TRUE;
	else
		with_cred = FALSE;

	const uni_char *method = UNI_L("GET");
	RETURN_IF_ERROR(OpCrossOrigin_Request::Make(cross_origin_request, origin_url, *img_url, method, with_cred, FALSE));

	security_callback = OP_NEW(LoadInlineElm_SecurityCheckCallback, (this));
	RETURN_OOM_IF_NULL(security_callback);
	OP_STATUS status = OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, OpSecurityContext(origin_url, cross_origin_request), OpSecurityContext(*img_url), security_callback);
	OP_ASSERT(!security_callback);
	if (OpStatus::IsError(status))
	{
		info.access_forbidden = TRUE;
		OP_DELETE(security_callback);
		security_callback = NULL;
	}

	g_url_api->MakeUnique(*img_url);
	info.is_cors_request = TRUE;

	return status;
}

OP_STATUS
LoadInlineElm::CheckCrossOriginAccess(BOOL &allowed)
{
	allowed = FALSE;
	OP_ASSERT(cross_origin_request);
	OpSecurityContext source_context(cross_origin_request->GetURL(), cross_origin_request);
	URL final_url = GetUrl()->GetAttribute(URL::KMovedToURL, TRUE);
	OpSecurityContext target_context(final_url.IsEmpty() ? *GetUrl() : final_url);
	/* This is guaranteed to be a sync security check. */
	OP_STATUS status = OpSecurityManager::CheckSecurity(OpSecurityManager::CORS_RESOURCE_ACCESS, source_context, target_context, allowed);
	OP_DELETE(cross_origin_request);
	cross_origin_request = NULL;
	RETURN_IF_ERROR(status);
	if (allowed)
		for (HEListElm *helm = GetFirstHEListElm(); helm; helm = helm->Suc())
			helm->SetIsCrossOriginAllowed();

	return OpStatus::OK;
}

#endif // CORS_SUPPORT
