/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined _NATIVE_SSL_SUPPORT_ && defined USE_SSL_CERTINSTALLER && defined LIBOPEAY_PKCS12_SUPPORT

#include "modules/libssl/sslbase.h"
#include "modules/libssl/ui/sslcctx.h"
#include "modules/libssl/certs/certexp.h"
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"


SSL_PKCS12_Certificate_Export::SSL_PKCS12_Certificate_Export()
 :	ask_password(NULL),
	window(NULL),
	message_handler(NULL),
	finished_message(MSG_NO_MESSAGE),
	finished_id(0),
	finished_advanced_export(FALSE),
	advanced_export_success(FALSE),
	optionsManager(NULL),
	certificate_number(-1),
	export_state(Export_Not_started)
{
}

SSL_PKCS12_Certificate_Export::~SSL_PKCS12_Certificate_Export()
{
	OP_DELETE(ask_password);
	ask_password = NULL;

	if(optionsManager && optionsManager->dec_reference() == 0)
		OP_DELETE(optionsManager);
	optionsManager = NULL;

	password_export.Wipe();
	g_main_message_handler->UnsetCallBacks(this);
}

OP_STATUS SSL_PKCS12_Certificate_Export::Construct(SSL_Options *optManager, int cert_number,
								const OpStringC &filename, SSL_dialog_config &config)
{
	if(optManager == NULL)
		return OpStatus::ERR_NULL_POINTER;

	optionsManager = optManager;
	certificate_number = cert_number;
	optionsManager->Init(SSL_LOAD_CA_STORE | SSL_LOAD_INTERMEDIATE_CA_STORE | SSL_LOAD_CLIENT_STORE);

	RETURN_IF_ERROR(target_filename.Set(filename));

	window = config.window;
	message_handler = config.msg_handler;
	finished_message = config.finished_message;
	finished_id = config.finished_id;

	g_main_message_handler->SetCallBack(this, MSG_FINISHED_OPTIONS_PASSWORD, (MH_PARAM_1) this);
	g_main_message_handler->SetCallBack(this, MSG_FINISHED_PASSWORD, (MH_PARAM_1) this);
	return OpStatus::OK;
}

OP_STATUS SSL_PKCS12_Certificate_Export::StartExport()
{
	return ProgressExport();
}

BOOL SSL_PKCS12_Certificate_Export::Finished()
{
	return finished_advanced_export;
}

BOOL SSL_PKCS12_Certificate_Export::ExportSuccess()
{
	return advanced_export_success;
}


void	SSL_PKCS12_Certificate_Export::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(par1 != (MH_PARAM_1) this)
		return;
	switch(msg)
	{
	case MSG_FINISHED_OPTIONS_PASSWORD:
		if(par2 != IDOK)
		{
			FinishedExport(FALSE);
			return;
		}
		export_state = Export_Retrieve_Key;
		break;
	case MSG_FINISHED_PASSWORD:
		if(par2 != IDOK || !ask_password || 
			OpStatus::IsError(password_export.Set(ask_password->GetNewPassword())))
		{
			OP_DELETE(ask_password);
			ask_password = NULL;
			FinishedExport(FALSE);
			return;
		}
		export_state = Export_Write_cert;
		break;
	}

	ProgressExport();
}

OP_STATUS SSL_PKCS12_Certificate_Export::ProgressExport()
{
	while(export_state != Export_Finished)
	{
		switch(export_state)
		{
		case Export_Not_started:
			export_state = Export_Retrieve_Key;
			break;
		case Export_Retrieve_Key:
			{
				URL empty_url;
				SSL_dialog_config config(window, g_main_message_handler, MSG_FINISHED_OPTIONS_PASSWORD,(MH_PARAM_1) this, empty_url);

				SSL_CertificateItem *cert_item = optionsManager->Find_Certificate(SSL_ClientStore, certificate_number);
				OP_STATUS op_err = OpStatus::OK;
				if (cert_item && cert_item->private_key_salt.GetLength() > 0)
					op_err = optionsManager->DecryptPrivateKey(certificate_number, private_key, config);
				else
					private_key = cert_item->private_key;

				if(op_err == InstallerStatus::ERR_PASSWORD_NEEDED)
				{
					export_state = Export_Asking_Security_Password;
					return OpStatus::OK;
				}
				else if(OpStatus::IsError(op_err))
				{
					FinishedExport(FALSE);
					return op_err;
				}
				SSL_dialog_config config1(window, g_main_message_handler, MSG_FINISHED_PASSWORD,(MH_PARAM_1) this, empty_url);
				ask_password = OP_NEW(SSLSecurtityPasswordCallbackImpl, (
					OpSSLListener::SSLSecurityPasswordCallback::NewPassword,
					OpSSLListener::SSLSecurityPasswordCallback::CertificateExport,
					Str::DI_IDM_PASSWORD_BOX,
					Str::SI_MSG_SSL_EXPORT_PASSWORD,
					config1
				));
				if(ask_password == NULL)
				{
					FinishedExport(FALSE);
					return OpStatus::ERR_NO_MEMORY;
				}

				op_err = ask_password->StartDialog();
				if(OpStatus::IsError(op_err))
				{
					FinishedExport(FALSE);
					return op_err;
				}

				export_state = Export_Asking_Cert_Password;
				return OpStatus::OK;
			}
		case Export_Asking_Security_Password:
		case Export_Asking_Cert_Password:
			return OpStatus::OK;

		case Export_Write_cert:
			{
				OP_STATUS op_err = optionsManager->ExportPKCS12_Key_and_Certificate(target_filename, private_key, certificate_number, password_export);
				FinishedExport(OpStatus::IsSuccess(op_err));
			}
		}
	}
	return OpStatus::OK;
}

void SSL_PKCS12_Certificate_Export::FinishedExport(BOOL status)
{
	private_key.Resize(0);
	password_export.Wipe();
	finished_advanced_export= TRUE;
	advanced_export_success = status;
	export_state= Export_Finished;
	message_handler->PostMessage(finished_message, finished_id, status);
}
#endif // USE_SSL_CERTINSTALLER
