/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_HARDCORE_HARDCORE_CAPABILITIES_H
#define MODULES_HARDCORE_HARDCORE_CAPABILITIES_H

#define HC_CAP_MH_HAS_DOCMAN

#define HC_CAP_MH_IS_TWOWAY_POINTER

#define HC_CAP_NEWUNISTRLIB

#define HC_CAP_HAS_CHAR_CLASS

#define HC_CAP_GLOBAL_OPERA_OBJECT

#define HC_CAP_BUFLWRUPR	///< uni_buflwr() and uni_bufupr()

#define HC_CAP_DISK_FEATURES ///< FEATURE_DISK & friends

#define HC_CAP_MODULE_FEATURE_FILES ///< Post processing of features in the modules url, datastream, libssl, libopeay, libopenpgp and olddebug
#define HC_CAP_DEFINES_OP_MALLOC ///< Defines op_malloc, op_free, op_calloc and op_realloc

#define HC_CAP_PREFS_READ ///< FEATURE_PREFS_READ

#define HC_CAP_FEWER_OP_MACROS // Removed op_isalnum, op_isprint, op_isdigit from the System

#define HC_CAP_SYSTEM_INCLUDES // System include files (<stdio.h>) and std-functions (strlen) are not allowed.
#define NO_SYSTEM_INCLUDES

#define HC_CAP_VIEWERS	// FEATURE_DYNAMIC_VIEWERS

#define HC_CAP_NEW_TIMER_MESSAGES ///< has MSG_ANIMATE_IMAGE and MSG_VISDEV_UPDATE

#define HC_CAP_OPMEMCHR // Defines op_memchr through the system.

#define HC_CAP_FEATURE_CSS_COUNTER // Has FEATURE_CSS_COUNTER

#define HC_CAP_SYSTEM_INCLUDES_REMOVED_FROM_SYSTEM // SYSTEM_LOCALE_H, SYSTEM_STRINGS_H, SYSTEM_STDINT_H etc removed

#define HC_CAP_OPKEY_SYSTEM // opkeys are generated from a script so that they can easily be set per product

#define HC_CAP_DEF_LANGFILE // OperaInitInfo has def_lang_file_folder and def_lang_file_name

#define HC_CAP_STATIC_OPERA_OBJECT // FEATURE_STATIC_OPERA_OBJECT exists

#define HC_CAP_HAS_MSG_UPDATE_WINDOW_TITLE // has MSG_UPDATE_WINDOW_TITLE

#define HC_CAP_PROGRESS_URL // MessageHandlerList::SetProgressInformation has a URL parameter

#define HC_CAP_PAGE_INFO // Has FEATURE_PAGE_INFO

#define HC_CAP_MSG_DOC_DELAYED_ACTION // MSG_DOC_DELAYED_ACTION exists

#define HC_CAP_OPERA_RUN // Opera object has Run() function.

#define HC_CAP_STDLIB // Introduces the stdlib module

#define HC_CAP_TWEAKS // TWEAKS system

#define HC_CAP_OPERA_RUNSLICE // Opera has RequestRunSlice().

#define HC_CAP_FEATURE_DOM2_EVENTS_API_DEPRECATED // FEATURE_DOM2_EVENTS_API has been deprecated

#define HC_CAP_ACTION_SYSTEM // we have an action system

#define HC_CAP_NO_UNICODE_FUNCTIONS // Moved uni_-functions to stdlib

#define HC_CAP_ORIGINAL_WINDOW_ENABLED // Report parent window to SSL engine

#define HC_CAP_ARRAY_SIZE // Has ARRAY_SIZE macro.

#define HC_CAP_SVG_BASIC_FEATURES // FEATURE_SVG_{GRADIENTS,OPACITY,PATTERNS,ELLIPTICAL_ARCS,STENCIL,FILTERS}, and FEATURE_LOG_ERRORS

#define HC_CAP_NO_UNI_COMPAT_FUNCTIONS // uni_fopen, uni_unlink etc removed

#define HC_CAP_MESSAGE_TYPE // Use enum OpMessage for messages

#define HC_CAP_LIBJPEG_DEPRECATED // FEATURE_LIBJPEG is deprecated.

#define HC_CAP_API_SYSTEM // The API system exists.

#define HC_CAP_ADJUNCT_DEPRECATED // FEATURE_ADJUNCT deprecated

#define HC_CAP_RUNSLICE_API // API_HC_OPERA_RUNSLICE exists

#define HC_CAP_TOLOWER_TOUPPER_DEPRECATED // SYSTEM_TOLOWER and SYSTEM_TOUPPER are deprecated

#define HC_CAP_REGEXP_DEPRECATED // FEATURE_REGEXP is deprecated

#define HC_CAP_SVG_HAS_EMBEDDED_FONTS_FEATURE // FEATURE_SVG_EMBEDDED_SYSTEM_FONTS exists

#define HC_CAP_YIELD // has OpStatus::ERR_YIELD

#define HC_CAP_SYSTEM_64BIT // has SYSTEM_64BIT

#define HC_CAP_FEATURE_DYNAMIC_PROXIES // Has FEATURE_DYNAMIC_PROXIES

#define HC_CAP_SETUP_INFO // Has FEATURE_SETUP_INFO

#define HC_CAP_URL_FILTER	// Has FEATURE_URL_FILTER

#define HC_CAP_FAILED_MODULE_INDEX // Has Opera::failed_module_index

#define HC_CAP_DEFINES_OP_KEY_MACROS // Defines the various OP_KEY_* related macros, and ShiftKeyState and the SHIFTKEY_* macros, if PI_CAP_NO_IS_OP_KEY_MACROS is defined

#define HC_CAP_SMALL_HANDLE_MESSAGE_FUNCTION // Various URL and SSL messages are not handled by us.

#define HC_CAP_SVG_PROFILES // Defines SVG profile features

#define HC_CAP_DEPRECATE_SETCALLBACK_PRIORITY // MessageHandler's SetCallBack{List,} don't want a priority arg

#define HC_CAP_USE_MEMORY_MODULE // This release of hardcore is designed to work with the memory module

#define HC_CAP_DEPRECATED_LEA_MALLOC_MONOLITHIC // FEATURE_LEA_MALLOC_MONOLITHIC is deprecated

#define HC_CAP_OPERA_RUN_PROCESSED_MESSAGES_ARGUMENT // Opera::Run() supports the optional processed_messages argument.

#define HC_CAP_PERIODIC_TASK // Implements periodic tasks that only run when the browser is active.

#define HC_CAP_OP_GET_A_VALUE // OP_GET_x_VALUE exists (x = R, G, B and A)

#define HC_CAP_RGB_HAS_ALPHA // OP_RGB adds an alpha component (set to 1.0) if STYLE_CAP_RGBA_COLORS is also defined

#define HC_CAP_TYPED_MEM_MAN // MEM_MAN knows to use FramesDocument pointers rather than raw Document pointers.

#define HC_CAP_FEATURE_PLUGIN_EXT_API_DEPRECATED // FEATURE_PLUGIN_EXT_API is mandatory and now deprecated.

#define HC_CAP_HAS_DO_KEY_RANGE // has the OpKey range for WML DO elements

#define HC_CAP_SHARED_USERJS_DEFINE // Both browser.js and user.js enables USER_JAVASCRIPT

#define HC_CAP_ENABLE_DISABLE_PERIODIC // Periodic tasks can be enabled and disabled

#endif // !MODULES_HARDCORE_HARDCORE_CAPABILITIES_H
