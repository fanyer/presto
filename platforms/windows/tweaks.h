/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef  WINDOWS_TWEAKS_H
#define  WINDOWS_TWEAKS_H

#include "adjunct/quick/quick-tweaks.h"
#include "platforms/windows/system.h"

#undef   TWEAK_AUTH_NTLM_SUPPORT
#define  TWEAK_AUTH_NTLM_SUPPORT                                           YES

#undef   TWEAK_AUTOUPDATE_PACKAGE_INSTALLATION
#define  TWEAK_AUTOUPDATE_PACKAGE_INSTALLATION                             YES

#undef   TWEAK_DISPLAY_DELAYED_STYLE_MANAGER_INIT
#define  TWEAK_DISPLAY_DELAYED_STYLE_MANAGER_INIT                          YES

#undef   TWEAK_DISPLAY_PER_GLYPH_SHADOW_LIMIT
#define  TWEAK_DISPLAY_PER_GLYPH_SHADOW_LIMIT                              YES
#define  GLYPH_SHADOW_MAX_RADIUS                                           0

#undef   TWEAK_GEOLOCATION_DEFAULT_POLL_INTERVAL
#define  TWEAK_GEOLOCATION_DEFAULT_POLL_INTERVAL                           YES
#define  GEOLOCATION_DEFAULT_POLL_INTERVAL                                 (5*60*1000)	// every 5 minutes

#undef   TWEAK_LIBGOGI_USE_MMAP
#define  TWEAK_LIBGOGI_USE_MMAP                                            YES

#undef   TWEAK_LOC_EXTRASECTION_NAME
#define  TWEAK_LOC_EXTRASECTION_NAME                                       YES
#define  LOCALE_EXTRASECTION                                               L"Windows"

#undef   TWEAK_MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
#define  TWEAK_MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS                         YES

#if defined(_DEBUG) && !defined(DESKTOP_STARTER) && !defined(OPERA_MAPI) && !defined(PLUGIN_WRAPPER)
# undef   TWEAK_MEMORY_ASSERT_ON_ERROR
# define  TWEAK_MEMORY_ASSERT_ON_ERROR                                     NO // too talkative

// Enable the use of "electric fence" protection on freed memory to detect write-after-free cases.
// You will also need to provide a comma separated class list with the -fence argument to Opera.
// Once you get it right all use-after-free bugs should result in immediate crashes.
# undef   TWEAK_MEMORY_ELECTRIC_FENCE
# define  TWEAK_MEMORY_ELECTRIC_FENCE                                      YES
  
# undef   TWEAK_MEMORY_LOG_USAGE_PER_ALLOCATION_SITE
# define  TWEAK_MEMORY_LOG_USAGE_PER_ALLOCATION_SITE                       YES

# undef   TWEAK_MEMORY_MMAP_DEFAULT_MAX_UNUSED
# define  TWEAK_MEMORY_MMAP_DEFAULT_MAX_UNUSED                             NO
# undef   MEMORY_MMAP_DEFAULT_MAX_UNUSED

# undef   TWEAK_MEMORY_USE_CODELOC
# define  TWEAK_MEMORY_USE_CODELOC                                         YES
#endif   // _DEBUG

#undef   TWEAK_M2_MAPI_SUPPORT
#define  TWEAK_M2_MAPI_SUPPORT                                             YES

#undef   TWEAK_MEMORY_SMALL_EXEC_SEGMENTS
#define  TWEAK_MEMORY_SMALL_EXEC_SEGMENTS                                  YES

#undef   TWEAK_NS4P_SILVERLIGHT_WORKAROUND
#define  TWEAK_NS4P_SILVERLIGHT_WORKAROUND                                 YES

#undef   TWEAK_NS4P_WMP_STOP_STREAMING
#define  TWEAK_NS4P_WMP_STOP_STREAMING                                     YES

#ifndef SIXTY_FOUR_BIT
#undef   TWEAK_NS4P_COMPONENT_PLUGINS
#define  TWEAK_NS4P_COMPONENT_PLUGINS                                      NO

#undef   TWEAK_NS4P_DYNAMIC_PLUGINS
#define  TWEAK_NS4P_DYNAMIC_PLUGINS                                        YES
#endif // !SIXTY_FOUR_BIT

#undef   TWEAK_PF_BUILDNUMSUFFIX
#define  TWEAK_PF_BUILDNUMSUFFIX                                           YES
#define  BUILDNUMSUFFIX                                                    "Windows"

#undef   TWEAK_PREFS_DEFAULT_PREFERRED_RENDERER
#define  TWEAK_PREFS_DEFAULT_PREFERRED_RENDERER                            YES
#define  DEFAULT_PREFERRED_RENDERER                                        1

#undef   TWEAK_PREFS_FORCE_CD
#define  TWEAK_PREFS_FORCE_CD                                              YES

#undef   TWEAK_PREFS_SMOOTH_SCROLL
#define  TWEAK_PREFS_SMOOTH_SCROLL                                         YES
#define  DEFAULT_SMOOTH_SCROLLING                                          TRUE

#undef   TWEAK_QUICK_TOOLKIT_TOOLTIP_WINDOW_STYLE
#define  TWEAK_QUICK_TOOLKIT_TOOLTIP_WINDOW_STYLE                          YES
#undef   TOOLTIP_WINDOW_STYLE
#define  TOOLTIP_WINDOW_STYLE                                              OpWindow::STYLE_NOTIFIER

#undef   TWEAK_SVG_FIX_BROKEN_PAINTER_ALPHA
#define  TWEAK_SVG_FIX_BROKEN_PAINTER_ALPHA                                NO // see bug no #262307

#undef   TWEAK_UTIL_CONVERT_SLASHES_TO_BACKSLASHES
#define  TWEAK_UTIL_CONVERT_SLASHES_TO_BACKSLASHES                         YES

#undef   TWEAK_VEGA_BACKENDS_API_ENTRY
#define  TWEAK_VEGA_BACKENDS_API_ENTRY                                     YES
#define  VEGA_GL_API_ENTRY                                                 __stdcall

#undef   TWEAK_VEGA_BACKENDS_D2D_INTEROPERABILITY
#define  TWEAK_VEGA_BACKENDS_D2D_INTEROPERABILITY                          YES

#undef   TWEAK_VEGA_BACKENDS_DYNAMIC_LIBRARY_LOADING
#define  TWEAK_VEGA_BACKENDS_DYNAMIC_LIBRARY_LOADING                       YES

#undef   TWEAK_VEGA_MDEFONT_SUPPORT
#define  TWEAK_VEGA_MDEFONT_SUPPORT                                        NO

#undef   TWEAK_VEGA_NATIVE_FONT_SUPPORT
#define  TWEAK_VEGA_NATIVE_FONT_SUPPORT                                    YES

#undef   TWEAK_VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
#define  TWEAK_VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND                 YES

#undef   TWEAK_WIDGETS_TRIPPLE_CLICK_ONE_LINE
#define  TWEAK_WIDGETS_TRIPPLE_CLICK_ONE_LINE                              NO

#define  TWEAK_WINDOWS_CAMERA_GRAPH_TEST                                   NO

#undef TWEAK_AUTOUPDATECHECKER_PLATFORM_HEADER
#define TWEAK_AUTOUPDATECHECKER_PLATFORM_HEADER	                           YES
#define OAUC_PLATFORM_INCLUDES "adjunct/autoupdate/autoupdate_checker/platforms/windows/includes/platform.h"

// This include MUST be last in the file
#include "adjunct/quick/custombuild-tweaks.h"

#endif   // WINDOWS_TWEAKS_H
