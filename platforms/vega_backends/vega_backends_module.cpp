/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch_system_includes.h"

#ifdef VEGA_BACKENDS_BLOCKLIST_FETCH

#include "modules/url/url_api.h"
#include "modules/util/opautoptr.h"
#include "platforms/vega_backends/vega_backends_module.h"
#include "platforms/vega_backends/vega_blocklist_file.h"

void VegaBackendsModule::FetchBlocklist(VEGABlocklistDevice::BlocklistType type, unsigned long delay)
{
	OP_ASSERT(type < VEGABlocklistDevice::BlocklistTypeCount);

	MessageHandler* mh = g_main_message_handler;
	const OpMessage msg = MSG_VEGA_BACKENDS_FETCH_BLOCKLIST;
	MH_PARAM_1 par1 = (MH_PARAM_1)type;
	MH_PARAM_2 par2 = 0;

	// tricky: if message cannot be posted no blocklist updates will
	// be done until opera is restarted. i don't see a way out here...
	mh->UnsetCallBack(this, msg, par1);
	RETURN_VOID_IF_ERROR(mh->SetCallBack(this, msg, par1));
	mh->RemoveDelayedMessage(msg, par1, par2);
	if (!mh->PostMessage(msg, par1, par2, delay))
		mh->UnsetCallBack(this, msg, par1);
}

OP_STATUS VegaBackendsModule::FetchBlocklist(VEGABlocklistDevice::BlocklistType type)
{
	OpString urlstr;
	RETURN_IF_ERROR(VEGABlocklistFile::GetURL(type, urlstr));
	OP_ASSERT(urlstr.HasContent());

	OpAutoPtr<BlocklistUrlObj> o(OP_NEW(BlocklistUrlObj, ()));
	if (!o.get())
		return OpStatus::ERR_NO_MEMORY;

	o->type = type;
	o->url = g_url_api->GetURL(urlstr.CStr());

	// blocklist is already being loaded - everything's just peachy
	if (m_blocklist_url_hash.Contains((void*)o->url.Id()))
		return OpStatus::OK;

	MessageHandler* mh = g_main_message_handler;
	const MH_PARAM_1 par1 = o->url.Id();

	URL ref;
	const CommState res = o->url.Load(mh, ref);
	switch (res)
	{
	case COMM_LOADING:
		break;
	case COMM_REQUEST_FINISHED:
		return VEGABlocklistFile::OnBlocklistFetched(o->url, type);
		break;
	case COMM_REQUEST_FAILED:
		return OpStatus::ERR;
		break;
	}

	OP_STATUS status = mh->SetCallBack(this, MSG_URL_LOADING_FAILED, par1);
	if (OpStatus::IsSuccess(status))
		status = mh->SetCallBack(this, MSG_URL_DATA_LOADED, par1);
	if (OpStatus::IsSuccess(status))
		status = m_blocklist_url_hash.Add(o.get());
	if (OpStatus::IsError(status))
	{
		o->url.StopLoading(mh);

		mh->UnsetCallBack(this, MSG_URL_LOADING_FAILED, par1);
		mh->UnsetCallBack(this, MSG_URL_DATA_LOADED, par1);

		return status;
	}

	o.release();
	return OpStatus::OK;
}

void VegaBackendsModule::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	MessageHandler* mh = g_main_message_handler;

	if (msg == MSG_VEGA_BACKENDS_FETCH_BLOCKLIST)
	{
		mh->UnsetCallBack(this, msg, par1);
		// try again on failure
		if (OpStatus::IsError(FetchBlocklist((VEGABlocklistDevice::BlocklistType)par1)))
			VEGABlocklistFile::FetchLater((VEGABlocklistDevice::BlocklistType)par1);
		return;
	}

	BlocklistUrlObj* o = m_blocklist_url_hash.Get(par1);
	if (!o)
	{
		OP_ASSERT(!"lost a url");
		return;
	}

	UINT32 http_response;
	switch (msg)
	{
	case MSG_URL_LOADING_FAILED:
		OpStatus::Ignore(m_blocklist_url_hash.Remove(o));
		VEGABlocklistFile::FetchLater((VEGABlocklistDevice::BlocklistType)par1);
		break;

	case MSG_URL_DATA_LOADED:
		if (o->url.Status(TRUE) != URL_LOADED)
			return;

		if (o->url.Type() == URL_HTTP || o->url.Type() == URL_HTTPS)
			http_response = o->url.GetAttribute(URL::KHTTP_Response_Code, URL::KFollowRedirect);
		else
			http_response = HTTP_OK;
		if (http_response != HTTP_OK && http_response != HTTP_NOT_MODIFIED)
		{
			// URL didn't load properly (404 etc)
			OpStatus::Ignore(m_blocklist_url_hash.Remove(o));
			VEGABlocklistFile::FetchLater(o->type);
			break;
		}

		// re-fetch triggered from OnBlocklistFetched
		// non-OOM errors hare handled by OnBlocklistFetched
		if (OpStatus::IsMemoryError(VEGABlocklistFile::OnBlocklistFetched(o->url, o->type)))
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		break;
	}

	o->url.StopLoading(mh);
	mh->UnsetCallBack(this, MSG_URL_LOADING_FAILED, par1);
	mh->UnsetCallBack(this, MSG_URL_DATA_LOADED, par1);
	OpStatus::Ignore(m_blocklist_url_hash.Remove(o));
	OP_DELETE(o);
}

#endif // VEGA_BACKENDS_BLOCKLIST_FETCH
