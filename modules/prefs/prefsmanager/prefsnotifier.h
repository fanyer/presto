/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#ifndef PREFSNOTIFIER_H
#define PREFSNOTIFIER_H

// Determine if we need the object

#ifdef DYNAMIC_PROXY_UPDATE
# define PREFS_NOTIFIER
#elif defined PREFS_HAVE_REREAD_FONTS
# define PREFS_NOTIFIER
#endif

#ifdef PREFS_NOTIFIER
/** Adapt to changes in the environment. The preferences notifier is
  * used when core or non-core code wants to signal to the preferences
  * code that a platform default setting has been changed, to allow
  * the preferences framework to dynamically adapt itself to the
  * changed environment.
  *
  * PrefsNotifier is responsible for calling the relevant internal
  * prefernces APIs to adapt to the changed settings, these should not
  * be called directly by non-core code.
  */
class PrefsNotifier
{
public:
#ifdef DYNAMIC_PROXY_UPDATE
	/** Signal changes in regular proxy settings. The preferences code
	  * will read the new proxy settings through the regular porting
	  * interfaces. */
	static void OnProxyChangedL();
#endif

#ifdef PREFS_HAVE_REREAD_FONTS
	/** Signal changes in system font settings. The preferences code
	  * will read the new font settings through the regular porting
	  * interfaces. */
	static void OnFontChangedL();
#endif

#ifdef PREFS_HAVE_NOTIFIER_ON_COLOR_CHANGED
	/** Signal changes in system color settings. The preferences code
	 * will read the new color settings through the regular porting
	 * interfaces. */
	static void OnColorChangedL();
#endif
};
#endif // PREFS_NOTIFIER

#endif
