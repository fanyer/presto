/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _SUPPORT_PROXY_NTLM_AUTH_

#define SECURITY_WIN32

#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/base64_decode.h"
#include "modules/formats/encoder.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#include "modules/auth/auth_elm.h"
#include "platforms/windows/auth/auth_ntlm.h"

AuthScheme CheckAuthenticateName(const char *name);

void ShutdownNTLMAUth()
{
	NTLM_AuthElm::Terminate();
}

BOOL SetUpNTLM_NegotiateL(BOOL proxy, HeaderList &headers, HTTP_Request *request, BOOL &set_waiting, AuthElm *auth_element, BOOL first_request)
{
	BOOL ret = FALSE;

	set_waiting = FALSE;

	ParameterList *ntlm_params = NULL;
	ParameterList *negotiate_params = NULL;
	HeaderEntry *header = headers.GetHeaderByID((proxy ? HTTP_Header_Proxy_Authenticate : HTTP_Header_WWW_Authenticate),NULL);
	
	while(header)
	{
		ParameterList *params = header->GetParameters((PARAM_SEP_COMMA | PARAM_WHITESPACE_FIRST_ONLY | PARAM_STRIP_ARG_QUOTES | PARAM_NO_ASSIGN), KeywordIndex_Authentication);//FIXME:OOM-yngve Can return NULL due to OOM, should this be signalled?
		
		if(params && params->First())
		{
			Parameters *param = params->First();

			AuthScheme scheme = (AuthScheme) param->GetNameID();

			switch(scheme)
			{
			case AUTH_SCHEME_HTTP_NEGOTIATE:
				if(!negotiate_params)
				{
					negotiate_params = params;
				}
				break;
			case AUTH_SCHEME_HTTP_NTLM:
				if(/*proxy && */!ntlm_params)
				{
					ntlm_params = params;
				}
				break;
			case AUTH_SCHEME_HTTP_UNKNOWN:
			case AUTH_SCHEME_HTTP_BASIC:
				break;
			default:
				{
					// Basic does not override NTLM, Digest and more advanced (known) methods do.
					ntlm_params = NULL;
					negotiate_params = NULL;
					HeaderEntry *next_header = headers.GetHeaderByID((proxy ? HTTP_Header_Proxy_Authenticate : HTTP_Header_WWW_Authenticate),NULL);
					while(next_header)
					{
						header = next_header;
						next_header  = headers.GetHeaderByID(0, header);
						
						ParameterList *params = header->GetParameters((PARAM_SEP_COMMA | PARAM_WHITESPACE_FIRST_ONLY | PARAM_STRIP_ARG_QUOTES | PARAM_NO_ASSIGN), KeywordIndex_Authentication);//FIXME:OOM-yngve Can return NULL due to OOM, should this be signalled?
						
						if(params && params->First())
						{
							Parameters *param = params->First();
							
							AuthScheme scheme = (AuthScheme) param->GetNameID();
							
							if(scheme == AUTH_SCHEME_HTTP_NEGOTIATE || scheme == AUTH_SCHEME_HTTP_NTLM)
							{
								header->Out();
								delete header;
							}
						}
					}
					
					header = NULL;
					goto finished_ntlm_search;
				}
			}
		}
		header = headers.GetHeaderByID(0, header);
	}
finished_ntlm_search:;

	if(ntlm_params || negotiate_params)
	{
		if(auth_element)
		{
			if(negotiate_params && (auth_element->GetScheme() & AUTH_SCHEME_HTTP_MASK)  != AUTH_SCHEME_HTTP_NTLM)
			{
				if(negotiate_params->Cardinal() == 1)
				{
					set_waiting = TRUE;
					if(first_request)
						return FALSE;
				}
				else
					LEAVE_IF_ERROR(auth_element->Update_Authenticate(negotiate_params));
			}
			if(ntlm_params && (auth_element->GetScheme() & AUTH_SCHEME_HTTP_MASK) != AUTH_SCHEME_HTTP_NEGOTIATE)
			{
				if(ntlm_params->Cardinal() == 1)
				{
					set_waiting = TRUE;
					if(first_request)
						return FALSE;
				}
				else
					LEAVE_IF_ERROR(auth_element->Update_Authenticate(ntlm_params ));
			}
			
			OpStringS8 auth_str;
			ANCHOR(OpStringS8, auth_str);
			LEAVE_IF_ERROR(auth_element->GetAuthString(auth_str, NULL, NULL));
			if(proxy)
				request->SetProxyAuthorization(auth_str);
			else
				request->SetAuthorization(auth_str);
			
			ret = auth_str.HasContent();
			set_waiting = TRUE;
		}
	}
	
	return ret;
}

PSecurityFunctionTable NTLM_AuthElm::SecurityInterface = 0;
//SecPkgInfo *NTLM_AuthElm::SecurityPackages;
//DWORD NTLM_AuthElm::NumOfPkgs;
//DWORD NTLM_AuthElm::PkgToUseIndex;
//const char * const Base64_Encoding_chars =
//		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";


AuthElm *CreateNTLM_Element(authenticate_params &auth_params,const char *user_name, const char *user_password)
{
	NTLM_AuthElm *elm = new NTLM_AuthElm(auth_params.scheme,  (auth_params.proxy ? URL_HTTP : (URLType) auth_params.url.GetAttribute(URL::KType)), auth_params.port);

	if(!elm)
		return NULL;

	if(OpStatus::IsError(elm->Construct(user_name, user_password)) ||
		OpStatus::IsError(elm->SetHost(auth_params.servername, auth_params.port)) )
	{
		delete elm;
		return NULL;
	}

	return elm;
}


NTLM_AuthElm::NTLM_AuthElm(AuthScheme scheme,	URLType a_urltype, 	unsigned short a_port)
: AuthElm_Base(a_port, scheme, a_urltype)
{
	gotten_credentials = FALSE;
	try_negotiate = try_ntml = FALSE;
	memset(&Credentials, 0, sizeof(Credentials));
	memset(&SecurityContext, 0, sizeof(SecurityContext));
}

NTLM_AuthElm::~NTLM_AuthElm()
{
	last_challenge.Wipe();
	if(SecurityInterface)
	{
		(*SecurityInterface->FreeCredentialHandle)(&Credentials);
		(*SecurityInterface->DeleteSecurityContext)(&SecurityContext);
	}
}

OP_STATUS NTLM_AuthElm::Construct(OpStringC8 a_user, OpStringC8 a_passwd)
{
	RETURN_IF_ERROR(AuthElm_Base::Construct("", NULL));
	RETURN_IF_ERROR(password.SetFromUTF8(a_passwd));
	int pos = a_user.FindFirstOf('\\');
	if(pos != KNotFound)
	{
		RETURN_IF_ERROR(domain.SetFromUTF8(a_user, pos));
		pos ++;
	}
	else 
	{
		domain.Set("");
		pos = 0;
	}

	RETURN_IF_ERROR(user_name.SetFromUTF8(a_user+pos));

	id.User = user_name.CStr();//UNI_L("Administrator");
	id.UserLength = user_name.Length(); // uni_strlen(id.User);
	id.Domain = domain.CStr(); // UNI_L("HADES");
	id.DomainLength = domain.Length(); // (id.Domain ? uni_strlen(id.Domain) : 0);
	id.Password = password.CStr(); // UNI_L("OperaQA");
	id.PasswordLength = password.Length(); // uni_strlen(id.Password);
	id.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	return OpStatus::OK;
}

OP_STATUS NTLM_AuthElm::Construct(NTLM_AuthElm *master)
{
	if(master == NULL)
		return OpStatus::ERR_NULL_POINTER;

	SetId(master->GetId());
	RETURN_IF_ERROR(AuthElm_Base::Construct(NULL, NULL));
	RETURN_IF_ERROR(password.Set(master->password));
	RETURN_IF_ERROR(domain.Set(master->domain));
	RETURN_IF_ERROR(user_name.Set(master->user_name));
	RETURN_IF_ERROR(SPN.Set(master->SPN));

	Credentials = master->Credentials;
	memset(&master->Credentials, 0, sizeof(Credentials));
	SecurityContext = master->SecurityContext;
	memset(&master->SecurityContext, 0, sizeof(SecurityContext));
	gotten_credentials = master->gotten_credentials;
	master->gotten_credentials = FALSE;
	try_negotiate = master->try_negotiate;
	master->try_negotiate = FALSE;
	try_ntml = master->try_ntml;
	master->try_ntml = FALSE;

	id.User = user_name.CStr();//UNI_L("Administrator");
	id.UserLength = user_name.Length(); // uni_strlen(id.User);
	id.Domain = domain.CStr(); // UNI_L("HADES");
	id.DomainLength = domain.Length(); // (id.Domain ? uni_strlen(id.Domain) : 0);
	id.Password = password.CStr(); // UNI_L("OperaQA");
	id.PasswordLength = password.Length(); // uni_strlen(id.Password);
	id.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	return OpStatus::OK;
}

OP_STATUS NTLM_AuthElm::SetHost(ServerName *sn, unsigned short port)
{
	SPN.Empty();
	if(sn == NULL || !sn->UniName())
		return OpStatus::OK;

	return SPN.AppendFormat(UNI_L("www/%s:%d/"), sn->UniName(), (port ? port : 80));
}

void NTLM_AuthElm::Terminate()
{
	/*
	if(SecurityPackages)
	{
		FreeContextBuffer(SecurityPackages);
		SecurityPackages = NULL;
	}
	*/
	SecurityInterface = NULL;
}
    
OP_STATUS NTLM_AuthElm::GetAuthString(OpStringS8 &ret_str, URL_Rep *url,
#if defined(SSL_ENABLE_DELAYED_HANDSHAKE) && (!defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY))
										  BOOL secure,
#endif
										  HTTP_Method http_method,
										  HTTP_request_st* request,
#ifdef HTTP_DIGEST_AUTH
										  HTTP_Request_digest_data *auth_digest,
										  HTTP_Request_digest_data *proxy_digest,
#endif
										  Upload_Base* upload_data,
										  OpStringS8 &data)
{
	return GetAuthString(ret_str,url, NULL);
};

void NTLM_AuthElm::ClearAuth()
{
	last_challenge.Empty();
	ntlm_challenge.Empty();
	if(gotten_credentials)
	{
		(*SecurityInterface->FreeCredentialHandle)(&Credentials);
		(*SecurityInterface->DeleteSecurityContext)(&SecurityContext);
		memset(&Credentials, 0, sizeof(Credentials));
		memset(&SecurityContext, 0, sizeof(SecurityContext));
		gotten_credentials = FALSE;
	}
}


OP_STATUS NTLM_AuthElm::GetAuthString(OpStringS8 &ret_str, URL_Rep *, HTTP_Request *http)
{
	SECURITY_STATUS status;

	ret_str.Empty();

	if(!SecurityInterface)
	{
		SecurityInterface = InitSecurityInterface();

		if(!SecurityInterface)
		{
			Terminate();
			return OpStatus::OK;
		}
	}

	TimeStamp stamp;
	BOOL initializing = FALSE;

	AuthScheme scheme = (GetScheme() & AUTH_SCHEME_HTTP_MASK);

	if(!gotten_credentials)
	{
		do{


		status = (*SecurityInterface->AcquireCredentialsHandle)(
					NULL,
					(scheme == AUTH_SCHEME_HTTP_NTLM_NEGOTIATE || 
					scheme == AUTH_SCHEME_HTTP_NEGOTIATE  ? TEXT("Negotiate") : TEXT("NTLM")),
					SECPKG_CRED_OUTBOUND,
					0,
					&id,
					0,
					0,
					&Credentials,
					&stamp
					);
			if(status == SEC_E_OK)
			{
				if(scheme == AUTH_SCHEME_HTTP_NTLM_NEGOTIATE )
					SetScheme(AUTH_SCHEME_HTTP_NEGOTIATE | (GetScheme() & AUTH_SCHEME_HTTP_PROXY));
				break; // Success
			}
			// error

			if(scheme != AUTH_SCHEME_HTTP_NTLM_NEGOTIATE) // If we looked for NTLM or does not have NTLM, return
				return OpStatus::OK;
			
			// If we tried Negotiate, try NTLM
			SetScheme(AUTH_SCHEME_HTTP_NTLM | (GetScheme() & AUTH_SCHEME_HTTP_PROXY));
			last_challenge.TakeOver(ntlm_challenge);
		}
		while(1);
		gotten_credentials = TRUE;
		initializing = TRUE;
	}

	DWORD ContextAttributes=0;
	SecBufferDesc BufferDescriptor;
	SecBuffer Buffer;
	PSecBufferDesc	InputDescriptor = NULL;
	SecBufferDesc	InputBufferDesc;
	SecBuffer		InputBuffer;

	InputBuffer.BufferType = SECBUFFER_TOKEN;
	InputBuffer.cbBuffer = 0;
	InputBuffer.pvBuffer = NULL;

	InputBufferDesc.ulVersion = SECBUFFER_VERSION;
	InputBufferDesc.cBuffers = 1;
	InputBufferDesc.pBuffers = &InputBuffer;

	if(last_challenge.HasContent())
	{
		unsigned long read_pos= 0, chlen = last_challenge.Length() ;
		BOOL warning=FALSE;
		unsigned char *buffer = new unsigned char[chlen];
		if(buffer == NULL)
		{
			return OpStatus::OK;
		}

		unsigned long tlen = GeneralDecodeBase64((const unsigned char *) last_challenge.CStr(), chlen, read_pos, buffer, warning);

		OP_ASSERT(read_pos == chlen && !warning);

		InputBuffer.cbBuffer = tlen;
		InputBuffer.pvBuffer = buffer;
		InputDescriptor = &InputBufferDesc;
	}
	else
	{
		InputBuffer.cbBuffer = 0;
		InputBuffer.pvBuffer = new unsigned char[1];
		if(InputBuffer.pvBuffer == NULL)
		{
			return OpStatus::OK;
		}
		((unsigned char *)InputBuffer.pvBuffer)[0] = 0;
		InputDescriptor = &InputBufferDesc;
	}

	Buffer.cbBuffer = 0;
	Buffer.pvBuffer = NULL;
	Buffer.BufferType = SECBUFFER_TOKEN;

	BufferDescriptor.ulVersion = SECBUFFER_VERSION;
	BufferDescriptor.cBuffers = 1;
	BufferDescriptor.pBuffers = &Buffer;

	status = (*SecurityInterface->InitializeSecurityContext)(
					&Credentials,
					(!initializing ? &SecurityContext : NULL),
					SPN.CStr(),
					ISC_REQ_USE_DCE_STYLE |// ISC_REQ_DELEGATE |
					/*ISC_REQ_MUTUAL_AUTH | ISC_REQ_REPLAY_DETECT |*/ ISC_REQ_ALLOCATE_MEMORY |
					//ISC_REQ_SEQUENCE_DETECT | // ISC_REQ_CONFIDENTIALITY |
					ISC_REQ_CONNECTION /*| ISC_REQ_PROMPT_FOR_CREDS*/,
					0,
					0,
					InputDescriptor, // input
					0,
					(initializing ? &SecurityContext : NULL),
					&BufferDescriptor,
					&ContextAttributes,
					&stamp
					);

	if(status != SEC_I_CONTINUE_NEEDED)
	{
		(*SecurityInterface->FreeCredentialHandle)(&Credentials);
		(*SecurityInterface->DeleteSecurityContext)(&SecurityContext);
		memset(&Credentials, 0, sizeof(Credentials));
		memset(&SecurityContext, 0, sizeof(SecurityContext));
		gotten_credentials = FALSE;
	}

	char *result = NULL;

	if(BufferDescriptor.cBuffers)
	{
		int len=0;
		MIME_Encode_SetStr(result,len,(const char *) BufferDescriptor.pBuffers[0].pvBuffer,BufferDescriptor.pBuffers[0].cbBuffer,NULL,GEN_BASE64_ONELINE);
	}

	(*SecurityInterface->FreeContextBuffer)(BufferDescriptor.pBuffers[0].pvBuffer);
	delete[] ((unsigned char *)InputBuffer.pvBuffer);

	OP_STATUS op_err = OpStatus::OK;
	if(result && *result)
	{
		op_err = ret_str.SetConcat(
			(scheme == AUTH_SCHEME_HTTP_NTLM_NEGOTIATE || 
			scheme == AUTH_SCHEME_HTTP_NEGOTIATE  ? "Negotiate " : "NTLM "),
			result);
	}
	delete [] result;
	return op_err;
}

OP_STATUS NTLM_AuthElm::Update_Authenticate(ParameterList *header)
{
	if(header)
	{
		Parameters *param = header->First();
		if(param)
		{
			BOOL negotiate = (param->GetName().CompareI("Negotiate") == 0 ? TRUE : FALSE);

			param = param->Suc();
			
			if((GetScheme() &  AUTH_SCHEME_HTTP_MASK) != AUTH_SCHEME_HTTP_NTLM_NEGOTIATE || negotiate)
				RETURN_IF_ERROR(last_challenge.Set(param ? param->Name() : NULL));
			else
				RETURN_IF_ERROR(ntlm_challenge.Set(param ? param->Name() : NULL));
		}
	}
	return OpStatus::OK;
}

OP_STATUS NTLM_AuthElm::SetPassword(OpStringC8 a_passwd)
{
	/*
			_SEC_WINNT_AUTH_IDENTITY id;

			id.User = UNI_L("Administrator");
			id.UserLength = uni_strlen(id.User);
			id.Domain = UNI_L("HADES");
			id.DomainLength = (id.Domain ? uni_strlen(id.Domain) : 0);
			id.Password = UNI_L("OperaQA");
			id.PasswordLength = uni_strlen(id.Password);
			id.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
			*/
	// Not in use
	return OpStatus::OK;
}

BOOL NTLM_AuthElm::MustClone() const
{
	return TRUE;
}

AuthElm *NTLM_AuthElm::MakeClone()
{
	NTLM_AuthElm *elem = new NTLM_AuthElm(GetScheme(), GetUrlType(), GetPort());

	if(!elem)
		return NULL;

	if(OpStatus::IsError(elem->Construct(this)))
	{
		delete elem;
		return NULL;
	}

	return elem;
}


#endif
