/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file libcrypto_public_api.h
 *
 * List of libcrypto public API headers.
 *
 * This file serves 2 purposes:
 *   -#) Provide a quick and easy way to include the whole libcrypto API,
 *       if it is acceptable by the module user.
 *   -#) Provide a list of the public API header files.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef LIBCRYPTO_PUBLIC_API_H
#define LIBCRYPTO_PUBLIC_API_H

// Public API header files, as a rule, have name ClassName.h
// and reside in "include/" directory.
#include "modules/libcrypto/include/CryptoExternalApiManager.h"
#include "modules/libcrypto/include/CryptoSignature.h"
#include "modules/libcrypto/include/CryptoStreamEncryptionCFB.h"
#include "modules/libcrypto/include/CryptoStreamEncryptionRC4.h"
#include "modules/libcrypto/include/CryptoVerifySignedTextFile.h"
#include "modules/libcrypto/include/CryptoXmlSignature.h"
#include "modules/libcrypto/include/GadgetSignatureVerifier.h"
#include "modules/libcrypto/include/OCSPCertificateChainVerifier.h"
#include "modules/libcrypto/include/OpCryptoKeyManager.h"
#include "modules/libcrypto/include/OpEncryptedFile.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"

// Candidates to platform interfaces.
#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/libcrypto/include/CryptoHMAC.h"
#include "modules/libcrypto/include/CryptoCertificate.h"
#include "modules/libcrypto/include/CryptoCertificateChain.h"
#include "modules/libcrypto/include/CryptoCertificateStorage.h"
#include "modules/libcrypto/include/CryptoSymmetricAlgorithm.h"
#include "modules/libcrypto/include/OpOCSPRequest.h"
#include "modules/libcrypto/include/OpOCSPRequestProducer.h"
#include "modules/libcrypto/include/OpOCSPResponseVerifier.h"

#endif /* LIBCRYPTO_PUBLIC_API_H */
