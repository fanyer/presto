/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/sslrand.h"

void SSL_Options::ClearObfuscated()
{
	OP_DELETE(obfuscation_cipher);
	obfuscation_cipher = NULL;

	obfuscator_iv.Resize(0);
	obfuscated_SystemCompletePassword.Resize(0);
}

void SSL_Options::Obfuscate()
{
	ClearObfuscated();

	if(SystemCompletePassword.GetLength() != 0)
	{
		OP_STATUS op_err = OpStatus::OK;
		OpStackAutoPtr<SSL_GeneralCipher> obfuscator(g_ssl_api->CreateSymmetricCipher(SSL_3DES, op_err));
		if(OpStatus::IsError(op_err))
			return;

		OpStackAutoPtr<SSL_GeneralCipher> obfuscator2(g_ssl_api->CreateSymmetricCipher(SSL_3DES, op_err));
		if(OpStatus::IsError(op_err) || !obfuscator.get() || !obfuscator2.get() )
			return;

		SSL_secure_varvector32 keys;

		SSL_RND(keys, obfuscator->KeySize() + obfuscator->IVSize());

		if(keys.Error())
			return;

		obfuscator->SetCipherDirection(SSL_Encrypt);
		obfuscator2->SetCipherDirection(SSL_Decrypt);

		const byte *src = obfuscator->LoadKey(keys.GetDirect());
		obfuscator->LoadIV(src);
		obfuscator_iv.Set(src, obfuscator->IVSize());
		obfuscator2->LoadKey(keys.GetDirect());
		obfuscator->LoadIV(obfuscator_iv.GetDirect());
		obfuscator->SetPaddingStrategy(SSL_PKCS5_PADDING);
		obfuscator2->SetPaddingStrategy(SSL_PKCS5_PADDING);

		if(obfuscator->Error() || obfuscator2->Error() || obfuscator_iv.Error())
		{
			ClearObfuscated();
			return;
		}

		obfuscator->EncryptVector(SystemCompletePassword, obfuscated_SystemCompletePassword);

		if(obfuscator->Error() || obfuscated_SystemCompletePassword.Error())
		{
			ClearObfuscated();
			return;
		}

		obfuscation_cipher = obfuscator2.release();
	}
}


void SSL_Options::DeObfuscate(SSL_secure_varvector32 &target)
{
	target.Resize(0);

	if(obfuscated_SystemCompletePassword.GetLength() != 0)
	{
		if(obfuscation_cipher && obfuscator_iv.GetLength() >= obfuscation_cipher->IVSize())
		{
			obfuscation_cipher->LoadIV(obfuscator_iv.GetDirect());
			obfuscation_cipher->DecryptVector(obfuscated_SystemCompletePassword, target);
			if(obfuscation_cipher->Error() || target.Error())
			{
				ClearObfuscated();
				target.Resize(0);
			}
		}
		else
			ClearObfuscated();
	}
}

#endif // relevant support
