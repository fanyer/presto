/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */


#include "core/pch.h"

#include "modules/url/url_docload.h"
#include "modules/url/url_tools.h"
#include "modules/locale/locale-enum.h"

URL_DocumentLoader::URL_DocumentLoader()
: timeout_max(0), timeout_idle(0), timeout_minidle(0), timeout_startreq(FALSE),timeout_startconn(FALSE)
{
}

OP_STATUS URL_DocumentLoader::Construct(MessageHandler *mh)
{
	if(mh == NULL)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(document_mh == NULL);

	document_mh = mh;

	return document_mh->SetCallBack(this, MSG_URL_DOCHANDLER_FINISHED, (UINTPTR) this);
}

URL_DocumentLoader::~URL_DocumentLoader()
{
	// Note: The PublicSuffix_Updater inherits (via XML_Updater and
	// URL_Updater) from this class and in some situations
	// PublicSuffix_Updater::SetFinished() may add additional message
	// callbacks by calling DNS_RegistryCheck_Handler::SetCallbacks(),
	// so ensure to reset all callbacks:
	if (document_mh != NULL)
		document_mh->UnsetCallBacks(this);

	loading_urls_refs.RemoveAll();
}

OP_STATUS URL_DocumentLoader::LoadDocument(URL &url, 
										   const URL& referer_url, const URL_LoadPolicy &loadpolicy, 
										   URL_DocumentHandler *handler, BOOL ocsp_crl_url)
{
	if(url.IsEmpty())
		return OpStatus::OK; // Ignore attempts to load empty URLs

	if(document_mh == NULL)
	{
		OP_ASSERT(!"Construct not called with a message handler; using main message handler, and ignoring OOM when constructing!");
		OpStatus::Ignore(Construct(g_main_message_handler));
	}

	HandlingURL *item = loading_urls.First();
	
	while(item)
	{
		HandlingURL *hndlr = item;
		item = item->Suc();

		if(hndlr->url == url && hndlr->handler == handler && !hndlr->finished && hndlr->loader.GetURL().IsValid())
			return OpStatus::OK; // double load. Ignore
	}

	LoadingURLRef *url_item = loading_urls_refs.First();
	while(url_item)
	{
		if(url_item->url == url)
			break;
		url_item = (LoadingURLRef *)url_item->Suc();
	}
	OpAutoPtr<LoadingURLRef> url_item_2(NULL);
	if(!url_item)
	{
		url_item_2.reset(OP_NEW(LoadingURLRef, (url, document_mh)));
		if(url_item_2.get() == NULL)
		{
			OpAutoPtr<URL_DataDescriptor> temp_desc_ptr;
			if (handler)
				handler->OnURLLoadingFailed(url, Str::SI_ERR_OUT_OF_MEMORY, temp_desc_ptr);

			return OpStatus::ERR_NO_MEMORY;
		}
		url_item = url_item_2.get();
	}

	OpAutoPtr<HandlingURL> new_item(OP_NEW(HandlingURL,(url_item, document_mh, (handler ? handler : this), (UINTPTR) this)));
	if(new_item.get() == NULL)
	{
		OpAutoPtr<URL_DataDescriptor> temp_desc_ptr;
		if (handler)
			handler->OnURLLoadingFailed(url, Str::SI_ERR_OUT_OF_MEMORY, temp_desc_ptr);

		return OpStatus::ERR_NO_MEMORY;
	}
	if(url_item_2.get() != NULL)
	{
		url_item_2.release();
		url_item->Into(&loading_urls_refs);
	}

	if(timeout_max)
		url.SetAttribute(URL::KTimeoutMaximum, timeout_max);

	if(timeout_idle)
		url.SetAttribute(URL::KTimeoutPollIdle, timeout_idle);

	if(timeout_minidle)
		url.SetAttribute(URL::KTimeoutMinimumBeforeIdleCheck, timeout_minidle);

	if(timeout_startreq)
		url.SetAttribute(URL::KTimeoutStartsAtRequestSend, timeout_startreq);

	if(timeout_startconn)
		url.SetAttribute(URL::KTimeoutStartsAtConnectionStart, timeout_startreq);


	RETURN_IF_ERROR(new_item->LoadDocument(referer_url, loadpolicy));

	new_item.release()->Into(&loading_urls);

	return OpStatus::OK;
}

OP_STATUS URL_DocumentLoader::LoadDocument(const OpStringC8 &url, URL &base_url, 
										   const URL& referer_url, const URL_LoadPolicy &loadpolicy,
										   URL &loading_url, BOOL unique, 
										   URL_DocumentHandler *handler)
{
	OP_ASSERT(base_url.IsValid());

	URL url_to_load = g_url_api->GetURL(base_url, url, unique);

	if(url_to_load.IsEmpty())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	loading_url = url_to_load;

	return LoadDocument(url_to_load, referer_url, loadpolicy, handler);
}

OP_STATUS URL_DocumentLoader::LoadDocument(const OpStringC  &url, URL &base_url, 
										   const URL& referer_url, const URL_LoadPolicy &loadpolicy, 
										   URL &loading_url, BOOL unique, 
										   URL_DocumentHandler *handler)
{
	OP_ASSERT(base_url.IsValid());

	URL url_to_load = g_url_api->GetURL(base_url, url, unique);

	if(url_to_load.IsEmpty())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	loading_url = url_to_load;

	return LoadDocument(url_to_load, referer_url, loadpolicy, handler);
}

void URL_DocumentLoader::StopLoading(URL &url)
{
	if(loading_urls.Empty())
		return;

	HandlingURL *item = loading_urls.First();
	URL finished_url;
	
	while(item)
	{
		HandlingURL *handler = item;
		item = item->Suc();

		if(handler->url == url)
		{
			handler->Out();
			finished_url = url;

			OP_DELETE(handler);
		}
	}

	if(finished_url.IsValid())
		OnURLLoadingFinished(finished_url);

	if(loading_urls.Empty())
		OnAllDocumentsFinished();
}

void URL_DocumentLoader::StopLoadingAll()
{
	if(loading_urls.Empty())
		return;
	
	HandlingURL *item;

	while((item = loading_urls.First()) != NULL)
	{
		HandlingURL *handler = item;

		URL finished_url = handler->url;;

		handler->Out();
		OP_DELETE(handler);

		item = loading_urls.First();
	
		while(item)
		{
			handler = item;
			item = item->Suc();

			if(handler->url == finished_url)
			{
				handler->Out();
				OP_DELETE(handler);
			}
		}

		OnURLLoadingFinished(finished_url);
	}

	if(loading_urls.Empty())
		OnAllDocumentsFinished();
}

void URL_DocumentLoader::TwoWayPointerActionL(TwoWayPointer_Target *action_source, uint32 action_val)
{
	OP_ASSERT(action_source == document_mh.get());

	URL_DocumentLoader::HandlingURL *item;

	while((item = loading_urls.First()) != NULL)
	{
		item->Out();

		if(item->handler != NULL)
		{
			item->handler->OnURLLoadingStopped(item->url, item->stored_desc);
		}

		OnURLLoadingFinished(item->url);

		OP_DELETE(item);
	}

	OnAllDocumentsFinished();
}

void URL_DocumentLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_URL_DOCHANDLER_FINISHED:
		{
			OP_ASSERT((UINTPTR) par1 == (UINTPTR) this);

			HandlingURL *item= loading_urls.First();
			while(item)
			{
				HandlingURL *handler = item;
				item = item->Suc();

				if(handler->url.Id() == (URL_ID)par2 && handler->finished)
				{
					handler->Out();

					OnURLLoadingFinished(handler->url);

					OP_DELETE(handler);
				}
			}

			if(loading_urls.Empty())
				OnAllDocumentsFinished();
			break;
		}
	}
}

void URL_DocumentLoader::OnURLLoadingFinished(URL &url)
{
	// Default. NoOp
}

void URL_DocumentLoader::OnAllDocumentsFinished()
{
	// Default. NoOp
}

static const OpMessage docload_loading_messages[] =
{
    MSG_HEADER_LOADED,
    MSG_URL_DATA_LOADED,
	MSG_MULTIPART_RELOAD,
	MSG_URL_LOADING_FAILED,
	MSG_URL_MOVED,
	MSG_FORCE_DATA_LOADED
};

URL_DocumentLoader::HandlingURL::HandlingURL(LoadingURLRef *a_url, MessageHandler *mh, URL_DocumentHandler *hndlr, MH_PARAM_1 ownr_id)
: url(a_url->url), url_handler(a_url), loader(a_url->url),  // Assume a_url is non-NULL
	document_mh(mh), handler(this, hndlr), 
	header_loaded(FALSE), 
	owner_id(ownr_id),
	finished(FALSE),
	sent_poll_message(FALSE),
	got_data_loaded_message(FALSE)
{
}

URL_DocumentLoader::HandlingURL::~HandlingURL()
{
	// Automatically stops loading when there are no references
	url_handler = NULL;

	if(document_mh != NULL)
		document_mh->UnsetCallBacks(this);
}

void URL_DocumentLoader::HandlingURL::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(url.Id() == (URL_ID) par1);


	BOOL continue_loading = TRUE;
	BOOL stop_loading_when_ending =TRUE;

	switch(msg)
	{
	case MSG_HEADER_LOADED:
		if(!header_loaded)
		{
			stored_desc.reset();
			continue_loading = handler->OnURLHeaderLoaded(url);
		}
		header_loaded = TRUE;
		break;
	case MSG_FORCE_DATA_LOADED:
		if(got_data_loaded_message)
		{
			got_data_loaded_message = FALSE;
			document_mh->PostDelayedMessage(MSG_FORCE_DATA_LOADED, url.Id(),0, 100);
			sent_poll_message = TRUE;
			return;
		}
		sent_poll_message = FALSE;
		// Fall through to MSG_URL_DATA_LOADED
	case MSG_URL_DATA_LOADED:
		{
			BOOL is_finished = (loader->GetAttribute(URL::KLoadStatus) != URL_LOADING);
			continue_loading = handler->OnURLDataLoaded(url, is_finished, stored_desc);

			if(is_finished)
			{
				if(stored_desc.get() == NULL)
				{
					continue_loading = FALSE;
					stop_loading_when_ending = FALSE;
				}
				else
				{
					got_data_loaded_message = (msg == MSG_URL_DATA_LOADED);
					if(!sent_poll_message)
					{
						document_mh->PostDelayedMessage(MSG_FORCE_DATA_LOADED, url.Id(),0, 100);
						sent_poll_message = TRUE;
					}
				}
			}
			break;
		}
	case MSG_MULTIPART_RELOAD:
		if(par2)
		{
			// restart of same resource
			BOOL restart_processing = FALSE;
			continue_loading = handler->OnURLRestartSuggested(url, restart_processing, stored_desc);
			if(restart_processing)
				header_loaded = FALSE;
		}
		else
		{
			continue_loading = handler->OnURLNewBodyPart(url, stored_desc);
			header_loaded = FALSE;
		}
		break;
	case MSG_URL_LOADING_FAILED:
		handler->OnURLLoadingFailed(url, 
#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
			Str::LocaleString(par2),
#else
			ConvertUrlStatusToLocaleString(par2),
#endif
			stored_desc);
		continue_loading = FALSE;
		stop_loading_when_ending = FALSE;
		break;
	case MSG_URL_MOVED:
		{
			URL redirect_target = url.GetAttribute(URL::KMovedToURL);
#if MAX_PLUGIN_CACHE_LEN>0
			if(url.GetAttribute(URL::KUseStreamCache))
				redirect_target.SetAttribute(URL::KUseStreamCache, 1);
#endif
			continue_loading = handler->OnURLRedirected(url, redirect_target);
			if(continue_loading)
			{
				OP_STATUS op_err = document_mh->SetCallBackList(this, redirect_target.Id(), docload_loading_messages, ARRAY_SIZE(docload_loading_messages));
				if(OpStatus::IsError(op_err))
				{
					handler->OnURLLoadingFailed(url, Str::SI_ERR_COMM_INTERNAL_ERROR, stored_desc);
					continue_loading = FALSE;
					break;
				}
				document_mh->RemoveCallBacks(this, url.Id());
				
				loader.SetURL(redirect_target);
				url = redirect_target;
			}
		}
		break;
	}

	if(!continue_loading)
	{
		finished = TRUE;
		if (document_mh)
		{
			document_mh->UnsetCallBacks(this);
			document_mh->PostMessage(MSG_URL_DOCHANDLER_FINISHED, owner_id, url.Id());
		}

		if(stop_loading_when_ending && (loader->GetAttribute(URL::KLoadStatus) == URL_LOADING))
		{
			// Automatically stops loading when there are no references
			url_handler = NULL;
			loader.UnsetURL();
			if (handler)
				handler->OnURLLoadingStopped(url, stored_desc);
		}
	}
}

OP_STATUS URL_DocumentLoader::HandlingURL::LoadDocument(const URL& referer_url, const URL_LoadPolicy &loadpolicy)
{
	if(url.IsEmpty() || !(loader.GetURL() == url) || document_mh == NULL || handler == NULL)
	{
		if(handler)
			handler->OnURLLoadingFailed(url, Str::SI_ERR_COMM_INTERNAL_ERROR, stored_desc);

		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}

	header_loaded = finished = FALSE;

	CommState	state = loader->LoadDocument(document_mh, referer_url, loadpolicy);

	OP_STATUS op_err = (state == COMM_LOADING ? OpStatus::OK : OpStatus::ERR_NO_SUCH_RESOURCE);

	if(OpStatus::IsSuccess(op_err))
	{
		op_err = document_mh->SetCallBackList(this, url.Id(), docload_loading_messages, ARRAY_SIZE(docload_loading_messages));

		if(!handler->OnURLLoadingStarted(url))
		{
			loader->StopLoading(document_mh);
			loader.UnsetURL();
			handler->OnURLLoadingStopped(url, stored_desc);
		}
	}
	else
		handler->OnURLLoadingFailed(url, Str::SI_ERR_COMM_INTERNAL_ERROR, stored_desc);

	return op_err;
}

void URL_DocumentLoader::HandlingURL::TwoWayPointerActionL(TwoWayPointer_Target *action_source, uint32 action_val)
{
	OP_ASSERT(action_source == handler.get());
	
	if(action_val == TwoWayPointer_Target::TWOWAYPOINTER_ACTION_DESTROY)
	{
		// Automatically stops loading when there are no references
		url_handler = NULL;
		// Not necessary to call back; target already destroyed; would crash since only the base class still exists
		loader.UnsetURL(); // removing lock on URL

		// Inform owner; extra benefit: avoids callstack loopback conflict
		if(document_mh != NULL)
		{
			document_mh->UnsetCallBacks(this);
			document_mh->PostMessage(MSG_URL_DOCHANDLER_FINISHED, owner_id, url.Id());
		}

		finished = TRUE;
	}
}
