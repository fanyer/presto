/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef  QUICK_TWEAKS_H
#define  QUICK_TWEAKS_H

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Tweaks for the Quick module
//
//  Please redefine any platform specific tweaks in the tweaks.h in your platform
//
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "modules/hardcore/tweaks/profile_tweaks_desktop.h"


/******************************************
 ** Tweaks with no predefined Desktop value
 ******************************************/
#define  TWEAK_ABOUT_NPAPI_SRC_CODE_URL                                    NO
#define  TWEAK_ALL_PLUGINS_ARE_WINDOWLESS                                  NO
#define  TWEAK_DEBUG_EXTERNAL_MACROS                                       NO
#define  TWEAK_DISPLAY_SORTED_CODEPAGE_PREFERENCE                          YES
#define  TWEAK_DISPLAY_STRIP_ZERO_WIDTH_CHARS                              NO
#define  TWEAK_DISPLAY_SUPPORT_BROKEN_ALPHA_PAINTER                        NO
#define  TWEAK_DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES                        NO
#define  TWEAK_DISPLAY_VSCROLL_ALWAYS_ON                                   NO
#define  TWEAK_DOCHAND_ASK_BEFORE_SUBMIT_TO_MAILTO                         YES
#define  TWEAK_DOCHAND_CLEAR_RAMCACHE_ON_STARTPROGRESS                     NO
#define  TWEAK_DOCHAND_SKIP_REFRESH_ON_BACK                                YES
#define  TWEAK_DOCHAND_SKIP_REFRESH_SECONDS                                NO
#define  TWEAK_DOCHAND_USE_AFILEDIALOG                                     YES
#define  TWEAK_DOCHAND_USE_ONDOWNLOADREQUEST                               NO
#define  TWEAK_DOCHAND_HISTORY_SAVE_ZOOM_LEVEL                             YES
#define  TWEAK_DOCUMENTEDIT_SPATIAL_FRAMES_FOCUS                           NO
#define  TWEAK_ESUTILS_TIMESLICE_LENGTH                                    NO
#define  TWEAK_GSTREAMER_SRC_CODE_URL                                      YES
#define  GSTREAMER_SRC_CODE_URL                                            "http://sourcecode.opera.com/gstreamer/"
#define  TWEAK_HC_AGGRESSIVE_ECMASCRIPT_GC                                 NO
#define  TWEAK_HC_DEBUG_MESSAGEHANDLER                                     NO
#define  TWEAK_HC_INIT_BLACKLIST                                           NO
#define  TWEAK_HISTORY_WRITE_TIMEOUT_PERIOD                                NO
#define  TWEAK_HUNSPELL_SRC_CODE_URL                                       YES
#define  HUNSPELL_SRC_CODE_URL                                             "http://redir.opera.com/hunspell/"
#define  TWEAK_LAYOUT_SSR_MAX_WIDTH                                        NO
#define  TWEAK_LEA_MALLOC_PLATFORM_CONFIG                                  NO
#define  TWEAK_LIBGOGI_ARGB32_FORMAT                                       NO
#define  TWEAK_LIBGOGI_BGR24_FORMAT                                        NO
#define  TWEAK_LIBGOGI_BILINEAR_BLITSTRETCH                                NO
#define  TWEAK_LIBGOGI_BREW_FORMATS                                        NO
#define  TWEAK_LIBGOGI_INTERNAL_MEMCPY_SCROLL                              NO
#define  TWEAK_LIBGOGI_MAX_OPBITMAP_SIZE                                   NO
#define  TWEAK_LIBGOGI_MBGR16_FORMAT                                       NO
#define  TWEAK_LIBGOGI_RGB16_FORMAT                                        NO
#define  TWEAK_LIBGOGI_RGBA16_FORMAT                                       NO
#define  TWEAK_LIBGOGI_RGBA24_FORMAT                                       NO
#define  TWEAK_LIBGOGI_RGBA32_FORMAT                                       NO
#define  TWEAK_LIBGOGI_SPLIT_OPBITMAP_SIZE                                 NO
#define  TWEAK_LIBGOGI_SRGB16_FORMAT                                       NO
#define  TWEAK_LOC_EXTRASECTION                                            YES
#define  TWEAK_LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD                      YES
#define  LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD                            250000
#define  TWEAK_LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD_THUMBNAIL            YES
#define  LOGDOC_BIG_IMAGES_ASYNC_LOAD_THRESHOLD_THUMBNAIL                  3200000
#define  TWEAK_MEDIA_BACKEND_GSTREAMER_MAX_THREADS                         NO
#define  TWEAK_MEMORY_ALIGNMENT                                            NO
#define  TWEAK_MEMORY_BLOCKSIZE_BITS                                       NO
#define  TWEAK_MEMORY_DEAD_BYTES_QUEUE                                     NO
#define  TWEAK_MEMORY_DEAD_BYTES_RECYCLE                                   NO
#define  TWEAK_MEMORY_ELO_HIGH_THRESHOLD                                   NO
#define  TWEAK_MEMORY_EXTERNAL_DECLARATIONS                                NO
#define  TWEAK_MEMORY_IMPLEMENT_ARRAY_ELEAVE                               YES
#define  TWEAK_MEMORY_IMPLEMENT_GLOBAL_DELETE                              YES
#define  TWEAK_MEMORY_IMPLEMENT_GLOBAL_NEW                                 YES
#define  TWEAK_MEMORY_IMPLEMENT_NEW_ELEAVE                                 YES
#define  TWEAK_MEMORY_IMPLEMENT_OP_NEW                                     YES
#define  TWEAK_MEMORY_INLINED_ARRAY_ELEAVE                                 YES
#define  TWEAK_MEMORY_INLINED_GLOBAL_DELETE                                YES
#define  TWEAK_MEMORY_INLINED_GLOBAL_NEW                                   YES
#define  TWEAK_MEMORY_INLINED_NEW_ELEAVE                                   YES
#define  TWEAK_MEMORY_INLINED_OP_NEW                                       YES
#define  TWEAK_MEMORY_INLINED_PLACEMENT_NEW                                YES
#define  MEMORY_INLINED_PLACEMENT_NEW                                      0
#define  TWEAK_MEMORY_KEEP_ALLOCSITE                                       YES
#define  TWEAK_MEMORY_KEEP_FREESITE                                        YES
#define  TWEAK_MEMORY_KEEP_REALLOCSITE                                     YES
#define  TWEAK_MEMORY_LOG_ALLOCATIONS                                      NO
#define  TWEAK_MEMORY_MAX_POOLS                                            NO
#define  TWEAK_MEMORY_NAMESPACE_OP_NEW                                     NO
#define  TWEAK_MEMORY_PLATFORM_HAS_ELEAVE                                  NO
#define  TWEAK_MEMORY_REGULAR_ARRAY_ELEAVE                                 YES
#define  TWEAK_MEMORY_REGULAR_GLOBAL_DELETE                                YES
#define  TWEAK_MEMORY_REGULAR_GLOBAL_NEW                                   YES
#define  TWEAK_MEMORY_REGULAR_NEW_ELEAVE                                   YES
#define  TWEAK_MEMORY_REGULAR_OP_NEW                                       YES
#define  TWEAK_MEMORY_USE_GLOBAL_VARIABLES                                 YES
#define  MEMORY_USE_GLOBAL_VARIABLES                                       1
#define  TWEAK_MEMORY_USE_LOCKING                                          YES
#if defined HAVE_LTH_MALLOC // add other HAVE_*_MALLOC here if they implement new and delete
  // Use of LTH_MALLOC with the memory module is discouraged and won't be supported in the future
# define  MEMORY_REGULAR_OP_NEW                                            0
# define  MEMORY_REGULAR_GLOBAL_NEW                                        1
# define  MEMORY_REGULAR_NEW_ELEAVE                                        1
# define  MEMORY_REGULAR_ARRAY_ELEAVE                                      1
# define  MEMORY_REGULAR_GLOBAL_DELETE                                     1
# define  MEMORY_IMPLEMENT_OP_NEW                                          0
# define  MEMORY_IMPLEMENT_GLOBAL_NEW                                      0
# define  MEMORY_IMPLEMENT_NEW_ELEAVE                                      0
# define  MEMORY_IMPLEMENT_ARRAY_ELEAVE                                    0
# define  MEMORY_IMPLEMENT_GLOBAL_DELETE                                   0
# define  MEMORY_INLINED_OP_NEW                                            1
# define  MEMORY_INLINED_GLOBAL_NEW                                        0
# define  MEMORY_INLINED_NEW_ELEAVE                                        0
# define  MEMORY_INLINED_ARRAY_ELEAVE                                      0
# define  MEMORY_INLINED_GLOBAL_DELETE                                     0
#elif defined _DEBUG
# define  MEMORY_REGULAR_OP_NEW                                            1
# define  MEMORY_REGULAR_GLOBAL_NEW                                        1
# define  MEMORY_REGULAR_NEW_ELEAVE                                        1
# define  MEMORY_REGULAR_ARRAY_ELEAVE                                      1
# define  MEMORY_REGULAR_GLOBAL_DELETE                                     1
# define  MEMORY_IMPLEMENT_OP_NEW                                          1
# define  MEMORY_IMPLEMENT_GLOBAL_NEW                                      1
# define  MEMORY_IMPLEMENT_NEW_ELEAVE                                      1
# define  MEMORY_IMPLEMENT_ARRAY_ELEAVE                                    1
# define  MEMORY_IMPLEMENT_GLOBAL_DELETE                                   1
# define  MEMORY_INLINED_OP_NEW                                            0
# define  MEMORY_INLINED_GLOBAL_NEW                                        0
# define  MEMORY_INLINED_NEW_ELEAVE                                        0
# define  MEMORY_INLINED_ARRAY_ELEAVE                                      0
# define  MEMORY_INLINED_GLOBAL_DELETE                                     0
#else
# define  MEMORY_REGULAR_OP_NEW                                            0
# define  MEMORY_REGULAR_GLOBAL_NEW                                        0
# define  MEMORY_REGULAR_NEW_ELEAVE                                        0
# define  MEMORY_REGULAR_ARRAY_ELEAVE                                      0
# define  MEMORY_REGULAR_GLOBAL_DELETE                                     0
# define  MEMORY_IMPLEMENT_OP_NEW                                          0
# define  MEMORY_IMPLEMENT_GLOBAL_NEW                                      0
# define  MEMORY_IMPLEMENT_NEW_ELEAVE                                      0
# define  MEMORY_IMPLEMENT_ARRAY_ELEAVE                                    0
# define  MEMORY_IMPLEMENT_GLOBAL_DELETE                                   0
# define  MEMORY_INLINED_OP_NEW                                            1
# define  MEMORY_INLINED_GLOBAL_NEW                                        1
# define  MEMORY_INLINED_NEW_ELEAVE                                        1
# define  MEMORY_INLINED_ARRAY_ELEAVE                                      1
# define  MEMORY_INLINED_GLOBAL_DELETE                                     1
#endif
#ifdef _DEBUG
# define  MEMORY_KEEP_ALLOCSITE                                            1
# define  MEMORY_KEEP_REALLOCSITE                                          1
# define  MEMORY_KEEP_FREESITE                                             1
#else
# define  MEMORY_KEEP_ALLOCSITE                                            0
# define  MEMORY_KEEP_REALLOCSITE                                          0
# define  MEMORY_KEEP_FREESITE                                             0
#endif
#ifdef _DEBUG
# define  TWEAK_MEMORY_DEAD_OBJECT_QUEUE                                   NO
# define  TWEAK_MEMORY_DEAD_OBJECT_RECYCLE                                 NO
#else
# define  TWEAK_MEMORY_DEAD_OBJECT_QUEUE                                   YES
# define  MEMORY_DEAD_OBJECT_QUEUE                                         0
# define  TWEAK_MEMORY_DEAD_OBJECT_RECYCLE                                 YES
# define  MEMORY_DEAD_OBJECT_RECYCLE                                       1
#endif
#define  TWEAK_PAN_KEYMASK                                                 NO
#define  TWEAK_PAN_START_THRESHOLD                                         NO
#define  TWEAK_PI_LINK_WITHOUT_OPENGL                                      YES
#define  TWEAK_PF_BUILDNUMSUFFIX                                           YES
#define  TWEAK_PF_RETURN_IMPLICIT_OBJECTS                                  YES
#define  TWEAK_REGEXP_CAPTURE_SEG_STORAGE_LOAD                             NO
#define  TWEAK_REGEXP_CAPTURE_SEG_STORAGE_SIZE                             NO
#define  TWEAK_REGEXP_FRAME_SEGMENT_SIZE                                   NO
#define  TWEAK_REGEXP_STORE_SEG_STORAGE_SIZE                               NO
#define  TWEAK_REGEXP_STRICT                                               NO
#define  TWEAK_REGEXP_SUBSET                                               NO
#define  TWEAK_SKIN_HIGHLIGHTED_ELEMENT                                    YES
#define  TWEAK_SKINNABLE_AREA_ELEMENT                                      NO
#define  TWEAK_STDLIB_INT_CAST_IS_ES262_COMPLIANT                          NO
#define  TWEAK_SVG_FIX_BROKEN_PAINTER_ALPHA                                YES
#define  TWEAK_TRUEZOOM_LAYOUT_IN_100_PERCENT                              NO
#ifdef   SIXTY_FOUR_BIT
# define  TWEAK_URL_USE_64BIT_URL_ID                                       YES
# define  TWEAK_URL_USE_64BIT_URL_CONTEXT_ID                               YES
#endif
#define  TWEAK_UTIL_CONVERT_SLASHES_TO_BACKSLASHES                         NO // set to YES in platform code if necessary
#define  TWEAK_UTIL_TIMECACHE                                              NO
#define  TWEAK_VEGA_GLYPH_CACHE_HASH_SIZE                                  NO
#define  TWEAK_VEGA_GLYPH_CACHE_INITIAL_SIZE                               NO
#define  TWEAK_WIDGETS_REMOVE_CARRIAGE_RETURN                              NO



/******************************************
 ** Tweaks with differing values
 ******************************************/
#undef   TWEAK_ABOUT_SPEEDDIAL_LINK
#define  TWEAK_ABOUT_SPEEDDIAL_LINK                                        YES

#undef   TWEAK_CACHE_CONTAINERS_ENTRIES
#define  TWEAK_CACHE_CONTAINERS_ENTRIES                                   YES
#undef   CACHE_CONTAINERS_ENTRIES
#define  CACHE_CONTAINERS_ENTRIES                                          12

#undef   TWEAK_CACHE_NUM_FILES_PER_DELETELIST_PASS
#define  TWEAK_CACHE_NUM_FILES_PER_DELETELIST_PASS                         YES
#define  CACHE_NUM_FILES_PER_DELETELIST_PASS                               4

#undef   TWEAK_DATABASE_QUERY_EXECUTION_TIMEOUT
#define  TWEAK_DATABASE_QUERY_EXECUTION_TIMEOUT                            YES
#undef   DATABASE_INTERNAL_QUERY_EXECUTION_TIMEOUT
#define  DATABASE_INTERNAL_QUERY_EXECUTION_TIMEOUT                         20000

#undef   TWEAK_DISPLAY_FONTCACHESIZE
#define  TWEAK_DISPLAY_FONTCACHESIZE                                       YES
#undef   FONTCACHESIZE
#define  FONTCACHESIZE                                                     60

#undef   TWEAK_DISPLAY_GLOW_COLOR_FACTOR
#define  TWEAK_DISPLAY_GLOW_COLOR_FACTOR                                   YES
#define  DISPLAY_GLOW_COLOR_FACTOR                                         60, 60, 140

#undef   TWEAK_DISPLAY_HOTCLICK
#define  TWEAK_DISPLAY_HOTCLICK                                            YES

#undef   TWEAK_DOC_SHIFTKEY_OPEN_IN_NEW_WINDOW
#define  TWEAK_DOC_SHIFTKEY_OPEN_IN_NEW_WINDOW                             YES
#define  SHIFTKEY_OPEN_IN_NEW_WINDOW                                       (SHIFTKEY_CTRL|SHIFTKEY_SHIFT)

#undef   TWEAK_DOM_FILE_REFRESH_LENGTH_LIMIT
#define  TWEAK_DOM_FILE_REFRESH_LENGTH_LIMIT                               YES
#define  DOM_FILE_REFRESH_LENGTH_LIMIT                                     50

#undef   TWEAK_ENC_LRU_SIZE
#define  TWEAK_ENC_LRU_SIZE                                                YES
#undef   ENCODINGS_LRU_SIZE
#define  ENCODINGS_LRU_SIZE                                                16

#undef   TWEAK_ENC_CHARSET_CACHE_SIZE
#define  TWEAK_ENC_CHARSET_CACHE_SIZE                                      YES
#undef   ENCODINGS_CHARSET_CACHE_SIZE
#define  ENCODINGS_CHARSET_CACHE_SIZE                                      2000

#undef   TWEAK_HC_MODULE_INIT_PROFILING
#define  TWEAK_HC_MODULE_INIT_PROFILING                                    YES
#define  PLATFORM_HC_PROFILE_INCLUDE                                       "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#define  HC_PROFILE_INIT_START(offset, module_name)                        { OP_PROFILE_METHOD(module_name);
#define  HC_PROFILE_INIT_STOP(offset, module_name)                         }
#define  HC_PROFILE_DESTROY_START(offset, module_name)                     { OP_PROFILE_METHOD(module_name);
#define  HC_PROFILE_DESTROY_STOP(offset, module_name)                      }

#undef   TWEAK_HC_PLATFORM_DEFINES_VERSION
#define  TWEAK_HC_PLATFORM_DEFINES_VERSION                                 YES

#undef   TWEAK_IMG_CACHE_MULTIPLE_ANIMATION_FRAMES
#define  TWEAK_IMG_CACHE_MULTIPLE_ANIMATION_FRAMES                         NO

#undef   TWEAK_LAYOUT_AUTO_ACTIVATE_HIDDEN_ON_DEMAND_PLACEHOLDERS
#define  TWEAK_LAYOUT_AUTO_ACTIVATE_HIDDEN_ON_DEMAND_PLACEHOLDERS          NO

#undef   TWEAK_LAYOUT_TEXT_WRAP_INCLUDE_FLOATS
#define  TWEAK_LAYOUT_TEXT_WRAP_INCLUDE_FLOATS                             NO

#undef   TWEAK_LAYOUT_TEXT_WRAP_SCAN_FOR_MENU_DELIMITER_CHARACTERS
#define  TWEAK_LAYOUT_TEXT_WRAP_SCAN_FOR_MENU_DELIMITER_CHARACTERS         NO

#undef   TWEAK_LIBGOGI_NO_OPBITMAP
#define  TWEAK_LIBGOGI_NO_OPBITMAP                                         YES

#undef   TWEAK_LIBGOGI_NO_OPPAINTER
#define  TWEAK_LIBGOGI_NO_OPPAINTER                                        YES

#undef   TWEAK_LIBGOGI_OPWINDOW_HAS_CHROME
#define  TWEAK_LIBGOGI_OPWINDOW_HAS_CHROME                                 YES

#undef   TWEAK_LIBGOGI_SUPPORT_CREATE_TILE
#define  TWEAK_LIBGOGI_SUPPORT_CREATE_TILE                                 NO

#undef   TWEAK_LIBGOGI_USE_ALPHA_LOOKUPTABLE
#define  TWEAK_LIBGOGI_USE_ALPHA_LOOKUPTABLE                               NO

#undef   TWEAK_LOC_EXTRASECTION_NAME
#define  TWEAK_LOC_EXTRASECTION_NAME                                       YES

#undef   TWEAK_LOGDOC_DONT_ANIM_OUT_VISUAL_VIEWPORT
#define  TWEAK_LOGDOC_DONT_ANIM_OUT_VISUAL_VIEWPORT                        YES

#undef   TWEAK_LOGDOC_HOTCLICK
#define  TWEAK_LOGDOC_HOTCLICK                                             YES

#undef   TWEAK_LOGDOC_PREALLOCATED_TEMP_BUFFERS
#define  TWEAK_LOGDOC_PREALLOCATED_TEMP_BUFFERS                            YES
#define  PREALLOC_BUFFER_SIZE                                              2048

#undef   TWEAK_MEDIA_BACKEND_GSTREAMER_BUNDLE_PLUGINS
#define  TWEAK_MEDIA_BACKEND_GSTREAMER_BUNDLE_PLUGINS                      YES

#undef   TWEAK_MEMORY_ASSERT_ON_ERROR
#define  TWEAK_MEMORY_ASSERT_ON_ERROR                                      YES

#undef   TWEAK_MIME_ALLOW_MULTIPART_CACHE_FILL
#define  TWEAK_MIME_ALLOW_MULTIPART_CACHE_FILL                             NO

#undef   TWEAK_NS4P_COMPONENT_PLUGINS
#define  TWEAK_NS4P_COMPONENT_PLUGINS                                      YES

#undef   TWEAK_NS4P_DYNAMIC_PLUGINS
#define  TWEAK_NS4P_DYNAMIC_PLUGINS                                        NO

#undef   TWEAK_NS4P_IGNORE_LOAD_ERROR
#define  TWEAK_NS4P_IGNORE_LOAD_ERROR                                      NO

#undef   TWEAK_NS4P_INVALIDATE_INTERVAL
#define  TWEAK_NS4P_INVALIDATE_INTERVAL                                    YES
#define  NS4P_INVALIDATE_INTERVAL                                          40

#undef   TWEAK_NS4P_TRY_CATCH_PLUGIN_CALL
#define  TWEAK_NS4P_TRY_CATCH_PLUGIN_CALL                                  NO

#undef   TWEAK_NS4P_WMP_STOP_STREAMING
#define  TWEAK_NS4P_WMP_STOP_STREAMING                                     NO

#undef   TWEAK_OBML_BRAND
#define  TWEAK_OBML_BRAND                                                  YES
#define  OBML_BRAND                                                        "evenes"

#undef   TWEAK_OBML_COMM_VERIFY_CONFIG_SIGNATURE
#define  TWEAK_OBML_COMM_VERIFY_CONFIG_SIGNATURE                           NO

#undef   TWEAK_PI_STATIC_GLOBAL_SCREEN_DPI
#define  TWEAK_PI_STATIC_GLOBAL_SCREEN_DPI                                 YES

#undef   TWEAK_POSIX_ASYNC_THREADS
#define  TWEAK_POSIX_ASYNC_THREADS                                         YES
#define  POSIX_ASYNC_THREADS                                               5

#undef   TWEAK_POSIX_FILE_REFLEX_LOCK
#define  TWEAK_POSIX_FILE_REFLEX_LOCK                                      YES

#undef   TWEAK_POSIX_LOWFILE_BASE
#define  TWEAK_POSIX_LOWFILE_BASE                                          YES
#define  PosixOpLowLevelFileBase                                           DesktopOpLowLevelFile

#undef   TWEAK_POSIX_LOWFILE_BASEINC
#define  TWEAK_POSIX_LOWFILE_BASEINC                                       YES
#define  POSIX_LOWFILE_BASEINC                                             "adjunct/desktop_pi/file/DesktopOpLowLevelFile.h"

#undef   TWEAK_POSIX_MAINTHREAD_OBJECTS
#define  TWEAK_POSIX_MAINTHREAD_OBJECTS                                    YES
#define  POSIX_MAINTHREAD_OBJECTS                                          64

#undef   TWEAK_POSIX_NEGATIVE_FILELENGTH
#define  TWEAK_POSIX_NEGATIVE_FILELENGTH                                   NO

#undef   TWEAK_POSIX_SYSINFO_BASE
#define  TWEAK_POSIX_SYSINFO_BASE                                          YES
#define  PosixOpSysInfoBase                                                DesktopOpSystemInfo

#undef   TWEAK_POSIX_SYSINFO_BASEINC
#define  TWEAK_POSIX_SYSINFO_BASEINC                                       YES
#define  POSIX_SYSINFO_BASEINC                                             "adjunct/desktop_pi/DesktopOpSystemInfo.h"

#undef   TWEAK_POSIX_THREADSAFE_MALLOC
#define  TWEAK_POSIX_THREADSAFE_MALLOC                                     YES

#undef   TWEAK_POSIX_USE_PROC_SELF_EXE
#define  TWEAK_POSIX_USE_PROC_SELF_EXE                                     NO

#undef   TWEAK_PREFS_ACCEPT_LICENSE
#define  TWEAK_PREFS_ACCEPT_LICENSE                                        NO // Shall be overridden by unix

#undef   TWEAK_PREFS_ADDRESS_SEARCH_DROP_DOWN_WIDTH
#define  TWEAK_PREFS_ADDRESS_SEARCH_DROP_DOWN_WIDTH                        YES
#define  DEFAULT_ADDRESS_SEARCH_DROP_DOWN_WIDTH                            -1 // this is in percent of the total width taken by address field + search field

#undef   TWEAK_PREFS_AUTORELOAD_TIMEOUT
#define  TWEAK_PREFS_AUTORELOAD_TIMEOUT                                    YES
#define  DEFAULT_AUTO_WINDOW_TIMEOUT                                       60

#undef   TWEAK_PREFS_BOOKMARKS_DESC_MAX_LENGTH
#define  TWEAK_PREFS_BOOKMARKS_DESC_MAX_LENGTH                             YES
#define  DEFAULT_BOOKMARKS_DESC_MAX_LENGTH                                 32767

#undef   TWEAK_PREFS_BOOKMARKS_FAVICON_FILE_MAX_LENGTH
#define  TWEAK_PREFS_BOOKMARKS_FAVICON_FILE_MAX_LENGTH                     YES
#define  DEFAULT_BOOKMARKS_FAVICON_FILE_MAX_LENGTH                         2048

#undef   TWEAK_PREFS_BOOKMARKS_THUMBNAIL_FILE_MAX_LENGTH
#define  TWEAK_PREFS_BOOKMARKS_THUMBNAIL_FILE_MAX_LENGTH                   YES
#define  DEFAULT_BOOKMARKS_THUMBNAIL_FILE_MAX_LENGTH                       2048

#undef   TWEAK_PREFS_BOOKMARKS_TITLE_MAX_LENGTH
#define  TWEAK_PREFS_BOOKMARKS_TITLE_MAX_LENGTH                            YES
#define  DEFAULT_BOOKMARKS_TITLE_MAX_LENGTH                                2048

#undef   TWEAK_PREFS_BOOKMARKS_URL_MAX_LENGTH
#define  TWEAK_PREFS_BOOKMARKS_URL_MAX_LENGTH                              YES
#define  DEFAULT_BOOKMARKS_URL_MAX_LENGTH                                  2147483647

#undef   TWEAK_PREFS_CHECK_CACHED_REDIRECT
#define  TWEAK_PREFS_CHECK_CACHED_REDIRECT                                 YES
#define  DEFAULT_CHECK_REDIRECT                                            FALSE

#undef   TWEAK_PREFS_CHECK_CACHED_REDIRECT_IMAGES
#define  TWEAK_PREFS_CHECK_CACHED_REDIRECT_IMAGES                          YES
#define  DEFAULT_CHECK_REDIRECT_CHANGED_IMAGES                             FALSE

#undef   TWEAK_PREFS_CONNECTIONS_SERVER
#define  TWEAK_PREFS_CONNECTIONS_SERVER                                    YES
#define  DEFAULT_MAX_CONNECTIONS_SERVER                                    16

#undef   TWEAK_PREFS_DEFAULT_HWACC_ENABLED
#define  TWEAK_PREFS_DEFAULT_HWACC_ENABLED                                 YES
#define  PREFS_DEFAULT_HWACC_ENABLED                                       0

#undef   TWEAK_PREFS_DEFAULT_THUMBNAILS_IN_WINDOWS_CYCLE
#define  TWEAK_PREFS_DEFAULT_THUMBNAILS_IN_WINDOWS_CYCLE                   YES
#define  DEFAULT_THUMBNAILS_IN_WINDOWS_CYCLE                               TRUE

#undef  TWEAK_PREFS_ENABLE_WEBGL
#define TWEAK_PREFS_ENABLE_WEBGL                                           YES
#define DEFAULT_ENABLE_WEBGL                                               0

#undef   TWEAK_PREFS_HOMEPAGE
#define  TWEAK_PREFS_HOMEPAGE                                              YES
#define  DF_HOMEPAGE                                                       UNI_L("http://redir.opera.com/portal/home/")

#undef   TWEAK_PREFS_HOSTNAME_AUTOCOMPLETE
#define  TWEAK_PREFS_HOSTNAME_AUTOCOMPLETE                                 YES
#define  DEFAULT_ENABLE_HOSTNAME_EXPANSION                                 TRUE

#undef   TWEAK_PREFS_IMGCACHE_ENABLE
#define  TWEAK_PREFS_IMGCACHE_ENABLE                                       YES
#define  DEFAULT_CACHE_RAM_FIGS                                            TRUE

#undef   TWEAK_PREFS_MAX_CONNECTION_SERVER
#define  TWEAK_PREFS_MAX_CONNECTION_SERVER                                 YES
#define  MAX_CONNECTIONS_SERVER                                            256

#undef   TWEAK_PREFS_MAX_CONNECTION_TOTAL
#define  TWEAK_PREFS_MAX_CONNECTION_TOTAL                                  YES
#define  MAX_CONNECTIONS_TOTAL                                             999

#undef   TWEAK_PREFS_MEDIA_CACHE_SIZE
#define  TWEAK_PREFS_MEDIA_CACHE_SIZE                                      YES
#define  DEFAULT_MEDIA_CACHE_SIZE                                          200000

#undef   TWEAK_PREFS_PERSISTENT_CONNECTIONS_SERVER
#define  TWEAK_PREFS_PERSISTENT_CONNECTIONS_SERVER                         YES
#define  DEFAULT_MAX_PERSISTENT_CONNECTIONS_SERVER                         6

#undef   TWEAK_PREFS_PRINT_BACKGROUND
#define  TWEAK_PREFS_PRINT_BACKGROUND                                      YES
#define  DEFAULT_PRINT_BACKGROUND                                          TRUE // Modern printers give better results with background

#undef   TWEAK_PREFS_PRINT_ZOOM
#define  TWEAK_PREFS_PRINT_ZOOM                                            YES
#define  DEFAULT_PRINT_SCALE                                               80 // Printers have higher resolution than a screen

#undef   TWEAK_PREFS_RAMCACHE_ENABLE
#define  TWEAK_PREFS_RAMCACHE_ENABLE                                       YES
#define  DEFAULT_CACHE_RAM_DOCS                                            TRUE

#undef   TWEAK_PREFS_OEMCACHE_TRUSTED
#define  TWEAK_PREFS_OEMCACHE_TRUSTED                                      YES
#undef   DEFAULT_OEM_TRUSTED_SERVERS
#define  DEFAULT_OEM_TRUSTED_SERVERS                                       UNI_L("help.opera.com;sitecheck2.opera.com")

#undef   TWEAK_PREFS_SELECT_MENU
#define  TWEAK_PREFS_SELECT_MENU                                           YES
#define  DEFAULT_AUTOSELECTMENU                                            FALSE

#undef   TWEAK_PREFS_WARN_INSECURE_FORM
#define  TWEAK_PREFS_WARN_INSECURE_FORM                                    YES
#define  DEFAULT_WARN_INSECURE_VALUE                                       FALSE

#undef   TWEAK_SEARCH_ENGINE_VPS_WRAPPER
#define  TWEAK_SEARCH_ENGINE_VPS_WRAPPER                                   YES

#undef   TWEAK_SKIN_NATIVE_ELEMENT
#define  TWEAK_SKIN_NATIVE_ELEMENT                                         YES

#undef   TWEAK_SVG_CACHE_BITMAPS
#define  TWEAK_SVG_CACHE_BITMAPS                                           YES
#define  SVG_CACHED_BITMAPS_MAX                                            32

#undef   TWEAK_SVG_CLIPRECT_TOLERANCE
#define  TWEAK_SVG_CLIPRECT_TOLERANCE                                      YES
#define  SVG_CLIPRECT_TOLERANCE                                            32

#undef   TWEAK_SYNC_CHECK_LAST_USED_DAYS
#define  TWEAK_SYNC_CHECK_LAST_USED_DAYS                                   YES
#define  SYNC_CHECK_LAST_USED_DAYS                                         0

#undef   TWEAK_SYNC_FREE_SYNCITEM_POOL_MAX_SIZE
#define  TWEAK_SYNC_FREE_SYNCITEM_POOL_MAX_SIZE                            YES
#define  SYNC_FREE_SYNCITEM_POOL_MAX_SIZE                                  1024

#undef   TWEAK_SYNC_MAX_SEND_ITEMS
#define  TWEAK_SYNC_MAX_SEND_ITEMS                                         YES
#define  SYNC_MAX_SEND_ITEMS                                               (1337 * 2)	// because we're twice as cool

#undef   TWEAK_SYNC_SPEED_DIAL_2
#define  TWEAK_SYNC_SPEED_DIAL_2                                           YES

#undef   TWEAK_THUMBNAILS_INTERNAL_BITMAP_SCALE
#define  TWEAK_THUMBNAILS_INTERNAL_BITMAP_SCALE                            YES

#undef   TWEAK_THUMBNAILS_MAX_CACHED_IMAGES
#define  TWEAK_THUMBNAILS_MAX_CACHED_IMAGES                                YES
#define  THUMBNAILS_MAX_CACHED_IMAGES                                      2048

#undef   TWEAK_THUMBNAILS_LOADING_TIMEOUT_SECONDS
#define  TWEAK_THUMBNAILS_LOADING_TIMEOUT_SECONDS                          YES // DSK-239372
#define  THUMBNAILS_LOADING_TIMEOUT_SECONDS                                120

#undef   TWEAK_THUMBNAILS_LOADING_TIMEOUT
#define  TWEAK_THUMBNAILS_LOADING_TIMEOUT                                  YES

#undef   TWEAK_THUMBNAILS_LOGO_TUNING_VIA_PREFS
#define  TWEAK_THUMBNAILS_LOGO_TUNING_VIA_PREFS                            YES

#undef   TWEAK_URL_DEBUG_DOC
#define  TWEAK_URL_DEBUG_DOC                                               NO

#undef   TWEAK_URL_DEBUG_DOC
#define  TWEAK_URL_DEBUG_DOC                                               NO

#undef   TWEAK_URL_ENABLE_OPERA_FEEDS_URL
#define  TWEAK_URL_ENABLE_OPERA_FEEDS_URL                                  NO

#undef   TWEAK_URL_UA_CUSTOM
#define  TWEAK_URL_UA_CUSTOM                                               YES

#undef   TWEAK_UTIL_CLEANUP_STACK
#define  TWEAK_UTIL_CLEANUP_STACK                                          YES
#define  UTIL_CLEANUP_STACK                                                g_cleanup_stack

#undef   TWEAK_UTIL_IPV6_DEPRECATED_SITE
#define  TWEAK_UTIL_IPV6_DEPRECATED_SITE                                   YES

#undef   TWEAK_UTIL_ZIP_WRITE
#define  TWEAK_UTIL_ZIP_WRITE                                              YES

#undef   TWEAK_VEGA_3D_HARDWARE_SUPPORT
#define  TWEAK_VEGA_3D_HARDWARE_SUPPORT                                    YES

#undef   TWEAK_VEGA_BACKENDS_BLOCKLIST_SHIPPED_FOLDER
#define  TWEAK_VEGA_BACKENDS_BLOCKLIST_SHIPPED_FOLDER                      YES
#define  VEGA_BACKENDS_BLOCKLIST_SHIPPED_FOLDER                            OPFILE_AUX_FOLDER

#undef   TWEAK_VEGA_BACKENDS_BLOCKLIST_FETCH
#define  TWEAK_VEGA_BACKENDS_BLOCKLIST_FETCH                               NO

#undef   TWEAK_VEGA_BACKENDS_DYNAMIC_LIBRARY_LOADING
#define  TWEAK_VEGA_BACKENDS_DYNAMIC_LIBRARY_LOADING                       NO

#undef   TWEAK_VEGA_GLYPH_CACHE_SIZE
#define  TWEAK_VEGA_GLYPH_CACHE_SIZE                                       NO
#undef   VEGA_GLYPH_CACHE_SIZE

#undef   TWEAK_VEGA_LINE_ALLOCATION_SIZE
#define  TWEAK_VEGA_LINE_ALLOCATION_SIZE                                   YES
#define  VEGA_LINE_ALLOCATION_SIZE                                         128

#undef   TWEAK_VEGA_INTERSECTION_ALLOCATION_SIZE
#define  TWEAK_VEGA_INTERSECTION_ALLOCATION_SIZE                           YES
#define  VEGA_INTERSECTION_ALLOCATION_SIZE                                 128

#undef   TWEAK_VEGA_POSTSCRIPT_PRINTING
#define  TWEAK_VEGA_POSTSCRIPT_PRINTING                                    NO

#undef   TWEAK_VEGA_SUBPIXEL_FONT_BLENDING
#define  TWEAK_VEGA_SUBPIXEL_FONT_BLENDING                                 YES

#undef   TWEAK_VEGA_SUBPIXEL_FONT_OVERWRITE_ALPHA
#define  TWEAK_VEGA_SUBPIXEL_FONT_OVERWRITE_ALPHA                          YES

#undef   TWEAK_VEGA_USE_ASM
#define  TWEAK_VEGA_USE_ASM                                                NO

#undef   TWEAK_WBMULTIPART_MIXED_SUPPORT
#define  TWEAK_WBMULTIPART_MIXED_SUPPORT                                   NO

#undef   TWEAK_WEB_RDV_PROXY_HOST
#define  TWEAK_WEB_RDV_PROXY_HOST                                          YES
#define  WEB_RDV_PROXY_HOST                                                UNI_L("operaunite.com")

#undef   TWEAK_WEBFEEDS_AUTO_DISPLAY
#define  TWEAK_WEBFEEDS_AUTO_DISPLAY                                       YES

#undef   TWEAK_WEBFEEDS_AUTOSAVE_FEEDS
#define  TWEAK_WEBFEEDS_AUTOSAVE_FEEDS                                     NO

#undef   TWEAK_WIDGETS_AUTOCOMPLETION_GOOGLE
#define  TWEAK_WIDGETS_AUTOCOMPLETION_GOOGLE                               YES

#undef   TWEAK_WIDGETS_HEURISTIC_LANG_DETECTION
#define  TWEAK_WIDGETS_HEURISTIC_LANG_DETECTION                            YES

#undef   TWEAK_WIDGETS_IME_CANDIDATE_HIGHLIGHT_COLOR
#define  TWEAK_WIDGETS_IME_CANDIDATE_HIGHLIGHT_COLOR                       YES
#define  WIDGETS_IME_CANDIDATE_HIGHLIGHT_COLOR                             OP_RGB(0,0,0)

#undef   TWEAK_WIDGETS_USE_NATIVECOLORSELECTOR
#define  TWEAK_WIDGETS_USE_NATIVECOLORSELECTOR                             YES

#undef   TWEAK_WINDOWCOMMANDER_MAX_FAVICON_SIZE
#define  TWEAK_WINDOWCOMMANDER_MAX_FAVICON_SIZE                            YES
#define  WIC_MAX_FAVICON_SIZE                                              16




/******************************************
 ** Tweaks with conditional differences
 ******************************************/
#ifdef   SIXTY_FOUR_BIT
# undef   TWEAK_ABOUT_PLUGINS_SHOW_ARCHITECTURE
# define  TWEAK_ABOUT_PLUGINS_SHOW_ARCHITECTURE                            YES
#endif

#ifndef   SUPPORT_SYNC_URLFILTER
# undef   TWEAK_CF_SYNC_CONTENT_FILTERS
# define  TWEAK_CF_SYNC_CONTENT_FILTERS                                    NO
#endif

#ifdef   _DEBUG
# undef   TWEAK_ES_HARDCORE_GC_MODE
# define  TWEAK_ES_HARDCORE_GC_MODE                                        YES
#endif

#ifndef   SUPPORT_SYNC_TYPED_HISTORY
# undef   TWEAK_HISTORY_SYNC_TYPED_HISTORY
# define  TWEAK_HISTORY_SYNC_TYPED_HISTORY                                 NO
#endif

#ifdef   _DEBUG
# undef   TWEAK_LIBGOGI_DEBUG_INFO
# define  TWEAK_LIBGOGI_DEBUG_INFO                                         YES
#endif

#ifdef _DEBUG
# undef   TWEAK_MEMORY_CALLSTACK_DATABASE
# define  TWEAK_MEMORY_CALLSTACK_DATABASE                                  YES
#endif

#ifndef _DEBUG
# undef   MEMORY_KEEP_ALLOCSTACK
# define  MEMORY_KEEP_ALLOCSTACK                                           0

# undef   MEMORY_KEEP_FREESTACK
# define  MEMORY_KEEP_FREESTACK                                            0

# undef   MEMORY_KEEP_REALLOCSTACK
# define  MEMORY_KEEP_REALLOCSTACK                                         0

# undef   TWEAK_MEMORY_USE_CODELOC
# define  TWEAK_MEMORY_USE_CODELOC                                         NO
#endif

#ifdef   _DEBUG
# undef   TWEAK_POSIX_STACK_GUARD_PAGE
# define  TWEAK_POSIX_STACK_GUARD_PAGE                                     YES
#endif

#ifdef   WIN_CE
# undef   TWEAK_PREFS_ALLOW_JS_HIDE_URL
# define  TWEAK_PREFS_ALLOW_JS_HIDE_URL                                    NO
# undef   DEFAULT_POPUP_ALLOW_HIDE_URL
#endif

#ifdef   MANUAL_CONTENT_MAGIC
# undef   TWEAK_PREFS_ERA_CONTENT_MAGIC
# define  TWEAK_PREFS_ERA_CONTENT_MAGIC                                    YES
# undef   PREFS_ERA_CONTENT_MAGIC
# define  PREFS_ERA_CONTENT_MAGIC                                          2
#endif

#ifdef   _DEBUG
# undef   TWEAK_SYNC_SEND_DEBUG_ELEMENT
# define  TWEAK_SYNC_SEND_DEBUG_ELEMENT                                    YES
#endif

#ifndef   UPNP_SUPPORT
# undef   TWEAK_UPNP_SERVICE_DISCOVERY
# define  TWEAK_UPNP_SERVICE_DISCOVERY                                     NO
#endif


/******************************************
 ** TODO: Tweaks where we assume wrong default values
 ******************************************/

// DOC_DOCEDIT_KEEP_HISTORY_DISTANCE is 3 (should be 10)
#undef   TWEAK_DOC_DOCEDIT_KEEP_HISTORY_DISTANCE
#define  TWEAK_DOC_DOCEDIT_KEEP_HISTORY_DISTANCE                           NO
#undef   DOC_DOCEDIT_KEEP_HISTORY_DISTANCE

// TEXT_WRAP_DONT_WRAP_FONT_SIZE is 24 (should be 0)
#undef   TWEAK_LAYOUT_TEXT_WRAP_DONT_WRAP_FONT_SIZE_LARGER_THAN
#define  TWEAK_LAYOUT_TEXT_WRAP_DONT_WRAP_FONT_SIZE_LARGER_THAN            NO
#undef   TEXT_WRAP_DONT_WRAP_FONT_SIZE

// MEMORY_EXTERNAL_GUARD_SIZE is 8 (should be 16)
#undef   TWEAK_MEMORY_EXTERNAL_GUARD_SIZE
#define  TWEAK_MEMORY_EXTERNAL_GUARD_SIZE                                  NO
#undef   MEMORY_EXTERNAL_GUARD_SIZE

// MEMORY_FRONT_GUARD_SIZE is 16 (should be 32)
#undef   TWEAK_MEMORY_FRONT_GUARD_SIZE
#define  TWEAK_MEMORY_FRONT_GUARD_SIZE                                     NO
#undef   MEMORY_FRONT_GUARD_SIZE

// MEMORY_REAR_GUARD_SIZE is 24 (should be 48)
#undef   TWEAK_MEMORY_REAR_GUARD_SIZE
#define  TWEAK_MEMORY_REAR_GUARD_SIZE                                      NO
#undef   MEMORY_REAR_GUARD_SIZE

// MAX_CACHE_RAM_FIGS_SIZE is 10000 (should be 100000)
#undef   TWEAK_PREFS_IMGCACHE_MAX_SIZE
#define  TWEAK_PREFS_IMGCACHE_MAX_SIZE                                     NO
#undef   MAX_CACHE_RAM_FIGS_SIZE

// MIN_CACHE_RAM_FIGS_SIZE is 200 (should be 2000)
#undef   TWEAK_PREFS_IMGCACHE_MIN_SIZE
#define  TWEAK_PREFS_IMGCACHE_MIN_SIZE                                     NO
#undef   MIN_CACHE_RAM_FIGS_SIZE

// MAX_CACHE_RAM_SIZE is 30000 (should be 300000)
#undef   TWEAK_PREFS_RAMCACHE_MAX_SIZE
#define  TWEAK_PREFS_RAMCACHE_MAX_SIZE                                     NO
#undef   MAX_CACHE_RAM_SIZE

// MIN_CACHE_RAM_SIZE is 200 (should be 2000)
#undef   TWEAK_PREFS_RAMCACHE_MIN_SIZE
#define  TWEAK_PREFS_RAMCACHE_MIN_SIZE                                     NO
#undef   MIN_CACHE_RAM_SIZE

// DEFAULT_MULTIMEDIA_STREAM_SIZE is 1024 (should be 4096)
#undef   TWEAK_PREFS_MULTIMEDIA_STREAM_SIZE
#define  TWEAK_PREFS_MULTIMEDIA_STREAM_SIZE                                NO
#undef   DEFAULT_MULTIMEDIA_STREAM_SIZE

// DEFAULT_COMPONENTINUASTRING is TRUE (should be FALSE)
#undef   TWEAK_PREFS_UASTRING_COMPONENTS
#define  TWEAK_PREFS_UASTRING_COMPONENTS                                   NO
#undef   DEFAULT_COMPONENTINUASTRING



/******************************************
 ** TODO: Defines that are neither tweaks nor features
 ******************************************/
// Non-tweakable defines required for proper VegaOpPainter support
#define  MDE_CREATE_IME_MANAGER
#define  MDE_DONT_CREATE_VIEW

//See: https://cgit.oslo.osa/cgi-bin/cgit.cgi/desktop/work.git/commit/?id=0410111321a6f377b157a45e3e4e11bdcc586f51
#ifdef   _UNIX_DESKTOP_
# define  DEFAULT_PAGEBAR_OPEN_URL_ON_MIDDLE_CLICK                         TRUE
#else
# define  DEFAULT_PAGEBAR_OPEN_URL_ON_MIDDLE_CLICK                         FALSE
#endif   // _UNIX_DESKTOP_

//See: https://cgit.oslo.osa/cgi-bin/cgit.cgi/desktop/work.git/commit/?id=1602f10b6492048be41099748a2fe7552869c90a
#define  FEATURE_SHOW_DISCOVERED_DEVICES                                   YES
#define  SHOW_DISCOVERED_DEVICES_SUPPORT


#endif   // QUICK_TWEAKS_H
