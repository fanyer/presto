/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#if !defined(ADS12) || defined(ADS12_TEMPLATING) // see end of streamtype.cpp

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/rootstore/rootstore_api.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/libssl/method_disable.h"

BOOL InitCrypto();

OP_STATUS SSL_Options::Init(SSL_CertificateStore store)
{
	int sel = 0;

	switch(store)
	{
	case SSL_CA_Store :
		sel = SSL_LOAD_CA_STORE | SSL_LOAD_INTERMEDIATE_CA_STORE;
		break;
	case SSL_ClientStore :
		sel = SSL_LOAD_CLIENT_STORE;
		break;
	case SSL_ClientOrCA_Store :
		sel = SSL_LOAD_CLIENT_STORE | SSL_LOAD_CA_STORE;
		break;
	case SSL_TrustedSites :
		sel = SSL_LOAD_TRUSTED_STORE;
		break;
	case SSL_UntrustedSites :
		sel = SSL_LOAD_UNTRUSTED_STORE;
		break;
	case SSL_IntermediateCAStore:
		sel = SSL_LOAD_INTERMEDIATE_CA_STORE;
		break;
	}

	return Init(sel);
}

OP_STATUS SSL_Options::Init(int sel)
{
	OP_STATUS op_err = OpStatus::OK;
	TRAP(op_err, op_err = InitL(sel));
	return op_err;
}

OP_STATUS SSL_Options::InitL(int sel)
{
	const SSL_CipherDescriptions *tempcipher;
	uint32 i,j,k, k2, count;
	OP_MEMORY_VAR int type_sel;

	BOOL upgrade;

	if(initializing)
		return OpStatus::OK;

	initializing = TRUE;

	extern OP_STATUS SSL_RND_Init();
	RETURN_IF_ERROR(SSL_RND_Init());

	upgrade = FALSE;
	if(!loaded_primary)
	{
		if(security_profile == NULL)
		{
			security_profile = OP_NEW(SSL_Security_ProfileList, ());
			current_security_profile = default_security_profile = security_profile;
			if(security_profile == NULL)
			{
				ComponentsLacking = TRUE;
				initializing = FALSE;
				return OpStatus::OK;
			}
		}
		{ // ADS 1.2 work-around
			Loadable_SSL_CompressionMethod tmp(SSL_Null_Compression);
			current_security_profile->compressions.Set(tmp);
		}

		SecurityEnabled= FALSE;

		ComponentsLacking = FALSE;

#ifdef PREFS_WRITE
		if((g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CryptoMethodOverride) & CRYPTO_METHOD_DISABLE_SSLV3) != 0)
		{
			RETURN_IF_LEAVE(g_pcnet->WriteIntegerL(PrefsCollectionNetwork::EnableSSL3_0, FALSE));
		}
#endif

		Enable_SSL_V3_0 = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableSSL3_0);
		Enable_TLS_V1_0 = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableSSL3_1);
		Enable_TLS_V1_1 = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTLS1_1);
#ifdef _SUPPORT_TLS_1_2
		Enable_TLS_V1_2 = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableTLS1_2);
#endif

		SecurityEnabled =
			Enable_SSL_V3_0 ||
			Enable_TLS_V1_0 ||
			Enable_TLS_V1_1
#ifdef _SUPPORT_TLS_1_2
			|| Enable_TLS_V1_2
#endif
			;
		PasswordAging = SSL_ASK_PASSWD_ONCE;
		SystemPasswordVerifiedLast = 0;

		ConfigureSystemCiphers();
		count = SystemCiphers.Cardinal();
		SystemCipherSuite.Resize(count);
		StrongCipherSuite.Resize(count);
		SSL_CipherSuites non_DHE_temp;
		SSL_CipherSuites DHE_temp;
		non_DHE_temp.Resize(count);
		DHE_temp.Resize(count);
		int non_dhe_n= 0, dhe_n= 0;

		i = j = k= k2= 0;
		tempcipher = SystemCiphers.First();
		while (tempcipher != NULL)
		{
			{
				SystemCipherSuite[(uint16) (i)] = tempcipher->id;          // v3 ciphers
				if(
					tempcipher->KeySize >= 16 &&
					tempcipher->kea_alg != SSL_Anonymous_Diffie_Hellman_KEA)
				{
					StrongCipherSuite[(uint16) (k++)]= tempcipher->id;          // v3 ciphers
					if(tempcipher->kea_alg == SSL_Ephemeral_Diffie_Hellman_KEA)
						DHE_temp[(uint16) (dhe_n++)] = tempcipher->id;          // v3 DHE ciphers
					else
						non_DHE_temp[(uint16) (non_dhe_n++)] = tempcipher->id;          // v3 non_DHE ciphers

				}
				i++;
			}
			tempcipher = tempcipher->Suc();
		}
		SystemCipherSuite.Resize((uint16) i);
		StrongCipherSuite.Resize((uint16) k);
		DHE_Reduced_CipherSuite.Resize(dhe_n+non_dhe_n);
		DHE_Reduced_CipherSuite.Transfer(0, DHE_temp, 0, dhe_n);
		DHE_Reduced_CipherSuite.Transfer(dhe_n, non_DHE_temp, 0, non_dhe_n);

		OP_MEMORY_VAR BOOL read_config = FALSE;
		TRAPD(op_err, read_config = ReadNewConfigFileL((BOOL&) sel, upgrade));
		if(OpStatus::IsMemoryError(op_err))
			LEAVE(op_err);
		if(!read_config)
		{
			SSL_ProtocolVersion ver;
			ver.Set(3,0);
			count = SystemCipherSuite.Count();
			current_security_profile->original_ciphers.Resize(count);
			for(j = 0,i=count;i>0;)
			{
				i--;
				tempcipher = GetCipherDescription(ver,(int) i);
				if(tempcipher != NULL &&
					tempcipher->KeySize >= 16 &&
					tempcipher->kea_alg != SSL_Anonymous_Diffie_Hellman_KEA)
					current_security_profile->original_ciphers[j++] = tempcipher->id;
			}
			current_security_profile->original_ciphers.Resize(j);
			current_security_profile->ciphers = current_security_profile->original_ciphers;
			some_secure_ciphers_are_disabled = FALSE;

			loaded_primary =TRUE;
#if defined LIBSSL_AUTO_UPDATE
			g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
#endif
			SaveL();
		}
	}

	if((sel & SSL_LOAD_CA_STORE) != 0)
		sel |= SSL_LOAD_INTERMEDIATE_CA_STORE; // Always init intermediate when the CA store is initialized, and open it first.

	for(type_sel = 1; type_sel < SSL_LOAD_ALL_STORES; type_sel <<= 1)
	{
		BOOL *loaded_flag = NULL;
		const uni_char *filename = NULL;
		SSL_CertificateHead *base = NULL;
		SSL_CertificateStore store = SSL_Unknown_Store;
		uint32 recordtag = 0;

		base = MapSelectionToStore((sel & type_sel), loaded_flag, filename, store, recordtag);

		if(base && loaded_flag && !(*loaded_flag))
		{
			OP_MEMORY_VAR BOOL read_config = FALSE;
			TRAPD(op_err, read_config = ReadNewCertFileL(filename, store, recordtag, base,upgrade));
			if(OpStatus::IsMemoryError(op_err))
				LEAVE(op_err);
			if(!read_config)
			{
				if(store == SSL_CA_Store)
				{
					g_root_store_api->InitAuthorityCerts(this, 0);
					loaded_cacerts = TRUE;
#if defined LIBSSL_AUTO_UPDATE
					g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
#endif
				}
				*loaded_flag = TRUE;
				SaveL();
				keybase_version = SSL_Options_Version;
			}
		}
	}
	if(upgrade)
		SaveL();

#if defined LIBSSL_AUTO_UPDATE && defined ROOTSTORE_USE_LOCALFILE_REPOSITORY
	if(!g_ssl_tried_auto_updaters)
		g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
#endif

	initializing = FALSE;
	return OpStatus::OK;
}

#endif // !ADS12 or ADS12_TEMPLATING
#endif // relevant support
