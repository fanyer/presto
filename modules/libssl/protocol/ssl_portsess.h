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

#ifndef SSLPORTSSESS_H
#define SSLPORTSSESS_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_AcceptCert_Item;

#include "modules/libssl/base/sslprotver.h"
#include "modules/url/url_sn.h"
class SSL;

// After ssl version testing, the test will be valid for 4 weeks
#define SSL_VERSION_TEST_VALID_TIME 4*7*24*3600

class SSL_Port_Sessions : public Link, public OpReferenceCounter
{
private:
	ServerName_Pointer server_name;
	unsigned short port;
	
	BOOL is_iis_4;
	BOOL is_iis_5;
	SSL_ProtocolVersion known_to_support_version;
	TLS_Feature_Test_Status feature_status;
	TLS_Feature_Test_Status previous_successfull_feature_status;

	time_t valid_until;
	BOOL tls_force_inactive;
	BOOL tls_use_ssl3_nocert;
	BOOL use_dhe_reduced; // reduce preference for DHE ciphers in cipher suite selection?
	
	SSL_SessionStateRecordHead sessioncache;
	AutoDeleteHead Session_Accepted_Certificates;
	
	SSL_CertificateItem *last_certificate;
	time_t Certificate_last_used;
	//SSL_CertificateHandler_List *last_certificate;
	//SSL_PublicKeyCipher *last_used_key;
	
	OtlList<SSL*> connections_using_this_session;

public:
	SSL_Port_Sessions(ServerName *server, unsigned short _port);
	virtual ~SSL_Port_Sessions();

	OP_STATUS AddNegotiationListener(SSL *connection) { if(connections_using_this_session.FindItem(connection)!=connections_using_this_session.End()) return OpStatus::OK; return connections_using_this_session.Prepend(connection); }
	void RemoveNegotiationListener(SSL *connection) {  connections_using_this_session.RemoveItem(connection);}

	/* This session of this connection is negotiated. Signal to all
	 * associated  SSL connections that the handshakes can continue.
	 *
	 * @param success If true the negotiation succeeded,
	 *                If false the negotiation failed.
	 */
	void BroadCastSessionNegotiatedEvent(BOOL success);

	void FreeUnusedResources(BOOL all);
	BOOL SafeToDelete();

	ServerName *GetServerName(){return server_name;}
	unsigned short Port() const{return port;}
	void SetKnownToSupportVersion(SSL_ProtocolVersion &ver){known_to_support_version = ver;}

	/** The version we know the server supports and what is used in this session. The highest version we know is suported from the server. */
	SSL_ProtocolVersion GetKnownToSupportVersion(){return known_to_support_version;}

	/** What version we send in client hello. */
	TLS_Feature_Test_Status GetFeatureStatus()const{ return feature_status;};
	void SetFeatureStatus(TLS_Feature_Test_Status state){feature_status = state;};

	TLS_Feature_Test_Status GetPreviousSuccesfullFeatureTest(){ return previous_successfull_feature_status;};
	void SetPreviousSuccesfullFeatureTest(TLS_Feature_Test_Status state){previous_successfull_feature_status = state;};

	BOOL IsInTestModus() const { return feature_status < TLS_Version_not_supported ? TRUE : FALSE; }

	void SetValidTo(time_t until_time){valid_until = until_time;}
	time_t GetValidTo() const {return valid_until;}

	BOOL IsIIS4Server() const{return is_iis_4;}
	BOOL IsIIS5Server() const{return is_iis_5;}
	void SetIsIIS4Server(BOOL val){is_iis_4 = val;}
	void SetIsIIS5Server(BOOL val){is_iis_5 = val;}
	
	BOOL TLSUseSSLv3NoCert() const{return tls_use_ssl3_nocert;}
	void SetTLSUseSSLv3NoCert(BOOL val){tls_use_ssl3_nocert = val;}
	
	BOOL TLSDeactivated() const{return tls_force_inactive;}
	void SetTLSDeactivated(BOOL val){tls_force_inactive = val;}

	BOOL UseReducedDHE() const{return use_dhe_reduced;}
	void SetUseReducedDHE(){use_dhe_reduced = TRUE;}
	
	BOOL HasCertificate();
	void CheckCertificateTime(time_t);
	SSL_CertificateHandler_List *GetCertificate();
	void SetCertificate(SSL_CertificateHandler_List *);
	
	SSL_SessionStateRecordList *FindSessionRecord();
	void RemoveSessionRecord(SSL_SessionStateRecordList *);
	void AddSessionRecord(SSL_SessionStateRecordList *target);
	void InvalidateSessionCache(BOOL url_shutdown = FALSE);
	BOOL InvalidateSessionForCertificate(SSL_varvector24 &);

	//SSL_varvector24_list &GetSessionCertificates(){return Session_Accepted_Certificates;};
	void AddAcceptedCertificate(SSL_AcceptCert_Item *new_item);
	SSL_AcceptCert_Item *LookForAcceptedCertificate(const SSL_varvector24 &, SSL_ConfirmedMode_enum mode = SSL_CONFIRMED);
	void ForgetSessionAcceptedCertificate(){Session_Accepted_Certificates.Clear();}

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	BOOL InvalidateExternalKeySession(const SSL_varvector24_list &certificate);
#endif
};

typedef OpSmartPointerNoDelete<SSL_Port_Sessions> SSL_Port_Sessions_Pointer;

#endif
#endif // SSLPORTSSESS_H
