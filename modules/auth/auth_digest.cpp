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

#if defined _ENABLE_AUTHENTICATE && defined HTTP_DIGEST_AUTH

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_sn.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/auth/auth_digest.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"

#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"

#include "modules/url/tools/arrays.h"

#include "modules/util/cleanse.h"


#if defined DEBUG && defined YNP_WORK 

#include "modules/olddebug/tstdump.h"

#define DEBUG_AUTH
#endif

int UU_encode (unsigned char* bufin, unsigned int nbytes, char* bufcoded);

#ifndef CLEAR_PASSWD_FROM_MEMORY
#error "For security reasons FEATURE_CLEAR_PASSWORDS *MUST* be YES when HTTP Authentication is used"
#endif

Digest_AuthElm::Digest_AuthElm(AuthScheme a_scheme,
			URLType a_type,	unsigned short a_port, BOOL authenticate_once)
	: AuthElm_Base(a_port, a_scheme, a_type, authenticate_once)
{
	digest_fields.noncecount = 0;
	digest_fields.qop.qop_found = FALSE;
	digest_fields.qop.qop_auth = FALSE;
	digest_fields.qop.qop_auth_int = FALSE;
	
	digest_fields.digest = CRYPTO_HASH_TYPE_MD5;
	
	state = 0;
}

OP_STATUS Digest_AuthElm::Construct(ParameterList *header, OpStringC8 a_realm,	
			OpStringC8 a_user, OpStringC8 a_passwd, ServerName *server)
{
	RETURN_IF_ERROR(AuthElm_Base::Construct(a_realm, a_user));
	RETURN_IF_ERROR(password.Set(a_passwd));
	return Init_Digest(header, server);
}

Digest_AuthElm::~Digest_AuthElm()
{
	digest_fields.H_A1.Wipe();
}

OP_STATUS Digest_AuthElm::SetPassword(OpStringC8 a_passwd)
{
	return password.Set(a_passwd.CStr());
}

const char* Digest_AuthElm::GetPassword() const
{
	return password.CStr();
}

OP_STATUS Digest_AuthElm::GetAuthString(OpStringS8 &ret_str, URL_Rep *url, HTTP_Request *http)
{
	return Calculate_Digest(ret_str, url,
#if !defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY)
						http->info.secure,
#endif
						http->method,
						http->request,
						http->auth_digest,
						http->proxy_digest,
						http);
}

OP_STATUS Digest_AuthElm::GetProxyConnectAuthString(OpStringS8 &ret_str, URL_Rep *url, Base_request_st *request, HTTP_Request_digest_data &proxy_digest)
{
	return Calculate_Digest(ret_str, url,
#if !defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY)
						FALSE,
#endif
						HTTP_METHOD_CONNECT, request, proxy_digest, proxy_digest, NULL);
}

PREFIX_CONST_DOUBLE_ARRAY(static, KeywordIndex, digest_algs, auth)
CONST_DOUBLE_ENTRY(keyword, NULL,       index, DIGEST_UNKNOWN)
CONST_DOUBLE_ENTRY(keyword, "MD5",      index, DIGEST_MD5)
CONST_DOUBLE_ENTRY(keyword, "MD5-SESS", index, DIGEST_MD5_SESS)
#ifdef HTTP_DIGEST_AUTH_SHA1
CONST_DOUBLE_ENTRY(keyword, "SHA",      index, DIGEST_SHA)
CONST_DOUBLE_ENTRY(keyword, "SHA-SESS", index, DIGEST_SHA_SESS)
#endif
CONST_DOUBLE_END(digest_algs)

digest_alg CheckDigestAlgs(const char *val
#ifdef EXTERNAL_DIGEST_API
							, ServerName *host
							, External_Digest_Handler *&digest
#endif
)
{
	digest_alg alg = DIGEST_UNKNOWN;

#ifdef EXTERNAL_DIGEST_API
	digest = NULL;
	if(host && host->Name() && *host->Name())
	{
		OpString8 alg_str;

		if(OpStatus::IsSuccess(alg_str.Set(val)))
		{
			BOOL sess_alg = FALSE;
			int len = alg_str.Length();

			if(len > 5 && OpStringC8(alg_str.CStr() + (len-5)).CompareI("-sess") == 0)
			{
				sess_alg = TRUE;
				alg_str.Delete(len-5);
			}

			OP_STATUS op_err = OpExternalDigestStatus::OK;
			digest = GetExternalDigestMethod(alg_str, host->Name(), op_err);

			if(digest != NULL)
			{
				alg = (digest_alg) (digest->GetMethodID() | (sess_alg ? 0x01 : 0));
			}
		}
	}

#ifdef EXTERNAL_DIGEST_API_TEST
	if(alg == DIGEST_UNKNOWN)
#endif
#endif
		alg = (digest_alg) CheckKeywordsIndex(val, g_digest_algs, CONST_ARRAY_SIZE(auth, digest_algs));

	return alg;
}

OP_STATUS Digest_AuthElm::Init_Digest(ParameterList *header, ServerName *server)
{
	Parameters *param = header->GetParameterByID(HTTP_Authentication_Stale);
	BOOL stale = FALSE;

	if(param && param->Value() && op_stricmp(param->Value(),"true") == 0 &&
		state == 1)
		stale = TRUE;

	if(!stale)
	{
		digest_fields.noncecount = 0;
		digest_fields.opaque.Empty();
		digest_fields.alg = DIGEST_UNKNOWN;
		digest_fields.H_A1.Wipe();
		digest_fields.H_A1.Empty();
		digest_fields.H_A1_size = 0;
		digest_fields.qop.qop_auth = FALSE;
		digest_fields.qop.qop_auth_int = FALSE;
		digest_fields.qop.qop_found = FALSE;


		param = header->GetParameterByID(HTTP_Authentication_Algorithm);
		if(param && param->Value())
		{
#ifdef EXTERNAL_DIGEST_API
			External_Digest_Handler *temp_digest = NULL;
#endif
			digest_fields.alg = CheckDigestAlgs(param->Value()
#ifdef EXTERNAL_DIGEST_API	
				, server, temp_digest
#endif
				);
			if(digest_fields.alg == DIGEST_UNKNOWN)
				goto error;
#ifdef EXTERNAL_DIGEST_API
			digest_handler = temp_digest;
#endif
		}
		else
		{
#ifdef EXTERNAL_DIGEST_API_TEST
			External_Digest_Handler *temp_digest = NULL;

			digest_fields.alg = CheckDigestAlgs("md5", server, temp_digest);
			if(digest_fields.alg == DIGEST_UNKNOWN)
				goto error;
			digest_handler = temp_digest;

			if(digest_fields.alg == DIGEST_UNKNOWN)
#endif
				digest_fields.alg = DIGEST_MD5;
		}


		param = header->GetParameterByID(HTTP_Authentication_Opaque);
		if(param)
			if(OpStatus::IsError(digest_fields.opaque.Set(param->Value())))
				goto error;

		param = header->GetParameterByID(HTTP_Authentication_Qop);
		if(param && param->GetParameters(PARAM_SEP_COMMA | PARAM_NO_ASSIGN, KeywordIndex_Authentication))
		{
			ParameterList *qop_params = param->SubParameters();

			if(qop_params->Empty())
				goto error;

			if(qop_params->GetParameterByID(HTTP_Authentication_Method_Auth))
				digest_fields.qop.qop_found = digest_fields.qop.qop_auth = TRUE;

			if(qop_params->GetParameterByID(HTTP_Authentication_Method_Auth_Int))
				digest_fields.qop.qop_found = digest_fields.qop.qop_auth_int = TRUE;
		}

		switch(digest_fields.alg)
		{
			case DIGEST_MD5:
			case DIGEST_MD5_SESS:
				digest_fields.digest = CRYPTO_HASH_TYPE_MD5;
				
				break;
#ifdef HTTP_DIGEST_AUTH_SHA1
			case DIGEST_SHA:
			case DIGEST_SHA_SESS:
				digest_fields.digest = CRYPTO_HASH_TYPE_SHA1;
#endif // HTTP_DIGEST_AUTH_SHA1
				break;
#ifdef EXTERNAL_DIGEST_API
			default:
				{	
					digest_fields.digest == SSL_NoHash;
					if(digest_handler == NULL)
						goto error;
					digest_fields.digest = digest_handler->GetAlgID();
				}
				break;
#endif
		}
		if(digest_fields.H_A1.Length() == 0)
		{
			digest_fields.H_A1_size = CRYPTO_MAX_HASH_SIZE*2;
			if(digest_fields.H_A1.Reserve(digest_fields.H_A1_size+1) == NULL)
			{
				digest_fields.H_A1_size = 0;
				goto error;
			}
		}
	}

	param = header->GetParameterByID(HTTP_Authentication_Nonce);
	if(param)
	{
		if(OpStatus::IsError(digest_fields.nonce.Set(param->Value() ? param->Value() : "")))
			goto error;
		digest_fields.noncecount = 0;
		state = 0;
	}

	{
		if(digest_fields.qop.qop_found)
		{
			char *cnonce = digest_fields.cnonce.Reserve(1 + (4*34)/3);
			if(!cnonce)
				goto error;

			unsigned char *random = (unsigned char *)g_memory_manager->GetTempBuf2k();
			OP_ASSERT(g_memory_manager->GetTempBuf2kLen() > 32);

			g_libcrypto_random_generator->GetRandom(reinterpret_cast<UINT8*>(random), 32);

			UU_encode(random,32,cnonce);
			OPERA_cleanse_heap(random,32);
		}

		if(state == 0 || digest_fields.alg == DIGEST_MD5
#ifdef HTTP_DIGEST_AUTH_SHA1
			|| digest_fields.alg == DIGEST_SHA
#endif
#ifdef EXTERNAL_DIGEST_API
			|| !EXTERNAL_IS_SESS_DIGEST(digest_fields.alg)
#endif
			)
		{
			OpAutoPtr<CryptoHash> hash(CryptoHash::CreateMD5());
			if (!hash.get())
				return OpStatus::ERR_NO_MEMORY;
#ifdef EXTERNAL_DIGEST_API
			OpParamsStruct1  ext_digest_param;
			
			ext_digest_param.digest_uri = NULL;
			ext_digest_param.servername = (server ? server->Name() : NULL);
			ext_digest_param.port_number = 0;
			ext_digest_param.method = NULL;
			ext_digest_param.realm = GetRealm();
			ext_digest_param.username = GetUser();
			ext_digest_param.password = password.CStr();
			ext_digest_param.nonce = digest_fields.nonce.CStr();
			ext_digest_param.nc_value = digest_fields.noncecount;
			ext_digest_param.cnonce = digest_fields.cnonce.CStr();
			ext_digest_param.qop_value = NULL;
			ext_digest_param.opaque = digest_fields.opaque;
#endif
			hash->InitHash();
			EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_AUTH_H_A1, &ext_digest_param);
			hash->CalculateHash(GetUser());
			hash->CalculateHash(":");
			hash->CalculateHash(GetRealm());
			hash->CalculateHash(":");
			if(password.CStr())
				hash->CalculateHash(password.CStr());


			ConvertToHex(hash, digest_fields.H_A1.DataPtr());
#ifdef DEBUG_AUTH
			PrintfTofile("http.txt","Digest init 1: %s:%s:%s\n",GetUser(),GetRealm(),(password.CStr() ? password.CStr() : ""));
			PrintfTofile("http.txt","Digest init 1 result : %s\n",digest_fields.H_A1.CStr());
#endif

			if(digest_fields.alg == DIGEST_MD5_SESS
#ifdef HTTP_DIGEST_AUTH_SHA1
				|| digest_fields.alg == DIGEST_SHA_SESS
#endif
#ifdef EXTERNAL_DIGEST_API
				|| EXTERNAL_IS_SESS_DIGEST(digest_fields.alg)
#endif
				)
			{
				hash->InitHash();
				EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_AUTH_H_SESS, &ext_digest_param);
				hash->CalculateHash(digest_fields.H_A1.CStr());
				hash->CalculateHash(":");

				const char* nonce = digest_fields.nonce.CStr();
				if (nonce)
					hash->CalculateHash(nonce);

				hash->CalculateHash(":");

				const char* cnonce = digest_fields.cnonce.CStr();
				if (cnonce)
					hash->CalculateHash(cnonce);

#ifdef DEBUG_AUTH
				PrintfTofile("http.txt","Digest init 2: %s:%s:%s\n", digest_fields.H_A1.CStr(), nonce ? nonce : "(no nonce)", cnonce ? cnonce : "(no cnonce)");
#endif
				ConvertToHex(hash, digest_fields.H_A1.DataPtr());
#ifdef DEBUG_AUTH
				PrintfTofile("http.txt","Digest init 2 result : %s\n",digest_fields.H_A1.CStr());
#endif
			}
        }
	}

	state = 1;
	return OpStatus::OK;
error:;
	state = -1;
	return OpStatus::ERR;

}


OP_STATUS Digest_AuthElm::Update_Authenticate(ParameterList *header)
{
	Parameters *param = header->GetParameterByID(HTTP_Authentication_Nextnonce);
	if(param)
	{
		RETURN_IF_ERROR(digest_fields.nonce.Set(param->Value() ? param->Value() : ""));
		digest_fields.noncecount=0;
		if(digest_fields.qop.qop_found)
		{
			char *cnonce = digest_fields.cnonce.Reserve(1 + (4*34)/3);
			if(cnonce)
			{
				unsigned char *random = (unsigned char *)g_memory_manager->GetTempBuf2k();
				OP_ASSERT(g_memory_manager->GetTempBuf2kLen() > 32);

				g_libcrypto_random_generator->GetRandom(reinterpret_cast<UINT8*>(random), 32);
				UU_encode(random,32,cnonce);
				OPERA_cleanse_heap(random,32);
			}
		}


		if(digest_fields.alg == DIGEST_MD5_SESS
#ifdef HTTP_DIGEST_AUTH_SHA1
			|| digest_fields.alg == DIGEST_SHA_SESS
#endif
#ifdef EXTERNAL_DIGEST_API
			|| EXTERNAL_IS_SESS_DIGEST(digest_fields.alg)
#endif
			)
		{
			OpAutoPtr<CryptoHash> hash(CryptoHash::CreateMD5());
			if (!hash.get())
				return OpStatus::ERR_NO_MEMORY; 			
#ifdef EXTERNAL_DIGEST_API
			OpParamsStruct1  ext_digest_param;
			
			ext_digest_param.digest_uri = NULL;
			ext_digest_param.servername = NULL;
			ext_digest_param.port_number = 0;
			ext_digest_param.method = NULL;
			ext_digest_param.realm = GetRealm();
			ext_digest_param.username = GetUser();
			ext_digest_param.password = password.CStr();
			ext_digest_param.nonce = digest_fields.nonce.CStr();
			ext_digest_param.nc_value = digest_fields.noncecount;
			ext_digest_param.cnonce = digest_fields.cnonce.CStr();
			ext_digest_param.qop_value = NULL;
			ext_digest_param.opaque = digest_fields.opaque;
#endif
			hash->InitHash();
			EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_AUTH_H_A1, &ext_digest_param);
			hash->CalculateHash(GetUser());
			hash->CalculateHash(":");
			hash->CalculateHash(GetRealm());
			hash->CalculateHash(":");
			if(password.CStr())
				hash->CalculateHash(password.CStr());

			ConvertToHex(hash, digest_fields.H_A1.DataPtr());

			hash->InitHash();
			hash->CalculateHash(digest_fields.H_A1.CStr());
			hash->CalculateHash(":");

			const char* nonce = digest_fields.nonce.CStr();
			if (nonce)
				hash->CalculateHash(nonce);

			hash->CalculateHash(":");

			const char* cnonce = digest_fields.cnonce.CStr();
			if (cnonce)
				hash->CalculateHash(cnonce);

			ConvertToHex(hash, digest_fields.H_A1.DataPtr());
		}
	}
	return OpStatus::OK;
}


PREFIX_CONST_ARRAY(static, const char*, HTTP_method_texts, auth)
 CONST_ENTRY("GET:")
 CONST_ENTRY("POST:")
 CONST_ENTRY("HEAD:")
 CONST_ENTRY("CONNECT:")
 CONST_ENTRY("PUT:")
 CONST_ENTRY(NULL)
CONST_END(HTTP_method_texts)

OP_STATUS Digest_AuthElm::Calculate_Digest(OpStringS8 &ret_str,
										   URL_Rep *url,
#if !defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY)
										   BOOL secure,
#endif
										   HTTP_Method http_method,
										   Base_request_st* request,
										   HTTP_Request_digest_data &auth_digest,
										   HTTP_Request_digest_data &proxy_digest,
										   Upload_Base* upload_data)
{
	HTTP_Request_digest_data* OP_MEMORY_VAR proxy_digest_ptr = &proxy_digest; // may be clobbered by TRAP.
	HTTP_Request_digest_data* OP_MEMORY_VAR auth_digest_ptr = &auth_digest; // may be clobbered by TRAP.
	OpAutoPtr<CryptoHash> hash(CryptoHash::CreateMD5());
	OpAutoPtr<CryptoHash> hash_A2(CryptoHash::CreateMD5());
	
	if (!hash.get() || !hash_A2.get())
		return OpStatus::ERR_NO_MEMORY;
	
	//char *buffer = (char*)g_memory_manager->GetTempBuf2k();
	//int buflen =  g_memory_manager->GetTempBuf2kLen();

	OP_MEMORY_VAR digest_qop qop = DIGEST_QOP_NONE;
	const char *OP_MEMORY_VAR qoptext = NULL;
	const char *OP_MEMORY_VAR qoptext1 = NULL;

	ret_str.Empty();

#if !defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY)
	OP_MEMORY_VAR HTTP_Method method = (GetScheme() & AUTH_SCHEME_HTTP_PROXY) &&
		(secure ) ?
			 HTTP_METHOD_CONNECT : http_method;
#else
	OP_MEMORY_VAR HTTP_Method method = http_method;
#endif
	if(method >HTTP_METHOD_MAX || size_t(method) >= CONST_ARRAY_SIZE(auth, HTTP_method_texts))
	{
		return OpStatus::OK;
	}

	if(digest_fields.qop.qop_found)
	{
		if(digest_fields.qop.qop_auth_int)
		{
			qop = DIGEST_QOP_AUTH_INT;
			qoptext = "qop=auth-int";
			qoptext1 = "auth-int";
		}
		else if(digest_fields.qop.qop_auth)
		{
			qop = DIGEST_QOP_AUTH;
			qoptext = "qop=auth";
			qoptext1 = "auth";
		}
	}

	// Minimum length response has digest, username, realm, uri and response-digest and spaces
	OP_MEMORY_VAR int len = 7 + 12 + op_strlen(GetUser()) +  11 + 2*hash->Size() +3 ;
	const char * OP_MEMORY_VAR request_uri2 = NULL;
	OpString8 request_uri1;
	ANCHOR(OpString8, request_uri1);

	if(GetRealm())
		len += 10 + op_strlen(GetRealm());

	if((GetScheme() & AUTH_SCHEME_HTTP_PROXY) && method == HTTP_METHOD_CONNECT)
	{
		char *buffer1 = (char *) g_memory_manager->GetTempBuf();

		if(op_snprintf(buffer1, 35, ":%u", request->origin_port)>35)
		{
			OP_ASSERT(0);	// should never happen...
			return OpStatus::ERR;
		}
		request_uri1.SetConcat((request->origin_host != NULL && request->origin_host->Name() ?
						request->origin_host->Name() : ""),  buffer1);
		request_uri2 = request_uri1.CStr();
		if(!request_uri2)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
	}
	else
	{
		request_uri2 = (request->request.HasContent() ? request->request.CStr() : "/");
	}

	OpStringC8 request_uri(request_uri2);

	len += 6 + request_uri.Length();


	if(digest_fields.nonce.HasContent())
		len += 10 + digest_fields.nonce.Length();

	if(digest_fields.cnonce.HasContent() && qop != DIGEST_QOP_NONE)
		len += 11 + digest_fields.cnonce.Length();

	if(digest_fields.opaque.HasContent())
		len += 11 + digest_fields.opaque.Length();

	if(qop != 0)
		len += op_strlen(qoptext) + 12+2; // qopfield + nonce count + spaces;

#ifdef EXTERNAL_DIGEST_API
	OpString8  algtext_ext;
#endif
const char *OP_MEMORY_VAR algtext = NULL;

	switch(digest_fields.alg)
	{
	case DIGEST_MD5 : algtext = "MD5"; break;
	case DIGEST_MD5_SESS : algtext = "MD5-SESS"; break;
#ifdef HTTP_DIGEST_AUTH_SHA1
	case DIGEST_SHA : algtext = "SHA"; break;
	case DIGEST_SHA_SESS : algtext = "SHA-SESS"; break;
#endif
#ifdef EXTERNAL_DIGEST_API
	default:
		if(digest_handler)
		{
			algtext_ext.SetConcat(digest_handler->Name(), (EXTERNAL_IS_SESS_DIGEST(digest_fields.alg) ? "-SESS" : ""));
		}
		algtext = (algtext_ext.CStr() ? algtext_ext.CStr() : "");
		break;
#endif

	}

	len += 30; // Digest algs argument

#ifdef EXTERNAL_DIGEST_API
	OpParamsStruct1  ext_digest_param;
	
	ext_digest_param.digest_uri = request_uri;
	ext_digest_param.servername = ((GetScheme() & AUTH_SCHEME_HTTP_PROXY) ? request->connect_host->Name() : request->origin_host->Name());
	ext_digest_param.port_number = ((GetScheme() & AUTH_SCHEME_HTTP_PROXY) ? request->connect_port: request->origin_port);
	ext_digest_param.method = g_HTTP_method_texts[method];
	ext_digest_param.realm = GetRealm();
	ext_digest_param.username = GetUser();
	ext_digest_param.password = password.CStr();
	ext_digest_param.nonce = digest_fields.nonce.CStr();
	ext_digest_param.nc_value = digest_fields.noncecount;
	ext_digest_param.cnonce = digest_fields.cnonce.CStr();
	ext_digest_param.qop_value = qoptext1;
	ext_digest_param.opaque = digest_fields.opaque;
#endif
	OpString8 buffer;
	ANCHOR(OpString8, buffer);

	TRAP_AND_RETURN(op_err, buffer.ReserveL(len+60));

	if(op_snprintf(buffer.DataPtr(), buffer.Capacity(), "Digest username=\"%s\", realm=\"%s\", uri=\"%s\", "
					"algorithm=%s, nonce=\"%s\", ",
				(GetUser() ? GetUser() : ""),
				(GetRealm() ? GetRealm() : ""),
				request_uri.CStr(), algtext,
				digest_fields.nonce.CStr()) > buffer.Capacity())
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	if(digest_fields.cnonce.CStr() && qop != DIGEST_QOP_NONE)
		buffer.AppendFormat("cnonce=\"%s\", ", digest_fields.cnonce.CStr());
	if(digest_fields.opaque.CStr())
		buffer.AppendFormat("opaque=\"%s\", ", digest_fields.opaque.CStr());

	if(qoptext != NULL)
	{
		digest_fields.noncecount++;
		buffer.AppendFormat("%s, nc=%0.8x, ", qoptext, digest_fields.noncecount);
	}

	HTTP_Request_digest_data * OP_MEMORY_VAR used = (((GetScheme() & AUTH_SCHEME_HTTP_PROXY) != 0) ? proxy_digest_ptr : auth_digest_ptr);

	used->hash_base = digest_fields.digest;
	OP_STATUS op_err = OpStatus::OK;
	OP_DELETE(used->entity_hash);

	used->entity_hash = CryptoHash::CreateMD5();
	OpStatus::Ignore(op_err);
	if(used->entity_hash)
	{
		used->entity_hash->InitHash();
		EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_UNSPECIFIED, &ext_digest_param);
	}
	
	RETURN_IF_ERROR(SetStr(used->used_A1, digest_fields.H_A1.CStr()));
	RETURN_IF_ERROR(SetStr(used->used_nonce, digest_fields.nonce.CStr()));
	RETURN_IF_ERROR(SetStr(used->used_cnonce, digest_fields.cnonce.CStr()));
	
	used->used_noncecount = digest_fields.noncecount;

	hash->InitHash();
	EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_AUTH_KD, &ext_digest_param);
	hash->CalculateHash(digest_fields.H_A1.CStr());
	hash->CalculateHash(":");
#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 1a: %s:\n",digest_fields.H_A1.CStr());
#endif

	hash_A2->InitHash();
	EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_AUTH_H_A2, &ext_digest_param);
	hash_A2->CalculateHash(g_HTTP_method_texts[method]);
	hash_A2->CalculateHash(request_uri.CStr());
#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 2a: %s%s\n",g_HTTP_method_texts[method], request_uri.CStr());
#endif

	switch(qop)
	{
		case DIGEST_QOP_NONE :
		{
			const char* nonce = digest_fields.nonce.CStr();
			if (nonce)
				hash->CalculateHash(nonce);

#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 1b: %s\n", nonce ? nonce : "(no nonce)");
#endif
			break;
		}

		case DIGEST_QOP_AUTH_INT :
		{
			OpAutoPtr<CryptoHash> hash_body(CryptoHash::CreateMD5());
			if (!hash_body.get())
				return OpStatus::ERR_NO_MEMORY;

			hash_body->InitHash();
			EXTERNAL_PERFORM_INIT(hash, OP_DIGEST_OPERATION_UNSPECIFIED, &ext_digest_param);
#ifdef _FILE_UPLOAD_SUPPORT_
			if(method == HTTP_METHOD_POST || method == HTTP_METHOD_PUT)
			{
				char *buffer1 = (char *) g_memory_manager->GetTempBuf2k();
				int max_len = g_memory_manager->GetTempBuf2kLen();
				int len;
				BOOL done;

				upload_data->ResetL();

				done = FALSE;

				while(!done)
				{
					len = 0;
					uint32 remaining_len = max_len-1;
					upload_data->OutputHeaders((unsigned char *) buffer1, remaining_len, done);
					len = max_len -1 - remaining_len;
					OP_ASSERT(done); // too small buffer;
				}

				done = FALSE;
				while(!done)
				{
					len = 0;
					len = upload_data->GetOutputData((unsigned char *) buffer1, max_len-1, done);
					hash_body->CalculateHash((unsigned char *) buffer1,len);
				}

				upload_data->ResetL();
			}
#endif // _FILE_UPLOAD_SUPPORT_
			char *digest1 = (char *) g_memory_manager->GetTempBuf();

			ConvertToHex(hash_body, digest1);

#ifdef DEBUG_AUTH
			PrintfTofile("http.txt","Digest calc 1c: :%s\n",digest1);
#endif
			hash_A2->CalculateHash(":");
			hash_A2->CalculateHash(digest1);

			OPERA_cleanse_heap(digest1, 2 * CRYPTO_MAX_HASH_SIZE + 1);
			// Fall through, the next calculation is done for both DIGEST_QOP_AUTH_INT and DIGEST_QOP_AUTH
		}
		case DIGEST_QOP_AUTH :
		{
			const char* nonce = digest_fields.nonce.CStr();
			if (nonce)
				hash->CalculateHash(nonce);

			hash->CalculateHash(":");
#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 1d: %s:\n", nonce ? nonce : "(no nonce)");
#endif
			{
				char *nc  = (char *) g_memory_manager->GetTempBuf();

				op_snprintf(nc, g_memory_manager->GetTempBufLen(), "%.8lx:",digest_fields.noncecount);
				hash->CalculateHash(nc);
#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 1e: %s\n",nc);
#endif
			}

			const char* cnonce = digest_fields.cnonce.CStr();
			if (cnonce)
				hash->CalculateHash(cnonce);

			hash->CalculateHash(":");

			if (qoptext1)
				hash->CalculateHash(qoptext1);

#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 1f: %s:%s\n", cnonce ? cnonce : "(no cnonce)", (qoptext1 ? qoptext1: "(no qoptext1)"));
#endif
			break;
		}
	}


	{
		char *digest1 = (char *) g_memory_manager->GetTempBuf();

		ConvertToHex(hash_A2, digest1);
#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 2 result: %s\n",digest1);
#endif

		hash->CalculateHash(":");
		hash->CalculateHash(digest1);
#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 1g: :%s\n",digest1);
#endif

		ConvertToHex(hash, digest1);
#ifdef DEBUG_AUTH
	PrintfTofile("http.txt","Digest calc 1 result: %s\n",digest1);
#endif

		buffer.AppendFormat("response=\"%s\"",(char *) digest1);

		OPERA_cleanse_heap(digest1, 2 * CRYPTO_MAX_HASH_SIZE + 1);
	}

	ret_str.TakeOver(buffer);
	return OpStatus::OK;
}

#endif // _Enable Authenticate

#ifdef ENABLE_CONVERT_TO_HEX
const char /* const*/ hexchars1[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};


void ConvertToHex(OpAutoPtr<CryptoHash> &hash, char *buffer)
{
	ConvertToHex(hash.get(), buffer);
}

void ConvertToHex(CryptoHash *hash, char *buffer)
{
	unsigned char *digest= (unsigned char *)g_memory_manager->GetTempBuf2k();
	OP_ASSERT(g_memory_manager->GetTempBuf2kLen() > static_cast<unsigned int>(hash->Size()));
	char *pos;
	int len, i;

	hash->ExtractHash(digest);
	len = hash->Size();
	pos = buffer;

	for(i = 0;i<len;i++)
	{
		unsigned char val = digest[i];
		*(pos++) = hexchars1[(val>>4) & 0x0f];
		*(pos++) = hexchars1[val & 0x0f];
	}
	*(pos++) = '\0';

	OPERA_cleanse_heap(digest, hash->Size());	
}
#endif
