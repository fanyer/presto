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

#ifdef _NATIVE_SSL_SUPPORT_
#ifdef _SSL_USE_PKCS11_

#include "modules/libssl/sslbase.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/pi/OpDLL.h"

#include "modules/libssl/external/p11/p11_man.h"
#include "modules/libssl/external/p11/p11_pubk.h"
#include "modules/url/url_man.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#define PKCS11_MECH_RSA_FLAG          (0x1<<0)
#define PKCS11_MECH_DSA_FLAG          (0x1<<1)
#define PKCS11_MECH_RC2_FLAG          (0x1<<2)
#define PKCS11_MECH_RC4_FLAG          (0x1<<3)
#define PKCS11_MECH_DES_FLAG          (0x1<<4)
#define PKCS11_MECH_DH_FLAG           (0x1<<5)
#define PKCS11_MECH_SKIPJACK_FLAG     (0x1<<6)
#define PKCS11_MECH_RC5_FLAG          (0x1<<7)
#define PKCS11_MECH_SHA1_FLAG         (0x1<<8)
#define PKCS11_MECH_MD5_FLAG          (0x1<<9)
#define PKCS11_MECH_MD2_FLAG          (0x1<<10)
#define PKCS11_MECH_RANDOM_FLAG       (0x1<<27)
#define PKCS11_PUB_READABLE_CERT_FLAG (0x1<<28)
#define PKCS11_DISABLE_FLAG           (0x1<<30)

class ES_RestartObject;

PKCS_11_Manager::PKCS_11_Manager(OpDLL *driver)
: p11_driver(driver),
	functions(NULL_PTR),
	certificate_login(TRUE)
{
}

OP_STATUS PKCS_11_Manager::Init(uni_char **arguments, int max_args)
{
	CK_C_GetFunctionList p11_getfuntion_list = NULL_PTR;
	CK_RV ret;

	// Arg 1 : type
	// Arg 2 : Name
	// Arg 3 : Mechanism_flags: Hexadecimal format

	/* From Netscape pkcs11.addmodule documentation
			PKCS11_MECH_RSA_FLAG          = 0x1<<0;
			PKCS11_MECH_DSA_FLAG          = 0x1<<1;
			PKCS11_MECH_RC2_FLAG          = 0x1<<2;
			PKCS11_MECH_RC4_FLAG          = 0x1<<3;
			PKCS11_MECH_DES_FLAG          = 0x1<<4;
			PKCS11_MECH_DH_FLAG           = 0x1<<5; //Diffie-Hellman
			PKCS11_MECH_SKIPJACK_FLAG     = 0x1<<6; //SKIPJACK algorithm as in Fortezza cards
			PKCS11_MECH_RC5_FLAG          = 0x1<<7;
			PKCS11_MECH_SHA1_FLAG         = 0x1<<8;
			PKCS11_MECH_MD5_FLAG          = 0x1<<9;
			PKCS11_MECH_MD2_FLAG          = 0x1<<10;
			PKCS11_MECH_RANDOM_FLAG       = 0x1<<27; //Random number generator
			PKCS11_PUB_READABLE_CERT_FLAG = 0x1<<28; //Stored certs can be read off the token w/o logging in
			PKCS11_DISABLE_FLAG           = 0x1<<30; //tell Navigator to disable this slot by default

			Only PKCS11_PUB_READABLE_CERT_FLAG and PKCS11_DISABLE_FLAG is heeded at the moment
	*/

	if(arguments && max_args>=3)
	{
		if(arguments[2])
		{
			unsigned long arg = uni_strtoul(arguments[2], NULL, 16);

			if(arg & PKCS11_DISABLE_FLAG)
				return OpSmartcardStatus::DISABLED_MODULE;

			if(arg & PKCS11_PUB_READABLE_CERT_FLAG)
				certificate_login = FALSE;
		}
	}


	if(!p11_driver)
		return OpStatus::ERR_NULL_POINTER;

	if(!p11_driver->IsLoaded())
		return OpStatus::ERR;

	p11_getfuntion_list = (CK_C_GetFunctionList) p11_driver->GetSymbolAddress("C_GetFunctionList");

	if(p11_getfuntion_list == NULL_PTR)
		return OpStatus::ERR_NULL_POINTER;

	ret = p11_getfuntion_list(&functions);
	if(ret != CKR_OK)
		return TranslateToOP_STATUS(ret);

	if(functions != NULL_PTR && functions->version.major != 2)
		return OpSmartcardStatus::UNSUPPORTED_VERSION;

	if(functions == NULL_PTR || 
		functions->C_Initialize == NULL_PTR || 
		functions->C_Finalize == NULL_PTR || 
		functions->C_GetInfo == NULL_PTR || 
		functions->C_GetSlotList == NULL_PTR || 
		functions->C_GetSlotInfo == NULL_PTR || 
		functions->C_GetTokenInfo == NULL_PTR || 
		functions->C_GetMechanismList == NULL_PTR || 
		functions->C_GetMechanismInfo == NULL_PTR || 
		functions->C_OpenSession == NULL_PTR || 
		functions->C_CloseSession == NULL_PTR || 
		functions->C_GetSessionInfo == NULL_PTR || 
		functions->C_Login == NULL_PTR || 
		functions->C_Logout == NULL_PTR || 
		functions->C_Encrypt == NULL_PTR || 
		functions->C_Decrypt == NULL_PTR || 
		functions->C_Sign == NULL_PTR || 
		functions->C_Verify == NULL_PTR)
	{
		functions = NULL_PTR;
		return OpStatus::ERR_NULL_POINTER;
	}

	ret = functions->C_Initialize(NULL_PTR);
	if(ret != CKR_OK)
	{
		functions = NULL_PTR;
		return TranslateToOP_STATUS(ret);
	}

	return OpStatus::OK;
}

PKCS_11_Manager::~PKCS_11_Manager()
{
	active_tokens.Clear();
	temp_tokens.Clear();

	OP_ASSERT(Get_Reference_Count() == 0);

	if(functions)
		functions->C_Finalize(NULL_PTR);

	if(p11_driver)
	{
		p11_driver->Unload();
		OP_DELETE(p11_driver);
	}

	functions = NULL_PTR;
}

void PKCS_11_Manager::ConfirmCardsPresent()
{
	PKCS_11_Token *next_token, *token;

	CK_RV status;
	CK_SLOT_ID slot=0;

	do{
		status = functions->C_WaitForSlotEvent(CKF_DONT_BLOCK, &slot, NULL_PTR);

		if(status != CKR_OK) // CKR_NO_EVENT or ignored errors
			break;

		next_token = active_tokens.First();
		while(next_token)
		{
			token = next_token;
			next_token = token->Suc();
			if(token->GetSlot() == slot)
			{
				if(!token->CheckCardPresent())
				{
					if(token->Get_Reference_Count() == 0)
					{
						token->Out();
						OP_DELETE(token);
					}
				}
				break;
			}
		}
	}
	while(1); // loop until CKR_NO_EVENT

	next_token = active_tokens.First();
	while(next_token)
	{
		token = next_token;
		next_token = token->Suc();
		if(token->TimedOut())
		{
			token->TerminateSession();
			token->Out();
			token->Into(&temp_tokens);
		}
	}

	next_token = temp_tokens.First();
	while(next_token)
	{
		token = next_token;
		next_token = token->Suc();

		if(token->Get_Reference_Count() == 0)
		{
			token->Out();
			OP_DELETE(token);
		}
	}
}

OP_STATUS PKCS_11_Manager::GetAvailableCards(SSL_CertificateHandler_ListHead *cipherlist)
{
	if(functions == NULL_PTR || cipherlist == NULL)
		return OpStatus::ERR_NULL_POINTER;

	CK_RV status;
	CK_ULONG slot_count=0;
	CK_SLOT_ID_PTR slots = NULL_PTR;
	ANCHOR_ARRAY(CK_SLOT_ID, slots);

	status = functions->C_GetSlotList(TRUE, NULL_PTR, &slot_count);
	RETURN_IF_ERROR(TranslateToOP_STATUS(status));

	if(slot_count == 0)
		return OpStatus::OK;

	slots = OP_NEWA(CK_SLOT_ID, slot_count);
	if(slots == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return OpStatus::ERR_NO_MEMORY;
	}

	status = functions->C_GetSlotList(TRUE, slots, &slot_count);
	if(status != CKR_OK)
	{
		return TranslateToOP_STATUS(status);
	}

	CK_ULONG i;
	for(i=0; i< slot_count; i++)
	{
		RETURN_IF_ERROR(GetAvailableCardsFromSlot(slots[i], cipherlist));
	}
	
	ANCHOR_ARRAY_DELETE(slots);

	return OpStatus::OK;
}

OP_STATUS PKCS_11_Manager::GetAvailableCardsFromSlot(CK_SLOT_ID slot, SSL_CertificateHandler_ListHead *cipherlist)
{
	CK_RV status;
	CK_ULONG	method_count=0;
	CK_ULONG j;
	CK_MECHANISM_TYPE_PTR methods = NULL_PTR;
	ANCHOR_ARRAY(CK_MECHANISM_TYPE, methods);
	
	status = functions->C_GetMechanismList(slot, NULL_PTR, &method_count);
	RETURN_IF_ERROR(TranslateToOP_STATUS(status));
	
	if(method_count == 0)
		return OpStatus::OK;

	methods = OP_NEWA(CK_MECHANISM_TYPE, method_count);
	if(methods == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return OpStatus::ERR_NO_MEMORY;
	}
	
	status = functions->C_GetMechanismList(slot, methods, &method_count);
	RETURN_IF_ERROR(TranslateToOP_STATUS(status));
	
	for(j = 0;j < method_count; j++)
	{
		CK_MECHANISM_TYPE current_method = methods[j];
		if(current_method == CKM_RSA_PKCS)
		{
			RETURN_IF_ERROR(SetUpCardFromSlot(slot, cipherlist, SSL_RSA));
			break;
		}
	}
	
	ANCHOR_ARRAY_DELETE(methods);
	
	return OpStatus::OK;
}


OP_STATUS PKCS_11_Manager::SetUpCardFromSlot(CK_SLOT_ID slot, SSL_CertificateHandler_ListHead *cipherlist, SSL_BulkCipherType ctyp)
{
	CK_RV status;
	CK_TOKEN_INFO token_info;

	OpString16 label;
	OpString16 serial;
	SSL_ASN1Cert_list certificate;
	PKCS_11_Token *token = NULL;
	OpStackAutoPtr<SSL_PublicKeyCipher> key = NULL;
	
	
	status = functions->C_GetTokenInfo(slot, &token_info);
	RETURN_IF_ERROR(TranslateToOP_STATUS(status));

	RETURN_IF_ERROR(label.SetFromUTF8((const char *) token_info.label, sizeof(token_info.label)));
	RETURN_IF_ERROR(serial.SetFromUTF8((const char *) token_info.serialNumber, sizeof(token_info.serialNumber)));

	token = active_tokens.FindToken(slot,label, serial);
	if(!token)
		token = temp_tokens.FindToken(slot, label, serial);

	if(!token)
	{
		OpStackAutoPtr<PKCS_11_Token> new_token = NULL;
		new_token.reset(OP_NEW(PKCS_11_Token, (this, functions, slot, ctyp)));

		if(!new_token.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(new_token->Construct());

		new_token->Into(&temp_tokens);
		token = new_token.release();
	}

	certificate = token->GetCertificate();

	if(certificate.Error())
		return OpStatus::ERR;

	if(certificate.Count() == 0 || certificate[0].GetLength() == 0)
		return OpStatus::OK;

	key.reset(token->GetKey());
	if(key.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if(key->Error())
		return OpStatus::ERR;

	return SetupCertificateEntry(cipherlist, label, serial, certificate, key.release());
}

OP_STATUS PKCS_11_Manager::TranslateToOP_STATUS(CK_RV val) const
{
	return (val == CKR_OK ? OpStatus::OK : OpStatus::ERR);
}

BOOL PKCS_11_Manager::TranslateToSSL_Alert(CK_RV val, SSL_Alert &msg) const
{
	switch(val)
	{
	case CKR_OK:
		msg.Set(SSL_NoError, SSL_No_Description);
		return FALSE;
	case CKR_PIN_INCORRECT:
	case CKR_PIN_INVALID:
	case CKR_PIN_LEN_RANGE:
	case CKR_PIN_EXPIRED:
	case CKR_PIN_LOCKED:
		msg.Set(SSL_Fatal, SSL_Bad_PIN);
		return TRUE;
	}

	msg.Set(SSL_Fatal, SSL_InternalError);
	return TRUE;
}

BOOL PKCS_11_Manager::TranslateToSSL_Alert(CK_RV val, SSL_Error_Status *msg) const
{
	SSL_Alert code;

	if(!TranslateToSSL_Alert(val, code))
		return FALSE;

	if(msg)
		msg->RaiseAlert(code);

	return TRUE;
}

void PKCS_11_Manager::ActivateToken(PKCS_11_Token *tokn)
{
	if(!tokn)
		return;

	if(tokn->InList())
		tokn->Out();

	tokn->Into(&active_tokens);
}


SmartCard_Master *SmartCard_Manager::CreatePKCS11_MasterL(OpDLL *dll)
{
	return OP_NEW_L(PKCS_11_Manager, (dll));
}

PKCS_11_Token* PKCS_11_Token_Head::FindToken(CK_SLOT_ID slot, OpStringC &label, OpStringC &serial)
{
	PKCS_11_Token *token = First();
	while(token)
	{
		if(slot == token->GetSlot() && token->GetLabel().Compare(label) == 0 && token->GetSerialNumber().Compare(serial) == 0)
			break;
		
		token = token->Suc();
	}

	return token;
}




PKCS_11_Token::PKCS_11_Token(PKCS_11_Manager *mstr, CK_FUNCTION_LIST_PTR func, CK_SLOT_ID slot, SSL_BulkCipherType ctype)
: master(mstr), functions(func), slot_id(slot), ciphertype(ctype)
{
	session = CK_INVALID_HANDLE;

	enabled = FALSE;
	logged_in = FALSE;
	need_login = TRUE;
	need_pin = TRUE;
	logged_in_time = 0;
	last_used = 0;
	confirmed_present = FALSE;
}

PKCS_11_Token::~PKCS_11_Token()
{
	OP_ASSERT(Get_Reference_Count() == 0);

	CloseSession();

	if(InList())
		Out();
}

OP_STATUS PKCS_11_Token::Construct()
{
	CK_RV status;
	CK_TOKEN_INFO token_info;

	SSL_ASN1Cert_list certificate;
	OpStackAutoPtr<PKCS11_PublicKeyCipher> key = NULL;
	
	
	status = functions->C_GetTokenInfo(slot_id, &token_info);
	RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));

	RETURN_IF_ERROR(label.SetFromUTF8((const char *) token_info.label, sizeof(token_info.label)));
	RETURN_IF_ERROR(serialnumber.SetFromUTF8((const char *) token_info.serialNumber, sizeof(token_info.serialNumber)));

	need_login = (token_info.flags & CKF_LOGIN_REQUIRED ? TRUE : FALSE);
	need_pin = (token_info.flags & CKF_PROTECTED_AUTHENTICATION_PATH? FALSE : TRUE);

	return GetCertificateFromToken();
}

SSL_PublicKeyCipher *PKCS_11_Token::GetKey()
{
	return OP_NEW(PKCS11_PublicKeyCipher, (master, this, functions, slot_id, ciphertype));
}

void PKCS_11_Token::Activate()
{
	master->ActivateToken(this);
	enabled = TRUE;
}

void PKCS_11_Token::UsedNow()
{
	last_used = prefsManager->CurrentTime();
}

BOOL PKCS_11_Token::TimedOut()
{
	if(!enabled)
		return FALSE;

	time_t timeout = prefsManager->GetIntegerPrefDirect(PrefsManager::SmartCardTimeOutMinutes)*60;

	return (last_used && last_used + timeout >= prefsManager->CurrentTime() ? FALSE : TRUE);
}

BOOL PKCS_11_Token::CheckCardPresent()
{
	if(!TimedOut() && IsTokenPresent())
		return TRUE;

	TerminateSession();

	return FALSE;
}

void PKCS_11_Token::TerminateSession()
{
	enabled = FALSE;

	CloseSession();

	// Disabled, as there is no connection to the actual use of a smartcard based SSL session
	// This will only disable the current session for the card, and any SSL sessions based on it
	//if(prefsManager->GetIntegerPrefDirect(PrefsManager::StrictSmartCardSecurityLevel)>0)
	//	windowManager->CloseAllWindows(TRUE);

	// Invalidate Sessions for certificate, etc.
	urlManager->InvalidateSmartCardSessions(certificate);
	urlManager->CloseAllConnections();

}

BOOL PKCS_11_Token::IsTokenPresent()
{
	CK_RV status;
	CK_TOKEN_INFO token_info;

	OpString16 lbl;
	OpString16 srl;
	
	status = functions->C_GetTokenInfo(slot_id, &token_info);
	RETURN_VALUE_IF_ERROR(master->TranslateToOP_STATUS(status), FALSE);

	RETURN_VALUE_IF_ERROR(lbl.SetFromUTF8((const char *) token_info.label, sizeof(token_info.label)), FALSE);
	RETURN_VALUE_IF_ERROR(srl.SetFromUTF8((const char *) token_info.serialNumber, sizeof(token_info.serialNumber)), FALSE);

	if(label.Compare(lbl) != 0 || serialnumber.Compare(srl) != 0)
		return FALSE;

	return TRUE;
}


CK_RV PKCS_11_Token::Login(SSL_secure_varvector32 &password)
{
	if(!CheckSession())
		return CKR_FUNCTION_FAILED;

	if(!logged_in)
	{
		CK_RV status;
		
		status = functions->C_Login(session, CKU_USER, password.GetDirect(), password.GetLength());
		if(status != CKR_OK)
			return status;

		logged_in = TRUE;
		logged_in_time = prefsManager->CurrentTime(); 
	}

	return CKR_OK;

}

BOOL PKCS_11_Token::LoggedIn()
{
	if(!logged_in || session == CK_INVALID_HANDLE)
		return FALSE;
	
	CK_RV status;
	CK_SESSION_INFO info;

	status = functions->C_GetSessionInfo(session, &info);
	if(OpStatus::IsError(master->TranslateToOP_STATUS(status)))
		return FALSE;


	if(info.state == CKS_RO_USER_FUNCTIONS || 
		info.state == CKS_RW_USER_FUNCTIONS || 
		info.state == CKS_RW_SO_FUNCTIONS)
		return TRUE;

	return FALSE;
}

BOOL PKCS_11_Token::CheckSession()
{
	CK_RV status;
	
	status = CreateSession(&session);
	RETURN_VALUE_IF_ERROR(master->TranslateToOP_STATUS(status), FALSE);

	return TRUE;
}

CK_RV PKCS_11_Token::CreateSession(CK_SESSION_HANDLE_PTR session_ptr) const
{
	CK_RV status;

	if(master == NULL || functions == NULL_PTR)
		return CKR_GENERAL_ERROR;

	if(*session_ptr != CK_INVALID_HANDLE)
		return CKR_OK;

	status = functions->C_OpenSession(slot_id, CKF_SERIAL_SESSION, NULL_PTR, (CK_NOTIFY) NULL_PTR, session_ptr);
	return status;
}

void PKCS_11_Token::CloseSession()
{
	if(logged_in)
	{
		CK_RV status;
		
		status = functions->C_Logout(session);
		// Ignore status;

		logged_in = FALSE;
	}

	CloseSession(&session);
}

void PKCS_11_Token::CloseSession(CK_SESSION_HANDLE_PTR session_ptr) const
{
	CK_RV status;

	if(master == NULL || functions == NULL_PTR)
		return;

	if(*session_ptr != CK_INVALID_HANDLE)
		return;

	status = functions->C_CloseSession(*session_ptr);
	// ignore error code
	*session_ptr = CK_INVALID_HANDLE;
}


OP_STATUS PKCS_11_Token::GetCertificateFromToken()
{
	CK_RV status;
	CK_CERTIFICATE_TYPE cert_type;
	CK_ULONG	cert_object_type = CKO_CERTIFICATE;
	//CK_ULONG key_class, key_alg;
	//CK_BBOOL key_signflag, key_signrflag;
	CK_ATTRIBUTE search_for[] = {
		{CKA_CLASS, &cert_object_type, sizeof(cert_object_type)}
	};
	CK_ATTRIBUTE get_type[] = {
		{CKA_CERTIFICATE_TYPE, &cert_type, sizeof(CK_CERTIFICATE_TYPE)}
	};
	CK_ATTRIBUTE certificate_value[] = {
		{CKA_VALUE, NULL_PTR, 0}
	};
	/*
	CK_ATTRIBUTE key_att[] = {
		{CKA_CLASS,		&key_class,	sizeof(key_class)},
		{CKA_KEY_TYPE,	&key_alg,	sizeof(key_alg)}
	};	
	CK_ATTRIBUTE key_flags[] = {
		{CKA_SIGN,		&key_signflag,	sizeof(key_signflag)},
		{CKA_SIGN_RECOVER,	&key_signrflag,	sizeof(key_signrflag)}
	};	
	*/
	//CK_OBJECT_HANDLE key = CK_INVALID_HANDLE;
	CK_OBJECT_HANDLE object = CK_INVALID_HANDLE;
	CK_ULONG object_count=0;
	//CK_ULONG key_count=0;

	if(!CheckSession())
		return OpStatus::ERR;


	if(master->MustLoginForCertificate() && NeedLogIn())
	{
		SSL_secure_varvector32 password;

#define MSG_SECURE_ASK_PINCODE Str::SI_MSG_SECURE_ASK_PASSWORD
		if(!NeedPin() || AskPassword(MSG_SECURE_ASK_PINCODE, password, SSL_present_hwnd))
		{
			RETURN_IF_ERROR(Login(password));
		}
	}

	/*
	key_class = CKO_PRIVATE_KEY;
	key_alg = CKK_RSA;

	status = functions->C_FindObjectsInit(session, key_att, ARRAY_SIZE(key_att));
	RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));

	status = functions->C_FindObjects(session, &key, 1, &key_count);
	RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));

	if(key_count == 0)
		return OpStatus::OK;

	key_signflag = FALSE;
	key_signrflag = FALSE;

	status = functions->C_GetAttributeValue(session, key, key_flags, ARRAY_SIZE(key_flags));
	RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));

	status = functions->C_FindObjectsFinal(session);
	RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));
	if(!key_signflag)
		return OpStatus::OK;

	*/

	status = functions->C_FindObjectsInit(session, search_for, ARRAY_SIZE(search_for));
	RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));

	do{
		status = functions->C_FindObjects(session, &object, 1, &object_count);
		RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));
		
		if(object_count)
		{
			status = functions->C_GetAttributeValue(session, object, get_type, 1);
			RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));
			
			if(cert_type != CKC_X_509 && cert_type != CKC_X_509_ATTR_CERT)
				continue; // Do not handle unsupported certificates
			
			status = functions->C_GetAttributeValue(session, object, certificate_value, 1);
			RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));
			if(((CK_LONG) certificate_value[0].ulValueLen) == -1 || certificate_value[0].ulValueLen ==0)
				continue;
			
			certificate.Resize(1);
			if(certificate.Error())
				return OpStatus::ERR;
			
			certificate[0].Resize(certificate_value[0].ulValueLen);
			if(certificate.Error())
				return OpStatus::ERR;
			
			certificate_value[0].pValue = certificate[0].GetDirect();
			certificate_value[0].ulValueLen = certificate[0].GetLength();
			
			status = functions->C_GetAttributeValue(session, object, certificate_value, 1);
			RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));
			
			break;
		}

	}while(object_count);

	status = functions->C_FindObjectsFinal(session);
	RETURN_IF_ERROR(master->TranslateToOP_STATUS(status));

	return OpStatus::OK;
}

OP_STATUS PKCS11_AddModuleL(OpString &Module_name, 
						   OpString &DLL_path, 
						   unsigned long methods,
						   unsigned long ciphers
#ifdef SMC_ES_THREAD
						   , ES_RestartObject* restartobject
#endif
						   )
{
	if(Module_name.IsEmpty() || DLL_path.IsEmpty())
		return OpStatus::ERR;

	if(DLL_path.FindFirstOf(',') != KNotFound)
		return OpStatus::ERR;

	OpString value;
	ANCHOR(OpString,value);

	OpString ininame;
	ANCHOR(OpString,ininame);

	RETURN_IF_ERROR(prefsManager->GetSmartCardDriverL(Module_name, value));

	if(!value.IsEmpty())
		return OpStatus::OK;

	Install_PKCS(NULL, DLL_path, 
#ifdef SMC_ES_THREAD
		restartobject, 
#endif
		methods, ciphers, &Module_name);

	return OpStatus::OK;
}

OP_STATUS PKCS11_AddModule(OpString &Module_name, 
						   OpString &DLL_path, 
						   unsigned long methods,
						   unsigned long ciphers
#ifdef SMC_ES_THREAD
						   , ES_RestartObject* restartobject
#endif
						   )
{
	OP_STATUS op_err = OpStatus::OK;

#ifdef SMC_ES_THREAD
	TRAP(op_err, op_err = PKCS11_AddModuleL(Module_name, DLL_path, methods, ciphers, restartobject))
#else
	TRAP(op_err, op_err = PKCS11_AddModuleL(Module_name, DLL_path, methods, ciphers));
#endif

	return op_err;
}

OP_STATUS PKCS11_DeleteModuleL(OpString &Module_name
#ifdef SMC_ES_THREAD
							   , ES_RestartObject* restartobject
#endif
							   )
{
	if(Module_name.IsEmpty())
		return OpStatus::ERR;

	OpString value;
	ANCHOR(OpString,value);

	OpString ininame;
	ANCHOR(OpString,ininame);

	RETURN_IF_ERROR(prefsManager->GetSmartCardDriverL(Module_name, value));

	if(value.IsEmpty())
		return OpStatus::OK;

	int i = value.FindFirstOf(',');
	if(i != KNotFound)
		value.Delete(i);

	Uninstall_PKCS(NULL, value,
#ifdef SMC_ES_THREAD
		restartobject, 
#endif
		&Module_name);

	return OpStatus::OK;
}


OP_STATUS PKCS11_DeleteModule(OpString &Module_name
#ifdef SMC_ES_THREAD
							  , ES_RestartObject* restartobject
#endif
							  )
{
	OP_STATUS op_err = OpStatus::OK;

#ifdef SMC_ES_THREAD
	TRAP(op_err, op_err = PKCS11_DeleteModuleL(Module_name, restartobject));
#else
	TRAP(op_err, op_err = PKCS11_DeleteModuleL(Module_name));
#endif

	return op_err;
}


#endif

void SSL_ExternalKeyManager::SetUpMasterL(OpStringC type, OpStringC path, uni_char **arguments, int max_args)
{

#if defined(_SSL_USE_PKCS11_)
	OpStackAutoPtr<OpDLL> dll = NULL;
	OpStackAutoPtr<SmartCard_Master > driver = NULL;
	OpDLL *temp_dll=NULL;

	LEAVE_IF_ERROR(g_factory->CreateOpDLL(&temp_dll));
	dll.reset(temp_dll);
	temp_dll = NULL;

	LEAVE_IF_ERROR(dll->Load(path));

	OP_ASSERT(dll->IsLoaded());

#ifdef _SSL_USE_PKCS11_
	if(type.CompareI(UNI_L("CryptoKi")) == 0 || type.CompareI(UNI_L("pkcs11")) == 0 )
	{
		driver.reset(CreatePKCS11_MasterL(dll.get()));
	}
#endif

	if(driver.get() != NULL)
	{
		dll.release();
		OP_STATUS op_err = driver->Init(arguments, max_args);

		if(op_err == OpSmartcardStatus::UNSUPPORTED_VERSION)
		{
			driver.reset(); // Unsupported version: Delete it.
			return; 
		}
		else if(OpStatus::IsError(op_err))
			LEAVE(op_err);

		driver->Into(&smartcard_masters);
		driver.release();
	}

	dll.reset();
	driver.reset();
#endif
}
#if defined(_SSL_USE_PKCS11_)
	OpString key, value;
	ANCHOR(OpString,key);
	ANCHOR(OpString,value);

	BOOL first = TRUE;
    const int	TOKENS_ON_A_LINE = 4;           //  # of tokens on a line in the ini-file
	int tokens;
    uni_char* tokenArr[TOKENS_ON_A_LINE];

	OpString ininame;
	ANCHOR(OpString,ininame);

	while(prefsManager->ReadSmartCardDriversL(key, value, op_err, first))
	{
		if(OpStatus::IsError(op_err)) 
			return op_err;

		first = FALSE;
		if(key.IsEmpty() || value.IsEmpty())
			continue;

		// format of section  name=dll-path,api, [apispecific attributes]
		// API: one of CryptoKi, pkcs11
		// Commas are not allowed in the dll-path

	    tokens = GetStrTokens(value.CStr(), UNI_L(","), UNI_L("\" \t"),
	                 tokenArr, TOKENS_ON_A_LINE);

		TRAP(op_err, SetUpMasterL(tokenArr[1], tokenArr[0], tokenArr, tokens));

		if(op_err != OpSmartcardStatus::DISABLED_MODULE && OpStatus::IsError(op_err))
		{
			return op_err;
		}
	}

	//TRAP_AND_RETURN(op_err, SetUpMasterL(UNI_L("CryptoKi"), UNI_L("C:\\WinNT\\system32\\SmartP11.dll")));
#endif

#endif
