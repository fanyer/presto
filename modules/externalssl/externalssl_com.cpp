/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#include "core/pch.h"

#ifdef _EXTERNAL_SSL_SUPPORT_

#include "modules/externalssl/externalssl_com.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/externalssl/externalssl_displaycontext.h"
#include "modules/externalssl/extssl.h"
#include "modules/externalssl/src/OpSSLSocket.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/windowcommander/src/WindowCommander.h"


External_SSL_Comm::External_SSL_Comm(MessageHandler* msg_handler, ServerName* hostname, unsigned short portnumber, BOOL connect_only, BOOL secure)
	: Comm(msg_handler, hostname, portnumber, connect_only)
	, m_real_server_name(0)
	, m_security_state(SECURITY_STATE_UNKNOWN)
	, m_security_problems(FALSE)
	, m_display_context(NULL)  /* Owned by External_SSL_DisplayContextManager */
	, m_server_certificate(NULL)
	, m_server_certificate_chain(NULL)
{
}

External_SSL_Comm::~External_SSL_Comm()
{
	OP_DELETE(m_server_certificate_chain);
	OP_DELETE(m_server_certificate);

	if (m_display_context)
	{
		m_display_context->CommDeleted();
		g_ssl_display_context_manager->DeleteDisplayContext(m_display_context); // Will only delete if m_display_context->OnCertificateBrowsingDone() has been called
	}

	OP_DELETE(m_real_server_name);
}

void External_SSL_Comm::OnSocketConnectError(OpSocket* aSocket, OpSocket::Error aError)
{
	if (!info.is_secure) 
	{
		Comm::OnSocketConnectError(aSocket, aError);
		return;
	}
	m_security_problems = TRUE;

	if (m_display_context && m_display_context->GetCertificateBrowsingDone())
	{
		g_ssl_display_context_manager->DeleteDisplayContext(m_display_context);
		m_display_context = NULL;
	}
	OP_ASSERT(m_display_context == NULL);

	AnchoredCallCount blocker(this);
	if( aError == OpSocket::SECURE_CONNECTION_FAILED)
	{
		// Extra check. Moved from outside of the loop to allow
		// Comm::OnSocketConnectError() to be called.
		if (m_display_context)
			return;

		OpWindow *opwin = 0;
		Window* window = NULL;
		SetProgressInformation(GET_ORIGINATING_WINDOW, 0, &opwin);
		
		if (opwin)
		{
			for(window = g_windowManager->FirstWindow(); window; window = window->Suc())
			{
				if (window->GetOpWindow() == opwin)
				{
					break;
				}
			}
		}		
		
		int errors = m_Socket->GetSSLConnectErrors();
	
		if (!(errors & OpSocket::CERT_BAD_CERTIFICATE) && (m_server_certificate = m_Socket->ExtractCertificate()) != NULL)
		{
			if (m_server_certificate && m_Host->IsCertificateAccepted(m_server_certificate))
			{
				m_Socket->AcceptInsecureCertificate(m_server_certificate);
				OP_DELETE(m_server_certificate);
				m_server_certificate = NULL;
				return;
			}

			WindowCommander *windowCommander = NULL;
			if (window && (windowCommander = window->GetWindowCommander()))
			{	
				OP_STATUS status;
				m_display_context = g_ssl_display_context_manager->OpenCertificateDialog(status, this, windowCommander,  errors, OpSSLListener::SSL_CERT_OPTION_NONE);
				
				if (m_display_context)
					return;
				else if (OpStatus::IsMemoryError(status) && mh && (window = mh->GetWindow()))
					window->RaiseCondition(status);
			}
		}
	}
	
	OP_DELETE(m_server_certificate);
	m_server_certificate = NULL;
	Comm::OnSocketConnectError(aSocket, aError);	
}

void External_SSL_Comm::OnSocketConnected(OpSocket* aSocket)
{
	if(info.is_secure)
	{
		OP_STATUS status;
		if(OpStatus::IsError(status = GetSSLSecurityDescription(m_security_state, m_securitytext)))
		{
			if (status == OpStatus::ERR_NO_MEMORY && mh)
				mh->GetWindow()->RaiseCondition(status);

			m_security_state = SECURITY_STATE_NONE;
			m_securitytext.Empty();
		}
	}

	Comm::OnSocketConnected(aSocket);
}

OP_STATUS External_SSL_Comm::SetSSLSocketOptions()
{
	if (!m_Socket)
		return OpStatus::ERR;

	const ServerName* server_name = GetRealServerName();
	OP_ASSERT(server_name);
	const uni_char* uni_name = server_name->UniName();
	OP_STATUS status = m_Socket->SetServerName(uni_name);
	RETURN_IF_ERROR(status);

	UINT32 cipher_count = 0;
	const cipherentry_st* ciphers = g_external_ssl_options->GetCipherArray(cipher_count);
	if (!ciphers)
		return OpStatus::ERR;

	status = m_Socket->SetSecureCiphers(ciphers, cipher_count);
	return status;
}

CommState External_SSL_Comm::ServerCertificateAccepted()
{
	if (OpStatus::IsSuccess(m_Host->AddAcceptedCertificate(m_server_certificate)) && OpStatus::IsSuccess(m_Socket->AcceptInsecureCertificate(m_server_certificate)))
		return COMM_LOADING;
	else
		return COMM_REQUEST_FAILED;
}

CommState External_SSL_Comm::Connect()
{
	m_securitytext.Empty();
	if (info.is_secure)
	{
		m_security_state = SECURITY_STATE_UNKNOWN;	
		if (OpStatus::IsError(SetSSLSocketOptions()))
			return COMM_REQUEST_FAILED;
	}
	else
	{
		m_security_state = SECURITY_STATE_NONE;	
	}
	
	return Comm::Connect();
}

const ServerName* External_SSL_Comm::GetRealServerName() const
{
	return m_real_server_name ? m_real_server_name : static_cast <const ServerName*> (m_Host);
}


OpCertificate* External_SSL_Comm::ExtractCertificate()
{
	return m_Socket->ExtractCertificate();
}

void External_SSL_Comm::SignalConnectionClosed()
{
	Comm::OnSocketConnectError(m_Socket, OpSocket::SECURE_CONNECTION_FAILED);	
}

OP_STATUS External_SSL_Comm::GetSSLSecurityDescription(int &sec_state, OpString &description)
{
	sec_state = SECURITY_STATE_NONE;
	description.Empty();

	if (!m_Socket)
		return OpStatus::OK;
	
	cipherentry_st used_cipher;
	BOOL TLS_v1_used = FALSE;
	UINT32 PK_keylength = 0;

	RETURN_IF_ERROR(m_Socket->GetCurrentCipher(&used_cipher, &TLS_v1_used, &PK_keylength));

	const uni_char *fullname_format = g_external_ssl_options->GetCipher(used_cipher.id, sec_state);

	if(sec_state > 1)
	{
#ifdef DIA_PK_KEYLENGTH_SUPPORTED
		if(PK_keylength< 646)
			sec_state = 1;
		else if(PK_keylength <838)
			sec_state--;
#endif
	}
	
	if (m_security_problems)
		sec_state = 0;
	
	const uni_char *prot = (TLS_v1_used ? UNI_L("TLS v1.0") : UNI_L("SSL v3.0"));

	if (fullname_format)
	{
		OpString temp_var;
		RETURN_IF_ERROR(temp_var.AppendFormat(fullname_format, PK_keylength));
		RETURN_IF_ERROR(description.SetConcat(prot, UNI_L(" "), temp_var));
	}

	return OpStatus::OK;
}


BOOL External_SSL_Comm::InsertTLSConnection(ServerName *host, unsigned short port)
{
	info.is_secure = TRUE;
	m_security_state = SECURITY_STATE_UNKNOWN;
	m_securitytext.Empty();
	if (m_Socket)
	{
		OP_STATUS status = OpStatus::OK;
		m_real_server_name = ServerName::Create(host->Name(), status);
		OP_ASSERT(OpStatus::IsSuccess(status) || !m_real_server_name);

		if(SetSSLSocketOptions() != OpStatus::OK)
			return FALSE;
		return m_Socket->UpgradeToSecure();
	}
	return TRUE;
}

OP_STATUS External_SSL_Comm::CreateSocket(OpSocket** aSocket, BOOL secure)
{
	return OpSSLSocket::Create(aSocket, this, secure);
}

#endif // _EXTERNAL_SSL_SUPPORT_
