/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_CACHE_MODULE_H
#define MODULES_CACHE_MODULE_H

/* Code related to this module routinely tests these defines > 0 (about which
 * Brew's ADS compiler grumbles, although it's harmless as undefined symbols in
 * a #if's condition are treated as 0) even when the tweaks that control them
 * aren't enabled; but the tweak system has only given them their default value,
 * 0, when they're enabled.  So supply fall-backs here to silence a mass of
 * noisy warnings.  The globals system happens after the tweaks-and-APIs
 * (handled as part of features). */
#ifndef CACHE_SMALL_FILES_SIZE
#define CACHE_SMALL_FILES_SIZE 0
#endif // only defined if both DISK_CACHE_SUPPORT, CACHE_FAST_INDEX are.
#ifndef CACHE_CONTAINERS_ENTRIES
#define CACHE_CONTAINERS_ENTRIES 0
#endif // only defined if DISK_CACHE_SUPPORT is.

#endif // MODULES_CACHE_MODULE_H
