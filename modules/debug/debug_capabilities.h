/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

/** \file
 *
 * \brief Capability defines for the debug module.
 *
 * Defines that can be tested for to adapt other code to deal with
 * different versions of the debug module.
 */

#ifndef DEBUG_CAPABILITIES_H
#define DEBUG_CAPABILITIES_H

/**
 * \brief Defined when \c Debug_OpAssert w/friends needs implementing.
 *
 * The capability \c DBG_CAP_IMPL_IN_PLATFORM is defined when the debug
 * module expects the platform specific functionality to be defined by
 * the platform itself.
 *
 * The 'Debug_OpAssert' function neeeds to be implemented when this
 * capability is defined and DEBUG_ENABLE_OPASSERT is defined.  This
 * condition does not hold, if tweak TWEAK_DEBUG_EXTERNAL_MACROS has
 * been applied, and the definition of OP_ASSERT() is provided by the
 * platform.
 *
 * 2006-11-30 mortenro
 * 2011-01-21: Still used in Desktop (platforms/mac) and Bream (platforms/vm/zephyr)
 */
#define DBG_CAP_IMPL_IN_PLATFORM

#endif // DEBUG_CAPABILITIES_H
