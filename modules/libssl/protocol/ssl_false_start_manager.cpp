/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef LIBSSL_ENABLE_SSL_FALSE_START

#include "ssl_false_start_manager.h"
#include "modules/util/opfile/opfile.h"
#include "modules/libssl/sslbase.h"
#include "modules/libssl/handshake/sslhand3.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/libssl/base/sslenum.h"
#include "modules/libssl/base/sslciphspec.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/keyex/sslkeyex.h"

/* static */ OP_STATUS SSLFalseStartManager::Make(SSLFalseStartManager *&manager)
{
	manager = OP_NEW(SSLFalseStartManager, ());
	RETURN_OOM_IF_NULL(manager);

	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSSLFalseStart))
		return manager->LoadBlackList();

	return OpStatus::OK;
}

BOOL SSLFalseStartManager::IsBlackListed(const char *server, int port)
{
	if (!server || m_black_list.Contains(server))
		return TRUE;

	OpString8 domain_and_port;
	OP_STATUS err = domain_and_port.AppendFormat("%s:%u", server, port);

	if (OpStatus::IsError(err) || m_black_list.Contains(server))
		return TRUE;

	return FALSE;
}

BOOL SSLFalseStartManager::SSLFalseStartCipherSuitesApproved(SSL_ConnectionState *connection)
{
	if (!connection || !connection->write.cipher->Method || !connection->read.cipher->Method || !connection->key_exchange)
		return FALSE;

	SSL_ConnectionState::direction_cipher *read = &connection->write;
	SSL_ConnectionState::direction_cipher *write = &connection->write;

	// https://tools.ietf.org/html/draft-bmoeller-tls-falsestart-00#section-6.1
	// Null cipher is not approved
	if (read->cipher->Method->CipherID() == SSL_NoCipher || write->cipher->Method->CipherID() == SSL_NoCipher)
		return FALSE;

	// Only ciphers with key size greater than 128 bits are approved
	if (write->cipher->Method->KeySize() < 16 || read->cipher->Method->KeySize() < 16)
		return FALSE;

	// Check key exchange algorithm white list.
	SSL_KeyExchangeAlgorithm current_key_exchange_algorithm = connection->key_exchange->GetKeyExhangeAlgorithm();

	if (!SSL_FALSE_START_KEY_EXCHANGE_WHITELIST)
		return FALSE;

	return TRUE;
}

BOOL SSLFalseStartManager::ConnectionApprovedForSSLFalseStart(class SSL_ConnectionState *connection)
{
	if (!m_file_loaded)
		return FALSE;  // The alternative would be to allow false start for all servers if file is not found, and risk failing pages.

	// BREW compiler requires an explicit check with NULL for OpSmartPointer.
	if (!connection || connection->server_info == NULL || !connection->server_info->GetServerName())
		return FALSE;

	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSSLFalseStart))
		return FALSE;

	if (!SSLFalseStartCipherSuitesApproved(connection))
		return FALSE;

	if (IsBlackListed(connection->server_info->GetServerName()->Name(), connection->server_info->Port()))
		return FALSE;

	return TRUE;
}

OP_STATUS SSLFalseStartManager::LoadBlackList()
{
	OpFile file;
	RETURN_IF_ERROR(file.Construct(UNI_L(SSL_FALSE_START_BLACK_LIST_FILE_NAME),  SSL_FALSE_START_BLACK_LIST_FOLDER));

	BOOL found;
	RETURN_IF_ERROR(file.Exists(found));
	if (!found)
	{
		OP_ASSERT(! "SSL false start optimization will not currently work. Missing blacklist file \n"
					"<SSL_FALSE_START_BLACK_LIST_FOLDER>/<SSL_FALSE_START_BLACK_LIST_FILE_NAME>  as defined in ssl_false_start_manager.h.\n"
					"Please copy this file from modules/libssl/ssl_false_start_blacklist.txt  or download from \n"
					"http://git.chromium.org/gitweb/?p=chromium.git;a=blob;f=net/base/ssl_false_start_blacklist.txt.");
		return OpStatus::ERR_FILE_NOT_FOUND;
	}

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	while (!file.Eof())
	{
		OpAutoPtr<OpString8> line(OP_NEW(OpString8, ()));
		RETURN_OOM_IF_NULL(line.get());
		RETURN_IF_ERROR(file.ReadLine(*line));

		line->Strip(TRUE, TRUE);

		if (line->HasContent() && ((*line)[0]) != '#')
		{
			RETURN_IF_ERROR(m_black_list.Add(line->CStr(), line.get()));
			line.release();
		}
	}

	m_file_loaded = TRUE;

	return OpStatus::OK;
}

#endif // LIBSSL_ENABLE_SSL_FALSE_START
