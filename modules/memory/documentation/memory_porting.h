/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Doxygen documentation file for porting
 *
 * This file is only used for documenting the memory module through
 * use of 'doxygen'.  It should not contain any code to be compiled,
 * and should therefore never be included by any other file.
 *
 * \author Morten Rolland, mortenro@opera.com
 */

/**
 * \page mem-porting Porting the memory module to a new platform
 *
 * \section mem-porting-overview Overview
 *
 * This page describes how the memory module could be ported to a
 * new platform/architecture.  There are several things to consider,
 * and some of the configuration issues requires detailed knowledge
 * of the target platform, and how you want Opera to make use of
 * and integrate with it.
 *
 * \b Hint: It may be beneficial to sit down with someone who have
 * done a port of the memory module, or others with knowledge of
 * the module (like the module owner...) to get going on a port.
 *
 * \b Note: Copying a configuration from another project is a sure
 * way to ensure you are using old features and settings,
 * non-standard settings and sub-optimal or even unsafe tweaks.
 *
 * \section mem-porting-tweaks Tweaks
 *
 * There are a significant number of tweaks, and they are all
 * documented in the \c memory/module.tweaks file.  Go there for
 * details.  The following is an short overview of some of the
 * tweaks.
 *
 * \subsection mem-porting-important-tweaks Important tweaks
 *
 * \li TWEAK_MEMORY_USE_GLOBAL_VARIABLES - Use global variables or
 *     not.
 *
 * \li TWEAK_MEMORY_ALIGNMENT - Override default alignment
 *     requirement, which is either 4 or 8 bytes.
 *
 * \li TWEAK_MEMORY_REGULAR_GLOBAL_NEW - declare global operator new?
 *
 * \li TWEAK_MEMORY_IMPLEMENT_GLOBAL_NEW - implement global operator new?
 *
 * \li TWEAK_MEMORY_INLINED_GLOBAL_NEW - inline the global operator new?
 *
 * \subsection mem-porting-debuggubg-tweaks Debugging tweaks
 *
 * \li TWEAK_MEMORY_DEAD_OBJECT_QUEUE - Adjust number of lingering objects.
 *
 * \li TWEAK_MEMORY_DEAD_BYTES_QUEUE - Adjust number of lingering bytes.
 */
