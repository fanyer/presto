/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef EXTERNAL_SSL_COM_H_
#define EXTERNAL_SSL_COM_H_

#ifdef _EXTERNAL_SSL_SUPPORT_

#include "modules/url/protocols/comm.h"
#include "modules/externalssl/extssl.h"

class External_SSL_CommCertificateDisplayContext;


class External_SSL_Comm : public Comm
{
public:
	// Constructor/destructor.
	External_SSL_Comm(
		MessageHandler* msg_handler,
		ServerName* hostname,
		unsigned short portnumber,
		BOOL connect_only = FALSE,
		BOOL secure = TRUE);
	virtual ~External_SSL_Comm();

public:
	// From Comm. Calls to these two functions must be sent up to COMM if m_secure == FALSE
	void OnSocketConnectError(OpSocket* aSocket, OpSocket::Error aError);
	void OnSocketConnected(OpSocket* aSocket);

public:
	// Commands.
	virtual CommState Connect();
	virtual BOOL InsertTLSConnection(ServerName* host, unsigned short port);
	OP_STATUS SetSSLSocketOptions();
	virtual void UpdateSecurityInformation() { SComm::SetProgressInformation(SET_SECURITYLEVEL, m_security_state, m_securitytext.CStr()); }
	void SignalConnectionClosed();

public:
	// Events.
	virtual CommState ServerCertificateAccepted();
	void DisplayContextDeleted() { m_display_context = NULL; }

public:
	// Getters.
	const ServerName* GetRealServerName() const;
	OP_STATUS GetSSLSecurityDescription(int& sec_state, OpString& description);
	virtual OpCertificate* ExtractCertificate();
	OpAutoVector <OpCertificate>* GetCurrentCertificateChain() { return m_server_certificate_chain ? m_server_certificate_chain : (m_server_certificate_chain = m_Socket->ExtractCertificateChain()); }

protected:
	virtual OP_STATUS CreateSocket(OpSocket** aSocket, BOOL secure);

private:
	ServerName* m_real_server_name;
	int m_security_state;
	OpString m_securitytext;
	BOOL m_security_problems;
	External_SSL_CommCertificateDisplayContext* m_display_context; // Owned by External_SSL_CommDisplayContextManager
	OpCertificate* m_server_certificate;
	OpAutoVector <OpCertificate>* m_server_certificate_chain;
};

#endif // _EXTERNAL_SSL_SUPPORT_
#endif /* EXTERNAL_SSL_COM_H_ */
