/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#include "core/pch.h"
#ifdef _EXTERNAL_SSL_SUPPORT_

#include "modules/externalssl/externalssl_displaycontext.h"
#include "modules/externalssl/externalssl_com.h"
#include "modules/pi/network/OpCertificate.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/locale/oplanguagemanager.h"

External_SSL_display_certificate::External_SSL_display_certificate()
	: m_settings(OpSSLListener::SSL_CERT_SETTING_NONE)
	, m_is_installed(FALSE)
{
	
}

OP_STATUS External_SSL_display_certificate::Make(OpSSLListener::SSLCertificate *&cert_out, OpCertificate *cert)
{
	cert_out = NULL;
	
	OpAutoPtr<External_SSL_display_certificate> temp_cert(OP_NEW(External_SSL_display_certificate, ()));
	if (temp_cert.get())
	{	
		RETURN_IF_ERROR(temp_cert->m_short_name.Set(cert->GetShortName()));
		RETURN_IF_ERROR(temp_cert->m_full_name.Set(cert->GetFullName()));

		RETURN_IF_ERROR(temp_cert->m_valid_from.Set(cert->GetValidFrom()));
		RETURN_IF_ERROR(temp_cert->m_valid_to.Set(cert->GetValidTo()));
		RETURN_IF_ERROR(temp_cert->m_info.Set(cert->GetInfo()));

		temp_cert->m_issuer.Set(cert->GetIssuer());
		cert_out = temp_cert.release();
		return OpStatus::OK;
	}
	
	
	return OpStatus::ERR_NO_MEMORY;
}

int External_SSL_display_certificate::GetSettings() const
{
	return m_settings;
}

void External_SSL_display_certificate::UpdateSettings(int settings)
{
	m_settings = settings;
}


External_SSL_CertificateDisplayContext::~External_SSL_CertificateDisplayContext()
{
}

External_SSL_CertificateDisplayContext::External_SSL_CertificateDisplayContext(External_SSL_DisplayContextManager *display_contexts_manager)
	: m_display_contexts_manager(display_contexts_manager)
	, m_certificate_browsing_done(FALSE)
{		
}

OP_STATUS External_SSL_CommCertificateDisplayContext::Make(External_SSL_CommCertificateDisplayContext *&display_context, int errors, External_SSL_DisplayContextManager *display_contexts_manager, External_SSL_Comm *comm_object) 
{
	display_context = NULL;
	OpAutoPtr<External_SSL_CommCertificateDisplayContext> temp_display_context(OP_NEW(External_SSL_CommCertificateDisplayContext, (display_contexts_manager, comm_object)));
	if (!temp_display_context.get())
		return OpStatus::ERR_NO_MEMORY;

	OpAutoVector<OpCertificate> *certificate_chain;
	if ((certificate_chain = comm_object->GetCurrentCertificateChain()))
	{
		unsigned int number_of_certificates = certificate_chain->GetCount();
		if (number_of_certificates > 0)
		{
			OP_STATUS status;
			OpCertificate *cert;
			OpSSLListener::SSLCertificate *display_cert;
			unsigned int i;
			for (i = 0; i < number_of_certificates; i++)
			{	 	
				display_cert = NULL;
				cert = certificate_chain->Get(i);
				if ( 
						OpStatus::IsError(status = External_SSL_display_certificate::Make(display_cert, cert)) || 
						OpStatus::IsError(status = temp_display_context->m_display_certificates.Add(display_cert))
					)
				{
					temp_display_context->m_display_certificates.DeleteAll();
					OP_DELETE(display_cert);
					return status;
				}
			}	
		}
	}

	RETURN_IF_ERROR(temp_display_context->CreateComments(errors));
	
	display_context = temp_display_context.release();
	return OpStatus::OK;
}

OpSSLListener::SSLCertificate* External_SSL_CertificateDisplayContext::GetCertificate(unsigned int index)
{
	return m_display_certificates.Get(index);
}

const uni_char* External_SSL_CertificateDisplayContext::GetCertificateComment(unsigned int index) const
{ 
    OpString* str = m_comments.Get(index);
    return str ? str->CStr() : NULL;
}

OP_STATUS External_SSL_CertificateDisplayContext::GetServerName(OpString* server_name) const
{
    return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS External_SSL_CommCertificateDisplayContext::GetServerName(OpString* server_name) const
{
	ServerName *server = m_comm_object->HostName();
	if (!server)
		return OpStatus::ERR;
	server_name->Set(server->UniName());
	return OpStatus::OK;
}

void External_SSL_CommCertificateDisplayContext::OnCertificateBrowsingDone(BOOL ok, OpSSLListener::SSLCertificateOption options)
{
	if (m_comm_object)
	{
		if (ok)
			m_comm_object->ServerCertificateAccepted();
		else
			m_comm_object->SignalConnectionClosed();
	}
	
	m_certificate_browsing_done = TRUE;
}

OP_STATUS External_SSL_CertificateDisplayContext::AddComment(Str::LocaleString str_number, uni_char *temp_buffer, unsigned int buffer_size, const uni_char *arg1, const uni_char *arg2)
{
	OpString str;
	OpAutoPtr<OpString> comment_string(OP_NEW(OpString,()));
	RETURN_OOM_IF_NULL(comment_string.get());
	
	RETURN_IF_ERROR(g_languageManager->GetString(str_number, str));
	uni_snprintf_ss(temp_buffer, buffer_size, str.CStr(), arg1, arg2);			

	RETURN_IF_ERROR(comment_string->Set(temp_buffer));	
	RETURN_IF_ERROR(m_comments.Add(comment_string.get()));
	comment_string.release();	

	return OpStatus::OK;
}

OP_STATUS External_SSL_CommCertificateDisplayContext::CreateComments(int errors)
{
	
	const ServerName *server = m_comm_object->GetRealServerName();
	const uni_char *server_name;
	OpSSLListener::SSLCertificate *certificate;
	uni_char *comment_temp;
	
	if (!server || !(server_name = server->UniName()) || !(certificate = m_display_certificates.Get(0)) || !(comment_temp = (uni_char *) g_memory_manager->GetTempBufUni()))
		return OpStatus::ERR;

	unsigned int comment_temp_len = g_memory_manager->GetTempBufUniLenUniChar();
	
	if (errors & OpSocket::CERT_NO_CHAIN)
	{
		// FIXME: not sure this is the correct comment
		RETURN_IF_ERROR(AddComment(Str::S_CERT_UNKNOWN_CA, comment_temp, comment_temp_len, certificate->GetFullName(), certificate->GetIssuer()));
	}

	if (errors & OpSocket::CERT_INCOMPLETE_CHAIN)
	{
		// FIXME: not sure this is the correct comment		
		RETURN_IF_ERROR(AddComment(Str::S_CERT_UNKNOWN_CA, comment_temp, comment_temp_len, certificate->GetFullName(), certificate->GetIssuer()));
	}

	if (errors & OpSocket::CERT_DIFFERENT_CERTIFICATE)
	{
		RETURN_IF_ERROR(AddComment(Str::S_CERT_MISMATCH_SERVERNAME, comment_temp, comment_temp_len, server_name, certificate->GetFullName()));
	}

	if (errors & OpSocket::CERT_EXPIRED)
	{
		RETURN_IF_ERROR(AddComment(Str::S_CERTIFICATE_EXPIRED, comment_temp, comment_temp_len, certificate->GetFullName(), certificate->GetValidTo()));
	}
	
	return OpStatus::OK;
}


External_SSL_CommCertificateDisplayContext::External_SSL_CommCertificateDisplayContext(External_SSL_DisplayContextManager *display_contexts_manager, External_SSL_Comm *comm_object)
	: External_SSL_CertificateDisplayContext(display_contexts_manager)
	, m_comm_object(comm_object)
{		
}


External_SSL_CommCertificateDisplayContext::~External_SSL_CommCertificateDisplayContext()
{
	if (m_comm_object)
		m_comm_object->DisplayContextDeleted();
}


External_SSL_CommCertificateDisplayContext *External_SSL_DisplayContextManager::OpenCertificateDialog(OP_STATUS &status, External_SSL_Comm *comm_object, WindowCommander* wic, int errors, OpSSLListener::SSLCertificateOption options)
{
	status = OpStatus::OK;
	unsigned int i = 0;
	// Remove old certificate displays;
	while (i < m_display_contexts.GetCount())
	{
		if (m_display_contexts.Get(i)->GetCertificateBrowsingDone())
		{
			External_SSL_CertificateDisplayContext *ctx = m_display_contexts.Remove(i);
			OP_DELETE(ctx);
		}
		else
		{
			i++;
		}
	}
	External_SSL_CommCertificateDisplayContext *display_context = NULL;
	if (OpStatus::IsError(status = External_SSL_CommCertificateDisplayContext::Make(display_context, errors, this, comm_object)))
	{
		OP_DELETE(display_context);
		return NULL;
	}
	
	OpSSLListener::SSLCertificateReason reason;

	if (errors & OpSocket::CERT_NO_CHAIN)
		reason = OpSSLListener::SSL_REASON_NO_CHAIN;
	
	else if (errors & OpSocket::CERT_INCOMPLETE_CHAIN)
		reason = OpSSLListener::SSL_REASON_INCOMPLETE_CHAIN;
	
	else if (errors & OpSocket::CERT_DIFFERENT_CERTIFICATE)
		reason = OpSSLListener::SSL_REASON_DIFFERENT_CERTIFICATE;
	
	else if (errors & OpSocket::CERT_EXPIRED)
		reason = OpSSLListener::SSL_REASON_CERT_EXPIRED;
	
	else
	{
		reason = OpSSLListener::SSL_REASON_UNKNOWN;
		OP_ASSERT(!"unknown certificate error reason");
	}
	
	OpSSLListener *ssl_listener = wic->GetSSLListener() ? wic->GetSSLListener() : g_windowCommanderManager->GetSSLListener();
	
	ssl_listener->OnCertificateBrowsingNeeded(wic, display_context, reason, options);
	
	if (
			display_context->GetCertificateBrowsingDone() ||
			OpStatus::IsError(status = m_display_contexts.Add(display_context))		
	)
	{
		OP_DELETE(display_context);
		return NULL;
	}
	
	return display_context;
}


External_SSL_DisplayContextManager::~External_SSL_DisplayContextManager()
{		
}

void External_SSL_DisplayContextManager::DeleteDisplayContext(External_SSL_CertificateDisplayContext *display_context)
{
	if (display_context && display_context->GetCertificateBrowsingDone())
		OpStatus::Ignore(m_display_contexts.Delete(display_context));
}

void External_SSL_DisplayContextManager::RemoveDisplayContext(External_SSL_CertificateDisplayContext *display_context)
{
	OpStatus::Ignore(m_display_contexts.RemoveByItem(display_context));
}

#ifdef EXTSSL_CERT_MANAGER

/* static */
OP_STATUS External_SSL_DisplayContextManager::BrowseCertificates(
	External_SSL_CertificateDisplayContext*& ctx,
	OpCertificateManager::CertificateType type)
{
	External_SSL_StorageCertificateDisplayContext* temp_ctx = NULL;
	
	OP_STATUS status;
	if (OpStatus::IsSuccess( status = External_SSL_StorageCertificateDisplayContext::Make(temp_ctx, type)))
	{
		if (OpStatus::IsSuccess( status = g_ssl_display_context_manager->m_display_contexts.Add(temp_ctx)))
		{
			ctx = temp_ctx;
			return OpStatus::OK;
		}
	}
	
	// error
	OP_DELETE(temp_ctx);
	return status;
}

External_SSL_StorageCertificateDisplayContext::External_SSL_StorageCertificateDisplayContext(
	External_SSL_DisplayContextManager* display_contexts_manager,
	OpCertificateManager::CertificateType type
)
	: External_SSL_CertificateDisplayContext(display_contexts_manager)
	, m_type(type)
	, m_cert_manager(NULL)
{
}

External_SSL_StorageCertificateDisplayContext::~External_SSL_StorageCertificateDisplayContext()
{
	if (m_cert_manager)
		m_cert_manager->Release();
}

OP_STATUS External_SSL_StorageCertificateDisplayContext::Init()
{
	OP_STATUS s = OpCertificateManager::Get(&m_cert_manager, m_type);
	RETURN_IF_ERROR(s);

	for(unsigned int c = 0; c < m_cert_manager->Count(); c++)
	{
		OpCertificate* cert = m_cert_manager->GetCertificate(c);
		OpSSLListener::SSLCertificate* display_cert = NULL;

		do
		{
			if (OpStatus::IsError(s = External_SSL_display_certificate::Make(display_cert, cert)))
				break;

			// init settings
			OpCertificateManager::Settings opsettings;
			if (OpStatus::IsError(s = m_cert_manager->GetSettings(c, opsettings)))
				break;			
			int settings = OpSSLListener::SSL_CERT_SETTING_NONE;
			if (opsettings.warn)	settings |= OpSSLListener::SSL_CERT_SETTING_WARN_WHEN_USE;
			if (opsettings.allow)	settings |= OpSSLListener::SSL_CERT_SETTING_ALLOW_CONNECTIONS;
			display_cert->UpdateSettings((OpSSLListener::SSLCertificateSetting)settings);

			if (OpStatus::IsError(s = m_display_certificates.Add(display_cert)))
				break;

		}
		while(0);

		if (OpStatus::IsError(s))
		{
			m_display_certificates.DeleteAll();
			OP_DELETE(display_cert);
			return s;
		}

	}
	return OpStatus::OK;
}

OP_STATUS External_SSL_StorageCertificateDisplayContext::Make(
	External_SSL_StorageCertificateDisplayContext*& display_context,
	OpCertificateManager::CertificateType type)
{
	display_context = NULL;
	OpAutoPtr<External_SSL_StorageCertificateDisplayContext> temp_display_context(OP_NEW(External_SSL_StorageCertificateDisplayContext, (g_ssl_display_context_manager, type)));
	if (!temp_display_context.get())
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = temp_display_context->Init();
	RETURN_IF_ERROR(status);

	display_context = temp_display_context.release();
	return OpStatus::OK;
}


void External_SSL_StorageCertificateDisplayContext::OnCertificateBrowsingDone(BOOL ok, OpSSLListener::SSLCertificateOption options)
{
	m_certificate_browsing_done = TRUE;
	if (options & OpSSLListener::SSL_CERT_OPTION_OK)
	{
		for(unsigned int i=0; i< m_cert_manager->Count(); i++)
		{
			OpSSLListener::SSLCertificateSetting s = GetCertificate(i)->GetSettings();
			OpCertificateManager::Settings newSettings;
			newSettings.allow	= (s & OpSSLListener::SSL_CERT_SETTING_ALLOW_CONNECTIONS);
			newSettings.warn	= (s & OpSSLListener::SSL_CERT_SETTING_WARN_WHEN_USE);

			m_cert_manager->UpdateSettings(i, newSettings);
		}
	}
	m_display_contexts_manager->DeleteDisplayContext(this);
}

#endif //EXTSSL_CERT_MANAGER

#endif //_EXTERNAL_SSL_SUPPORT_
