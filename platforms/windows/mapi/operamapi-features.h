/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_OPERAMAPI_FEATURES_H
#define WINDOWS_OPERAMAPI_FEATURES_H

#include "platforms/windows/features.h"

// Use the hack define to remove core stuff
#define NO_CORE_COMPONENTS

// Turn off selftests
#undef  FEATURE_SELFTEST
#define FEATURE_SELFTEST					NO

#endif // WINDOWS_OPERAMAPI_FEATURES_H
