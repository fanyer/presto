/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef _SSL_
#define _SSL_
#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"

#include "modules/libssl/protocol/sslbaserec.h"
#include "modules/libssl/protocol/sslplainrec.h"
#include "modules/libssl/protocol/sslcipherrec.h"

#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/options/secprofile.h"

#include "modules/libssl/handshake/chcipher.h"

class SSL_Certificate_DisplayContext;
class SSL_CertificateHandler;
struct SSL_CertificateVerification_Info;

#define SSL_RANDOM_LENGTH 32

//#define SSL_DISCONNECT_V3

#define SSL_EVENT_BLOCKSIZE (uint16) (32*1024)


class SSL_Record_Layer : public ProtocolComm, public SSL_Error_Status
{
private: 
	struct ssl_status_info_st{
		UINT WriteRecordsToOutBuffer:1;			//used to hold up send until a certain amount of data has been collected. Mainly during handshake.
		UINT PerformApplicationDataSplit:1; // If true, the next application data package will be split into a 1 byte and a (n-1) byte record.
		UINT AllowApplicationDataSplitting:1; // If false,  ApplicationDataSplit will  never be performed for this connection.
		UINT EncryptedRecordFull:1;
		UINT FirstData:1;
		UINT ProcessingInputData:1;
		UINT ChangeReadCipher:1;
		UINT DecryptFinished:1;
		UINT ApplicationActive:1;
		UINT present_recorddecryption:2;
		UINT present_recordencryption:2;
		UINT CurrentlyProcessingIncoming:1;
		UINT ReadingData:1; // blocks Process Data received
		UINT CurrentlyProcessingOutgoing:1;
	} statusinfo;      
    
    uint32 Bufferpos;

	struct SSL_Record_Head : public SSL_Head {
		SSL_Record_Base *First() const{
			return (SSL_Record_Base *)SSL_Head::First();
		};       //pointer
		SSL_Record_Base *Last() const{
			return (SSL_Record_Base *)SSL_Head::Last();
		};     //pointer
	};

	SSL_Record_Head Buffer_in;
	SSL_Record_Head Buffer_in_encrypted;
	
    uint32 network_buffer_size;
	uint32 unconsumed_buffer_length;
    byte *networkbuffer_in;
	
    SSL_Record_Base *loading_record;
    SSL_Record_Base *plain_record;
	uint32		plain_record_pos;
    
    SSL_Record_Base *sending_record;
    SSL_varvector32 out_buffer;
	
    SSL_DataQueueHead unprocessed_application_data;
	SSL_DataQueueHead unprocessed_ssl_messages_prioritized;
	ProgressState m_AllDoneMsgMode;				// All Done msg_mode
	ProgressState m_AllDone_requestMsgMode;		// All Done request mode msg_mode
	
protected:

	enum
	{
		LAST_SOCKET_OPERATION_NONE,
		LAST_SOCKET_OPERATION_READ,
		LAST_SOCKET_OPERATION_WRITE,
	} m_last_socket_operation;

	unsigned Handling_callback;
	

	/* Keeps track of how much raw data (application data + protocol data)
	 * has been sent to comm layer.  */
	unsigned long buffered_amount_raw_data;

	ServerName *servername;
	unsigned short port;
	SSL_Port_Sessions_Pointer server_info;
	SSL_ConnectionState *connstate, *pending_connstate;
#ifdef SSL_DOCUMENT_INFO
	SSL_DocumentInfo documentinfo;
#endif
	BOOL trying_to_reconnect;
	BOOL already_tried_to_reconnect;
	BOOL setting_up_records;

#ifdef LIBSSL_HANDSHAKE_HEXDUMP
	SSL_DataQueueHead selftest_sequence;
#endif

private:
	SSL_ConnectionState*	Prepare_connstate();
	SSL_Record_Base *		Fragment_message(SSL_secure_varvector32 *);
	
	void	PerformDecryption();

	/**
	 * Check if this cipher suit requires record splitting to
	 * prevent the BEAST attack (CORE-39857)
	 *
	 * return TRUE if record splitting is required.
	 */
	BOOL 	CipherSuitRequiresRecordSplitting() const;

	BOOL	DataWaitingToBeSent() const;


	void	EventEncrypt();
	void	ProgressEncryptionPipeline();
	uint32	Handle_Receive_Record(const byte *source, uint32 length);
	void InternalInit(ServerName* hostname, unsigned short portnumber, BOOL do_record_splitting);
	void InternalDestruct();

protected:
	SSL_Record_Layer(MessageHandler* msg_handler, ServerName* hostname,
		unsigned short portnumber, BOOL do_record_splitting);
	virtual		~SSL_Record_Layer();
	
	void StartingToSetUpRecord(BOOL flag, BOOL discard_buffered_records=FALSE);

	void Perform_ProcessReceivedData();

	virtual CommState	ConnectionEstablished();
	virtual void		RequestMoreData();
	void				StartEncrypt();
	virtual void		Flush();
	
	virtual void		ProcessReceivedData();
	virtual unsigned    ReadData(char *buf, unsigned buf_len);
	
	BOOL	ProcessingFinished(BOOL ignore_loading_record=FALSE);
	virtual void		Handle_Record(SSL_ContentType)=0;
	void	RemoveRecord();
	void	MoveRecordToApplicationBuffer();
	DataStream *	GetRecord();
	SSL_ContentType	GetRecordType(){return (plain_record ? plain_record->GetType() :SSL_ContentError);}
	void	Send(SSL_secure_varvector32 *source);
	
	void	PauseApplicationData(){statusinfo.ApplicationActive = FALSE;};
	void	ResumeApplicationData();
	void	SetProcessingInputData(BOOL proc){statusinfo.ProcessingInputData = proc;};
	BOOL	IsProcessingInputData() const{return statusinfo.ProcessingInputData;}; 
	BOOL	EmptyBuffers(BOOL ignore_loading_record) const;


	/* Ensure the out records are collected and written to internal buffer and not the network.
	 * The buffer will be flushed at a later point.
	 *
	 * Used to package TCP packages more efficiently
	 *
	 * @param write_to_buffer If TRUE write to internal buffer.
	 */
	void	WriteRecordsToBuffer(BOOL write_to_buffer) { statusinfo.WriteRecordsToOutBuffer = write_to_buffer; }
	void	FlushBuffers(BOOL complete=FALSE);
	int		Init();
	
	void	Do_ChangeCipher(BOOL writecipher);
	virtual BOOL	AcceptNewVersion(const SSL_ProtocolVersion &)=0;

    virtual BOOL    Closed();

	void	ForceFlushPrioritySendQueue();
	

public:
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual void	SetAllSentMsgMode(ProgressState parm, ProgressState request_parm);
	void	RemoveURLReferences();
};


class SSL : public SSL_Record_Layer{
private:
    SSL_STATE current_state;
    SSL_HandShakeMessage loading_handshake;
	
    SSL_ChangeCipherSpec_st		loading_chcipherspec;
    SSL_Alert					loading_alert;
    SSL_Security_ProfileList_Pointer security_profile;
	
    struct ssl_flags_st{
		UINT closing_connection:1;
		UINT closed_with_error:2;
		UINT internal_error_message_set:1;
		UINT allow_auto_disable_tls:1;
		UINT is_closing_connection:1;
		UINT use_sslv3_handshake:1;
		UINT application_records_permitted:1;
		UINT delayed_client_certificate_request:1;
		UINT received_closure_alert:1;
		UINT asking_password:1;
    } flags;
	
	SSL_Certificate_DisplayContext *certificate_ctx;

	/* Keeps track of the amount of application data has been sent to SSL
	 * layer. */
	unsigned long buffered_amount_application_data;

private:
	virtual void	Handle_Record(SSL_ContentType);
	
	SSL_STATE	Handle_Change_Cipher(SSL_STATE pref_next_state);
	SSL_STATE	Handle_Received_Alert( SSL_STATE pref_next_state);
	SSL_STATE	Handle_HandShake(SSL_STATE pref_next_state);
	SSL_STATE	Handle_Application(SSL_STATE pref_next_state);
	
	SSL_STATE	Handle_Start_Handshake(SSL_HandShakeMessage &,SSL_STATE, BOOL ignore_any_ongoing_session_negotiation = FALSE);
	void		CalculateHandshakeHash(SSL_secure_varvector32 *);
	SSL_STATE Handle_Raised_Error(SSL_STATE,BOOL usrknow = FALSE);
	SSL_STATE Handle_Raised_Error(SSL_AlertLevel,SSL_AlertDescription,SSL_STATE,BOOL usrknow = FALSE);
	SSL_STATE Handle_Local_Error(SSL_Alert &,SSL_STATE,BOOL usrknow = FALSE);
	SSL_STATE Handle_Local_Error(SSL_AlertLevel,SSL_AlertDescription,SSL_STATE,BOOL usrknow = FALSE);

	SSL_STATE HandleDialogFinished(MH_PARAM_2 par2);
	
	SSL_STATE SendMessageRecord(SSL_ContentType type, DataStream &source, BOOL hashmessage, SSL_STATE pref_next_state);
	SSL_STATE SendMessageRecord(SSL_ChangeCipherSpec_st &,SSL_STATE);
	SSL_STATE SendMessageRecord(SSL_HandShakeMessage &,SSL_STATE);
	SSL_STATE SendMessageRecord(SSL_Alert &,SSL_STATE);
	SSL_STATE Close_Connection(SSL_STATE);
	
	SSL_STATE Handle_SendChangeCipher(SSL_STATE);
	void	SendRecord(SSL_ContentType type,byte *source,uint32 blen);
	
protected:   
	void SetProgressInformation(ProgressState progress_level, unsigned long progress_info1=0, const void *progress_info2=NULL);

	virtual CommState    ConnectionEstablished();
	virtual void    SendData(char *,unsigned blen);
	virtual OP_STATUS SetCallbacks(MessageObject* master, MessageObject* parent);
	
	virtual void ResetConnection();

	/* the functions below returns finished. */
	virtual BOOL ApplicationHandleWarning(const SSL_Alert &, BOOL recv, BOOL &cont);
	/* This one has  always finished */
	virtual void ApplicationHandleFatal(const SSL_Alert &, BOOL recv);

	void InternalConstruct();
	void InternalDestruct();
public:
	SSL(MessageHandler* msg_handler, ServerName* hostname, unsigned short portnumber, BOOL do_record_splitting = FALSE);
	virtual ~SSL();

	/* Signal to this SSL connection that the associated key session
	 * has been negotiated, and the handshake can continue.
	 *
	 * This could happen if this connection used a key session
	 * initiated by another connection.
	 *
	 * @param success If true the negotiation succeeded,
	 *                If false the negotiation failed.
	 */
	void SessionNegotiatedContinueHandshake(BOOL success);

	virtual BOOL	SafeToDelete() const;
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual void Stop();
	virtual BOOL AcceptNewVersion(const SSL_ProtocolVersion &);
	
	virtual void	UpdateSecurityInformation();
	virtual int     GetSecurityLevel() const;  // 30/07/97 YNP
	//virtual const uni_char *GetSecurityText() const;
	
public:
	virtual BOOL HasId(unsigned int sid);

	void ForceV3Handshake();

private:
	/**
	 * Checks if the HTTP strict transport suppport is set for current server, and
	 * evaluates if the connection is allowed for potential warnings.
	 *
	 * Currently we allow OCSP and CRL requests to fail to avoid blocking
	 * pages that supports HSTS. CRL/OCSP can fail if there are network problem or
	 * the OCSP/CRL servers are failing to respond.
	 *
	 * @return TRUE if connection is allowed, FALSE otherwise
	 */
	BOOL EvaluateHSTSPassCondition();

	SSL_STATE Process_Handshake_Message(SSL_STATE pref_next_state);
	SSL_STATE Process_HandshakeActions(SSL_STATE nextstate);
	SSL_STATE Process_KeyExchangeActions(SSL_STATE nextstate, SSL_KEA_ACTION kea_action);
	SSL_STATE ProgressHandshake(SSL_STATE pref_next_state);

	SSL_STATE StartNewHandshake(SSL_STATE pref_next_state);
	SSL_KEA_ACTION Resume_Session();
	SSL_KEA_ACTION Commit_Session();

	void PostConsoleMessage(Str::LocaleString str, int errorcode=0);

public:
};

inline void
SSL::ForceV3Handshake()
{
    flags.use_sslv3_handshake = TRUE;
}

#endif // _NATIVE_SSL_SUPPORT_
#endif // _SSL_
