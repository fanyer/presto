/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.
* It may not be distributed under any circumstances.
*/

#ifndef  UNIX_TWEAKS_H
#define  UNIX_TWEAKS_H

#include "adjunct/quick/quick-tweaks.h"

#if defined (__GLIBC__) && defined (__GLIBC_MINOR__)
# ifndef  __GLIBC_PREREQ
#  define  __GLIBC_PREREQ(maj, min) \
    ((__GLIBC__ << 16) + __GLIBC_MINOR__ >= ((maj) << 16) + (min))
# endif
#endif




#ifdef   __FreeBSD__ // FreeBSD always allows different architectures for plugins (Linux and FreeBSD)
# undef   TWEAK_ABOUT_PLUGINS_SHOW_ARCHITECTURE
# define  TWEAK_ABOUT_PLUGINS_SHOW_ARCHITECTURE                            YES
#endif

#undef   TWEAK_DISPLAY_SCROLLWHEEL_STEPSIZE
#define  TWEAK_DISPLAY_SCROLLWHEEL_STEPSIZE                                YES
#define  DISPLAY_SCROLLWHEEL_STEPSIZE                                      27

#ifdef   HAVE_DL_MALLOC
# define  TWEAK_LEA_MALLOC_LOCK                                            YES

# define  TWEAK_LEA_MALLOC_REENTRANT                                       NO
#endif

#undef   TWEAK_LIBGOGI_USE_MMAP
#define  TWEAK_LIBGOGI_USE_MMAP                                            YES

#undef   TWEAK_LOC_EXTRASECTION_NAME
#define  TWEAK_LOC_EXTRASECTION_NAME                                       YES
#define  LOCALE_EXTRASECTION                                               UNI_L("Unix")

#undef   TWEAK_MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
#define  TWEAK_MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND                YES

#undef   TWEAK_MDEFONT_APPLY_KERNING
#define  TWEAK_MDEFONT_APPLY_KERNING                                       YES

#define  TWEAK_MDEFONT_FREETYPE_FONTENGINE                                 YES

#define  TWEAK_MDEFONT_ITYPE_FONTENGINE                                    NO

#define  TWEAK_MDEFONT_LIMITED_FONT_SIZE_SET                               NO

#undef   TWEAK_NS4P_UNIX_PLATFORM
#define  TWEAK_NS4P_UNIX_PLATFORM                                          YES

/* TWEAK_PF_BUILDNUMSUFFIX
 * The BUILDNUMSUFFIX must match the first component of the -p flag passed to
 * makelang.pl; this is LNGSYSTEM in the unix-build make files.  Normally this
 * coincides with SYSTEM; uname -s is massaged lightly (see NATIVE in
 * unix-build/prelude.mk).
 */
#ifdef __linux__
# define  BUILDNUMSUFFIX                                                 "Linux"
#elif defined __FreeBSD__
# define  BUILDNUMSUFFIX                                                 "FreeBSD"
#elif defined __NetBSD__
# define  BUILDNUMSUFFIX                                                 "NetBSD"
#else
# error "Need to set BUILDNUMSUFFIX appropriate to the platform."
# define  BUILDNUMSUFFIX                                                 "Unix"
#endif

#ifdef __FreeBSD__ // allegedly lacks fdatasync
# undef   TWEAK_POSIX_FDATASYNC
# define  TWEAK_POSIX_FDATASYNC                                            YES
# define  PosixFDataSync                                                   fsync // an adequate, if slow, substitute for fdatasync
#endif

#ifndef __linux__
# undef   TWEAK_POSIX_GHBN_2R7
# define  TWEAK_POSIX_GHBN_2R7                                             NO

# undef   TWEAK_POSIX_DNS_GETADDRINFO
# define  TWEAK_POSIX_DNS_GETADDRINFO                                      YES

# undef   TWEAK_POSIX_DNS_GETHOSTBYNAME
# define  TWEAK_POSIX_DNS_GETHOSTBYNAME                                    NO
#endif

#ifndef __linux__
# undef   TWEAK_POSIX_HAS_LANGINFO_1STDAY
# define  TWEAK_POSIX_HAS_LANGINFO_1STDAY                                  NO
#endif

#ifndef __linux__
# undef   TWEAK_POSIX_USE_IFCONF_IOCTL
# define  TWEAK_POSIX_USE_IFCONF_IOCTL                                     NO

# undef   TWEAK_POSIX_USE_IFREQ_IOCTL
# define  TWEAK_POSIX_USE_IFREQ_IOCTL                                      NO

# undef   TWEAK_POSIX_SELECTOR_EPOLL
# define  TWEAK_POSIX_SELECTOR_EPOLL                                       NO

# undef   TWEAK_POSIX_SELECTOR_SELECT_FALLBACK
# define  TWEAK_POSIX_SELECTOR_SELECT_FALLBACK                             NO
#endif

#if defined(__FreeBSD__)
# undef   TWEAK_POSIX_SELECTOR_KQUEUE
# define  TWEAK_POSIX_SELECTOR_KQUEUE                                      YES
#endif

#undef   TWEAK_POSIX_WINSYS
#define  TWEAK_POSIX_WINSYS                                                YES
#define  POSIX_WINSYS_PREFIX                                               "X11; "

#undef   TWEAK_PREFS_ACCEPT_LICENSE
#define  TWEAK_PREFS_ACCEPT_LICENSE                                        YES
#define  DEFAULT_ACCEPT_LICENSE                                            0

#undef   TWEAK_QUICK_TOOLKIT_TOOLTIP_WINDOW_STYLE
#define  TWEAK_QUICK_TOOLKIT_TOOLTIP_WINDOW_STYLE                          YES
#undef   TOOLTIP_WINDOW_STYLE
#define  TOOLTIP_WINDOW_STYLE                                              OpWindow::STYLE_TOOLTIP

#undef   TWEAK_SKIN_DEFAULT_SKIN_FILE
#define  TWEAK_SKIN_DEFAULT_SKIN_FILE                                      YES
#define  DEFAULT_SKIN_FILE                                                 UNI_L("unix_skin.zip")

#undef   TWEAK_VEGA_POSTSCRIPT_PRINTING
#define  TWEAK_VEGA_POSTSCRIPT_PRINTING                                    YES

#undef   TWEAK_WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN
#define  TWEAK_WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN                          NO

#undef   TWEAK_WIDGETS_SCROLLBAR_KNOB_SNAP
#define  TWEAK_WIDGETS_SCROLLBAR_KNOB_SNAP                                 NO

#undef TWEAK_AUTOUPDATECHECKER_PLATFORM_HEADER
#define TWEAK_AUTOUPDATECHECKER_PLATFORM_HEADER	                           YES
#define OAUC_PLATFORM_INCLUDES "platforms/posix/autoupdate_checker/includes/platform.h"


// Must be included at the end to make it possible to make a custom build
// using the ubs build system:
#include "adjunct/quick/custombuild-tweaks.h"

#endif   // UNIX_TWEAKS_H
