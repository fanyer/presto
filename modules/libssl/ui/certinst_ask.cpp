/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#ifdef USE_SSL_CERTINSTALLER

#include "modules/libssl/sslbase.h"
#include "modules/libssl/sslrand.h"

#include "modules/libssl/certs/certinstaller.h"
#include "modules/libssl/ui/certinst.h"

#include "modules/dochand/win.h"
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"


SSL_Interactive_Certificate_Installer::SSL_Interactive_Certificate_Installer()
: context(NULL),
  ask_password(NULL),
  message_handler(NULL),
  finished_message(MSG_NO_MESSAGE),
  finished_id(0),
  finished_advanced_install(FALSE),
  advanced_install_success(FALSE),
  has_incomplete_chain(FALSE),
  installation_state(Install_Not_started)
{
}

SSL_Interactive_Certificate_Installer::~SSL_Interactive_Certificate_Installer()
{
	// TODO cancel dialog or do it in the context
	OP_DELETE(context);
	context = NULL;
	OP_DELETE(ask_password);
	ask_password = NULL;
}

OP_STATUS SSL_Interactive_Certificate_Installer::StartInstallation()
{
	if(installation_state == Install_Not_started)
		installation_state = Install_Prepare_cert;

	return ProgressInstall();
}

BOOL SSL_Interactive_Certificate_Installer::Finished()
{
	return finished_advanced_install || SSL_Certificate_Installer::Finished();
}

BOOL SSL_Interactive_Certificate_Installer::InstallSuccess()
{
	return advanced_install_success || SSL_Certificate_Installer::InstallSuccess();
}

OP_STATUS SSL_Interactive_Certificate_Installer::Construct(URL &source, const SSL_Certificate_Installer_flags &install_flags,
														   SSL_dialog_config &config, SSL_Options *optManager)
{
	window = config.window;
	message_handler = config.msg_handler;
	finished_message = config.finished_message;
	finished_id = config.finished_id;

	source_url = source;

	return SSL_Certificate_Installer::Construct(source, install_flags, optManager);
}

OP_STATUS SSL_Interactive_Certificate_Installer::Construct(SSL_varvector32 &source, const SSL_Certificate_Installer_flags &install_flags,
														    SSL_dialog_config &config, SSL_Options *optManager)
{
	window = config.window;
	message_handler = config.msg_handler;
	finished_message = config.finished_message;
	finished_id = config.finished_id;

	return SSL_Certificate_Installer::Construct(source, install_flags, optManager);
}

void SSL_Interactive_Certificate_Installer::FinishedInstall(BOOL status)
{
	finished_advanced_install = TRUE;
	advanced_install_success = status;
	InstallationLastStep();
	installation_state = Install_Finished;
	message_handler->PostMessage(finished_message, finished_id, status);
}

OP_STATUS SSL_Interactive_Certificate_Installer::ProgressInstall()
{
	while(installation_state != Install_Finished)
	{
		switch(installation_state)
		{
		case Install_Not_started:
			installation_state = Install_Prepare_cert;
			break;
		case Install_Prepare_cert:
			{
				OP_STATUS op_err;
				op_err = PrepareInstallation();

				if(op_err == InstallerStatus::ERR_PASSWORD_NEEDED)
				{
					installation_state = Install_Prepare_cert_password;
				}
				else if(op_err == InstallerStatus::VERIFYING_CERT)
				{
					installation_state = Install_Prepare_verifycert;
					return InstallerStatus::VERIFYING_CERT;
				}
				else if(OpStatus::IsError(op_err))
				{
					FinishedInstall(FALSE);
					return op_err;
				}
				else
				{
					installation_state = Install_Asking_user;
				}
				break;
			}
		case Install_Installing_certificate:
			{
				OP_STATUS op_err;
				URL empty_url;
				SSL_dialog_config config(message_handler->GetWindow() ? (OpWindow *) message_handler->GetWindow()->GetOpWindow() : (OpWindow *) NULL, g_main_message_handler, MSG_FINISHED_OPTIONS_PASSWORD, (MH_PARAM_1) this, empty_url);

				op_err = PerformInstallation(config);

				if(op_err == InstallerStatus::ERR_PASSWORD_NEEDED)
				{
					op_err = g_main_message_handler->SetCallBack(this, MSG_FINISHED_OPTIONS_PASSWORD, (MH_PARAM_1) this);
					if(OpStatus::IsError(op_err))
					{
						FinishedInstall(FALSE);
						return op_err;
					}
					installation_state = Install_Installing_password;
				}
				else if(OpStatus::IsError(op_err))
				{
					FinishedInstall(FALSE);
					return op_err;
				}
				else
				{
					FinishedInstall(TRUE);
				}
				break;
			}
		case Install_Asking_user:
			{
				WORD action = 0;

				switch(store)
				{
				case SSL_CA_Store:
				case SSL_IntermediateCAStore:
					action = IDM_INSTALL_CA_CERTIFICATE;
					break;
				case SSL_ClientStore:
					action = (private_key.GetLength() > 0 ? IDM_SSL_IMPORT_KEY_AND_CERTIFICATE : IDM_INSTALL_PERSONAL_CERTIFICATE);
					break;
				default:
					FinishedInstall(FALSE);
					return OpStatus::ERR;
				}

				if(!action)
					break;

				context = OP_NEW(SSL_Certificate_DisplayContext, (action));
				if(!context)
				{
					FinishedInstall(FALSE);
					return OpStatus::ERR_NO_MEMORY;
				}

				context->SetExternalOptionsManager(optionsManager);
				context->SetWindow(message_handler->GetWindow() ? (OpWindow *) message_handler->GetWindow()->GetOpWindow() : (OpWindow *) NULL);

				SSL_CertificateHandler *cert_copy = certificate->Fork();
				if(cert_copy == NULL)
				{
					FinishedInstall(FALSE);
					return OpStatus::ERR;
				}

				context->SetCertificateChain(cert_copy);
				context->SetURL(source_url);

				URLType url_type = (URLType) source_url.GetAttribute(URL::KType);
				if(url_type == URL_HTTP || url_type == URL_FTP)	
				{
					OpString temp_name;
					OpStatus::Ignore(source_url.GetAttribute(URL::KUniName, 0, temp_name));
					context->AddCertificateComment(Str::S_CERT_CERTIFICATE_DOWNLOADED_UNSECURELY, temp_name, NULL);
				}

				if(has_incomplete_chain)
					context->AddCertificateComment(Str::SI_MSG_SECURE_NO_CHAIN_CERTIFICATE_CERT, NULL, NULL);

				context->SetCompleteMessage(MSG_CERT_INSTALL_ASK_USER, (MH_PARAM_1) this);
				if(OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_CERT_INSTALL_ASK_USER, (MH_PARAM_1) this)) ||
					!InitSecurityCertBrowsing(context->GetWindow(), context))
				{
					FinishedInstall(FALSE);
					return OpStatus::ERR;
				}
				installation_state = Install_Waiting_for_user;
				return OpStatus::OK;
			}
		case Install_Prepare_cert_password:
			{
				URL empty_url;
				SSL_dialog_config config((message_handler->GetWindow() ? (OpWindow *) message_handler->GetWindow()->GetOpWindow() : (OpWindow *) NULL),
									g_main_message_handler, MSG_FINISHED_PASSWORD, (MH_PARAM_1) this, empty_url);
				ask_password = OP_NEW(SSLSecurtityPasswordCallbackImpl, (
					OpSSLListener::SSLSecurityPasswordCallback::JustPassword,
					OpSSLListener::SSLSecurityPasswordCallback::CertificateImport,
					Str::DI_IDM_PASSWORD_BOX,
					Str::SI_MSG_SSL_IMPORT_PASSWORD,
					config
				));
				if(!ask_password || OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_FINISHED_PASSWORD, (MH_PARAM_1) this)))
				{
					FinishedInstall(FALSE);
					return OpStatus::ERR_NO_MEMORY;
				}
				OP_STATUS op_err = ask_password->StartDialog();
				if(OpStatus::IsError(op_err))
				{
					FinishedInstall(FALSE);
					return op_err;
				}

				installation_state = Install_Waiting_for_user;
				return OpStatus::OK;
			}
		case Install_Installing_password:
			installation_state = Install_Waiting_for_user;
			return OpStatus::OK;
		}
	}

	return OpStatus::OK;
}

OP_STATUS SSL_Interactive_Certificate_Installer::VerifyCertificate()
{
	SetCertificate(original_cert);
	SetCheckServerName(FALSE);
	SetShowDialog(FALSE);
	SetAcceptUnknownCA(TRUE);
	SetCertificatePurpose(store == SSL_ClientStore ? SSL_Purpose_Client_Certificate: SSL_Purpose_Any);
	if(Error())
		return GetOPStatus();

	SSL_Alert msg;
	SSL_CertificateVerifier::VerifyStatus verify_status = PerformVerifySiteCertificate(&msg);

	if(Error())
		return GetOPStatus();

	if(verify_status == SSL_CertificateVerifier::Verify_Failed)
	{
		VerifyFailedStep(msg);
		return InstallerStatus::ERR_INSTALL_FAILED;
	}

	if(verify_status == SSL_CertificateVerifier::Verify_Started)
		return InstallerStatus::VERIFYING_CERT;

	return VerifySucceededStep();
}

OP_STATUS SSL_Interactive_Certificate_Installer::VerifyFailedStep(SSL_Alert &msg)
{
	switch(msg.GetDescription())
	{
	case SSL_Unknown_CA:

	case SSL_Certificate_Expired:
		AddErrorString(Str::SI_MSG_HTTP_SSL_Certificate_Expired, NULL);
		return  InstallerStatus::ERR_PARSING_FAILED;
	default  :
		AddErrorString(Str::SI_MSG_SECURE_INSTALL_FAILED, NULL);
		return  InstallerStatus::ERR_PARSING_FAILED;
	}
}


void SSL_Interactive_Certificate_Installer::VerifyFailed(SSL_Alert &msg)
{
	VerifyFailedStep(msg);
	FinishedInstall(FALSE);
}

OP_STATUS SSL_Interactive_Certificate_Installer::VerifySucceededStep()
{
	// Check that the certificate chain is correctly ordered so that the user does not have to
	// parse the chain, and that there is only one chain, not multiple chains.
	SSL_ASN1Cert_list val_certs;
	val_certs = GetValidatedCertificate();
	if(val_certs.Error())
	{
		return val_certs.GetOPStatus();
	}

	certificate = ExtractCertificateHandler();
	uint24 count = certificate->CertificateCount();
	if(count >val_certs.Count())
	{
		AddErrorString(Str::SI_MSG_SECURE_INVALID_CHAIN, NULL);
		return  InstallerStatus::ERR_PARSING_FAILED;
	}
	OP_ASSERT(val_certs.Count() > 0);

	for(uint24 i = 0; i< count; i++)
	{
		if(val_certs[i] != original_cert[i])
		{
			AddErrorString(Str::SI_MSG_SECURE_INVALID_CHAIN, NULL);
			return  InstallerStatus::ERR_PARSING_FAILED;
		}
	}

	SSL_CertificateHandler *val_certificate = g_ssl_api->CreateCertificateHandler();
	if(val_certificate == NULL)
		return OpStatus::ERR_NO_MEMORY;

	val_certificate->LoadCertificate(val_certs);

	if(!val_certificate->SelfSigned(count-1))
	{
		has_incomplete_chain = TRUE;
	}

	OP_DELETE(val_certificate);

	return InstallerStatus::OK;
}

void SSL_Interactive_Certificate_Installer::VerifySucceeded(SSL_Alert &msg)
{
	if(OpStatus::IsSuccess(VerifySucceededStep()))
	{
		OP_STATUS op_err = CheckClientCert();

		if(OpStatus::IsError(op_err))
		{
			FinishedInstall(FALSE);
		}
		else if(op_err == InstallerStatus::INSTALL_FINISHED)
		{
			FinishedInstall(TRUE);
		}
		else
		{
			installation_state = Install_Asking_user;
		}
	}
	else
	{
		FinishedInstall(FALSE);
	}

	ProgressInstall();
}


void SSL_Interactive_Certificate_Installer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(par1 == (MH_PARAM_1) this)
	{
		switch(msg)
		{
		case MSG_CERT_INSTALL_ASK_USER:
			if(context->GetFinalAction() != SSL_CERT_ACCEPT)
			{
				OP_DELETE(context);
				context = NULL;
				FinishedInstall(FALSE);
				return;
			}

			context->GetGlobalFlags(forbid_use, warn_before_use);
			OP_DELETE(context);
			context = NULL;
			installation_state = Install_Installing_certificate;
			break;
		case MSG_FINISHED_PASSWORD:
			if(par2 != IDOK || !ask_password ||
				OpStatus::IsError(SetImportPassword(ask_password->GetOldPassword())))
			{
				OP_DELETE(ask_password);
				ask_password = NULL;
				FinishedInstall(FALSE);
				return;
			}
			OP_DELETE(ask_password);
			ask_password = NULL;
			installation_state = Install_Prepare_cert;
			break;
		case MSG_FINISHED_OPTIONS_PASSWORD:
			if(par2 != IDOK)
			{
				FinishedInstall(FALSE);
				return;
			}
			installation_state = Install_Installing_certificate;
			break;
		}

		ProgressInstall();
	}

	SSL_CertificateVerifier::HandleCallback(msg, par1, par2);
}

#endif
#endif
