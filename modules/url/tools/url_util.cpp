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

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/handy.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/encodings/decoders/inputconverter.h"

#include "modules/url/url2.h"
#include "modules/url/url_rep.h"

#include "modules/formats/argsplit.h"
#include "modules/url/url_tools.h"

/* Restricted Port List (primarily 1-1024)*/
const unsigned short restriced_port_list[] = {
	25, 110, 80, 20, 21, 70, 119, 443, 563, 210, 587, 143, 993, 995, 194, 6667, // Ports used by Opera
	1,7,9,11,13,15,17,19,20,21, 22,23,37,42,43,53,77,79,
	109,111,113,115,117,135, 389,512,513,514,515, 526,
	530,531,532, 540, 556, 601	
};

OP_STATUS ConvertUrlStatusToOpStatus(unsigned long url_status)
{
	switch (url_status)
	{
	case 0:
		return OpStatus::OK;
	case URL_ERRSTR(SI, ERR_ACCESS_DENIED):
		return OpStatus::ERR_NO_ACCESS;
	case URL_ERRSTR(SI, ERR_FILE_CANNOT_OPEN):
		return OpStatus::ERR_NO_ACCESS;
	case URL_ERRSTR(SI, ERR_FILE_DOES_NOT_EXIST):
		return OpStatus::ERR_FILE_NOT_FOUND;
	case URL_ERRSTR(SI, ERR_DISK_IS_FULL):
		return OpStatus::ERR_NO_DISK;
	case URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR):
		return OpStatus::ERR;
	default:
		OP_ASSERT(!"Handle this error");
		return OpStatus::ERR;
	}
}

Str::LocaleString ConvertUrlStatusToLocaleString(unsigned long url_status)
{
#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
	return (Str::LocaleString) url_status;
#else
	switch (url_status)
	{
	case ERR_UNKNOWN_ADDRESS_TYPE: return Str::SI_ERR_UNKNOWN_ADDRESS_TYPE;

	case ERR_OUT_OF_MEMORY: return Str::SI_ERR_OUT_OF_MEMORY;
	case ERR_REDIRECT_FAILED: return Str::SI_ERR_REDIRECT_FAILED;

	case ERR_AUTO_PROXY_CONFIG_FAILED: return Str::SI_ERR_AUTO_PROXY_CONFIG_FAILED;

	case ERR_ILLEGAL_URL: return Str::SI_ERR_ILLEGAL_URL;
#ifdef STRICT_CACHE_LIMIT
	case ERR_LOADING_ABORTED_OOM: return Str::D_ERR_LOADING_ABORTED_OOM;
#endif
	case ERR_COMM_CONNECTION_CLOSED: return Str::SI_ERR_COMM_CONNECTION_CLOSED;
	case ERR_NETWORK_PROBLEM: return Str::SI_ERR_NETWORK_PROBLEM;
	case ERR_WINSOCK_BLOCKING: return Str::SI_ERR_WINSOCK_BLOCKING;
	case ERR_WINSOCK_RESOURCE_FAIL: return Str::SI_ERR_WINSOCK_RESOURCE_FAIL;
	case ERR_COMM_INTERNAL_ERROR: return Str::SI_ERR_COMM_INTERNAL_ERROR;
	case ERR_COMM_CONNECT_FAILED: return Str::SI_ERR_COMM_CONNECT_FAILED;
	case ERR_WINSOCK_WRONG_VERSION: return Str::SI_ERR_WINSOCK_WRONG_VERSION;
	case ERR_COMM_HOST_NOT_FOUND: return Str::SI_ERR_COMM_HOST_NOT_FOUND;
	case ERR_COMM_HOST_UNAVAILABLE: return Str::SI_ERR_COMM_HOST_UNAVAILABLE;
	case ERR_COMM_CONNECTION_REFUSED: return Str::SI_ERR_COMM_CONNECTION_REFUSED;
	case ERR_COMM_REPEATED_FAILED: return Str::SI_ERR_COMM_REPEATED_FAILED;
	case ERR_COMM_NETWORK_UNREACHABLE: return Str::SI_ERR_COMM_NETWORK_UNREACHABLE;
	case ERR_WINSOCK_UNSUPPORTED_FUNC: return Str::SI_ERR_WINSOCK_UNSUPPORTED_FUNC;
	case ERR_WINSOCK_NO_RECOVERY: return Str::SI_ERR_WINSOCK_NO_RECOVERY;
	case ERR_CONNECT_TIMED_OUT: return Str::SI_ERR_CONNECT_TIMED_OUT;
	case ERR_COMM_PROXY_CONNECT_FAILED: return Str::SI_ERR_COMM_PROXY_CONNECT_FAILED;
	case ERR_COMM_PROXY_HOST_NOT_FOUND: return Str::SI_ERR_COMM_PROXY_HOST_NOT_FOUND;
	case ERR_COMM_PROXY_HOST_UNAVAILABLE: return Str::SI_ERR_COMM_PROXY_HOST_UNAVAILABLE;
	case ERR_COMM_PROXY_CONNECTION_REFUSED: return Str::SI_ERR_COMM_PROXY_CONNECTION_REFUSED;
	case ERR_UNSUPPORTED_SERVER_AUTH: return Str::SI_ERR_UNSUPPORTED_SERVER_AUTH;
	case ERR_UNSUPPORTED_PROXY_AUTH: return Str::SI_ERR_UNSUPPORTED_PROXY_AUTH;
	case ERR_ACCESS_DENIED: return Str::SI_ERR_ACCESS_DENIED;
	case ERR_FILE_DOES_NOT_EXIST: return Str::SI_ERR_FILE_DOES_NOT_EXIST;
	case ERR_DISK_IS_FULL: return Str::SI_ERR_DISK_IS_FULL;
	case ERR_CACHE_INTERNAL_ERROR: return Str::SI_ERR_CACHE_INTERNAL_ERROR;
	case ERR_AUTH_DOMAIN_MATCH: return Str::SI_ERR_AUTH_DOMAIN_MATCH;
		// case ERR_COMM_OUT_STATE: return Str::SI_ERR_COMM_OUT_STATE;
		// case ERR_COMM_RESERVED_MAC1: return Str::SI_ERR_COMM_RESERVED_MAC1;
#ifdef URL_FILTER
	case ERR_COMM_BLOCKED_URL: return Str::SI_ERR_COMM_BLOCKED_URL;
#endif

	case ERR_HTTP_FORBIDDEN: return Str::SI_ERR_HTTP_FORBIDDEN;
	case ERR_HTTP_NOT_FOUND: return Str::SI_ERR_HTTP_NOT_FOUND;
	case ERR_HTTP_SERVER_ERROR: return Str::SI_ERR_HTTP_SERVER_ERROR;
	case ERR_HTTP_NOT_IMPLEMENTED: return Str::SI_ERR_HTTP_NOT_IMPLEMENTED;
	case ERR_HTTP_NO_CONTENT: return Str::SI_ERR_HTTP_NO_CONTENT;
	case ERR_HTTP_100CONTINUE_ERROR: return Str::SI_ERR_HTTP_100CONTINUE_ERROR;
	case ERR_HTTP_DECODING_FAILURE: return Str::SI_ERR_HTTP_DECODING_FAILURE;
	case ERR_HTTP_AUTH_FAILED: return Str::SI_ERR_HTTP_AUTH_FAILED;
	case ERR_HTTP_PROXY_AUTH_FAILED: return Str::SI_ERR_HTTP_PROXY_AUTH_FAILED;
	case ERR_ILLEGAL_PORT: return Str::SI_ERR_ILLEGAL_PORT;
#ifndef NO_FTP_SUPPORT
	case ERR_FTP_SERVICE_UNAVAILABLE: return Str::SI_ERR_FTP_SERVICE_UNAVAILABLE;
	case ERR_FTP_INTERNAL_ERROR: return Str::SI_ERR_FTP_INTERNAL_ERROR;
	case ERR_FTP_NOT_LOGGED_IN: return Str::SI_ERR_FTP_NOT_LOGGED_IN;
	case ERR_FTP_USER_ERROR: return Str::SI_ERR_FTP_USER_ERROR;
	case ERR_FTP_NEED_PASSWORD: return Str::SI_ERR_FTP_NEED_PASSWORD;
		// case ERR_FTP_DIR_UNAVAILABLE: return Str::SI_ERR_FTP_DIR_UNAVAILABLE;
	case ERR_FTP_FILE_UNAVAILABLE: return Str::SI_ERR_FTP_FILE_UNAVAILABLE;
	case ERR_FTP_FILE_TRANSFER_ABORTED: return Str::SI_ERR_FTP_FILE_TRANSFER_ABORTED;
	case ERR_FTP_CANNOT_OPEN_PASV_DATA_CONN: return Str::SI_ERR_FTP_CANNOT_OPEN_PASV_DATA_CONN;
#endif
	case ERR_FILE_CANNOT_OPEN: return Str::SI_ERR_FILE_CANNOT_OPEN;
	case ERR_SSL_ERROR_HANDLED: return Str::NOT_A_STRING;
	case ERR_HTTP_METHOD_NOT_ALLOWED: return Str::SI_ERR_HTTP_METHOD_NOT_ALLOWED;
	case ERR_HTTP_NOT_ACCEPTABLE: return Str::SI_ERR_HTTP_NOT_ACCEPTABLE;
	case ERR_HTTP_TIMEOUT: return Str::SI_ERR_HTTP_TIMEOUT;
	case ERR_HTTP_CONFLICT: return Str::SI_ERR_HTTP_CONFLICT;
	case ERR_HTTP_GONE: return Str::SI_ERR_HTTP_GONE;
	case ERR_HTTP_LENGTH_REQUIRED: return Str::SI_ERR_HTTP_LENGTH_REQUIRED;
	case ERR_HTTP_PRECOND_FAILED: return Str::SI_ERR_HTTP_PRECOND_FAILED;
	case ERR_HTTP_REQUEST_ENT_TOO_LARGE: return Str::SI_ERR_HTTP_REQUEST_ENT_TOO_LARGE;
	case ERR_HTTP_REQUEST_URI_TOO_LARGE: return Str::SI_ERR_HTTP_REQUEST_URI_TOO_LARGE;
	case ERR_HTTP_UNSUPPORTED_MEDIA_TYPE: return Str::SI_ERR_HTTP_UNSUPPORTED_MEDIA_TYPE;
	case ERR_HTTP_RANGE_NOT_SATISFIABLE: return Str::SI_ERR_HTTP_RANGE_NOT_SATISFIABLE;
	case ERR_HTTP_EXPECTATION_FAILED: return Str::SI_ERR_HTTP_EXPECTATION_FAILED;
	case ERR_HTTP_BAD_GATEWAY: return Str::SI_ERR_HTTP_BAD_GATEWAY;
	case ERR_HTTP_SERVICE_UNAVAILABLE: return Str::SI_ERR_HTTP_SERVICE_UNAVAILABLE;
	case ERR_HTTP_GATEWAY_TIMEOUT: return Str::SI_ERR_HTTP_GATEWAY_TIMEOUT;
	case ERR_HTTP_VERSION_NOT_SUPPORTED: return Str::SI_ERR_HTTP_VERSION_NOT_SUPPORTED;
	case ERR_HTTP_PAYMENT_REQUIRED: return Str::SI_ERR_HTTP_PAYMENT_REQUIRED;
	case ERR_URL_TIMEOUT: return Str::SI_ERR_URL_TIMEOUT;
	case ERR_URL_IDLE_TIMEOUT: return Str::SI_ERR_URL_IDLE_TIMEOUT;
	default: return (Str::LocaleString) url_status;
	}
#endif // HC_CAP_ERRENUM_AS_STRINGENUM
}

unsigned long GetFileError(OP_STATUS op_err, URL_Rep *url, const uni_char *operation)
{
	uni_char *temp = (uni_char *) g_memory_manager->GetTempBuf2k();
	temp[0] = 0;

	switch(op_err)
	{
	case OpStatus::OK: return 0;
	case OpStatus::ERR_NO_ACCESS: return URL_ERRSTR(SI, ERR_ACCESS_DENIED);
	case OpStatus::ERR_FILE_NOT_FOUND: return URL_ERRSTR(SI, ERR_FILE_DOES_NOT_EXIST);
	case OpStatus::ERR_NO_DISK: return URL_ERRSTR(SI, ERR_DISK_IS_FULL);
	}

	OP_ASSERT( UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()) > 300);
	uni_snprintf(temp, 300, UNI_L("%s failed (%d)"), operation, (int) op_err);
	temp[299] = 0; // MSWIN snprintf does not nul terminate

	if(url)
		url->SetAttribute(URL::KInternalErrorString, temp);
#ifdef _DEBUG
	else
	{
		// Only needed during debug
		uni_char *temp_error = (uni_char *) g_memory_manager->GetTempBuf();
		size_t temp_errorlen = g_memory_manager->GetTempBufLen();
		uni_strlcpy(temp_error , temp, temp_errorlen);
	}
#endif

	return Str::SI_DEF_STRING;
}

BOOL Is_Restricted_Port(ServerName *server, unsigned short port, URLType type)
{
	if(port == 0)
		return FALSE;

	unsigned short protocolport=0;
	switch(type)
	{
	case URL_HTTP : 
	case URL_WEBSOCKET :
		if(port == 443)
			return FALSE;
		protocolport = 80;
		break;
	case URL_HTTPS : 
		protocolport = 443;
		break;
	case URL_WEBSOCKET_SECURE :
		if(port == 80)
			return FALSE;
		protocolport = 443;
		break;
	case URL_FTP : 
		protocolport = 21;
		break;
	case URL_NEWS : 
		protocolport = 119;
		break;
	case URL_SNEWS : 
		protocolport = 563;
		break;
	case URL_Gopher : 
		protocolport = 70;	
		break;
	case URL_WAIS : 
		protocolport = 210;
		break;

	default: break; // many other kinds of URL not handled.
	}
	if(protocolport == port)
		return FALSE;

	int i;
	const unsigned short *pos = restriced_port_list;
	for (i = ARRAY_SIZE(restriced_port_list); i >0;i--)
		if((*pos++) == port)
		{
			OpStringC values = g_pcnet->GetStringPref(PrefsCollectionNetwork::PermittedPorts, server);

			if(values.HasContent())
			{
				UniParameterList list;

				list.SetValue(values.CStr(), PARAM_SEP_COMMA | NVS_VAL_SEP_NO_ASSIGN);

				UniParameters *item = list.First();
				while(item)
				{
					unsigned int item_port = item->GetUnsignedName();
					
					if(item_port == port)
						return FALSE;

					item = item->Suc();
				}
			}

			return TRUE;
		}

	return FALSE;
}

Str::LocaleString GetCrossNetworkError(OpSocketAddressNetType referrer_net, OpSocketAddressNetType host_net)
{
	if(referrer_net> NETTYPE_PRIVATE)
	{
		if(host_net >= NETTYPE_PRIVATE) 
			return Str::S_MSG_CROSS_NETWORK_INTERNET_INTRANET;
		else
			return Str::S_MSG_CROSS_NETWORK_INTERNET_LOCALHOST;
	}

	return Str::S_MSG_CROSS_NETWORK_INTRANET_LOCALHOST;
}

BOOL IsRedirectResponse(int response)
{
	return (response == HTTP_MOVED
		||	response == HTTP_FOUND
		||	response == HTTP_TEMPORARY_REDIRECT
		||	response == HTTP_MULTIPLE_CHOICES
		||	response == HTTP_SEE_OTHER);
}

OP_STATUS SquashStatus(OP_STATUS e, OP_STATUS v1, OP_STATUS v2, OP_STATUS v3, OP_STATUS v4, OP_STATUS v5)
{
	return (e == v1 || e == v2 || e == v3 || e == v4 || e == v5) ? e : (OpStatus::IsSuccess(e) ? OpStatus::OK : OpStatus::ERR);
}
