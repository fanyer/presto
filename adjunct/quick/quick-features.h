/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.
* It may not be distributed under any circumstances.
*/
#ifndef  QUICK_FEATURES_H
#define  QUICK_FEATURES_H

/***********************************************************************************
**
**	Feature set for Quick versions
**
***********************************************************************************/

#include "modules/hardcore/features/profile_desktop.h"


/**
 ** Third-party Features
 **/
#define  FEATURE_3P_ALADDIN_MD5                                            NO
#define  FEATURE_3P_ANDROID_OPEN_SOURCE_CODE                               NO
#define  FEATURE_3P_BISON                                                  YES
#define  FEATURE_3P_BITSTREAM_VERA                                         NO
#define  FEATURE_3P_DROID_FONTS                                            NO
#define  FEATURE_3P_DTOA_DAVID_M_GAY                                       NO
#define  FEATURE_3P_DTOA_FLOITSCH_DOUBLE_CONVERSION                        YES
#define  FEATURE_3P_FREETYPE                                               NO
#define  FEATURE_3P_GLIBC                                                  NO
#define  FEATURE_3P_GOOGLE_ANALYTICS_SDK_FOR_ANDROID                       NO
#define  FEATURE_3P_GROWL                                                  NO
#define  FEATURE_3P_GSTREAMER                                              YES
#define  FEATURE_3P_HKSCS                                                  YES
#define  FEATURE_3P_HUNSPELL                                               YES
#define  FEATURE_3P_IANA                                                   YES
#define  FEATURE_3P_IBM_JNI_HEADERS                                        YES
#define  FEATURE_3P_ICU_DATA                                               YES
#define  FEATURE_3P_ITYPE_ENGINE                                           NO
#define  FEATURE_3P_KDDI                                                   NO
#define  FEATURE_3P_LCMS                                                   YES
#define  FEATURE_3P_LEA_MALLOC                                             NO
#define  FEATURE_3P_LIBVPX                                                 YES
#define  FEATURE_3P_LIBYAML                                                YES
#define  FEATURE_3P_MATHLIB                                                NO
#define  FEATURE_3P_MATRIX_SSL                                             NO
#define  FEATURE_3P_MOZTW                                                  YES
#define  FEATURE_3P_NS4_PLUGIN_API                                         YES
#define  FEATURE_3P_NTT_DOCOMO                                             NO
#define  FEATURE_3P_OPENGL_REGISTRY                                        YES
#define  FEATURE_3P_OPENSSL                                                YES
#define  FEATURE_3P_OPENTYPE                                               YES
#define  FEATURE_3P_PIKE_UNIVERSE                                          NO
#define  FEATURE_3P_PRINTF_SCANF                                           YES
#define  FEATURE_3P_RANDOM                                                 NO
#define  FEATURE_3P_REICHL_SHA1                                            NO
#define  FEATURE_3P_SDL                                                    NO
#define  FEATURE_3P_SQLITE                                                 YES
#define  FEATURE_3P_SSL_FALSE_START                                        NO
#define  FEATURE_3P_SUN_JNI_HEADERS                                        YES
#define  FEATURE_3P_UNICODE                                                YES
#define  FEATURE_3P_WEBP                                                   YES
#define  FEATURE_3P_XIPH                                                   YES
#define  FEATURE_3P_ZLIB                                                   YES


/**
 ** Features with no predefined Desktop value
 **/

#define  FEATURE_DOUBLEBUFFERING                                           YES
#define  FEATURE_INTERNAL_TEXTSHAPER                                       NO
#define  FEATURE_MEMORY_DEBUGGING                                          NO
#define  FEATURE_MEMORY_OOM_EMULATION                                      NO


/**
 ** Features with differing values
 **/
#undef   FEATURE_ASYNC_FILE_OPS
#define  FEATURE_ASYNC_FILE_OPS                                            NO // 2008-10-20 there is currently no code using this

#undef   FEATURE_BOOKMARKS_URL
#define  FEATURE_BOOKMARKS_URL                                             NO

#undef   FEATURE_DOM_EXTENSIONS_CONTEXT_MENU_API
#define  FEATURE_DOM_EXTENSIONS_CONTEXT_MENU_API                           YES

#undef   FEATURE_DOM_STREAM_API
#define  FEATURE_DOM_STREAM_API                                            YES

#undef   FEATURE_EMBEDDED_ICC_PROFILES
#define  FEATURE_EMBEDDED_ICC_PROFILES                                     YES

#undef   FEATURE_KEYBOARD_SELECTION
#define  FEATURE_KEYBOARD_SELECTION                                        YES

#undef   FEATURE_OPERAEXTENSIONS_URL
#define  FEATURE_OPERAEXTENSIONS_URL                                       NO

#undef   FEATURE_OPERAUNITE_URL
#define  FEATURE_OPERAUNITE_URL                                            NO

#undef   FEATURE_PICTOGRAM
#define  FEATURE_PICTOGRAM                                                 NO

#undef   FEATURE_SEARCH_ENGINES
#define  FEATURE_SEARCH_ENGINES                                            NO

#undef   FEATURE_SIGNED_GADGETS
#define  FEATURE_SIGNED_GADGETS                                            NO

#undef   FEATURE_SMILEY
#define  FEATURE_SMILEY                                                    YES

#undef   FEATURE_SYNCHRONOUS_DNS
#define  FEATURE_SYNCHRONOUS_DNS                                           YES

#undef   FEATURE_WEBFEEDS_SAVED_STORE
#define  FEATURE_WEBFEEDS_SAVED_STORE                                      YES

#undef   FEATURE_XML_AIT_SUPPORT
#define  FEATURE_XML_AIT_SUPPORT                                           NO // CORE-31257


/**
 ** Features with conditional differences
 **/
#ifdef   _DEBUG
# undef   FEATURE_SETUP_INFO
# define  FEATURE_SETUP_INFO                                               YES
#endif


/***********************************************************************************
**
**	Quick/Desktop defines
**
***********************************************************************************/

#define  QUICK
#define  M2_SUPPORT

// Disabling IRC support will take out most of the chat code.
// If actually removing the feature, search harder for chat-related code: There may well be some mostly self-contained utility functions or classes that should be purged as well.
// If adding new chat functionality, remember to ifdef it, or some platforms may not compile.
// This is only for the feature scanner. The actual feature is controlled by TWEAK_M2_CHAT_SUPPORT
#define  FEATURE_IRC                                                       YES

#define  QUICK_COOKIE_EDITOR_SUPPORT
#define  _DIRECT_URL_WINDOW_
#define  _QUICK_UI_FONT_SUPPORT_
#define  NEED_URL_MIME_DECODE_LISTENERS

#define  ACCESSIBILITY_EXTENSION_SUPPORT

// We want threads (arjanl)
#define  THREAD_SUPPORT

// For getting type conversion functions in OpAutoPtr - will become an API or tweak later (arjanl)
#define  HAVE_CXX_MEMBER_TEMPLATES


#define  _KIOSK_MANAGER_
#define  _BITTORRENT_SUPPORT_

#define  SUPPORT_VISUAL_ADBLOCK                                            // visual editing for adblock

#define  SUPPORT_SPEED_DIAL

// relative to the util module's OpTypedObject.h
#define  PRODUCT_OPFOLDER_FILE                                             "adjunct/quick/opfolder.inc"	// Contains OpFileFolder enum extras for quick (arjanl)
#define  PRODUCT_OPTYPEDOBJECT_WIDGET_FILE                                 "adjunct/quick/widget-types.inc"	// contains widget types
#define  PRODUCT_OPTYPEDOBJECT_DIALOG_FILE                                 "adjunct/quick/dialog-types.inc"	// contains dialog types
#define  PRODUCT_OPTYPEDOBJECT_WINDOW_FILE                                 "adjunct/quick/window-types.inc"	// contains window types
#define  PRODUCT_OPTYPEDOBJECT_PANEL_FILE                                  "adjunct/quick/panel-types.inc"   // Contains panel types
#define  PRODUCT_OPTYPEDOBJECT_DRAG_FILE                                   "adjunct/quick/drag-types.inc"    // Contains drag types

// #define FEATURE_SCROLL_MARKER

/***********************************************************************************
**
**	Function defines needed for Quick profiling, override per platform if needed
**
***********************************************************************************/

#define  PROFILE_OPERA_PUSH(x)
#define  PROFILE_OPERA_POP()
#define  PROFILE_OPERA_POP_THEN_PUSH(x)
#define  PROFILE_OPERA_PREPARE()

/***********************************************************************************
**
**	Other defines needed by Quick
**
***********************************************************************************/

#define  _SSL_SEED_FROMMESSAGE_LOOP_
#define  ENABLE_LAX_SECURITY_AND_GADGET_USER_JS                            0	// false, but FALSE may be an enum member, not suitable for #if
#define  SEARCH_ENGINE_FOR_MAIL
#define  REDIRECT_ON_ERROR                                                 // So we can please NetPia, see handling of PrefsCollectionNetwork::HostNameWebLookupAddress
#define  ENABLE_USAGE_REPORT

#define  DESKTOP_UTIL_SEARCH_ENGINES                                       // Define to use the old desktop_util version of searchmanager.
// This is current but will be deprecated ASAP to use the new core version in softcore
// Removing support for various toolbars will take out the functionality, but may not remove all relevant UI, including the view menu and the appearance dialog.
#define  SUPPORT_START_BAR                                                 // start bar is enabled
#define  SUPPORT_NAVIGATION_BAR                                            // navigation bar is enabled
#define  SUPPORT_MAIN_BAR
#define  UPLOAD2_OPSTRING16_SUPPORT                                        // Needed for m2

// Sync/Link defines while developing
#define  SUPPORT_SYNC_SPEED_DIAL                                           // Sync speed dials
#define  SUPPORT_SYNC_NOTES
#define  SUPPORT_SYNC_SEARCHES
#define  SUPPORT_SYNC_TYPED_HISTORY
#define  SUPPORT_SYNC_URLFILTER

#define  TEST_SYNC_NOW                                                     // Special test define to allow the action ACTION_SYNC_NOW

#define  SUPPORT_SYNC_MERGING

#define  DEVTOOLS_INTEGRATED_WINDOW                                        // Set to use the Dev Tools windows connected to the main window

#define  AUTO_UPDATE_SUPPORT                                               // Set to use the new AutoUpdater

#define  PERMANENT_HOMEPAGE_SUPPORT                                        // So we can give T-Online and others special builds where the users can't change the homepage.

#define  SCOPE_DESKTOP_SUPPORT                                             // Turns on Desktop scope for Watir testing code

#define  USE_COMMON_RESOURCES                                              // Add to use the new common resource code (adamm)
#define  FEATURE_TAB_THUMBNAILS                                            YES		// tab thumbnails

#if      FEATURE_TAB_THUMBNAILS == YES
# define  PAGEBAR_THUMBNAILS_SUPPORT
# define  TAB_THUMBNAILS_ON_SIDES
#endif   // FEATURE_TAB_THUMBNAILS

#define  SPEEDDIAL_SVG_SUPPORT                                             // enable SVG in speed dial
#define  DECORATED_GADGET_SUPPORT                                          // enable window decorations for gadgets

#define  FEATURE_WIDGET_RUNTIME                                            YES // Use widgets as standalone applications
#if      FEATURE_WIDGET_RUNTIME == YES
# define  WIDGET_RUNTIME_SUPPORT
#endif   // FEATURE_WIDGET_RUNTIME

#define  FEATURE_WIDGET_CONTROL_BUTTONS                                    YES // Global control buttons for widgets
#if      FEATURE_WIDGET_CONTROL_BUTTONS == YES
# define  WIDGET_CONTROL_BUTTONS_SUPPORT
#endif   // FEATURE_WIDGET_CONTROL_BUTTONS

// Automatic installation of missing plugins, DSK-312517

#define  FEATURE_GADGETS_AUTOUPDATE                                        YES
#if      FEATURE_GADGETS_AUTOUPDATE == YES
# define  GADGET_UPDATE_SUPPORT                                            // Set to enable gadgets update support, WIDGET_RUNTIME dependent
  //#define WIDGETS_UPDATE_SUPPORT					// Set to enable widgets update support, WIDGET_RUNTIME dependent
#endif   // FEATURE_GADGETS_AUTOUPDATE

#undef FEATURE_TOUCH_EVENTS 
#define FEATURE_TOUCH_EVENTS NO

#undef FEATURE_3P_XML_PARSER
#define FEATURE_3P_XML_PARSER YES

#undef FEATURE_3P_HTTP_STACK
#define FEATURE_3P_HTTP_STACK YES

// CORE-24999

// Defines that disable parts of search protection in Debug builds:
// 1) disable protection of default search and speed dial search
//#define SEARCH_PROTECTION_DISABLE_PROTECTION
// 2) disable verification of package search.ini files
//#define SEARCH_PROTECTION_DISABLE_SIGNATURES

// DSK-357686
#define  BUILD_NUMBER                                                      VER_BUILD_NUMBER

#endif   // !QUICK_FEATURES_H
