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

#ifndef _SSL_VER_H_
#define _SSL_VER_H_

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/base/sslprotver.h"
#include "modules/libssl/methods/sslphash.h"

class SSL_MAC;
class SSL_Record_Base;
class SSL_CipherDescriptions;
struct Handshake_actions_st;
class SSL_CipherSpec;

#define SSL_MASTER_SECRET_SIZE 48

enum Handshake_Add_Point{
	Handshake_Add_In_Front,		// Add at start of list
	Handshake_Add_Before_ID,	// Add immediately before the given ID, if it does not exist use Handshake_Add_At_End
	Handshake_Add_After_ID,		// Add immediately after the given ID, if it does not exist, use Handshake_Add_At_End
	Handshake_Add_At_End		// Add at end of list (default)
};

enum Handshake_Remove_Point{
	Handshake_Remove_From_ID,
	Handshake_Remove_Only_ID
};

enum Handshake_Queue {
	Handshake_Send,
	Handshake_Receive
};

struct SSL_DataQueueHead : public SSL_Head {
	SSL_secure_varvector32 *First() const{
		return (SSL_secure_varvector32 *)SSL_Head::First();
	};       //pointer
	SSL_secure_varvector32 *Last() const{
		return (SSL_secure_varvector32 *)SSL_Head::Last();
	};     //pointer
};

class SSL_Version_Dependent : public SSL_Error_Status
{  
protected:

	enum SSL_V3_handshake_id {
		SSL_V3_Recv_Hello_Request,
		SSL_V3_Recv_Server_Hello,
		SSL_V3_Recv_Certificate,
		TLS_Recv_CertificateStatus,
		SSL_V3_Recv_Server_keys,
		SSL_V3_Recv_Certificate_Request,
		SSL_V3_Recv_Server_Done,
		SSL_V3_Send_PreMaster_Key,
		SSL_V3_Send_Certficate,
		SSL_V3_Pre_Certficate_Verify,
		SSL_V3_Send_Certficate_Verify,
		SSL_V3_Send_No_Certficate,
		SSL_V3_Post_Certficate,
		SSL_V3_Send_Change_Cipher,
		SSL_V3_Recv_Server_Cipher_Change,
		SSL_V3_Recv_Server_Finished,
		SSL_V3_Send_Client_Finished,
		SSL_V3_Send_Handshake_Complete,
		SSL_V3_Send_Pause,
		TLS_Send_Next_Protocol,
#ifdef LIBSSL_ENABLE_SSL_FALSE_START
		SSL_V3_Send_False_Start_Application_Data
#endif // LIBSSL_ENABLE_SSL_FALSE_START
	};

private:
	Head	Send_Queue;
	Head	Receive_Queue;

	SSL_DataQueueHead handshake_queue;

protected:
	SSL_ProtocolVersion	version;
	SSL_Hash_Pointer final_hash;

#ifdef _DEBUG
	SSL_secure_varvector16 handshake;
#endif

	const SSL_CipherDescriptions *cipher_desc;
	SSL_ConnectionState *conn_state;

protected:
	SSL_Version_Dependent(uint8 ver_major, uint8 ver_minor);

	void ClearHandshakeActions(Handshake_Queue action_queue);
	void AddHandshakeAction(Handshake_Queue action_queue , SSL_V3_handshake_id id, SSL_HandShakeType mtyp, SSL_Handshake_Status _mstat,  SSL_Handshake_Action actn, Handshake_Add_Point add_policy = Handshake_Add_At_End, SSL_V3_handshake_id add_id = SSL_V3_Recv_Hello_Request);
	void RemoveHandshakeAction(Handshake_Queue action_queue, int id, Handshake_Remove_Point remove_policy = Handshake_Remove_Only_ID);
	SSL_Handshake_Status GetHandshakeStatus(Handshake_Queue action_queue, int id);

private:
	Handshake_actions_st *GetHandshakeItem(Handshake_Queue action_queue, int id);
	void Init();

public: 
	
    virtual ~SSL_Version_Dependent();
    const SSL_ProtocolVersion &Version() const{return version;};

	void SetConnState(SSL_ConnectionState *conn){conn_state = conn;}

    virtual SSL_MAC *GetMAC() const = 0;
    virtual SSL_Record_Base *GetRecord(SSL_ENCRYPTMODE mode) const;
	virtual void SetCipher(const SSL_CipherDescriptions *desc);
    
    void AddHandshakeHash(SSL_secure_varvector32 *);
    virtual void GetHandshakeHash(SSL_SignatureAlgorithm alg, SSL_secure_varvector32 &);
	void GetHandshakeHash(SSL_Hash_Pointer &hasher);
    virtual void GetFinishedMessage(BOOL client, SSL_varvector32 &target)= 0;
	
    virtual void CalculateMasterSecret(SSL_secure_varvector16 &mastersecret, const SSL_secure_varvector16 &premaster)=0;
	
    virtual void CalculateKeys(const SSL_varvector16 &mastersecret,
		SSL_CipherSpec *client, SSL_CipherSpec *server
		) /*const */=0;
	
    virtual BOOL SendAlert(SSL_Alert &, BOOL cont) const= 0; // If necessary translate passed alert
    virtual BOOL SendClosure() const;

	virtual SSL_Handshake_Action HandleHandshakeMessage(SSL_HandShakeType);
	virtual SSL_Handshake_Action NextHandshakeAction(SSL_HandShakeType &);
	virtual void SessionUpdate(SSL_SessionUpdate);
	virtual BOOL ExpectingCipherChange();
};

#endif
#endif
