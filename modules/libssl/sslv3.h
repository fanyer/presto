/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef __SSLV3_H
#define __SSLV3_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/util/opstrlst.h"

#include "modules/libssl/sslbase.h"

#include "modules/util/twoway.h"
#include "modules/locale/locale-enum.h"

#include "modules/libssl/base/sslprotver.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslphash.h"

#include "modules/libssl/handshake/hand_types.h"

#include "modules/libssl/protocol/sslstat.h"
#endif

#endif
