/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _SSL_KEYEX_H_
#define _SSL_KEYEX_H_

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/url/url2.h"
#include "modules/libssl/base/sslciphspec.h"
#include "modules/libssl/options/cipher_description.h"
#include "modules/libssl/keyex/certverify.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/handshake/tlssighash.h"

class SSL_CertificateHandler_List;
class SSL_CertificateItem;
struct SSL_CertificateVerification_Info;
class SSL_Auto_Root_Retriever;
class SSL_Auto_Untrusted_Retriever;

class SSL_KeyExchange : public SSL_CertificateVerifier
{
private:
	unsigned int id;

    SSL_ConnectionState *commstate;
    SSL_CertificateHandler_ListHead *client_cert_list;
	SSL_KEA_ACTION post_verify_action;

protected:

	SSL_secure_varvector16 pre_master_secret;
	SSL_secure_varvector16 pre_master_secret_encrypted;
	SSL_secure_varvector16 pre_master_secret_clear; // used by SSL v2


	SSL_Handshake_Status receive_certificate;
	SSL_Handshake_Status receive_server_keys;
	SSL_Handshake_Status receive_certificate_request;

#ifdef _SUPPORT_TLS_1_2
	TLS_SignatureAlgListBase supported_sig_algs;
#endif

private: 
	void InternalDestruct();
	
protected:
    virtual BOOL RecvServerKey(uint32 signkey_length);
	//void GetProtocolVersion(SSL_ProtocolVersion &ver, BOOL set_write){ver = (set_write ? commstate->write.version : commstate->read.version);};
	const SSL_ProtocolVersion &GetSentProtocolVersion(){return commstate->sent_version;};
	
public:
    
    SSL_CertificateHandler *AccessServerCertificateHandler(){
		return (commstate != NULL ? commstate->server_cert_handler : NULL);
	};
    SSL_ASN1Cert_list &AccessServerCertificateList();
    
    SSL_CipherSpec *AccessCipher(BOOL client){
		return (commstate != NULL ? (client ? commstate->write.cipher : commstate->read.cipher) : NULL);
	};
    const SSL_CipherDescriptions *AccessCipherDescription(){
		return (commstate != NULL  && commstate->session != NULL ? static_cast<const SSL_CipherDescriptions *>(commstate->session->cipherdescription) : (const SSL_CipherDescriptions *) NULL);
	};

	virtual SSL_ConnectionState *AccessConnectionState(){return commstate;}
	
    uint32 ProcessClientCertRequest(SSL_CertificateRequest_st &);
    void SetSigAlgorithm(SSL_SignatureAlgorithm sigalg){ if(commstate != NULL) commstate->sigalg = sigalg;};
    
	
public:
    SSL_KeyExchange();
    SSL_KeyExchange(const SSL_KeyExchange *other);
    virtual ~SSL_KeyExchange();

	/** The Id of this object */
	unsigned int		Id() const { return id; };

	void SetCommState(SSL_ConnectionState *); // Also sets ForwardTo
		
    const SSL_secure_varvector16 &PreMasterSecret() const{return pre_master_secret;};
    void PreMasterUsed(){pre_master_secret.Resize(0);};
    const SSL_secure_varvector16 &Encrypted_PreMasterSecret() const{return pre_master_secret_encrypted;};
    void Encrypted_PreMasterUsed(){pre_master_secret_encrypted.Resize(0);};
    const SSL_secure_varvector16 &Clear_v2_PreMasterSecret_Part() const{return pre_master_secret_clear;};
    void Clear_v2_PreMasterPartUsed(){pre_master_secret_clear.Resize(0);};


    SSL_CertificateHandler_ListHead *ClientCertificateCandidates();
	SSL_CertificateHandler_ListHead *LookAtClientCertificateCandidates(){return client_cert_list;}
    void SelectClientCertificate(SSL_CertificateHandler_List *, SSL_PublicKeyCipher *clientkey);
	virtual int SetupServerCertificateCiphers()=0;
	virtual int SetupServerKeys(SSL_Server_Key_Exchange_st *serverkeys);

	virtual void PreparePremaster()=0;
    virtual SSL_KeyExchangeAlgorithm GetKeyExhangeAlgorithm() = 0;

	virtual SSL_KEA_ACTION ReceivedCertificate(); // Certificate in commstate
	virtual SSL_KEA_ACTION ReceivedServerKeys(SSL_Server_Key_Exchange_st *); // Signature have been verified by caller
	virtual SSL_KEA_ACTION ReceivedCertificateRequest(SSL_CertificateRequest_st *);

	virtual BOOL GetIsAnonymous();

	SSL_KEA_ACTION GetPostVerifyAction() const {return post_verify_action;}

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

protected:
	virtual int VerifyCheckExtraWarnings(int &low_security_reason);
	virtual void VerifyFailed(SSL_Alert &msg);
	virtual void VerifySucceeded(SSL_Alert &msg);
private:
	void VerifyFailedStep(SSL_Alert &msg);
	void VerifySucceededStep();
	SSL_KEA_ACTION SetupPrivateKey(BOOL asked_for_password);
};

SSL_KeyExchange *CreateKeyExchangeL(SSL_KeyExchangeAlgorithm kea, SSL_SignatureAlgorithm signertype);

#endif
#endif // _SSL_KEYEX_H_
