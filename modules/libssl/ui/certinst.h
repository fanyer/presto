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


#ifndef SSL_CERTINST_INT_H
#define SSL_CERTINST_INT_H

#if defined _NATIVE_SSL_SUPPORT_ && defined USE_SSL_CERTINSTALLER

#include "modules/libssl/ui/sslcctx.h"
#include "modules/libssl/certs/certinst_base.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/keyex/certverify.h"

class SSLSecurtityPasswordCallbackImpl;


class SSL_Interactive_Certificate_Installer : public SSL_Certificate_Installer, public SSL_CertificateVerifier
{
private:
	SSL_Certificate_DisplayContext *context;
	SSLSecurtityPasswordCallbackImpl *ask_password;
	OpWindow *window;
	MessageHandler *message_handler;
	OpMessage finished_message;
	MH_PARAM_1 finished_id;

	URL source_url;

	BOOL finished_advanced_install;
	BOOL advanced_install_success;

	BOOL has_incomplete_chain;

	enum install_state {
		Install_Not_started,
		Install_Prepare_cert,
		Install_Prepare_cert_password,
		Install_Prepare_verifycert,
		Install_Asking_user,
		Install_Waiting_for_user,
		Install_Installing_certificate,
		Install_Installing_password,
		Install_Finished
	} installation_state;

public: 

	SSL_Interactive_Certificate_Installer();
	virtual ~SSL_Interactive_Certificate_Installer();

	virtual OP_STATUS StartInstallation();
	virtual BOOL Finished();
	virtual BOOL InstallSuccess();


	OP_STATUS Construct(URL &source, const SSL_Certificate_Installer_flags &install_flags, SSL_dialog_config &config, SSL_Options *optManager=NULL);
	OP_STATUS Construct(SSL_varvector32 &source, const SSL_Certificate_Installer_flags &install_flags, SSL_dialog_config &config, SSL_Options *optManager=NULL);

	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:

	OP_STATUS ProgressInstall();

	void FinishedInstall(BOOL status);
	OP_STATUS VerifyFailedStep(SSL_Alert &msg);
	OP_STATUS VerifySucceededStep();

protected:
	virtual OP_STATUS VerifyCertificate();
	virtual void VerifyFailed(SSL_Alert &msg);
	virtual void VerifySucceeded(SSL_Alert &msg);


};

#endif // USE_SSL_CERTINSTALLER

#endif
