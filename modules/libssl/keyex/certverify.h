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

#ifndef _SSL_CERTVERIFY_H_
#define _SSL_CERTVERIFY_H_

//#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/handshake/cert_message.h"
//#include "modules/libssl/options/certitem.h"
#include "modules/libssl/certs/crlitem.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/ui/sslcctx.h"

#include "modules/url/url2.h"
#include "modules/updaters/update_man.h"
#include "modules/url/url_docload.h"
#include "modules/network/op_batch_request.h"
#include "modules/url/url_sn.h"

struct SSL_CertificateVerification_Info;
class SSL_Auto_Root_Retriever;
class SSL_Auto_Untrusted_Retriever;
class SSL_CertificateHandler;

class SSL_CertificateVerifier : public MessageObject, public SSL_Error_Status, public OpBatchRequestListener
{
public:

	virtual void OnBatchResponsesFinished();
	virtual void OnRequestRedirected(OpRequest *req, OpResponse *res, OpURL from, OpURL to);
	virtual void OnRequestFailed(OpRequest *req, OpResponse *res, Str::LocaleString error);
	virtual void OnAuthenticationRequired(OpRequest *req, OpAuthenticationCallback* callback) { OP_ASSERT(0); }
	virtual void OnCertificateBrowsingNeeded(OpRequest *req, OpSSLListener::SSLCertificateContext* context, OpSSLListener::SSLCertificateReason reason, OpSSLListener::SSLCertificateOption options) { OP_ASSERT(0); }
	virtual void OnResponseFinished(OpRequest *req, OpResponse *res) {}

	enum VerifyStatus{
		Verify_Failed,
		Verify_Started,
		Verify_Completed,
		// Internal status
		Verify_TaskCompleted
	};
private:
	enum VerifyProgress{
		Verify_NotStarted,
		Verify_Init,
		Verify_CheckingCertificate,
		Verify_UpdateIntermediates,
		Verify_VerifyCertificate,
		Verify_CheckingUntrusted,
		Verify_CheckingKeysize,
		Verify_ExtractNames,
		Verify_ExtractVerifyStatus,
		Verify_CheckingMissingCertificate,
		Verify_CheckingWarnings,
		Verify_HandleDownloadedIntermediates,
		Verify_LoadingCRLOrOCSP,
		Verify_LoadingAIACert,
		Verify_LoadingRepositoryCert,
		Verify_LoadingUntrustedCert,
		Verify_WaitForRepository,
		Verify_LoadingCRLCompleted,
		Verify_LoadingAIACertCompleted,
		Verify_LoadingRepositoryCertCompleted,
		Verify_LoadingUntrustedCertCompleted,
		Verify_WaitForRepositoryCompleted,
		Verify_CheckHostName,
		Verify_CheckTrustedCertForHost,
		Verify_CheckAskUser,
		Verify_AskingUser,
		Verify_AskingUserCompleted,
		Verify_AskingUserServerKeyCompleted,
		Verify_FinishedSuccessfully,
		Verify_FinishedFailed,
		Verify_LoadingExternalRepositoryCert,
		Verify_LoadingExternalRepositoryCertCompleted,
		Verify_Aborted
	};
protected:
	uint24 warncount;
	SSL_CertificateVerification_Info *warnstatus;
	int warn_mask;

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	CRL_List crl_list;
	AutoDeleteHead  loading_crls;
	BOOL loaded_crls;
#endif // LIBSSL_ENABLE_CRL_SUPPORT

	enum SSL_KeyExLoadingMode{
		Loading_None,
		Loading_Intermediate,
		Loading_CRL_Or_OCSP
	} url_loading_mode;

	URL *pending_url;
	URL_InUse pending_url_ref;

	BOOL block_ica_requests; // ica == intermediate CA

	AutoDeleteHead intermediate_list;
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	BOOL block_retrieve_requests;
	SSL_varvector24 pending_issuer_id;
	AutoFetch_Manager auto_fetcher;
	BOOL ignore_repository;
#endif

	BOOL accept_unknown_ca;

private: 
	VerifyProgress	verify_state;
	BOOL doing_iteration;

protected:
	ServerName_Pointer servername;
	uint16 port;
	SSL_Port_Sessions_Pointer server_info;

private:
	SSL_CertificatePurpose certificate_purpose;
	SSL_ConfirmedMode_enum accept_search_mode;
	int dialog_type;
	SSL_CertificateHandler *cert_handler;

	OpBatchRequest *batch_request;

    SSL_ASN1Cert_list Certificate;
	SSL_ASN1Cert_list Validated_Certificate;

#ifndef TLS_NO_CERTSTATUS_EXTENSION
	BOOL ocsp_extensions_sent;
	BOOL certificate_checked_for_ocsp;
	BOOL certificate_loading_ocsp;
	SSL_varvector32 sent_ocsp_extensions;
	SSL_varvector32 received_ocsp_response;
#endif
	OpString	ocsp_fail_reason;

	BOOL		check_name;
	OpString	Matched_name;
	OpString_list CertificateNames;

	int certificate_status;
	SSL_ConfirmedMode_enum UserConfirmed;
	int security_rating;
	int low_security_reason;
	SSL_keysizes lowkeys;

	OpAutoPtr<SSL_CertificateHandler> cert_handler_validated;
	uint24 val_certificate_count;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	BOOL check_extended_validation;
	BOOL extended_validation;
	BOOL disable_EV;
#endif
	SSL_Alert verify_msg;
    SSL_CipherDescriptions_Pointer cipherdescription;
	SSL_ProtocolVersion used_version;

protected:

	BOOL		show_dialog;
	URL			DisplayURL;
	OpWindow	*window;
	OpAutoPtr<SSL_Certificate_DisplayContext> certificate_ctx;
	OpString8	certificate_name_list;


private: 
	void InternalDestruct();

private:
	VerifyStatus IterateVerifyCertificate(SSL_Alert *msg);
	VerifyStatus VerifyCertificate(SSL_Alert *msg);
	void ProgressVerifySiteCertificate();

	VerifyStatus VerifyCertificate_Init(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckCert(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_AddIntermediates(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckUntrusted(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckKeySize(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_ExtractNames(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_ExtractVerificationStatus(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckMissingCerts(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckWarnings(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_HandleDownloadedIntermediates(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckName(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckTrusted(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckInteractionNeeded(SSL_Alert *msg);
	VerifyStatus VerifyCertificate_CheckInteractionCompleted(SSL_Alert *msg);


protected:

	SSL_CertificateHandler *ExtractCertificateHandler(){SSL_CertificateHandler *temp = cert_handler; cert_handler = NULL; return temp;}

	virtual void VerifyFailed(SSL_Alert &msg) = 0;
	virtual void VerifySucceeded(SSL_Alert &msg) = 0;

	virtual int VerifyCheckExtraWarnings(int &low_security_reason){return 0;}

public:

	void SetCertificate(SSL_ASN1Cert_list &cert){Certificate = cert;}
	SSL_ASN1Cert_list &GetValidatedCertificate(){return Validated_Certificate;}

	OP_STATUS SetHostName(ServerName *servername, uint32 port);
	void SetHostName(SSL_Port_Sessions *ptr);

	const OpStringC	&GetMatched_name() const{return Matched_name;};
	OP_STATUS GetCertificateNames(OpString_list &target);

	int GetCertificateStatus() const{return certificate_status;}
	SSL_ConfirmedMode_enum GetConfirmedMode() const{return UserConfirmed;}
	int GetSecurityRating()const{return security_rating;}
	int GetLowSecurityReason()const{return low_security_reason;}
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	BOOL GetExtendedValidation()const{return extended_validation;}
	void SetCheckExtendedValidation(BOOL flag){check_extended_validation = flag;}
#endif
	const SSL_keysizes &GetLowKeySizes() const{return lowkeys;}
#ifndef TLS_NO_CERTSTATUS_EXTENSION
	void SetSentOCSPExtensions(BOOL flag, SSL_varvector32 &sent_ext, SSL_varvector32 &recv_ext){
				ocsp_extensions_sent=flag;
				sent_ocsp_extensions = sent_ext;
				received_ocsp_response = recv_ext;
			}
#endif
	//Takes over
	void GetOCSPFailreason(OpString &str){str.TakeOver(ocsp_fail_reason);}

public:
    VerifyStatus PerformVerifySiteCertificate(SSL_Alert *);
    const SSL_CertificateVerification_Info *WarnList() const;

	SSL_CertificateVerifier(int dialog=0);
	virtual ~SSL_CertificateVerifier();

	VerifyStatus CheckOCSP();
	VerifyStatus ValidateOCSP(BOOL full_check_if_fail);

	class SSL_DownloadedIntermediateCert : public Link
	{
	public:
		URL download_url;
		SSL_ASN1Cert certificate;
	};

	void ProcessIntermediateCACert();

	URL GetPending_URL(MH_PARAM_1);
	BOOL StartLoadingURL(MessageObject *target, URL &url, BOOL allow_redirects=FALSE);
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	BOOL StartLoadingCrls(MessageObject *target);
#endif // LIBSSL_ENABLE_CRL_SUPPORT
	BOOL StartPendingURL(MessageObject *target);
	VerifyStatus ProcessFinishedLoad(MessageObject *target);
	
	/* Will stop loading all ongoing urls, and mark them as failed.
	 * 
	 * The failed urls are stored in SSL_Options::AutoRetrieved_urls and 
	 * have a timeout of 24 hours before they are checked again.
	 */
	void CancelPendingLoad();

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	OP_STATUS StartRootRetrieval(MH_PARAM_1 &fin_id);
	OP_STATUS StartUntrustedRetrieval(MH_PARAM_1 &fin_id);

	void SetIgnoreRepository(){ignore_repository = TRUE;};
#endif

	void SetServerName(ServerName *sn){servername = sn;}

	void SetShowDialog(BOOL flag){show_dialog = flag;}
	void SetDisplayURL(URL &url){DisplayURL = url;}
	void SetDisplayWindow(OpWindow *win){window = win;}
	void SetCheckServerName(BOOL flag){check_name = flag;}
	void SetCipherDescription(SSL_CipherDescriptions *ptr){cipherdescription = ptr;}
	void SetProtocolVersion(SSL_ProtocolVersion &ver){used_version = ver;};
	void SetAcceptUnknownCA(BOOL flag){accept_unknown_ca = flag;}

	void SetCertificatePurpose(SSL_CertificatePurpose purp){certificate_purpose = purp;};
	void SetAcceptSearchMode(SSL_ConfirmedMode_enum mode){accept_search_mode = mode;}
};

#endif // _SSL_CERTVERIFY_H_
