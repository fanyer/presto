/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"
#include "modules/url/protocols/common.h"
#include "modules/url/url_man.h"
#include "modules/util/cleanse.h"

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
void GetAutoProxyL(const uni_char *apcString, Base_request_st *request)
{
	UniParameterList proxylist;
	ANCHOR(UniParameterList, proxylist);
	ServerName *candidate;
	unsigned short candidate_port;

	proxylist.SetValueL(apcString, PARAM_SEP_SEMICOLON | PARAM_NO_ASSIGN | PARAM_ONLY_SEP);

	UniParameters *param = proxylist.First();
	PROXY_type current_type = PROXY_HTTP;
	ProxyListItem  *item = NULL;
#ifdef SOCKS_SUPPORT
	ServerName*  socks_server_name = NULL;
	UINT         socks_server_port = 0;
#endif

	while(param && current_type != NO_PROXY && param->GetParametersL(PARAM_SEP_WHITESPACE | PARAM_NO_ASSIGN | PARAM_ONLY_SEP, KeywordIndex_Automatic_Proxy_Configuration))
	{
		UniParameterList *apc_item = param->SubParameters();

		UniParameters *apc_param = apc_item->First();
		if(apc_param) switch(apc_param->GetNameID())
		{
		case APC_Keyword_Type_Direct:
			item = OP_NEW_L(ProxyListItem, ());
			item->type = current_type = NO_PROXY;
			item->proxy_candidate = request->origin_host;
			item->port = request->origin_port;
			item->Into(&request->connect_list);
			item = NULL;
			break;
		case APC_Keyword_Type_Proxy:
			current_type = PROXY_HTTP;
			apc_param = apc_param->Suc();

			if(apc_param != NULL)
			{
				// Should possibly call prepareservername here.
				// Ask Yngve about this.
				OP_STATUS op_err = OpStatus::OK;
				candidate = urlManager->GetServerName(op_err, apc_param->Name(), candidate_port, TRUE);//FIXME:OOM (unable to report)
				LEAVE_IF_ERROR(op_err);
				if (!candidate)
				{
					LEAVE(OpStatus::ERR_NO_MEMORY);
				}
				if(candidate && candidate->MayBeUsedAsProxy(candidate_port))
				{
					item = OP_NEW_L(ProxyListItem, ());
					item->type = current_type;
					item->proxy_candidate = candidate;
					item->port = candidate_port;
					item->Into(&request->connect_list);
				}
			}
			break;
#ifdef SOCKS_SUPPORT
		case APC_Keyword_Type_Socks:
			//SOCKS proxy (if any) is handled transparently (not chained as other proxies)
			OP_ASSERT(socks_server_name == NULL);

			apc_param = apc_param->Suc();

			if(apc_param != NULL)
			{
				OP_STATUS op_err = OpStatus::OK;
				candidate = urlManager->GetServerName(op_err, apc_param->Name(), candidate_port, TRUE);
				LEAVE_IF_ERROR(op_err);
				if (!candidate)
				{
					LEAVE(OpStatus::ERR_NO_MEMORY);
				}
				if(candidate && candidate->MayBeUsedAsProxy(candidate_port))
				{
					// substitute the (URL_Manager owned) candidate with a (SocksModule owned) copy:
					candidate = g_opera->socks_module.GetSocksServerName(candidate);
					socks_server_name = candidate;
					socks_server_port = candidate_port;
				}
			}
			break;
#endif // SOCKS_SUPPORT
		}
	param = param->Suc();

	}

	if(request->connect_list.Cardinal() == 1)
	{
		item = (ProxyListItem *) request->connect_list.First();
		item->Out();
		request->connect_host = item->proxy_candidate;
		request->connect_port = item->port;
		request->proxy = item->type;
		OP_DELETE(item);
	}
	else if(!request->connect_list.Empty())
	{
		request->current_connect_host = item = (ProxyListItem *) request->connect_list.First();
		request->connect_host = item->proxy_candidate;
		request->connect_port = item->port;
		request->proxy = item->type;
	}

#ifdef SOCKS_SUPPORT
	if (socks_server_name != NULL)
	{
		request->connect_host->SetSocksServerName(socks_server_name);
		request->connect_host->SetSocksServerPort(socks_server_port);
	}
	else // flag that the autoconfig did not mention any socks:
	{
		request->connect_host->SetNoSocksServer();
	}
#endif

}
#endif //SUPPORT_AUTO_PROXY_CONFIGURATION

authdata_st::authdata_st()
{
	connect_host = NULL;
	origin_host = NULL;
	connect_port = 0;
	origin_port = 0;
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	session_based_support = FALSE;
#endif
	auth_headers = OP_NEW(HeaderList, ()); //OOM: This is checked in CheckAuthData.
}

authdata_st::~authdata_st()
{
	OP_DELETE(auth_headers);
}

#ifdef HTTP_DIGEST_AUTH

void HTTP_Request_digest_data::ClearAuthentication()
{
	if(used_A1)
		OPERA_cleanse(used_A1, op_strlen(used_A1));
	if(used_cnonce)
		OPERA_cleanse(used_cnonce, op_strlen(used_cnonce));
	if(used_nonce)
		OPERA_cleanse(used_nonce, op_strlen(used_nonce));

	OP_DELETE(entity_hash);
	entity_hash = NULL;

	OP_DELETEA(used_A1);
	OP_DELETEA(used_cnonce);
	OP_DELETEA(used_nonce);
	used_A1 = NULL;
	used_cnonce = NULL;
	used_nonce = NULL;
}

#endif //HTTP_DIGEST_AUTH
