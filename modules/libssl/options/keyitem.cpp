/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_) && !defined(SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION)

#if defined USE_SSL_CERTINSTALLER
#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/pi/OpLocale.h"
#include "modules/libssl/options/passwd_internal.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

OP_STATUS SSL_Options::AddPrivateKey(SSL_BulkCipherType type, uint32 bits, SSL_secure_varvector32 &privkey, 
									 SSL_varvector16 &pub_key_hash, const OpStringC &url_arg, SSL_dialog_config &config)
{
	OpAutoPtr<SSL_CertificateItem> newcert;

	RETURN_IF_ERROR(Init(SSL_LOAD_CLIENT_STORE));
	if (EncryptClientCertificates())
		RETURN_IF_ERROR(GetPassword(config));

	newcert.reset(OP_NEW(SSL_CertificateItem, ()));
	if(newcert.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;

	switch(type)
	{
	case SSL_RSA:
		newcert->certificatetype = SSL_rsa_sign;
		break;
	case SSL_DSA:
		newcert->certificatetype = SSL_dss_sign;
		break;
	case SSL_DH:
		newcert->certificatetype = SSL_rsa_fixed_dh;
		break;
	default:
		break;
	}

	newcert->public_key_hash = pub_key_hash;
	if(newcert->public_key_hash.ErrorRaisedFlag || newcert->public_key_hash.GetLength() == 0)
		return newcert->public_key_hash.GetOPStatus();
	
#ifdef TST_DUMP
	DumpTofile(privkey,privkey.GetLength(),"Private key DER encoded","sslkgen.txt");
#endif  

	if (EncryptClientCertificates())
	{
		EncryptWithPassword(privkey, newcert->private_key, newcert->private_key_salt,
							SystemCompletePassword,GetPrivateKeyverificationString());

		CheckPasswordAging();
	}
	else
	{
		OP_ASSERT(privkey.GetLength() > 0);
		newcert->private_key.Set(privkey);
		newcert->private_key_salt.Resize(0);
	}

	if(newcert->private_key.GetLength() == 0 || (EncryptClientCertificates() && newcert->private_key_salt.GetLength() == 0))
		return OpStatus::ERR;
	else
	{
		uni_char tempdate[100]; /* ARRAY OK 2009-02-03 yngve */
		
		OpString Noloadtext;
		RETURN_IF_ERROR(SetLangString(Str::SI_MSG_SSL_TEXT_No_Cert_Loaded, Noloadtext));

		newcert->cert_title.Empty();
		time_t gentime = g_timecache->CurrentTime();
		struct tm *gentime_tm = op_localtime(&gentime);
		g_oplocale->op_strftime(tempdate, 99, UNI_L("%x %X"), gentime_tm);
		tempdate[99]='\0';
		RETURN_IF_ERROR(newcert->cert_title.AppendFormat(Noloadtext.CStr(),(unsigned) bits, tempdate, (url_arg.HasContent() ? url_arg.CStr() : UNI_L("")) ));
	}

	if(register_updates)
		newcert->cert_status = Cert_Inserted;

	newcert->Into(&System_ClientCerts);
	newcert.release();


	return OpStatus::OK;
}
#endif
#endif
