/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MAC_DESKTOP_PRODUCT_CONFIG_H
#define MAC_DESKTOP_PRODUCT_CONFIG_H

#define PRODUCT_SYSTEM_FILE "platforms/mac/system.h"
#define PRODUCT_OPKEY_FILE "platforms/mac/opkeys.h"
#define PRODUCT_ACTIONS_FILE "platforms/mac/actions.h"

#ifdef PLUGIN_WRAPPER
# define PRODUCT_FEATURE_FILE "platforms/mac/pluginwrapper/pluginwrapper-features.h"
# define PRODUCT_TWEAKS_FILE "platforms/mac/pluginwrapper/pluginwrapper-tweaks.h"
#else
# define PRODUCT_FEATURE_FILE "platforms/mac/features.h"
# define PRODUCT_TWEAKS_FILE "platforms/mac/tweaks.h"
#endif // PLUGIN_WRAPPER

#endif // MAC_DESKTOP_PRODUCT_CONFIG_H
