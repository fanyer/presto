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

#ifndef _ROOTSTORE_AUTO_UPDATE_VERSION_H_
#define _ROOTSTORE_AUTO_UPDATE_VERSION_H_

#if defined ROOTSTORE_SIGNKEY

#if (defined _DEBUG && defined YNP_WORK) || defined ROOTSTORE_USE_INTERNAL_UPDATE_SERVER
// Debug settings
#define AUTOUPDATE_SCHEME "http"
#define AUTOUPDATE_SERVER "rootstore-alpha.oslo.osa"
#define AUTOUPDATE_VERSION "01"
#define AUTOUPDATE_VERSION_NUM 0x01

#elif defined ROOTSTORE_USE_LOCALFILE_REPOSITORY

#ifndef ROOTSTORE_USE_LOCALFILE_REPOSITORY_LOCATION
# error "ROOTSTORE_USE_LOCALFILE_REPOSITORY_LOCATION must be set to a valid path"
#endif

#define AUTOUPDATE_SCHEME "file"
#define AUTOUPDATE_SERVER "localhost" ## ROOTSTORE_USE_LOCALFILE_REPOSITORY_LOCATION
#define AUTOUPDATE_VERSION "03"
#define AUTOUPDATE_VERSION_NUM 0x03

#else // debug
// Normal settings
#define AUTOUPDATE_SCHEME "https"
#define AUTOUPDATE_SERVER "certs.opera.com"
#define AUTOUPDATE_VERSION "03"
#define AUTOUPDATE_VERSION_NUM 0x03
#endif // !debug


#if AUTOUPDATE_VERSION_NUM == 0x01
#define AUTOUPDATE_PUBKEY_INCLUDE "modules/rootstore/pubkey/pubkey_1.h"
#define AUTOUPDATE_CERTNAME CERT_DIST_PUB_KEY_v0001
#elif AUTOUPDATE_VERSION_NUM == 0x02 || AUTOUPDATE_VERSION_NUM == 0x03
#define AUTOUPDATE_PUBKEY_INCLUDE "modules/rootstore/pubkey/version02.pubkey.h"
#define AUTOUPDATE_CERTNAME CERT_DIST_PUB_KEY_v02
#else
#error "Add new include statement"
#endif

#endif

#endif // _AUTO_UPDATE_VERSION_H_

