/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#ifdef UPDATERS_ENABLE_URL_AUTO_FETCH
#include "modules/updaters/url_updater.h"
#include "modules/url/url_sn.h"

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define UPD_ERRSTR(p,x) Str::##p##_##x
#else
#define UPD_ERRSTR(p,x) x
#endif

URL_Updater::URL_Updater()
:	finished(FALSE),
	load_policy(URL_Load_Normal)
{
}

URL_Updater::~URL_Updater()
{
	update_url.UnsetURL();
}

OP_STATUS URL_Updater::Construct(URL &url, OpMessage fin_msg, MessageHandler *mh)
{
	AutoFetch_Element::Construct(fin_msg);
	URL_DocumentLoader::Construct(mh ? mh : g_main_message_handler);

	if(url.IsEmpty())
		return OpStatus::ERR;

	update_loader = url;
	update_url.SetURL(update_loader);

	return OpStatus::OK;
}

void URL_Updater::SetFinished(BOOL success)
{
	if(finished)
		return;

	finished = TRUE;

	PostFinished(success ? TRUE : FALSE);
}

OP_STATUS URL_Updater::StartLoading()
{
	if(update_loader.IsEmpty())
		return OpStatus::ERR;

	if((URLStatus) update_loader.GetAttribute(URL::KLoadStatus) == URL_LOADING)
		return OpStatus::OK;

	update_loader.SetAttribute(URL::KDisableProcessCookies, TRUE);
	update_loader.SetAttribute(URL::KHTTP_Managed_Connection, TRUE);
	update_loader.SetAttribute(URL::KBlockUserInteraction, TRUE);
	if(!update_loader.GetAttribute(URL::KTimeoutMaximum))
		update_loader.SetAttribute(URL::KTimeoutMaximum, 30);

	if(update_loader.GetServerName())
		update_loader.GetServerName()->SetConnectionNumRestricted(TRUE);

	URL ref;
	URL_LoadPolicy policy(FALSE, load_policy);

	return LoadDocument(update_loader,ref,policy);
}

void URL_Updater::OnAllDocumentsFinished()
{
	URLStatus status = (URLStatus) update_loader.GetAttribute(URL::KLoadStatus,URL::KFollowRedirect);
	OP_ASSERT(status != URL_LOADING);
	URLType url_type = update_loader.Type();

	if(status != URL_LOADED || (
		(url_type == URL_HTTP || url_type == URL_HTTPS) && 
			update_loader.GetAttribute(URL::KHTTP_Response_Code,URL::KFollowRedirect) != 200))
	{
		SetFinished(FALSE);
		return;
	}

	OP_STATUS op_err;
	if(OpStatus::IsError(op_err = ResourceLoaded(update_loader)))
	{
		RAISE_IF_MEMORY_ERROR(op_err);
		SetFinished(FALSE);
		return;
	}
	SetFinished(TRUE);
}

#endif
