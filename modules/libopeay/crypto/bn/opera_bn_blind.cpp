/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(_SSL_USE_OPENSSL_) || defined(USE_OPENSSL_CERTIFICATE_VERIFICATION)
extern "C" {
#include "bn_blind.c"
}
#endif // _SSL_USE_OPENSSL_
