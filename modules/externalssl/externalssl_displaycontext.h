/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef EXTERNALSSL_DISPLAYCONTEXT_H_
#define EXTERNALSSL_DISPLAYCONTEXT_H_

#ifdef _EXTERNAL_SSL_SUPPORT_

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/externalssl/externalssl_com.h"
#include "modules/util/adt/opvector.h"
#include "modules/pi/network/OpCertificateManager.h"

class External_SSL_DisplayContextManager;
class OpCertificate;
class WindowCommander;


class External_SSL_display_certificate : public OpSSLListener::SSLCertificate
{
public:
	virtual ~External_SSL_display_certificate(){};
	
	static OP_STATUS Make(OpSSLListener::SSLCertificate *&cert_out, OpCertificate *cert);
	
	virtual const uni_char* GetShortName() const { return m_short_name.CStr(); }
	virtual const uni_char* GetFullName()  const { return m_full_name.CStr(); }
	virtual const uni_char* GetIssuer()    const { return m_issuer.CStr(); }
	virtual const uni_char* GetValidFrom() const { return m_valid_from.CStr(); }
	virtual const uni_char* GetValidTo()   const { return m_valid_to.CStr(); }
	virtual const uni_char* GetInfo()      const { return m_info.CStr(); }
	virtual BOOL IsInstalled()             const { return m_is_installed; }
	
	virtual int  GetSettings() const;
	virtual void UpdateSettings(int settings);

private:
	// Implementation of OpCertificate::GetCertificateHash() (stub).
	virtual const char* GetCertificateHash(unsigned int &length) const
		{ OP_ASSERT(!"This function is never called."); length = 0; return NULL; }

private:
	
	External_SSL_display_certificate();
	
	OpString m_short_name;
	OpString m_full_name;
	OpString m_issuer;
	
	OpString m_valid_from;
	OpString m_valid_to;
	OpString m_info;
	
	int m_settings;
	BOOL m_is_installed;
	
};


/**
 * Base class for all external SSL display contexts
 */
class External_SSL_CertificateDisplayContext : public OpSSLListener::SSLCertificateContext
{
public:
	virtual unsigned int GetNumberOfCertificates() const { return m_display_certificates.GetCount(); }

	virtual OpSSLListener::SSLCertificate* GetCertificate(unsigned int index);

	virtual unsigned int GetNumberOfComments() const { return m_comments.GetCount(); }

	virtual const uni_char* GetCertificateComment(unsigned int index) const;

	virtual OP_STATUS GetServerName(OpString* server_name) const;
	
	BOOL GetCertificateBrowsingDone(){ return m_certificate_browsing_done;}

	virtual ~External_SSL_CertificateDisplayContext();

protected:
	External_SSL_CertificateDisplayContext(External_SSL_DisplayContextManager *display_contexts_manager);
	
	OP_STATUS AddComment(Str::LocaleString str_number, uni_char *temp_buffer, unsigned int buffer_size, const uni_char *arg1, const uni_char *arg2);
	
	External_SSL_DisplayContextManager *m_display_contexts_manager;

	OpAutoVector<OpSSLListener::SSLCertificate> m_display_certificates;

	BOOL m_certificate_browsing_done;
	
	OpAutoVector<OpString> m_comments;
	
friend class External_SSL_DisplayContextManager;
};

/**
 * Display context used to display certificates in the SSL chain. 
 */
class External_SSL_CommCertificateDisplayContext : public External_SSL_CertificateDisplayContext
{
public:
	static OP_STATUS Make(External_SSL_CommCertificateDisplayContext *&display_context, int errors, External_SSL_DisplayContextManager *display_contexts_manager, External_SSL_Comm *comm_object);

	virtual void OnCertificateBrowsingDone(BOOL ok, OpSSLListener::SSLCertificateOption options);

	void CommDeleted(){ m_comm_object = NULL; }	

	virtual ~External_SSL_CommCertificateDisplayContext();

	virtual OP_STATUS GetServerName(OpString* server_name) const;

private:
	External_SSL_CommCertificateDisplayContext(External_SSL_DisplayContextManager *display_contexts_manager, External_SSL_Comm *comm_object);

	OP_STATUS CreateComments(int errors);

	External_SSL_Comm *m_comm_object;
};


#ifdef EXTSSL_CERT_MANAGER

/**
 * Context used to display certificates in the device storage, like root or personal 
 * certificates
 */
class External_SSL_StorageCertificateDisplayContext : public External_SSL_CertificateDisplayContext
{
public:
	static OP_STATUS Make(External_SSL_StorageCertificateDisplayContext*& display_context, OpCertificateManager::CertificateType type);

	virtual void OnCertificateBrowsingDone(BOOL ok, OpSSLListener::SSLCertificateOption options);

	virtual ~External_SSL_StorageCertificateDisplayContext();

private:
	External_SSL_StorageCertificateDisplayContext(External_SSL_DisplayContextManager* display_contexts_manager, OpCertificateManager::CertificateType type);

	OP_STATUS Init();

	OpCertificateManager::CertificateType m_type;
	OpCertificateManager* m_cert_manager;
};

#endif // EXTSSL_CERT_MANAGER


class External_SSL_DisplayContextManager 
{
public:
	External_SSL_DisplayContextManager() {};

	~External_SSL_DisplayContextManager();

	External_SSL_CommCertificateDisplayContext *OpenCertificateDialog(OP_STATUS &status, External_SSL_Comm *comm_object, WindowCommander* wic, int errors, OpSSLListener::SSLCertificateOption options);

#ifdef EXTSSL_CERT_MANAGER
	static OP_STATUS BrowseCertificates(External_SSL_CertificateDisplayContext*& ctx, OpCertificateManager::CertificateType type);
#endif //EXTSSL_CERT_MANAGER

	void DeleteDisplayContext(External_SSL_CertificateDisplayContext *comm_object);

private:	
	void RemoveDisplayContext(External_SSL_CertificateDisplayContext *display_context);
	
	OpAutoVector<External_SSL_CertificateDisplayContext> m_display_contexts;

friend class External_SSL_CertificateDisplayContext;	
};
#endif // _EXTERNAL_SSL_SUPPORT_
#endif // !EXTERNALSSL_DISPLAYCONTEXT_H_
