/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/secprofile.h"
#include "modules/libssl/options/cipher_description.h"

template class OpSmartPointerWithDelete<SSL_Security_ProfileList>;
template class OpSmartPointerWithDelete<SSL_CipherDescriptions>;

#ifdef _SSL_USE_SMARTCARD_

#ifdef _SSL_USE_PKCS11_
#include "modules/libssl/external/p11/p11_man.h"
template class OpSmartPointerNoDelete<PKCS_11_Manager>;
template class OpSmartPointerNoDelete<PKCS_11_Token>;
#endif // _SSL_USE_PKCS11_

#ifdef _SSL_USE_MSCAPI_
#include "modules/libssl/external/mscapi/mscapi_man.h"

template class OpSmartPointerNoDelete<MSCAPI_Manager>;
#endif // _SSL_USE_MSCAPI_

#endif // _SSL_USE_SMARTCARD_

#endif // _NATIVE_SSL_SUPPORT_
