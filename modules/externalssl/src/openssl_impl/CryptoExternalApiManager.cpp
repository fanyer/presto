/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file CryptoExternalApiManager.cpp
 *
 * Initialization and uninitialization of the crypto library.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#if defined(_EXTERNAL_SSL_SUPPORT_) && defined(CRYPTO_API_SUPPORT) && defined(EXTERNAL_SSL_OPENSSL_IMPLEMENTATION)

#include "modules/libcrypto/include/CryptoExternalApiManager.h"


// Will be called at startup
OP_STATUS CryptoExternalApiManager::InitCryptoLibrary()
{
	return OpStatus::OK;
}

// Will be called at shutdown
OP_STATUS CryptoExternalApiManager::DestroyCryptoLibrary()
{
	return OpStatus::OK;
}

#endif // _EXTERNAL_SSL_SUPPORT_ && CRYPTO_API_SUPPORT && EXTERNAL_SSL_OPENSSL_IMPLEMENTATION
