/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIC_LIMIT_PROGRESS_FREQUENCY

#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/url/url_man.h"

LoadingListenerProxy::LoadingListenerProxy(OpWindowCommander* commander)
	: commander(commander),
	  m_send_start(FALSE),
	  m_start_sent(FALSE),
	  m_send_stop(FALSE)
{
	has_pending_progress = 0;
	progress_timer_running = FALSE;
	progress_timer.SetTimerListener(this);
	SetLoadingListener(NULL);
}

void LoadingListenerProxy::SetLoadingListener(OpLoadingListener* listener)
{
	product_loadinglistener = listener && listener != this ? listener : &null_loading_listener;
}

void LoadingListenerProxy::HandleProgressEnqueued()
{
	OP_ASSERT(has_pending_progress);

	if (!progress_timer_running)
	{
		// No timer running. Start it. 0ms timeout the first time.

		progress_timer_running = TRUE;
		progress_timer.Start(0);
		FlushPendingProgress();
	}
}

void LoadingListenerProxy::FlushPendingProgress()
{
	OP_NEW_DBG("LoadingListenerProxy::FlushPendingProgress","wic.loading_listener");
	OP_DBG((
			   "start_stop=%i url_change=%i load_inf=%i send_start=%i start_sent=%i send_stop=%i",
			   pending_progress.start_stop_loading,
			   pending_progress.url_changed,
			   pending_progress.loading_information,
			   m_send_start, m_start_sent, m_send_stop));

	// Attempt to call OnStartLoading() before anything else.
	// Attempt to call OnLoadingFinished() after everything else.

	if (pending_progress.start_stop_loading)
	{
		// Send the start event if we have been told to or also if
		// we have not sent start and stop will be triggered now.
		if(  m_send_start || (m_send_stop && !m_start_sent) )
		{
			m_start_sent = TRUE;
			m_send_start = FALSE;
			product_loadinglistener->OnStartLoading(commander);
#ifdef OPERA_PERFORMANCE
			urlManager->OnStartLoading();
#endif // OPERA_PERFORMANCE
		}
	}

	if (pending_progress.url_changed)
	{
		pending_progress.url_changed = 0;
		product_loadinglistener->OnUrlChanged(commander, url.CStr());
	}

	if (pending_progress.loading_information)
	{
		pending_progress.loading_information = 0;
		loading_information.loadingMessage = loading_information.stored_loading_message.CStr();
		product_loadinglistener->OnLoadingProgress(commander, &loading_information);
	}

	if (pending_progress.start_stop_loading)
	{
		pending_progress.start_stop_loading = 0;

		if( m_send_stop )
		{
			m_send_stop = FALSE;
			m_start_sent = FALSE;
			product_loadinglistener->OnLoadingFinished(commander, loading_finish_status);
#ifdef OPERA_PERFORMANCE
			urlManager->OnLoadingFinished();
#endif // OPERA_PERFORMANCE
		}
	}
}

void LoadingListenerProxy::OnTimeOut(OpTimer* timer)
{
	OP_NEW_DBG("LoadingListenerProxy::OnTimeOut","wic.loading_listener");
	OP_DBG((""));

	OP_ASSERT(timer == &progress_timer);

	if (has_pending_progress)
		progress_timer.Start(WIC_PROGRESS_DELAY);
	else
		progress_timer_running = FALSE;

	FlushPendingProgress();

//	OP_ASSERT(!has_pending_progress);

	/* FIXME: If has_pending_progress is TRUE at this point, it means that
	   calling the listener methods triggered new calls to OpLoadingListener
	   methods. We currently don't handle that properly (but then again I don't
	   know if doing such a thing should be allowed at all) */
}

void LoadingListenerProxy::OnUrlChanged(OpWindowCommander* commander, const uni_char* url)
{
	OP_NEW_DBG("LoadingListenerProxy::OnUrlChanged","wic.loading_listener");
	OP_DBG((UNI_L("url=%s"),url));

#ifdef WIC_REMOVE_USERNAME_FROM_URL
	// too many places call OnUrlChanged so we do it here
	URL url_obj = g_url_api->GetURL(url);
	url = url_obj.GetAttribute(URL::KUniName_With_Fragment).CStr();
#endif

	RAISE_AND_RETURN_VOID_IF_ERROR(this->url.Set(url));
	pending_progress.url_changed = 1;
	FlushPendingProgress();
}

void LoadingListenerProxy::OnStartLoading(OpWindowCommander* commander)
{
	OP_NEW_DBG("LoadingListenerProxy::OnStartLoading","wic.loading_listener");
	OP_DBG((""));

	pending_progress.start_stop_loading = 1;
	if( !m_start_sent ) m_send_start = TRUE;
	HandleProgressEnqueued();
}

void LoadingListenerProxy::OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info)
{
	OP_NEW_DBG("LoadingListenerProxy::OnLoadingProgress","wic.loading_listener");
	OP_DBG(("bytes=%i/%i img=%i/%i",info->loadedBytes,info->totalBytes,info->loadedImageCount,info->totalImageCount));

	*((LoadingInformation *)&loading_information) = *info;

	// Make a copy of the loading message. Nobody knows when it is going to be deleted.

	const uni_char* message = loading_information.loadingMessage;
	if (message)
	{
		loading_information.loadingMessage = NULL;
		RAISE_AND_RETURN_VOID_IF_ERROR(loading_information.stored_loading_message.Set(message));
	}

	pending_progress.loading_information = 1;

	HandleProgressEnqueued();
}

void LoadingListenerProxy::OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
	OP_NEW_DBG("LoadingListenerProxy::OnLoadingFinished","wic.loading_listener");
	OP_DBG(("status: ") << status);

	loading_finish_status = status;
	pending_progress.start_stop_loading = 1;
	m_send_stop = TRUE;

	// Need to flush progress, to get correct order of received OnStartLoading and OnLoadingFinished
	FlushPendingProgress();
}

#ifdef URL_MOVED_EVENT
void LoadingListenerProxy::OnUrlMoved(OpWindowCommander* commander, const uni_char* url)
{
	OP_NEW_DBG("LoadingListenerProxy::OnUrlMoved","wic.loading_listener");
	OP_DBG((""));

	FlushPendingProgress();
	product_loadinglistener->OnUrlMoved(commander, url);
}
#endif // URL_MOVED_EVENT

void LoadingListenerProxy::OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback)
{
	OP_NEW_DBG("LoadingListenerProxy::OnAuthenticationRequired","wic.loading_listener");
	OP_DBG((""));

	FlushPendingProgress();
	product_loadinglistener->OnAuthenticationRequired(commander, callback);
}

void LoadingListenerProxy::OnStartUploading(OpWindowCommander* commander)
{
	OP_NEW_DBG("LoadingListenerProxy::OnStartUploading","wic.loading_listener");
	OP_DBG((""));

	FlushPendingProgress();
	product_loadinglistener->OnStartUploading(commander);
}

void LoadingListenerProxy::OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status)
{
	OP_NEW_DBG("LoadingListenerProxy::OnUploadingFinished","wic.loading_listener");
	OP_DBG((""));

	FlushPendingProgress();
	product_loadinglistener->OnUploadingFinished(commander, status);
#ifdef OPERA_PERFORMANCE
	urlManager->OnLoadingFinished();
#endif // OPERA_PERFORMANCE
}

BOOL LoadingListenerProxy::OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url)
{
	OP_NEW_DBG("LoadingListenerProxy::OnLoadingFailed","wic.loading_listener");
	OP_DBG((UNI_L("url=%s"),url));

	FlushPendingProgress();
	return product_loadinglistener->OnLoadingFailed(commander, msg_id, url);
}

#ifdef EMBROWSER_SUPPORT
void LoadingListenerProxy::OnUndisplay(OpWindowCommander* commander)
{
	OP_NEW_DBG("LoadingListenerProxy::OnUndisplay","wic.loading_listener");
	OP_DBG((""));

	FlushPendingProgress();
	product_loadinglistener->OnUndisplay(commander);
}

void LoadingListenerProxy::OnLoadingCreated(OpWindowCommander* commander)
{
	OP_NEW_DBG("LoadingListenerProxy::OnLoadingCreated","wic.loading_listener");
	OP_DBG((""));

	FlushPendingProgress();
	product_loadinglistener->OnLoadingCreated(commander);
}
#endif // EMBROWSER_SUPPORT

BOOL LoadingListenerProxy::OnAboutToLoadUrl(OpWindowCommander* commander, const char* url, BOOL inline_document)
{
	OP_NEW_DBG("LoadingListenerProxy::OnAboutToLoadUrl","wic.loading_listener");
	OP_DBG(("url=%s",url));

	FlushPendingProgress();
	return product_loadinglistener->OnAboutToLoadUrl(commander, url, inline_document);
}


void LoadingListenerProxy::OnXmlParseFailure(OpWindowCommander* commander)
{
	FlushPendingProgress();
	product_loadinglistener->OnXmlParseFailure(commander);
}

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
void LoadingListenerProxy::OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback)
{
	product_loadinglistener->OnSearchSuggestionsRequested(commander, url, callback);
}
#endif
#ifdef XML_AIT_SUPPORT
OP_STATUS LoadingListenerProxy::OnAITDocument(OpWindowCommander* commander, AITData* ait_data)
{
	FlushPendingProgress();
	return product_loadinglistener->OnAITDocument(commander, ait_data);
}
#endif // XML_AIT_SUPPORT

#endif // WIC_LIMIT_PROGRESS_FREQUENCY
