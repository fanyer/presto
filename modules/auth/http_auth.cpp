/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#ifdef _ENABLE_AUTHENTICATE

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/auth/auth_digest.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_pd.h"
#include "modules/util/cleanse.h"

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define AUTH_ERRSTR(p,x) Str::##p##_##x
#else
#define AUTH_ERRSTR(p,x) x
#endif

#ifdef HTTP_DIGEST_AUTH 
#include "modules/libcrypto/include/CryptoHash.h"
#endif // HTTP_DIGEST_AUTH

#ifdef EXTERNAL_DIGEST_API
#include "modules/libssl/external/ext_digest/external_digest_man.h"
#endif

#if defined DEBUG && defined YNP_WORK 

#include "modules/olddebug/tstdump.h"

#define DEBUG_AUTH
#endif

int UU_encode (unsigned char* bufin, unsigned int nbytes, char* bufcoded);

#ifndef CLEAR_PASSWD_FROM_MEMORY
#error "For security reasons FEATURE_CLEAR_PASSWORDS *MUST* be YES when HTTP Authentication is used"
#endif

#ifdef HTTP_DIGEST_AUTH 

void HTTP_Request::ProcessEntityBody(const unsigned char *buf, unsigned len)
{
#include "modules/url/url_sn.h"
	if(auth_digest.entity_hash)
		auth_digest.entity_hash->CalculateHash(buf, len);
	if(proxy_digest.entity_hash)
		proxy_digest.entity_hash->CalculateHash(buf, len);
}

void HTTP_Request::ClearAuthentication()
{
	auth_digest.ClearAuthentication();
	proxy_digest.ClearAuthentication();
}

OP_STATUS HTTP_Request::Check_Digest_Authinfo(HTTP_Authinfo_Status &ret_status, ParameterList *header, BOOL proxy, BOOL finished)
{
	return ::Check_Digest_Authinfo(ret_status, header, proxy, finished, 
								   auth_digest, proxy_digest,
								   method, request, auth_id, proxy_auth_id);
}

OP_STATUS Check_Digest_Authinfo(HTTP_Authinfo_Status &ret_status, ParameterList *header, BOOL proxy, BOOL finished,
								HTTP_Request_digest_data &auth_digest,
								HTTP_Request_digest_data &proxy_digest,
								HTTP_Method method,
								HTTP_request_st* request,
								unsigned long auth_id,
								unsigned long proxy_auth_id
								)
{
	KeywordIndex digest_qops[3];
	digest_qops[0].keyword = NULL;
	digest_qops[0].index = DIGEST_QOP_NONE;
	digest_qops[1].keyword = "auth";
	digest_qops[1].index = DIGEST_QOP_AUTH;
	digest_qops[2].keyword = "auth-int";
	digest_qops[2].index = DIGEST_QOP_AUTH_INT;

	ret_status = AuthInfo_Failed;
	if(header == NULL)
	{
		return OpStatus::OK;
	}

	BOOL status = FALSE;
	digest_qop qop = DIGEST_QOP_NONE;
	HTTP_Request_digest_data *used = (proxy ? &proxy_digest : &auth_digest);

	Parameters *param;
	Parameters *qop_param = header->GetParameterByID(HTTP_Authentication_Qop);
	if(qop_param)
	{
		qop = (digest_qop) CheckKeywordsIndex(qop_param->Value(),digest_qops, ARRAY_SIZE(digest_qops));
		if(qop == DIGEST_QOP_NONE)
			goto mopup;

		if(qop == DIGEST_QOP_AUTH_INT && !finished)
		{
			if(used->entity_hash)
				ret_status = Authinfo_Not_Finished;
			return OpStatus::OK;
		}
	}

	param = header->GetParameterByID(HTTP_Authentication_Cnonce);
	if(param && param->Value())
	{
		if(!used->used_cnonce || op_strcmp(param->Value(),used->used_cnonce) != 0)
			goto mopup;
	}
	//else if(used->used_cnonce)
	//	goto mopup;

	param = header->GetParameterByID(HTTP_Authentication_NC);
	if(!param)
		param = header->GetParameterByID(HTTP_Authentication_Nonce_Count);

	if(param)
	{
		OpStringC8 temp(param->Value());

		if(qop == DIGEST_QOP_NONE || temp.IsEmpty() ||
			temp.SpanNotOf("0123456789abcdefABCDEF") != 0) //NULL)
			goto mopup;
		unsigned long nc_val = param->GetUnsignedValue(16);
		if(nc_val != used->used_noncecount)
			goto mopup;
	}
	//else if(qop != DIGEST_QOP_NONE)
	//	goto mopup;


	param = header->GetParameterByID(HTTP_Authentication_Rspauth);
	if(!param)
		status = TRUE;
	else
	{
	OpAutoPtr<CryptoHash> hash(CryptoHash::CreateMD5());
	OpAutoPtr<CryptoHash> hash_A2(CryptoHash::CreateMD5());

	if (!hash.get() || !hash_A2.get())
		return OpStatus::ERR_NO_MEMORY;
	
#ifdef EXTERNAL_DIGEST_API
		OpParamsStruct1  ext_digest_param;
		
		ext_digest_param.digest_uri = (request->request ? request->request.CStr() : "/");
		ext_digest_param.servername = (request->origin_host ? request->origin_host->Name() : NULL);
		ext_digest_param.port_number = request->origin_port;
		ext_digest_param.method = NULL;
		ext_digest_param.realm = NULL;
		ext_digest_param.username = NULL;
		ext_digest_param.password = NULL;
		ext_digest_param.nonce = used->used_nonce;
		ext_digest_param.nc_value = used->used_noncecount;
		ext_digest_param.cnonce = used->used_cnonce;
		ext_digest_param.qop_value = (qop_param ? qop_param->Value(): NULL);
		ext_digest_param.opaque = NULL;
#endif
		
		hash->InitHash();
		EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_AUTH_RESP_KD, &ext_digest_param);
		hash->CalculateHash(used->used_A1);
		hash->CalculateHash(":");
#ifdef DEBUG_AUTH
		PrintfTofile("http.txt","Digest info calc 1a: %s:\n",used->used_A1);
#endif
		
		hash_A2->InitHash();
		EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_AUTH_RESP_H_A2, &ext_digest_param);
		hash_A2->CalculateHash(":");
		if(proxy && method == HTTP_METHOD_CONNECT)
		{
			hash_A2->CalculateHash((request->origin_host != NULL && request->origin_host->Name() ? request->origin_host->Name() : ""));
			char *temp = (char *) g_memory_manager->GetTempBuf();
			
			temp[0] = ':';
			op_ltoa(request->origin_port,temp+1,10);
			hash->CalculateHash(temp);
#ifdef DEBUG_AUTH
			PrintfTofile("http.txt","Digest info calc 2a: :%s:%d\n",(request->origin_host != NULL && request->origin_host->Name() ? request->origin_host->Name() : ""), request->origin_port);
#endif
		}
		else
		{
			hash_A2->CalculateHash(request->request.HasContent() ? request->request.CStr() : "/");
#ifdef DEBUG_AUTH
			PrintfTofile("http.txt","Digest info calc 2b: :%s\n",(request->request.HasContent() ? request->request.CStr() : "/"));
#endif
		}
		
		hash->CalculateHash(used->used_nonce);
#ifdef DEBUG_AUTH
		PrintfTofile("http.txt","Digest info calc 1b: :%s\n",used->used_nonce);
#endif
		switch(qop)
		{
		case DIGEST_QOP_NONE :
			break;

		case DIGEST_QOP_AUTH_INT :
			{
				char *digest = (char *) g_memory_manager->GetTempBuf();

				ConvertToHex(used->entity_hash, digest);
				hash_A2->CalculateHash(":");
				hash_A2->CalculateHash(digest);
#ifdef DEBUG_AUTH
				PrintfTofile("http.txt","Digest info calc 2c: :%s\n",digest);
#endif
				OPERA_cleanse_heap(digest, 2 * CRYPTO_MAX_HASH_SIZE + 1);
			}

		case DIGEST_QOP_AUTH :
			hash->CalculateHash(":");
			{
				char *nc = (char *) g_memory_manager->GetTempBuf();;
				
				op_snprintf(nc, g_memory_manager->GetTempBufLen(), "%.8lx:",used->used_noncecount);
				hash->CalculateHash(nc);
#ifdef DEBUG_AUTH
				PrintfTofile("http.txt","Digest info calc 1c: :%s\n",nc);
#endif
			}
			hash->CalculateHash(used->used_cnonce);
			hash->CalculateHash(":");
			hash->CalculateHash((qop_param ? qop_param->Value(): NULL));
#ifdef DEBUG_AUTH
			PrintfTofile("http.txt","Digest info calc 1d: %s:%s\n",used->used_cnonce,(qop_param ? qop_param->Value(): ""));
#endif
			break;
		}
		
		
		{
			char *digest1 = (char *) g_memory_manager->GetTempBuf();

			ConvertToHex(hash_A2, digest1);
		
			hash->CalculateHash(":");
			hash->CalculateHash(digest1);
#ifdef DEBUG_AUTH
			PrintfTofile("http.txt","Digest info calc 1e (result 2): :%s\n",digest1);
#endif
		
			ConvertToHex(hash, digest1);
#ifdef DEBUG_AUTH
			PrintfTofile("http.txt","Digest info result1 %s\n",digest1);
#endif

#ifdef DEBUG_AUTH
			PrintfTofile("http.txt","Digest info rspaauth %s\n",(param && param->Value() ? param->Value() : "") );
#endif
			if(param && param->GetValue().CompareI(digest1) == 0)
				status = TRUE;
			OPERA_cleanse_heap(digest1, 2 * CRYPTO_MAX_HASH_SIZE + 1);
		}
	}
	
mopup:;

	ret_status = status ? AuthInfo_Succeded : AuthInfo_Failed;
	if( proxy )
		RETURN_IF_ERROR(request->connect_host->Update_Authenticate(proxy_auth_id, header));
	else
		RETURN_IF_ERROR(request->origin_host->Update_Authenticate(auth_id, header));
	
	return OpStatus::OK;
}
#endif //HTTP_DIGEST_AUTH 

void URL_HTTP_LoadHandler::Authenticate(AuthElm *auth_elm)
{
	TRAPD(op_err, AuthenticateL(auth_elm));
	if(op_err == OpStatus::ERR_NO_MEMORY && url->GetDataStorage())
		url->GetDataStorage()->FailAuthenticate(AUTH_MEMORY_FAILURE);
	//If other errors : cannot add authentication data (but let the process continue)
}

void URL_HTTP_LoadHandler::AuthenticateL(AuthElm *auth_elm)
{
	if(!auth_elm)
		return;

	URL_DataStorage *url_ds = url->GetDataStorage();
	if(url_ds == NULL)
		return; // should never happen

	URL_HTTP_ProtocolData *hptr = url_ds->GetHTTPProtocolDataL();
	if(hptr == NULL)
		return; // cannot add authentication data (but let the process continue)

	hptr->flags.connect_auth = FALSE;
	if(!url_ds->GetAttribute(URL::KReloadSameTarget))
	{
		req->SetIfModifiedSinceL(NULL);
		req->SetIfNoneMatchL(NULL);
	}

	if((auth_elm->GetScheme() & AUTH_SCHEME_HTTP_PROXY) != 0)
	{
		OpStringS8 auth_str;
		LEAVE_IF_ERROR(auth_elm->GetAuthString(auth_str, url, req));

		req->SetProxyAuthorization(auth_str);
		req->SetProxyAuthorizationId(auth_elm->GetId());
		hptr->flags.proxy_auth_status = HTTP_AUTH_HAS;
#ifdef HTTP_DIGEST_AUTH
		if(auth_elm->GetScheme() == AUTH_SCHEME_HTTP_PROXY_DIGEST)
			info.check_proxy_auth_info = TRUE;
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		if(auth_elm->MustClone())
		{
			HTTP_Request *req_target = (req->secondary_request ? req->secondary_request : req);
			OP_DELETEA(req_target->proxy_ntlm_auth_element);
			req_target->proxy_ntlm_auth_element = auth_elm->MakeClone();
			req_target->info.ntlm_updated_proxy = TRUE;
			if(req_target != req || (URLType) url->GetAttribute(URL::KType) != URL_HTTPS)
			{
				req_target->master->SetHTTP_1_1(TRUE);
				req_target->master->SetHTTP_1_1_Pipelined(FALSE);
				req_target->master->SetHTTP_ProtocolDetermined(TRUE);
			}
			req_target->info.ntlm_updated_proxy = TRUE;
		}
#endif
	}
	else
	{
		OpStringS8 auth_str;
		ANCHOR(OpStringS8, auth_str);

		LEAVE_IF_ERROR(auth_elm->GetAuthString(auth_str, url, req));

		req->SetAuthorization(auth_str);
		req->SetAuthorizationId(auth_elm->GetId());
#ifdef HTTP_DIGEST_AUTH
		if(auth_elm->GetScheme() == AUTH_SCHEME_HTTP_DIGEST)
			info.check_auth_info = TRUE;
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		if(auth_elm->MustClone())
		{
			req->server_negotiate_auth_element = auth_elm->MakeClone();
			req->info.ntlm_updated_server= TRUE;
			req->master->SetHTTP_1_1(TRUE);
			req->master->SetHTTP_1_1_Pipelined(FALSE);
			req->master->SetHTTP_ProtocolDetermined(TRUE);
		}
#endif
		if (!auth_elm->GetAuthenticateOnce())
		{
			url->SetAttributeL(URL::KHaveAuthentication, TRUE);
			hptr->flags.auth_status = HTTP_AUTH_HAS;
		}
	}

	{
		CommState cookie_stat = UpdateCookieL();

		if(cookie_stat != COMM_REQUEST_WAITING && cookie_stat != COMM_LOADING)
		{
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), AUTH_ERRSTR(SI,ERR_COMM_INTERNAL_ERROR));
			return;
		}

		url_ds->SetAttributeL(URL::KHeaderLoaded, FALSE);
		url_ds->MoveCacheToOld(TRUE);
		url_ds->SetAttributeL(URL::KHTTP_Response_Code, 0);
		req->Clear();

		if(cookie_stat == COMM_REQUEST_WAITING)
			return;

	}
	req->ResetL();
	req->Load();
}

BOOL URL_HTTP_LoadHandler::CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy)
{
	return (!auth_elm || !req ||
		(!proxy? req->GetAuthorizationId() : req->GetProxyAuthorizationId()) == auth_elm->GetId() ? TRUE : FALSE);
}

#endif // _Enable Authenticate
