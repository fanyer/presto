/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef CORE_PREFS_HELPERS_H
#define CORE_PREFS_HELPERS_H

/** @brief Namespace with helper functions to apply or change core preferences
  */
namespace CorePrefsHelpers
{
	/** Apply settings for cache sizes to the memory manager
	  * NB: This functionality should move into core.
	  */
	void ApplyMemoryManagerPrefs();
};

#endif // CORE_PREFS_HELPERS_H
