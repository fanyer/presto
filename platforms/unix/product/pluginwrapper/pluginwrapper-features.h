/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PLUGIN_WRAPPER_FEATURES_H
#define PLUGIN_WRAPPER_FEATURES_H

#include "platforms/unix/product/features.h"

// Common plugin wrapper features file
#include "adjunct/pluginwrapper/pluginwrapper-features.h"

// No memory debugging in the plugin wrapper
#undef FEATURE_MEMORY_DEBUGGING
#define FEATURE_MEMORY_DEBUGGING NO

#endif // PLUGIN_WRAPPER_FEATURES_H
