/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CRYPTO_API_H_
#define CRYPTO_API_H_

#ifdef CRYPTO_API_SUPPORT
#ifdef _EXTERNAL_SSL_SUPPORT_
/* Must be implemented by platform when openssl is not used */
class CryptoExternalApiManager
{
public:
	static OP_STATUS InitCryptoLibrary(); // Will be called at startup 
	static OP_STATUS DestroyCryptoLibrary(); // Will be called at shutdown
};

#endif // _EXTERNAL_SSL_SUPPORT_
#endif // CRYPTO_API_SUPPORT

#endif /* CRYPTO_API_H_ */
