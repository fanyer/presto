/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


// URL Rep/Datastorage Forms data

#include "core/pch.h"

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/url_pd.h"

#include "modules/util/str.h"
#include "modules/upload/upload.h"
#include "modules/url/url_dns_man.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

BOOL URL_Rep::CheckStorage()
{
	if( storage || this == EmptyURL_Rep)
		return (storage != NULL);

	URLType type = (URLType) name.GetAttribute(URL::KType);
	if(type == URL_NEWS)
		return (storage != NULL);

	RAISE_IF_ERROR(CreateStorage());

	return (storage != NULL);
}

BOOL URL_Rep::CheckStorage(OP_STATUS &op_err)
{
	op_err = OpStatus::OK;
	if( storage || this == EmptyURL_Rep)
		return (storage != NULL);

	URLType type = (URLType) name.GetAttribute(URL::KType);
	if( type == URL_NEWS)
		return (storage != NULL);

	op_err = CreateStorage();

	return (storage != NULL);
}

OP_STATUS URL_Rep::CreateStorage()
{
	OpAutoPtr<URL_DataStorage> temp_storage(OP_NEW(URL_DataStorage, (this)));
	if(temp_storage.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(temp_storage->Init());
	storage = temp_storage.release();

	return OpStatus::OK;
}

OP_STATUS	URL_Rep::SetHTTP_Data(const char* data, BOOL new_data, BOOL with_headers)
{
	if(CheckStorage())
		RETURN_IF_ERROR(storage->SetHTTP_Data(data, new_data, with_headers));
	return OpStatus::OK;
}

OP_STATUS	URL_DataStorage::SetHTTP_Data(const char* data, BOOL new_data, BOOL with_headers)
{
	if(GetHTTPProtocolData())
	{
		http_data->ClearHTTPData();
		RETURN_IF_ERROR(http_data->sendinfo.http_data.Set(data));	
		http_data->flags.http_data_new = new_data;
		http_data->flags.http_data_with_header = with_headers;
	}
	return OpStatus::OK;
}

#ifdef HAS_SET_HTTP_DATA
void	URL_Rep::SetHTTP_Data(Upload_Base* data, BOOL new_data) 
{ 
	if(CheckStorage())
		storage->SetHTTP_Data(data, new_data); 
	else
		OP_DELETE(data);
}

void	URL_Rep::SetMailData(Upload_Base* data)
{
	if(CheckStorage())
		storage->SetMailData(data);
	else
		OP_DELETE(data);
}

BOOL	URL_Rep::PrefetchDNS()
{
	URLType url_type = (URLType) GetAttribute(URL::KType);

	// No point in prefetching DNS addresses if we are using a proxy.
	if (
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticProxyConfig) ||
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
		(url_type == URL_HTTP && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPProxy))
		|| (url_type == URL_HTTPS && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPSProxy)))
		return FALSE;

	ServerName *server_name = (ServerName *) GetAttribute(URL::KServerName, (void *) NULL,  URL::KNoRedirect);

	if(server_name && !server_name->IsResolvingHost() && !server_name->IsHostResolved())
	{
		g_url_dns_manager->Resolve(server_name, NULL);
		return TRUE;
	}
	return FALSE;
}

void	URL_DataStorage::SetHTTP_Data(Upload_Base* data, BOOL new_data) 
{ 
	URLStatus load_status = static_cast<URLStatus>(url->GetAttribute(URL::KLoadStatus, FALSE));

	if (load_status == URL_LOADING || load_status == URL_LOADING_FROM_CACHE || load_status == URL_LOADING_WAITING)
	{
		OP_ASSERT(!"Does not make sense to set HTTP data after loading has started");
		OP_DELETE(data);
		return;
	}

	if(GetHTTPProtocolData() && !loading)
	{
		OP_DELETE(http_data->sendinfo.upload_data);
		http_data->sendinfo.upload_data = data;
		http_data->flags.http_data_new = new_data; 
	}
	else
		OP_DELETE(data);
}

void	URL_DataStorage::SetMailData(Upload_Base* data)
{
	if(GetMailtoProtocolData())
	{
		OP_DELETE(protocol_data.mailto_data->upload_data);
		protocol_data.mailto_data->upload_data = data;
	}
	else
		OP_DELETE(data);
}
#endif
