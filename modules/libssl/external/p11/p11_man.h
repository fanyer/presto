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

#include "modules/libssl/smartcard/smc_man.h"
#include "modules/libssl/external/p11/pkcs11/cryptoki.h"

#ifndef __P11_MAN_H__
#define __P11_MAN_H__

class PKCS_11_Token;

class PKCS_11_Token_Head: public SSL_Head{
public:
    PKCS_11_Token* First() const{
		return (PKCS_11_Token *) Head::First();
    };     //pointer
    PKCS_11_Token* Last() const{
		return (PKCS_11_Token *) Head::Last();
    };     //pointer

	PKCS_11_Token* FindToken(CK_SLOT_ID slot, OpStringC &label, OpStringC &serial); 
};  

class PKCS_11_Manager : public SmartCard_Master
{
private:
	/** Pointer to PKSC#11 DLL handler */
	OpDLL *p11_driver;

	/** Pointer to list of available PKCS #11 functions */
	CK_FUNCTION_LIST_PTR functions;

	PKCS_11_Token_Head active_tokens;
	PKCS_11_Token_Head temp_tokens;

	BOOL	certificate_login;
public:
	/** Constructor. driver MUST be a valid object, initialized and loaded. This object takes ownership */
	PKCS_11_Manager(OpDLL *driver);

	/** Intialize the Object */
	virtual OP_STATUS Init(uni_char **arguments, int max_args);

	~PKCS_11_Manager();

	
	/** Retrieve a list of the Public Keys and certificates available from this provider, items are added to cipherlist */
	virtual OP_STATUS	GetAvailableCards(SSL_CertificateHandler_ListHead *cipherlist);

	/** Checks that all cards are present in their readers, and takes necessary actions for those cards that are not present */
	virtual void ConfirmCardsPresent();

	BOOL MustLoginForCertificate(){return certificate_login;}

	/** Moves the token from the temporary list to the active token line */
	void ActivateToken(PKCS_11_Token *tokn);

	/** Translate PKCS11 error code into an OP_STATUS code */
	OP_STATUS	TranslateToOP_STATUS(CK_RV val) const;
	
	/** Translate PKCS11 error code into an SSL_Alert code */
	BOOL		TranslateToSSL_Alert(CK_RV val, SSL_Alert &msg) const;

	/** Translate PKCS11 error code into an SSL_Alert code and set it*/
	BOOL		TranslateToSSL_Alert(CK_RV val, SSL_Error_Status *msg) const;


private:
	OP_STATUS PKCS_11_Manager::GetAvailableCardsFromSlot(CK_SLOT_ID slot, SSL_CertificateHandler_ListHead *cipherlist);

	OP_STATUS PKCS_11_Manager::SetUpCardFromSlot(CK_SLOT_ID slot, SSL_CertificateHandler_ListHead *cipherlist, SSL_BulkCipherType ctyp);
};



// PKCS_11_Token will be active as long as the given token is in the reader
class PKCS_11_Token : public Link, public OpReferenceCounter
{
private:

	OpSmartPointerNoDelete<PKCS_11_Manager> master;

	CK_FUNCTION_LIST_PTR functions;

	CK_SLOT_ID slot_id;
	CK_SESSION_HANDLE session;

	SSL_BulkCipherType ciphertype;

	OpString label;
	OpString serialnumber;

	BOOL enabled;

	BOOL logged_in;
	time_t	logged_in_time;

	time_t last_used;


	BOOL need_login;
	BOOL need_pin;

	SSL_varvector24_list certificate;

	BOOL confirmed_present;

public:

	PKCS_11_Token(PKCS_11_Manager *mstr, CK_FUNCTION_LIST_PTR func, CK_SLOT_ID slot, SSL_BulkCipherType ctype);
	~PKCS_11_Token();

	OP_STATUS Construct();

	SSL_PublicKeyCipher *GetKey();

	void Activate();
	BOOL CheckCardPresent();

	void Disable(){enabled = FALSE;}

    CK_RV Login(SSL_secure_varvector32 &password);

	CK_SLOT_ID GetSlot(){return slot_id;}
	OpStringC GetLabel(){return label;}
	OpStringC GetSerialNumber(){return serialnumber;}

	SSL_varvector24_list &GetCertificate(){return certificate;}
	
	BOOL Enabled(){return enabled;}
	BOOL LoggedIn();
	BOOL NeedLogIn(){return need_login && !LoggedIn();}
	BOOL NeedPin(){return need_pin;}

	void UsedNow();
	BOOL TimedOut();
	void TerminateSession();

	BOOL GetConfirmedPresent(){return confirmed_present;}
	void SetConfirmedPresent(BOOL val){confirmed_present = val;}

	/** Return TRUE if session was created */
	BOOL CheckSession();
	CK_RV CreateSession(CK_SESSION_HANDLE_PTR session_ptr) const;
	void CloseSession();
	void CloseSession(CK_SESSION_HANDLE_PTR session_ptr) const;

	OP_STATUS GetCertificateFromToken();
	BOOL IsTokenPresent();

	PKCS_11_Token *Suc(){return (PKCS_11_Token *) Link::Suc();}
	PKCS_11_Token *Pred(){return (PKCS_11_Token *) Link::Pred();}
};

#endif
