/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file ExternalSSLLibrary.cpp
 *
 * External SSL Library object creation function.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */
#include "core/pch.h"

#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#include "modules/externalssl/src/ExternalSSLLibrary.h"
#include "modules/externalssl/src/openssl_impl/OpenSSLLibrary.h"


ExternalSSLLibrary* ExternalSSLLibrary::CreateL()
{
	ExternalSSLLibrary* library = OP_NEW_L(OpenSSLLibrary, ());
	return library;
}

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION
