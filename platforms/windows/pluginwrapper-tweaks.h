/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWS_PLUGINWRAPPER_TWEAKS_H
#define WINDOWS_PLUGINWRAPPER_TWEAKS_H

#include "platforms/windows/tweaks.h"

// Common plugin wrapper tweaks file
#include "adjunct/pluginwrapper/pluginwrapper-tweaks.h"

/* When wishing to enable component plugins for 64-bit builds only,
   it's impossible to base that decision on the bitness of the main
   build as these are two different projects. So always compile
   plugin wrappers. Packaging will take care of distributing (or not)
   wrapper binaries. */
# undef TWEAK_NS4P_COMPONENT_PLUGINS
# define TWEAK_NS4P_COMPONENT_PLUGINS					YES

#endif // WINDOWS_PLUGINWRAPPER_TWEAKS_H

