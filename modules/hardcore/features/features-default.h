/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_HARDCORE_FEATURES_FEATURES_DEFAULT_H
#define MODULES_HARDCORE_FEATURES_FEATURES_DEFAULT_H

// Define											Responsible

#define __IDNA__SUPPORT								// yngve
#define IMG_DALI_2									// kilsmo
#define REPORT_MEMMAN								// kilsmo
#define _COMM_SUPPORT_								// yngve
#define _USERDEF_TITLE_								// peter
#define URL_DETECT_PASSWORD_COOKIES					// yngve
#define HAVE_LANGUAGEMANAGER						// peter
#define _SQUEEZE_LINE_HEIGHT_						// geir
#define WINDOW_COMMANDER							// rikard
#define WINDOW_COMMANDER_7							// rikard
#define _ALLOW_TRUSTED_EXTERNAL_PROTOCOLS_			// ?
#define IMAGE_DECODES_TO_PLATFORM_ENDIAN			// kilsmo
#define SYNCRONIZED_ANIMATIONS						// emil
#define FRAME_STACKING								// geir
#define NEW_OPFILE                                  // markus
#define SUPPORT_ABSOLUTE_TEXTPATH                   // lth
#define USE_UNICODE_NS                              // ph
#define INI_KEYS_ASCII								// axel
#define HAVE_DISK									// peter
#define OPMEMFILE_SUPPORT							// andre
#define ZIPFILE_DIRECT_ACCESS_SUPPORT				// andre

#ifndef UNICODE
#define UNICODE										// peter
#endif // UNICODE

// Capabilities: for each module M that is part of the default checkout
// for this product (regardless of whether it is enabled or not by features),
// define M_MODULE below and comment it properly.  These capabilities can
// be used to cope with code that moves as part of the modularization work.

// In alphabetical order, please

#define AUTOPROXY_MODULE							// lth -- we have modules/autoproxy/
#define CONSOLE_MODULE								// peter
#define DATASTREAM_MODULE							// yngve -- we have modules/datastream/, not modules/util/fl*
#define DEBUG_MODULE								// markus -- we have modules/debug/, not dbug/
#define DISPLAY_MODULE								// rikard
#define DOC_MODULE									// geir/stighal/karlo -- we have modules/doc/, not doc/
#define DOCHAND_MODULE								// jens
#define DOM_MODULE									// jl -- we have modules/dom/, not doc/dom/
#define ECMASCRIPT_MODULE							// lth -- we have modules/ecmascript/, not lang/ecmascript2/
#define ECMASCRIPT_UTILS_MODULE                     // jl -- we have modules/ecmascript_utils, not core/es*
#define ENCODINGS_MODULE							// peter -- we have modules/encodings/, not doc/i18n/
#define EXPAT_MODULE								// kilsmo -- we have modules/expat/, not lib/expat/
#define FORMS_MODULE								// danielb
#define HARDCORE_MODULE								// kilsmo -- we have modules/hardcore/
#define IMG_MODULE									// kilsmo -- we have modules/img, not img2/
#define JSPLUGINS_MODULE							// jl/rikard -- we have modules/jsplugins/
#define LAYOUT_MODULE								// karlo
#define LIBJPEG_MODULE								// kilsmo -- we have modules/libjpeg/, not lib/jpeg/
#define LIBOPEAY_MODULE								// yngve -- we have modules/libopeay/ not lib/opeay095
#define LIBSSL_MODULE								// yngve -- we have modules/libssl/ not lib/ssl8
#define LOCALE_MODULE                               // peter -- we have modules/locale, not locale/
#define LOGDOC_MODULE								// stighal
#define MINPNG_MODULE								// ph/kilsmo -- we have modules/minpng/
#define NS4PLUGINS_MODULE							// hela -- we have modules/ns4plugins/, not plugins/
#define OLDDEBUG_MODULE								// yngve -- we have modules/olddebug/
#define OPPLUGIN_MODULE								// kilsmo -- we have modules/opplugin/
#define PI_MODULE									// rikard -- we have modules/pi/, not Porting_Interfaces/
#define PREFS_MODULE								// peter -- we have modules/prefs/, not prefs/ (well, both, actually)
#define PREFS2_MODULE                               // peter -- we have *only* modules/prefs (almost ;-) , not prefs/
#define PREFSFILE_MODULE							// peter -- we have modules/prefsfile/, not modules/prefs/storage/
#define REGKEY_MODULE                               // rikard -- we have modules/regkey/, not regkey/
#define SELFTEST_MODULE								// ph -- we have modules/selftest/, not selftest/
#define SKIN_MODULE                                 // rikard -- we have modules/skin/, not skin/
#define SPATIAL_NAVIGATION_MODULE					// h2 -- we have modules/spatial_navigation/, not core/spatial_navigation/
#define STYLE_MODULE								// rune -- we have modules/style/, not modules/doc/style/
#define SVG_MODULE									// ed
#define URL_MODULE									// yngve -- we have modules/url/, not url2/
#define UTIL_MODULE                                 // lth -- we have modules/util/, not util/
#define VOICE_MODULE                                // rikard -- we have modules/voice/, not xmlplugins/
#define WAND_MODULE                                 // emil -- we have modules/wand/, not wand/
#define WINDOWCOMMANDER_MODULE						// rikard -- we have modules/windowcommander/, not core/ui_interaction/
#define WIDGETS_MODULE                              // emil -- we have modules/widgets/, not widgets/
#define XMLPARSER_MODULE                            // jl
#define XMLUTILS_MODULE                             // jl
#define XPATH_MODULE                                // jl
#define XSLT_MODULE                                 // jl
#define ZLIB_MODULE									// kilsmo -- we have modules/zlib/, not lib/zlib/

#define HARDCORE_SYSTEM

// User agent defines which really don't belong here.
#define UA_MOZILLA_SPOOF_VERSION UNI_L("5.0")
#define UA_DEFAULT_MOZILLA_SPOOF_VERSION UNI_L("4.78")
#define UA_DEFAULT_MSIE_SPOOF_VERSION UNI_L("6.0")
#define UA_MOZILLA_SPOOF_VERSION_3 UNI_L("3.0")

#define UA_MAX_OPERA_VERSIONS 1
#define UA_MAX_MOZILLA_VERSIONS 2
#define UA_MAX_MSIE_VERSION 1

#endif // !MODULES_HARDCORE_FEATURES_FEATURES_DEFAULT_H
