/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef WIDGET_PREFS_H
#define WIDGET_PREFS_H

#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#ifdef DOM_GEOLOCATION_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"
#endif // DOM_GEOLOCATION_SUPPORT

class OpWidget;

/** @brief Functions to make setting / getting of prefs related to widgets easier
  */
namespace WidgetPrefs
{
	/** Retrieve an integer preference and set its value on a widget
	  * @param widget Widget to set the preference on
	  * @param preference Preference to retrieve
	  * @param host Retrieve preference override for this host, or 0 for generic preference
	  */
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionApp::integerpref preference);
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionCore::integerpref preference, const uni_char *host = 0);
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionDisplay::integerpref preference, const uni_char *host = 0);
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionDoc::integerpref preference, const uni_char *host = 0);
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionJS::integerpref preference, const uni_char *host = 0);
#ifdef M2_SUPPORT
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionM2::integerpref preference) ;
#endif //M2_SUPPORT
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionNetwork::integerpref preference, const uni_char *host = 0);
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionUI::integerpref preference, const uni_char *host = 0);
#ifdef _PRINT_SUPPORT_
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionPrint::integerpref preference, const uni_char *host = 0);
#endif // _PRINT_SUPPORT_

	/** Retrieve a string preference and set its value on a widget
	  * @param widget Widget to set the preference on
	  * @param preference Preference to retrieve
	  * @param host Retrieve preference override for this host, or 0 for generic preference
	  */
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionApp::stringpref preference);
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionCore::stringpref preference, const uni_char *host = 0);
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionJS::stringpref preference, const uni_char *host = 0);
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionUI::stringpref preference, const uni_char *host = 0);
#ifdef _PRINT_SUPPORT_
	OP_STATUS GetStringPref(OpWidget* widget, PrefsCollectionPrint::stringpref preference, const uni_char *host = 0);
#endif // _PRINT_SUPPORT_

	/** Set an integer preference based on the value of a widget
	  * @param widget Widget to retrieve value from
	  * @param preference Preference to set
	  * @param host Set preference for this specific host, or 0 for generic preference
	  */
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionApp::integerpref preference);
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionCore::integerpref preference, const uni_char *host = 0);
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionDisplay::integerpref preference, const uni_char *host = 0);
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionDoc::integerpref preference, const uni_char *host = 0);
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionJS::integerpref preference, const uni_char *host = 0);
#ifdef M2_SUPPORT
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionM2::integerpref preference);
#endif //M2_SUPPORT
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionNetwork::integerpref preference, const uni_char *host = 0);
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionUI::integerpref preference, const uni_char *host = 0);
#ifdef _PRINT_SUPPORT_
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionPrint::integerpref preference, const uni_char *host = 0);
#endif // _PRINT_SUPPORT_

	/** Set a string preference based on the value of a widget
	  * @param widget Widget to retrieve value from
	  * @param preference Preference to set
	  * @param host Set preference for this specific host, or 0 for generic preference
	  */
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionApp::stringpref preference);
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionCore::stringpref preference, const uni_char *host = 0);
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionJS::stringpref preference, const uni_char *host = 0);
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionUI::stringpref preference, const uni_char *host = 0);
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionNetwork::stringpref preference, const uni_char *host = 0);
#ifdef _PRINT_SUPPORT_
	OP_STATUS SetStringPref(OpWidget* widget, PrefsCollectionPrint::stringpref preference, const uni_char *host = 0);
#endif // _PRINT_SUPPORT_
#ifdef DOM_GEOLOCATION_SUPPORT
	OP_STATUS GetIntegerPref(OpWidget* widget, PrefsCollectionGeolocation::integerpref preference);
	OP_STATUS SetIntegerPref(OpWidget* widget, PrefsCollectionGeolocation::integerpref preference);
#endif // DOM_GEOLOCATION_SUPPORT
};

#endif // WIDGET_PREFS_H
