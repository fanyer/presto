/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _AUTO_UPDATE_VERSION_H_
#define _AUTO_UPDATE_VERSION_H_

#if defined _NATIVE_SSL_SUPPORT_ && defined LIBSSL_AUTO_UPDATE

#ifdef ROOTSTORE_SIGNKEY
#include "modules/rootstore/auto_update_versions.h"
#else

#if defined _DEBUG && defined YNP_WORK
// Debug settings
#define AUTOUPDATE_SCHEME "http"
#define AUTOUPDATE_SERVER "rowan.coreteam.oslo.opera.com/~yngve/auto-update"
#define AUTOUPDATE_VERSION "01"
#define AUTOUPDATE_VERSION_NUM 0x01

#else // debug
// Normal settings
#define AUTOUPDATE_SCHEME "https"
#define AUTOUPDATE_SERVER "certs.opera.com"

#define AUTOUPDATE_VERSION "02"
#define AUTOUPDATE_VERSION_NUM 0x02
#endif // !debug


#if AUTOUPDATE_VERSION_NUM == 0x01
#define AUTOUPDATE_PUBKEY_INCLUDE "modules/rootstore/pubkey/pubkey_1.h"
#define AUTOUPDATE_CERTNAME CERT_DIST_PUB_KEY_v0001
#elif AUTOUPDATE_VERSION_NUM == 0x02
#define AUTOUPDATE_PUBKEY_INCLUDE "modules/rootstore/pubkey/version02.pubkey.h"
#define AUTOUPDATE_CERTNAME CERT_DIST_PUB_KEY_v02
#else
#error "Add new include statement"
#endif

#endif // ROOTSTORE_SIGNKEY

#define ROOTSTORE_CRL_LEVEL CORE_VERSION_MILESTONE
#define ROOTSTORE_CATEGORY  CORE_VERSION_MILESTONE // Desktop

#endif

#endif // _AUTO_UPDATE_VERSION_H_

