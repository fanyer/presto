/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#if defined _NATIVE_SSL_SUPPORT_ && defined LIBSSL_ENABLE_KEYGEN

#include "modules/libssl/ssl_api.h"
#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/keygen_tracker.h"
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"


SSL_Private_Key_Generator::SSL_Private_Key_Generator()
 : m_format(SSL_Netscape_SPKAC),
 m_type(SSL_RSA),
 m_keysize(0),
 window(NULL),
 msg_handler(NULL),
 finished_message(MSG_NO_MESSAGE),
 finished_id(0),
 optionsManager(NULL),
 external_opt(FALSE),
 asking_password(NULL)
{
}

SSL_Private_Key_Generator::~SSL_Private_Key_Generator()
{
	m_spki_string.Wipe();
	m_spki_string.Empty();

	if(optionsManager && optionsManager->dec_reference() == 0)
		OP_DELETE(optionsManager);
	optionsManager = NULL;

	OP_DELETE(asking_password);
	asking_password = NULL;

	g_main_message_handler->UnsetCallBacks(this);
}

OP_STATUS SSL_Private_Key_Generator::Construct(SSL_dialog_config &config, URL &target, SSL_Certificate_Request_Format format, SSL_BulkCipherType type,
											   const OpStringC8 &challenge, unsigned int keygen_size, SSL_Options *opt)
{
	m_target_url = target;
	m_format = format;
	m_type = type;
	m_keysize = keygen_size;

	RETURN_IF_ERROR(m_challenge.Set(challenge));

	window = config.window;
	msg_handler = config.msg_handler;
	finished_message = config.finished_message;
	finished_id = config.finished_id;

	if(opt)
	{
		optionsManager = opt;
		opt->inc_reference();
		external_opt = TRUE;
	}
	return OpStatus::OK;
}

void SSL_Private_Key_Generator::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_LIBSSL_RUN_KEYALGORITHM_ITERATION:
		{
			OP_STATUS op_err = IterateKeyGeneration();
			if(op_err != InstallerStatus::ERR_PASSWORD_NEEDED && OpStatus::IsError(op_err))
			{
				Finished(FALSE);
				return;
			}
			else if(op_err == InstallerStatus::KEYGEN_WORKING)
			{
				g_main_message_handler->PostMessage( MSG_LIBSSL_RUN_KEYALGORITHM_ITERATION, (MH_PARAM_1) this, 0);
			}
		}
		break;
	case MSG_FINISHED_OPTIONS_PASSWORD:
		{
			g_main_message_handler->UnsetCallBack(this, MSG_FINISHED_OPTIONS_PASSWORD);
			if(par2 != IDOK)
			{
				Finished(FALSE);
				return;
			}
			OP_STATUS op_err = InternalStoreKey();
			if(OpStatus::IsSuccess(op_err) || op_err != InstallerStatus::ERR_PASSWORD_NEEDED)
				Finished(OpStatus::IsSuccess(op_err));
		}
		break;
	}
}

OP_STATUS SSL_Private_Key_Generator::StartKeyGeneration()
{
	OP_STATUS op_err = InitiateKeyGeneration();
	
	if(op_err == InstallerStatus::ERR_PASSWORD_NEEDED)
		return OpStatus::OK;

	else if(OpStatus::IsError(op_err))
		return op_err;
	else if(op_err == InstallerStatus::KEYGEN_WORKING)
	{
		g_main_message_handler->PostMessage( MSG_LIBSSL_RUN_KEYALGORITHM_ITERATION, (MH_PARAM_1) this, 0);
		return g_main_message_handler->SetCallBack(this, MSG_LIBSSL_RUN_KEYALGORITHM_ITERATION, (MH_PARAM_1) this);
	}

	return op_err;
}

OP_STATUS SSL_Private_Key_Generator::StoreKey(SSL_secure_varvector32 &pkcs8_private_key, SSL_secure_varvector32 &public_key_hash)
{
	m_pkcs8_private_key = pkcs8_private_key;
	if(m_pkcs8_private_key.Error())
		return m_pkcs8_private_key.GetOPStatus();
	m_public_key_hash = public_key_hash;
	if(m_public_key_hash.Error())
		return m_public_key_hash.GetOPStatus();

	if(!optionsManager)
	{
		optionsManager = g_ssl_api->CreateSecurityManager(TRUE);

		if(optionsManager == NULL)
		{
			Finished(FALSE);
			return OpStatus::ERR_NO_MEMORY;
		}
		external_opt = FALSE;
	}

	OP_STATUS op_err = OpStatus::OK;
	if(OpStatus::IsError(op_err = optionsManager->Init(SSL_ClientStore)))
	{
		Finished(FALSE);
		return op_err;
	}

	return InternalStoreKey();
}

OP_STATUS SSL_Private_Key_Generator::InternalStoreKey()
{
	if(optionsManager == NULL)
		return OpStatus::ERR_NULL_POINTER;

	SSL_dialog_config dialog_options(window, g_main_message_handler, MSG_FINISHED_OPTIONS_PASSWORD, (MH_PARAM_1) this, m_target_url);
	OpString temp_name;
	OpStatus::Ignore(m_target_url.GetAttribute(URL::KUniName, 0, temp_name));
	OP_STATUS op_err = optionsManager->AddPrivateKey(m_type, m_keysize, m_pkcs8_private_key, m_public_key_hash, temp_name, dialog_options);
	if(op_err == InstallerStatus::ERR_PASSWORD_NEEDED)
	{
		g_main_message_handler->SetCallBack(this,  MSG_FINISHED_OPTIONS_PASSWORD, (MH_PARAM_1) this);
		return op_err;
	}

	Finished(OpStatus::IsSuccess(op_err));
	return op_err;
}

void SSL_Private_Key_Generator::Finished(BOOL success)
{
	if(success && !external_opt)
		g_ssl_api->CommitOptionsManager(optionsManager);

	msg_handler->PostMessage(finished_message, finished_id, (success ? TRUE : FALSE));
}


#endif // LIBSSL_ENABLE_KEYGEN
