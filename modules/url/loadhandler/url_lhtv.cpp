/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Ole Gunnar Westgaard
**
*/

#include "core/pch.h"


#ifdef HAS_ATVEF_SUPPORT
// OGW; Remember that defines may differ in implementation sometime later!!

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/base/opstatus.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_pd.h"

#include "modules/util/simset.h"
#include "modules/url/url2.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/loadhandler/url_lhtv.h"

void URL_DataStorage::StartLoadingTvDocument()
{
	loading = OP_NEW(URL_Tv_LoadHandler, (url, g_main_message_handler));
	
	// We need this to be able to continue in the next stage (url_ds.cpp)
	if (!loading) //OOM
	{
		SetAttribute(URL::KLoadStatus,URL_LOADING_FAILURE);
		SetAttribute(URL::KHeaderLoaded,TRUE);		
		SetAttribute(URL::KReloading,FALSE);
		SetAttribute(URL::KResumeSupported,FALSE);
		SetAttribute(URL::KReloadSameTarget,FALSE);
		BroadcastMessage(MSG_URL_LOADING_FAILED, url->GetID(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR), MH_LIST_ALL);
		//g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return;
	}

	SetAttribute(URL::KMIME_ForceContentType, "image/atvef");
	SetAttributeL(URL::KMIME_Type, "image/atvef");
	SetAttributeL(URL::KContentType, URL_TV_CONTENT);

	// Broadcast that header has loaded...
	BroadcastMessage(MSG_HEADER_LOADED, url->GetID(),
					 url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);

	// Broadcast that data has been loaded.
	BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0,MH_LIST_ALL);	

	UnsetListCallbacks();
	mh_list->Clean();
	url->SetIsFollowed(TRUE);
	SetAttribute(URL::KHeaderLoaded, TRUE);
	SetAttribute(URL::KLoadStatus,URL_LOADED);
}

URL_Tv_LoadHandler::URL_Tv_LoadHandler(URL_Rep *url_rep, MessageHandler *mh)
  : URL_LoadHandler(url_rep, mh)
{
}

URL_Tv_LoadHandler::~URL_Tv_LoadHandler()
{
	g_main_message_handler->UnsetCallBacks(this);
}

CommState URL_Tv_LoadHandler::Load()
{
	OpStringC server_name(url->GetAttribute(URL::KUniHostName));
	URL_LoadHandler::SetProgressInformation(REQUEST_FINISHED,0,	server_name.CStr());
	
	g_main_message_handler->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);

	return COMM_LOADING;
}

#endif // HAS_ATVEF_SUPPORT
