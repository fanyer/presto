/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/smartptr.h"
#include "modules/url/url_smartptr.inc"

#ifdef _SUPPORT_OPENPGP_
#include "modules/libopenpgp/pgp_smartptr.inc"
#endif // _SUPPORT_OPENPGP_

#if defined(_SSL_SUPPORT_) && defined(_NATIVE_SSL_SUPPORT_) 
#include "modules/libssl/libssl_smartptr.inc"
#endif // _SSL_SUPPORT_
