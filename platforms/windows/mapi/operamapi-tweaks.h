/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_OPERAMAPI_TWEAKS_H
#define WINDOWS_OPERAMAPI_TWEAKS_H

#include "platforms/windows/tweaks.h"

#undef TWEAK_UTIL_CLEANUP_STACK
#undef UTIL_CLEANUP_STACK
#define TWEAK_UTIL_CLEANUP_STACK NO

#undef TWEAK_NS4P_COMPONENT_PLUGINS
#define TWEAK_NS4P_COMPONENT_PLUGINS					NO

#endif // WINDOWS_OPERAMAPI_TWEAKS_H

