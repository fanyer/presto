/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef PURPOSES_H
#define PURPOSES_H

#if defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)

#include <openssl/x509v3.h>

#define OPERA_X509_PURPOSE_CODE_SIGN		(X509_PURPOSE_MAX + 1)

#endif // defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)
#endif // PURPOSES_H
