/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef UNIX_DESKTOP_PRODUCT_CONFIG_H
#define UNIX_DESKTOP_PRODUCT_CONFIG_H

#define PRODUCT_SYSTEM_FILE "platforms/unix/product/system.h"
#define PRODUCT_OPKEY_FILE "platforms/unix/product/opkeys.h"
#define PRODUCT_ACTIONS_FILE "platforms/unix/product/actions.h"

#ifdef PLUGIN_WRAPPER
# define PRODUCT_FEATURE_FILE "platforms/unix/product/pluginwrapper/pluginwrapper-features.h"
# define PRODUCT_TWEAKS_FILE "platforms/unix/product/pluginwrapper/pluginwrapper-tweaks.h"
#else
# define PRODUCT_FEATURE_FILE "platforms/unix/product/features.h"
# define PRODUCT_TWEAKS_FILE "platforms/unix/product/tweaks.h"
#endif // PLUGIN_WRAPPER

#ifdef OPERASETUP
# include "platforms/unix/product/compilerdefines.h"
#endif

#endif // UNIX_DESKTOP_PRODUCT_CONFIG_H
