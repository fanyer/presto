/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _WINDOWS_CONFIG_H_
#define _WINDOWS_CONFIG_H_

#define PRODUCT_SYSTEM_FILE "platforms/windows/system.h"
#define PRODUCT_OPKEY_FILE "platforms/windows/opkeys.h"
#define PRODUCT_ACTIONS_FILE "platforms/windows/actions.h"

#if defined(PLUGIN_WRAPPER)
# define PRODUCT_FEATURE_FILE "platforms/windows/pluginwrapper-features.h"
# define PRODUCT_TWEAKS_FILE "platforms/windows/pluginwrapper-tweaks.h"
#elif defined(OPERA_MAPI)
# define PRODUCT_FEATURE_FILE "platforms/windows/mapi/operamapi-features.h"
# define PRODUCT_TWEAKS_FILE "platforms/windows/mapi/operamapi-tweaks.h"
#else
# define PRODUCT_FEATURE_FILE "platforms/windows/features.h"
# define PRODUCT_TWEAKS_FILE "platforms/windows/tweaks.h"
#endif // PLUGIN_WRAPPER

#endif // _WINDOWS_CONFIG_H_
