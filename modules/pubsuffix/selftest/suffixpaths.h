/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve N. Pettersen
*/

#ifndef _PUBSUFFIX_PATHS_H_
#define _PUBSUFFIX_PATHS_H_

#include "modules/network_selftest/urldoctestman.h"

#define SuffixSelftestBaseHost(host, path) "http://" host "/" URL_NAME "selftests/suffixtests/" path

#define SUFFIXSelftestBase(path) SuffixSelftestBaseHost("netcore.lab.osa", path)

#endif // _PUBSUFFIX_PATHS_H_
