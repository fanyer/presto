/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file prefsloader_capabilities.h
 *
 * Defines the capabilites available in this version of the prefsloader module.
 */

#ifndef PREFSLOADER_CAPABILITIES_H
#define PREFSLOADER_CAPABILITIES_H

/** Module exists.
  * Available in all versions. Please remove any tests on as soon as
  * the module has settled. */
#define PL_CAP_EXISTS

/** PrefsLoadManager support is available.
  * Added 2004-06 on Smurf 1. */
#define PREFS_CAP_DOWNLOAD

#endif
