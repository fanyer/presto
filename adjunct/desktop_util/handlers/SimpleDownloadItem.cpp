/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#include "core/pch.h"


#include "adjunct/desktop_util/handlers/SimpleDownloadItem.h"

#include "adjunct/desktop_util/algorithms/opunique.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "modules/url/url2.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/path.h"
//////////////////////////////////////////////////////////////////////////



SimpleDownloadItem::SimpleDownloadItem(MessageHandler &mh, SimpleDownloadItem::Listener& listener)
	: m_mh(&mh)
	, m_listener(&listener)
{
}

SimpleDownloadItem::~SimpleDownloadItem()
{
	m_download_url.StopLoading(m_mh);
	UnsetCallbacks();
}

OP_STATUS
SimpleDownloadItem::Init(const uni_char* url_name)
{
	const URL url = g_url_api->GetURL(url_name);
	return Init(url);
}

OP_STATUS
SimpleDownloadItem::Init(const URL& url)
{
	m_download_url = url;
	if (m_download_url.IsEmpty())
		return OpStatus::ERR;

	CommState state = m_download_url.Load(m_mh, URL(), TRUE, TRUE, FALSE);

	if (state != COMM_LOADING)
		return OpStatus::ERR;

	RETURN_IF_ERROR(SetCallbacks());

	return OpStatus::OK;
}

void
SimpleDownloadItem::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_URL_MOVED:
		UnsetCallbacks();
		m_download_url = m_download_url.GetAttribute(URL::KMovedToURL); 
		SetCallbacks();
		break;
	case MSG_URL_LOADING_FAILED:
		HandleError();
		return;
	case MSG_URL_DATA_LOADED:
	case MSG_NOT_MODIFIED:
	case MSG_MULTIPART_RELOAD:
		if (m_download_url.Status(TRUE) == URL_LOADED)
		{
			if (OpStatus::IsError(HandleFinished()))
				m_listener->DownloadFailed();
		}
		else
		{
			m_listener->DownloadInProgress(m_download_url);
		}
		return;
#if defined(CACHE_CAP_SENDS_MSG_CACHE_RESTART_ALL_CONNECTIONS) && \
	defined(NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS)
	case MSG_CACHE_RESTART_ALL_CONNECTIONS:
		m_download_url.StopAndRestartLoading();
		break;
#endif // CACHE_CAP_SENDS_MSG_CACHE_RESTART_ALL_CONNECTIONS && NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
	default:
		OP_ASSERT(!"You shouldn't be here, are we sending messages to ourselves that we do not handle?");
	}
}

OP_STATUS
SimpleDownloadItem::SetCallbacks()
{
	URL_ID id = m_download_url.Id();

	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_DATA_LOADED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_NOT_MODIFIED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_LOADING_FAILED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_MOVED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_MULTIPART_RELOAD, id));

#if defined(CACHE_CAP_SENDS_MSG_CACHE_RESTART_ALL_CONNECTIONS) && \
	defined(NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS)
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_CACHE_RESTART_ALL_CONNECTIONS, 0));
#endif // CACHE_CAP_SENDS_MSG_CACHE_RESTART_ALL_CONNECTIONS && NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS

	return OpStatus::OK;
}

void
SimpleDownloadItem::UnsetCallbacks()
{
	m_mh->UnsetCallBacks(this);

#if defined(CACHE_CAP_SENDS_MSG_CACHE_RESTART_ALL_CONNECTIONS) && \
	defined(NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS)
	m_mh->UnsetCallBack(this, MSG_CACHE_RESTART_ALL_CONNECTIONS);
#endif // CACHE_CAP_SENDS_MSG_CACHE_RESTART_ALL_CONNECTIONS && NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
}

OP_STATUS SimpleDownloadItem::HandleFinished()
{
	UnsetCallbacks();

	OpString filename;
	RETURN_IF_LEAVE(m_download_url.GetAttributeL(URL::KSuggestedFileName_L, filename));
	
	OpString path;
	RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TEMP_FOLDER, path));

	RETURN_IF_ERROR(OpPathDirFileCombine(path,path,filename));

	for (OpUnique unique(path, UNI_L("_%d")); DesktopOpFileUtils::MightExist(path); )
		RETURN_IF_ERROR(unique.Next());

	if (m_download_url.SaveAsFile(path) != 0)
		return OpStatus::ERR;

	m_listener->DownloadSucceeded(path);

	return OpStatus::OK;
}

void SimpleDownloadItem::HandleError()
{
	UnsetCallbacks();
	m_listener->DownloadFailed();
}


