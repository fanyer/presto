/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGACONFIG_ARM_H
#define VEGACONFIG_ARM_H

// Are we compiling for ARM?
#if defined(__arm__) || defined(__ARM_NEON__) || defined(__ARM_EABI__) || defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_7A__)

#ifndef ARCHITECTURE_ARM
#define ARCHITECTURE_ARM
#endif // ARCHITECTURE_ARM

#if defined(__ARM_ARCH_7A__) && !defined(ARCHITECTURE_ARM_NEON)
// Linking object files with NEON instructions seems to make opera crash on startup on some ARMv6 devices.
// See EMO-6455 for details.
#define ARCHITECTURE_ARM_NEON
#endif // defined(__ARM_ARCH_7A__) && !defined(ARCHITECTURE_ARM_NEON)

#endif

#endif // VEGACONFIG_ARM_H
