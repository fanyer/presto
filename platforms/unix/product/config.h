/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "system.h" // VER_NUM_STR

/* Make sure to sync. this with "../system.h" as well.  That information is used
 * by some modules.  (Whatever became of having to keep cydoor up to date ?)
 * Perhaps we should #include "system.h" as well ?
 */
#ifndef BROWSER_FULL_VERSION
#  define BROWSER_FULL_VERSION	VER_NUM_STR
#endif
#ifndef BROWSER_UA_VERSION
#	define BROWSER_UA_VERSION		BROWSER_FULL_VERSION // core-1: softcore/ua.cpp
#endif
 
#ifdef FINAL_RELEASE // what it's defined to doesn't matter
// Skip BROWSER_SUBVER_LONG
#elif defined(BETA_RELEASE) // if given a value, must be string: gcc '-DBETA_RELEASE="1"'
# define BROWSER_SUBVER_LONG	"Beta " BETA_RELEASE
#elif defined(PREVIEW_RELEASE) // similar, gcc '-DPREVIEW_RELEASE="1"'
# define BROWSER_SUBVER_LONG	"Preview " PREVIEW_RELEASE
#elif defined(ALPHA_RELEASE) // ignores any value
# define BROWSER_SUBVER_LONG	"Alpha"
#endif // c.f. quick/desktopapplication.cpp's DesktopApplication::registerOperaWindow().

#define VER_BUILD_NUMBER_STR	BROWSER_BUILD_NUMBER // for compatibility with MSWIN
#define VER_BUILD_NUMBER		BROWSER_BUILD_NUMBER_INT // for compatibility with MSWIN
#define BROWSER_BUILD_NUMBER_L	UNI_L(BROWSER_BUILD_NUMBER)

// In the window caption:
#define BROWSER_VERSION_L			UNI_L(BROWSER_VERSION)
#define BROWSER_TITLE_POST_STRING_L	BROWSER_VERSION_L // core-1: prefsmanager/fragments/pm_ui.cpp

/* Name of preferences files. This was previously defined inside the
 * prefs module. */
#ifndef PREFERENCES_FILENAME
# define OLD_PREFERENCES_FILENAME UNI_L("opera.ini")
  // we've traditionally used the old names for SDKs
# define PREFERENCES_FILENAME	UNI_L("opera6.ini")
# define GLOBAL_PREFERENCES	UNI_L("opera6rc")
# define FIXED_PREFERENCES		UNI_L("opera6rc.fixed")
#endif

#endif // _CONFIG_H_
