/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * Web server implementation -- overall server logic
 */

#include "core/pch.h"
#ifdef WEBSERVER_SUPPORT

/* Work in progress. This is a collection of code from the old http digest implementation, to save the digest code. This will not compile */

#ifdef WEB_HTTP_DIGEST_SUPPORT

#include "modules/libcrypto/include/CryptoHash.h"

#include "modules/webserver/webserver_digest_auth.h"



WebserverServerNonceElement::WebserverServerNonceElement()
	:	 m_nc(0)
		,m_timeStamp(0)
		,m_nonceBitTable(0)
{
	
}

WebserverServerNonceElement::~WebserverServerNonceElement()
{
	OP_DELETEA(m_nonceBitTable);
}
	
OP_STATUS WebserverServerNonceElement::ContructNonce()
{
	m_timeStamp = OpDate::GetCurrentUTCTime();

	OP_ASSERT(m_nonceBitTable == 0);
	
	m_nonceBitTable = OP_NEWA(UINT32, WEB_PARM_NONCE_BIT_TABLE_SIZE);
	
	if (m_nonceBitTable == NULL)
		return OpStatus::ERR_NO_MEMORY;
	
	for (int j = 0; j < WEB_PARM_NONCE_BIT_TABLE_SIZE; j++)
	{
		m_nonceBitTable[j] = 0;
	}
	
	const unsigned int timeStampStringSize = sizeof(UINT64)*2;  /*two characters per byte*/


	char timeStampString[timeStampStringSize + 1]; /* ARRAY OK 2008-06-10 haavardm */ 
	
	UINT64 time = (UINT64)(m_timeStamp)^(UINT64)(this);/*we xor with this pointer in case two nonce's get the same timeStamp( not very likely)*/
	
	unsigned int i;
	for (i = 0; i < timeStampStringSize; i++)
	{
		if (time == 0)
			break;
		char a = (char)(time & 15);
		if (a < 10)
		{
			a += '0';
		}
		else
		{
			a += 'a' - 10;
		}
		timeStampString[i] = a;
		time >>= 4;
	}
	
	timeStampString[i] = '\0';
	OP_ASSERT(op_strlen(timeStampString) < timeStampStringSize);

	OpAutoPtr<CryptoHash> timeStampHash(CryptoHash::CreateMD5());
	if (!timeStampHash.get())
		return OpStatus::ERR_NO_MEMORY;

	timeStampHash->InitHash();
	timeStampHash->CalculateHash(timeStampString);
	
	const int len = SSL_MAX_HASH_LENGTH * 2 + 1;
	char buffer[len]; /* ARRAY OK 2008-06-10 haavardm */ 
	ConvertToHex(timeStampHash, buffer);
	
	/*we construct a nonce : timestamp:HASH(timestamp)*/
	RETURN_IF_ERROR(m_nonce.Set(timeStampString));
	RETURN_IF_ERROR(m_nonce.Append(buffer));
	return OpStatus::OK;
	
}

BOOL WebserverServerNonceElement::CheckAndSetUsedNonce (unsigned int clientNonceNumber)
{
	if (clientNonceNumber > WEB_PARM_NONCE_BIT_TABLE_SIZE * 32 || clientNonceNumber == 0)
		return FALSE;
	clientNonceNumber--;

	OP_ASSERT(clientNonceNumber < WEB_PARM_NONCE_BIT_TABLE_SIZE*32);

	int wordNumber = clientNonceNumber >> 5;
	int bitNumber = clientNonceNumber & 31;

	OP_ASSERT(wordNumber < WEB_PARM_NONCE_BIT_TABLE_SIZE);

	/*Is this nonce already in use? If so return FALSE*/
	if (  (1 & (m_nonceBitTable[wordNumber] >> bitNumber) )  != 0)
		return FALSE;

	/*we set bit number clientNonce to one*/
	m_nonceBitTable[wordNumber] |= (1 << bitNumber); 
	return TRUE;
}


	/*return the nonce*/
const OpStringC8 &WebserverServerNonceElement::GetNonce()
{
	return m_nonce;	
}
	/*returns the time stamp*/
double WebserverServerNonceElement::GetTimeStamp()
{
	return m_timeStamp;	
}



virtual OP_STATUS WebserverAuthenticationId_HttpDigest::CheckAuthentication(WebserverRequest *request_object)
{
	/******* http authentication */
	
	/*We retrieve the Authorization header from the header list sent by the client browser*/
	HeaderEntry *authorizationHeader = headerList->GetHeader("Authorization");
	
	/*We get out the parameters from the the Authorization header*/
	ParameterList *parameters = NULL;
	if (authorizationHeader != NULL)
	{
		*result = WEB_FAIL_NOT_AUTHENTICATED;
		RETURN_IF_LEAVE(parameters = authorizationHeader->GetParametersL((PARAM_SEP_COMMA | PARAM_SEP_WHITESPACE | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES), KeywordIndex_HTTP_General_Parameters));
	}		

	const char* uriDirectiveValue = requestObject->GetOriginalRequest();
	OP_ASSERT(uriDirectiveValue);
	
	WebServerMethod method = requestObject->GetHttpMethod();	
	const char* methodString = methods[method];
	
	BOOL nonceExpired = FALSE;			
	if (authorizationHeader != NULL && parameters != NULL)
	{
		OP_BOOLEAN authenticathed;
		RETURN_IF_ERROR(authenticathed = WebResource_Auth::checkDigestAuthentication(parameters, uriDirectiveValue, methodString, authElement->m_user_access_control_list, nonceExpired)); 
		if (authenticathed == OpBoolean::IS_TRUE)
		{
			/* FIXME: have a look at this:*/
			/*here we must generate a nonce for the next request*/
			/*This means that the authentication was successfull*/
			*result = WEB_FAIL_NO_FAILURE;
			return OpBoolean::IS_TRUE; 
		}
	}

	/* we allow POST and PUT on authentication uri's only if the path has been authenticated using a get first */
	
	if (method == WEB_METH_POST || method == WEB_METH_PUT)
	{
		*result = WEB_FAIL_METHOD_NOT_SUPPORTED;
		
		return OpBoolean::IS_FALSE;
	}
		
	
	authResource = OP_NEW(WebResource_Auth, (service, requestObject, authElement, WebserverAuthenticationId::WEB_AUTH_TYPE_HTTP_DIGEST));
	status = OpStatus::ERR_NO_MEMORY;
	if ( authResource == NULL || OpStatus::IsError(status = authResource->GenerateDigestResponse(nonceExpired)))
	{
		OP_DELETE(authResource);
		return status;
	}

	/* end of http authentication */
	
	*result = WEB_FAIL_NOT_AUTHENTICATED;

	webResource = authResource;
	
	return OpBoolean::IS_FALSE;	
	
	
}

/*static*/
unsigned int WebserverAuthenticationId_HttpDigest::HexStringToUint(const char *hexString, unsigned int length)
{
	OP_ASSERT(op_strlen(hexString) >= length);

	unsigned int intRes = 0;
	for (unsigned int i = 0;i < length; ++i)
	{
		char res = 0;
		char C = hexString[i];
		if (C <= 'f' && C >= 'a')
		{
			res = C-'a'+10;	
		}
		else
		if (C <= 'F' && C>='A')
		{
			res = C-'A'+10;	
		}
		else 
		if (C >= '0' && C<='9')
		{
			res = C-'0';			
		}
		else
			break;	
		
		intRes <<= 4;
		intRes += res;
	}
	return intRes;			
}

/*static*/
OP_BOOLEAN WebserverAuthenticationId_HttpDigest::checkBasicAuthentication(ParameterList *authHeaderParameters,
														const OpStringC8 &uriDirectiveValue,
														const OpStringC8 &method,
														OpAutoStringHashTable< WebserverAuthUser > &user_list,
														/*out*/ BOOL &nonceExpired 
														)
{
	if (authHeaderParameters == NULL)
		return OpBoolean::IS_FALSE;
	
	OP_ASSERT(!"NOT IMPLEMENTED");
	nonceExpired = FALSE; /*basic auth does not have nonce*/
	return OpBoolean::IS_FALSE;
}

/*static*/
OP_BOOLEAN WebserverAuthenticationId_HttpDigest::checkDigestAuthentication(ParameterList *authHeaderParameters,
														const OpStringC8 &uriDirectiveValue,
														const OpStringC8 &method,
														OpAutoStringHashTable< WebserverAuthUser > &user_list,
														/*out*/ BOOL &nonceExpired 
														)
{
	nonceExpired = FALSE;
	
	if (authHeaderParameters == NULL)
	{
		OP_ASSERT(!"authHeaderParameters cannot be NULL");
		return OpStatus::ERR;
	}

	
	Parameters *userNameParameter = authHeaderParameters->GetParameter("username", PARAMETER_ANY);	
	
	if (userNameParameter == NULL)
		return OpBoolean::IS_FALSE;	
		
	const char* userName = userNameParameter->Value();
	
	if (userName == NULL)
		return OpBoolean::IS_FALSE;
	
	OpString unicharUserName;
	
	unicharUserName.SetFromUTF8(userName);
	
	if (user_list.Contains(unicharUserName.CStr()) == FALSE)
		return OpBoolean::IS_FALSE;

	WebserverAuthUser *user_object = NULL;
	
	if (OpStatus::IsError(user_list.GetData(unicharUserName.CStr(), &user_object)))
		return OpBoolean::IS_FALSE;

	if (user_object->m_authentication_type != WEB_AUTH_TYPE_HTTP_DIGEST)
		return OpBoolean::IS_FALSE;
	
	
	Parameters *nonceParameter = authHeaderParameters->GetParameter("nonce", PARAMETER_ANY);	
	if (nonceParameter == NULL)
		return OpBoolean::IS_FALSE;	
		
	const char* nonce = nonceParameter->Value();
	if (nonce == NULL)
	{
		nonceExpired = TRUE;	
		return OpBoolean::IS_FALSE;		
	}
		
	WebserverServerNonceElement *serverNonceElement = g_webserverPrivateGlobalData->GetAuthenctionNonceElement(nonce);
	if (serverNonceElement == NULL)
	{
		nonceExpired = TRUE;	
		return OpBoolean::IS_FALSE;		
	}

	Parameters *cnonceParameter = authHeaderParameters->GetParameter("cnonce", PARAMETER_ANY);	
	if (cnonceParameter == NULL)
		return OpBoolean::IS_FALSE;	
		
	const char* cnonce = cnonceParameter->Value();
	if (cnonce == NULL)
		return OpBoolean::IS_FALSE;	

	Parameters *qopParameter = authHeaderParameters->GetParameter("qop", PARAMETER_ANY);	
	if (qopParameter == NULL)
		return OpBoolean::IS_FALSE;	
		
	const char* qop = qopParameter->Value();
	if (qop == NULL)
		return OpBoolean::IS_FALSE;	

	const OpStringC8 serverNonce = serverNonceElement->GetNonce();
	OP_ASSERT(serverNonce.Length() != 0);
	
	/*We check that the nonnce count is legal. This prevents replay attacks*/
	
	Parameters *ncParameter = authHeaderParameters->GetParameter("nc", PARAMETER_ANY);	
	if (ncParameter == NULL)
		return OpBoolean::IS_FALSE;	

	const char* nc = ncParameter->Value();
	if (nc == NULL)
		return OpBoolean::IS_FALSE;

	unsigned int clientNonce = HexStringToUint(nc, 8);
	
	if (serverNonceElement->CheckAndSetUsedNonce(clientNonce) == FALSE)
	{
		nonceExpired = TRUE;
		return OpBoolean::IS_FALSE;
	}


	/*End of nonce check*/
	
	OpString8 calculatedResponseDigest;
	
	RETURN_IF_ERROR(WebserverAuthenticationId_HttpDigest::calculateCorrectResponseDigest(	unicharUserName,
																		user_object->m_user_auth_data.CStr(),
																		uriDirectiveValue,
																		method,
																		nc,
																		cnonce,
																		qop,
																		serverNonce,										
																		calculatedResponseDigest));
														
	if (calculatedResponseDigest.Length() == 0)
		return OpBoolean::IS_FALSE;		
		
	Parameters *responseParameter = authHeaderParameters->GetParameter("response",PARAMETER_ANY);	
	if (responseParameter == NULL)
		return OpBoolean::IS_FALSE;
		 
	const char* response = responseParameter->Value();
	if (response == NULL)
		return OpBoolean::IS_FALSE;
	
	if ( op_strlen(response) == op_strlen(calculatedResponseDigest) && op_strcmp(response, calculatedResponseDigest) == 0 )
	{
		return OpBoolean::IS_TRUE;	
	}

	return OpBoolean::IS_FALSE;	
}

OP_STATUS WebserverAuthenticationId_HttpDigest::GenerateDigestResponse(BOOL nonceExpired)
{
	Header_Item *authentHeader = OP_NEW(Header_Item, ());
	Header_Parameter_Collection *headerCollection = OP_NEW(Header_Parameter_Collection, (SEPARATOR_COMMA));
	
	if (authentHeader == NULL || headerCollection == NULL )
	{
		OP_DELETE(authentHeader);
		OP_DELETE(headerCollection);
		return OpStatus::ERR_NO_MEMORY;
	}
	
	OpAutoPtr< Header_Item> authentHeaderAnchor(authentHeader);
	OpAutoPtr< Header_Parameter_Collection> headerCollectionAnchor(headerCollection);
	
	RETURN_IF_LEAVE(authentHeader->InitL("WWW-Authenticate"));
	
	authentHeader->SetSeparator(SEPARATOR_SPACE);
	
	RETURN_IF_LEAVE(authentHeader->AddParameterL("Digest", NULL));

	OpString8 serverNonce;
	RETURN_IF_ERROR(g_webserverPrivateGlobalData->CreateAuthenctionNonceElement(serverNonce));
	
	OpString8 realm;
	RETURN_IF_ERROR(realm.SetUTF8FromUTF16(m_authElement->m_realm));
	
	RETURN_IF_LEAVE(headerCollection->AddParameterL("realm", realm, TRUE));
	RETURN_IF_LEAVE(headerCollection->AddParameterL("qop", "auth", TRUE));
	RETURN_IF_LEAVE(headerCollection->AddParameterL("nonce", serverNonce, TRUE));

	if (nonceExpired == TRUE)
	{
		RETURN_IF_LEAVE(headerCollection->AddParameterL("stale","true",TRUE));
	}
	else
	{
		RETURN_IF_LEAVE(headerCollection->AddParameterL("stale","false",TRUE));
	}
			
	RETURN_IF_LEAVE(headerCollection->AddParameterL("algorithm","MD5",FALSE));

	RETURN_IF_ERROR(SetResponseCode(401)); 

	RETURN_IF_ERROR(m_service->StartServingData());
	
		
	authentHeader->AddParameter(headerCollectionAnchor.release()); /* takes over */
	
	m_userResponseHeaders.InsertHeader(authentHeader);
	
	authentHeaderAnchor.release();

	return OpStatus::OK;
}
	
/*static*/
OP_STATUS WebserverAuthenticationId_HttpDigest::calculateHA1(const OpStringC &user, const OpStringC &password, const OpStringC &realm, OpString &digest)
{
	OpAutoPtr<CryptoHash> hash_A1(CryptoHash::CreateMD5());
	if (!hash_A1.get())
		return OpStatus::ERR_NO_MEMORY;
	

	OpString8 user_utf8;
	OpString8 password_utf8;
	OpString8 realm_utf8;
	
	
	RETURN_IF_ERROR(user_utf8.SetUTF8FromUTF16(user));
	RETURN_IF_ERROR(password_utf8.SetUTF8FromUTF16(password));
	RETURN_IF_ERROR(realm_utf8.SetUTF8FromUTF16(realm));
	
	hash_A1->InitHash();
	hash_A1->CalculateHash(user_utf8);
	hash_A1->CalculateHash(":");
	hash_A1->CalculateHash(realm_utf8);
	hash_A1->CalculateHash(":");
	hash_A1->CalculateHash(password_utf8);

	const unsigned int len = SSL_MAX_HASH_LENGTH*2+1;
	char buffer[len]; /* ARRAY OK 2008-06-10 haavardm */
	ConvertToHex(hash_A1, &buffer[0]);
	RETURN_IF_ERROR(digest.SetFromUTF8(&buffer[0]));	
	OP_ASSERT(op_strlen(buffer) + 1 < len);
	return OpStatus::OK;
}

/*static*/
OP_STATUS WebserverAuthenticationId_HttpDigest::calculateHA2(const OpStringC8 &method, const OpStringC8 &originalUri, OpString8 &digest)
{
	OpAutoPtr<CryptoHash> hash_A2(CryptoHash::CreateMD5());
	if (!hash_A2.get())
		return OpStatus::ERR_NO_MEMORY;

	hash_A2->InitHash();
	hash_A2->CalculateHash(method);
	hash_A2->CalculateHash(":");
	hash_A2->CalculateHash(originalUri);
	
	const unsigned int len = SSL_MAX_HASH_LENGTH*2+1;
	char buffer[len]; /* ARRAY OK 2008-06-10 haavardm */
	
	ConvertToHex(hash_A2, &buffer[0]);

	OP_ASSERT(op_strlen(buffer) + 1 < len);
	
	RETURN_IF_ERROR(digest.Set(&buffer[0]));
	return OpStatus::OK;
}

/*static*/ OP_STATUS
WebserverAuthenticationId_HttpDigest::calculateCorrectResponseDigest( const OpStringC &userName,
														const OpStringC &HA1_hash,
														const OpStringC8 &OriginalRequestURI,
														const OpStringC8 &method,
														const OpStringC8 &nc,
														const OpStringC8 &cnonce,
														const OpStringC8 &qop,
														const OpStringC8 &nonce,														
														OpString8 &reponse)
{
	  
	OpString8 digestA2;
	calculateHA2(method, OriginalRequestURI, digestA2);

	OpAutoPtr<CryptoHash> responseHash(CryptoHash::CreateMD5());
	if (!responseHash.get())
		return OpStatus::ERR_NO_MEMORY;
	
	
	responseHash->InitHash();
	
	OpString8 HA1_hash_utf8;
	RETURN_IF_ERROR(HA1_hash_utf8.SetUTF8FromUTF16(HA1_hash));
	
	responseHash->CalculateHash(HA1_hash_utf8);
	responseHash->CalculateHash(":");
	responseHash->CalculateHash(nonce);
	responseHash->CalculateHash(":");
	responseHash->CalculateHash(nc);
	responseHash->CalculateHash(":");
	responseHash->CalculateHash(cnonce);
	responseHash->CalculateHash(":");
	responseHash->CalculateHash(qop);
	responseHash->CalculateHash(":");
	responseHash->CalculateHash(digestA2);	

	const unsigned int len = SSL_MAX_HASH_LENGTH*2+1;
	char buffer[len];	 /* ARRAY OK 2008-06-10 haavardm */


		
	ConvertToHex(responseHash, &buffer[0]);
	OP_ASSERT(op_strlen(buffer) + 1 < len);	
	RETURN_IF_ERROR(reponse.Set(&buffer[0]));	

	return OpStatus::OK;
}

#endif WEB_HTTP_DIGEST_SUPPORT
#endif // WEBSERVER_SUPPORT
