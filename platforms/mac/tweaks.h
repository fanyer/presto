/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef  MAC_TWEAKS_H
#define  MAC_TWEAKS_H

#include "adjunct/quick/quick-tweaks.h"


#undef   TWEAK_ALL_PLUGINS_ARE_WINDOWLESS
#define  TWEAK_ALL_PLUGINS_ARE_WINDOWLESS                                  YES

#undef   TWEAK_AUTOUPDATE_PACKAGE_INSTALLATION
#define  TWEAK_AUTOUPDATE_PACKAGE_INSTALLATION                             YES

#undef   TWEAK_DESKTOPUTIL_TRANSFER_CLICK_TO_OPEN
#define  TWEAK_DESKTOPUTIL_TRANSFER_CLICK_TO_OPEN                          YES

#undef   TWEAK_QUICK_DISABLE_NAVIGATION_FROM_EDIT_WIDGET
#define  TWEAK_QUICK_DISABLE_NAVIGATION_FROM_EDIT_WIDGET                   YES

#undef   TWEAK_DISPLAY_INVERT_CARET
#define  TWEAK_DISPLAY_INVERT_CARET                                        NO // More Mac like, and fixes DSK-361171

#undef   TWEAK_DISPLAY_NO_MULTICLICK_LOOP
#define  TWEAK_DISPLAY_NO_MULTICLICK_LOOP                                  YES

#undef   TWEAK_DISPLAY_SCALE_MODIFIER
#define  TWEAK_DISPLAY_SCALE_MODIFIER                                      YES
#define  DISPLAY_SCALE_MODIFIER                                            SHIFTKEY_ALT

#undef   TWEAK_DISPLAY_SCROLLWHEEL_STEPSIZE
#define  TWEAK_DISPLAY_SCROLLWHEEL_STEPSIZE                                YES
#define  DISPLAY_SCROLLWHEEL_STEPSIZE                                      1

#undef   TWEAK_DND_USE_LINKS_TEXT_INSTEAD_OF_URL
#define  TWEAK_DND_USE_LINKS_TEXT_INSTEAD_OF_URL                           YES

// doc
# undef  TWEAK_DOC_SHIFTKEY_PREVENT_OPEN_IN_BACKGROUND
# define TWEAK_DOC_SHIFTKEY_PREVENT_OPEN_IN_BACKGROUND	YES
# undef  SHIFTKEY_PREVENT_OPEN_IN_BACKGROUND
# define SHIFTKEY_PREVENT_OPEN_IN_BACKGROUND			SHIFTKEY_SHIFT

#undef   TWEAK_DOM_REVERSE_CTRL_AND_META_IN_DOM_EVENTS
#define  TWEAK_DOM_REVERSE_CTRL_AND_META_IN_DOM_EVENTS                     YES

#if TARGET_CPU_X86
# undef   TWEAK_ES_GCC_STACK_REALIGNMENT
# define  TWEAK_ES_GCC_STACK_REALIGNMENT                                   YES
#endif

#undef   TWEAK_ES_NATIVE_STACK_ALIGNMENT
#define  TWEAK_ES_NATIVE_STACK_ALIGNMENT                                   YES
#define  ES_STACK_ALIGNMENT                                                16

#undef   TWEAK_GEOLOCATION_DEFAULT_POLL_INTERVAL
#define  TWEAK_GEOLOCATION_DEFAULT_POLL_INTERVAL                           YES
#define  GEOLOCATION_DEFAULT_POLL_INTERVAL                                 20000

#undef   TWEAK_LOC_EXTRASECTION_NAME
#define  TWEAK_LOC_EXTRASECTION_NAME                                       YES
#define  LOCALE_EXTRASECTION                                               L"Mac"

#undef   TWEAK_M2_CHAT_SUPPORT
#define  TWEAK_M2_CHAT_SUPPORT                                             NO

#undef   TWEAK_MEDIA_BACKEND_GSTREAMER_AUDIOSINK
#define  TWEAK_MEDIA_BACKEND_GSTREAMER_AUDIOSINK                           YES
#define  MEDIA_BACKEND_GSTREAMER_AUDIOSINK                                 "osxaudiosink"

#undef   TWEAK_MEDIA_BACKEND_GSTREAMER_USE_OPDLL
#define  TWEAK_MEDIA_BACKEND_GSTREAMER_USE_OPDLL                           NO

#undef   TWEAK_QUICK_MOUSE_GESTURE_ON_WORKSPACE
#define  TWEAK_QUICK_MOUSE_GESTURE_ON_WORKSPACE                            NO

#undef   TWEAK_NS4P_ALL_PLUGINS_ARE_WINDOWLESS
#define  TWEAK_NS4P_ALL_PLUGINS_ARE_WINDOWLESS                             YES

#if !defined(SIXTY_FOUR_BIT) && !defined(NO_CORE_COMPONENTS)
#undef   TWEAK_NS4P_COMPONENT_PLUGINS
#define  TWEAK_NS4P_COMPONENT_PLUGINS                                      NO

#undef   TWEAK_NS4P_DYNAMIC_PLUGINS
#define  TWEAK_NS4P_DYNAMIC_PLUGINS                                        YES
#endif // !SIXTY_FOUR_BIT

#undef   TWEAK_PF_BUILDNUMSUFFIX
#define  TWEAK_PF_BUILDNUMSUFFIX                                           YES
#define  BUILDNUMSUFFIX                                                    "Mac"

#undef   TWEAK_PI_LINK_WITHOUT_OPENGL
#define  TWEAK_PI_LINK_WITHOUT_OPENGL                                      NO

#undef   TWEAK_POSIX_DIRLIST_WRAPPABLE
#define  TWEAK_POSIX_DIRLIST_WRAPPABLE                                     YES

#undef   TWEAK_POSIX_FDATASYNC
#define  TWEAK_POSIX_FDATASYNC                                             YES
#define  PosixFDataSync                                                    fsync

#undef   TWEAK_POSIX_DNS_GETADDRINFO
#define  TWEAK_POSIX_DNS_GETADDRINFO                                       YES

#undef   TWEAK_POSIX_DNS_GETHOSTBYNAME
#define  TWEAK_POSIX_DNS_GETHOSTBYNAME                                     NO

#undef   TWEAK_POSIX_HAS_EAI_OVERFLOW
#define  TWEAK_POSIX_HAS_EAI_OVERFLOW                                      NO

#undef   TWEAK_POSIX_HAS_LANGINFO_1STDAY
#define  TWEAK_POSIX_HAS_LANGINFO_1STDAY                                   NO

#undef   TWEAK_POSIX_LENIENT_READ_LOCK
#define  TWEAK_POSIX_LENIENT_READ_LOCK                                     NO

#undef   TWEAK_POSIX_LOWFILE_WRAPPABLE
#define  TWEAK_POSIX_LOWFILE_WRAPPABLE                                     YES

#undef   TWEAK_POSIX_DNS_USE_RES_NINIT
#define  TWEAK_POSIX_DNS_USE_RES_NINIT                                     NO

#undef   TWEAK_POSIX_SELECTOR_EPOLL
#define  TWEAK_POSIX_SELECTOR_EPOLL                                        NO

#undef   TWEAK_POSIX_SELECTOR_SELECT_FALLBACK
#define  TWEAK_POSIX_SELECTOR_SELECT_FALLBACK                              NO

#undef   TWEAK_POSIX_TIME_NOJUMP
#define  TWEAK_POSIX_TIME_NOJUMP                                           YES
#define  POSIX_IMPLAUSIBLE_ELAPSED_TIME                                    60 // minutes

#undef   TWEAK_POSIX_TIMERS
#define  TWEAK_POSIX_TIMERS                                                NO

#undef   TWEAK_POSIX_UDP_BCAST_NOBIND
#define  TWEAK_POSIX_UDP_BCAST_NOBIND                                      YES

#undef   TWEAK_POSIX_USE_IFCONF_IOCTL
#define  TWEAK_POSIX_USE_IFCONF_IOCTL                                      NO

#undef   TWEAK_POSIX_USE_IFREQ_IOCTL
#define  TWEAK_POSIX_USE_IFREQ_IOCTL                                       NO

#undef   TWEAK_POSIX_UNSETENV_INT_RETURN
#define  TWEAK_POSIX_UNSETENV_INT_RETURN                                   NO

#undef   TWEAK_PREFS_CSS_SCROLLBARS
#define  TWEAK_PREFS_CSS_SCROLLBARS                                        NO
#undef   DEFAULT_CSS_SCROLLBARS

#undef   TWEAK_PREFS_PRINT_MARGIN_BOTTOM
#define  TWEAK_PREFS_PRINT_MARGIN_BOTTOM                                   YES
#define  DEFAULT_BOTTOM_MARGIN                                             100

#undef   TWEAK_PREFS_PRINT_MARGIN_LEFT
#define  TWEAK_PREFS_PRINT_MARGIN_LEFT                                     YES
#define  DEFAULT_LEFT_MARGIN                                               0

#undef   TWEAK_PREFS_PRINT_MARGIN_RIGHT
#define  TWEAK_PREFS_PRINT_MARGIN_RIGHT                                    YES
#define  DEFAULT_RIGHT_MARGIN                                              0

#undef   TWEAK_PREFS_PRINT_MARGIN_TOP
#define  TWEAK_PREFS_PRINT_MARGIN_TOP                                      YES
#define  DEFAULT_TOP_MARGIN                                                100

#undef   TWEAK_PREFS_SELECT_MENU
#define  TWEAK_PREFS_SELECT_MENU                                           YES
#define  DEFAULT_AUTOSELECTMENU                                            FALSE

#undef   TWEAK_PREFS_WINSTORAGE_FILE
#define  TWEAK_PREFS_WINSTORAGE_FILE                                       YES
#define  DEFAULT_WINSTORAGE_FILE                                           UNI_L("Opera.win")

#undef   TWEAK_QUICK_SEARCH_DROPDOWN_MAC_STYLE
#define  TWEAK_QUICK_SEARCH_DROPDOWN_MAC_STYLE                             YES

#undef   TWEAK_QUICK_TOOLKIT_ACCORDION_MAC_STYLE
#define  TWEAK_QUICK_TOOLKIT_ACCORDION_MAC_STYLE                           YES

#undef   TWEAK_QUICK_TOOLKIT_CENTER_TABS_BY_DEFAULT
#define  TWEAK_QUICK_TOOLKIT_CENTER_TABS_BY_DEFAULT                        YES

#undef   TWEAK_QUICK_TOOLKIT_PLATFORM_TREEVIEWDROPDOWN
#define  TWEAK_QUICK_TOOLKIT_PLATFORM_TREEVIEWDROPDOWN                     YES

#undef   TWEAK_QUICK_TOOLKIT_SELECTABLE_FILLS_BY_DEFAULT
#define  TWEAK_QUICK_TOOLKIT_SELECTABLE_FILLS_BY_DEFAULT                   NO

#undef   TWEAK_QUICK_TOOLKIT_OVERLAY_IS_SHEET_BY_DEFAULT
#define  TWEAK_QUICK_TOOLKIT_OVERLAY_IS_SHEET_BY_DEFAULT                   YES

#undef   TWEAK_QUICK_WINDOW_MOVE_FROM_TOOLBAR
#define  TWEAK_QUICK_WINDOW_MOVE_FROM_TOOLBAR                              YES

#undef   TWEAK_SKIN_DEFAULT_SKIN_FILE
#define  TWEAK_SKIN_DEFAULT_SKIN_FILE                                      YES
#define  DEFAULT_SKIN_FILE                                                 UNI_L("mac_skin.zip")

#undef   TWEAK_SKIN_USE_GRADIENT_SKIN
#define  TWEAK_SKIN_USE_GRADIENT_SKIN                                      YES

#undef   TWEAK_SVG_FIX_BROKEN_PAINTER_ALPHA
#define  TWEAK_SVG_FIX_BROKEN_PAINTER_ALPHA                                NO

#undef   TWEAK_VEGA_MDEFONT_SUPPORT
#define  TWEAK_VEGA_MDEFONT_SUPPORT                                        NO

#undef   TWEAK_VEGA_NATIVE_FONT_SUPPORT
#define  TWEAK_VEGA_NATIVE_FONT_SUPPORT                                    YES

#undef   TWEAK_VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND
#define  TWEAK_VEGA_SUBPIXEL_FONT_INTERPOLATION_WORKAROUND                 YES

#undef   TWEAK_WIDGETS_CUSTOM_DROPDOWN_WINDOW
#define  TWEAK_WIDGETS_CUSTOM_DROPDOWN_WINDOW                              YES

#undef   TWEAK_WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN
#define  TWEAK_WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN                          NO

#undef   TWEAK_WIDGETS_POPUP_BORDER_THICKNESS
#define  TWEAK_WIDGETS_POPUP_BORDER_THICKNESS                              YES
#define  WIDGETS_POPUP_BORDER_THICKNESS                                    0

#undef   TWEAK_WIDGETS_PROPERTY_PAGE_CHILDREN_OF_TABS
#define  TWEAK_WIDGETS_PROPERTY_PAGE_CHILDREN_OF_TABS                      NO

#undef   TWEAK_WIDGETS_SCROLLBAR_KNOB_SNAP
#define  TWEAK_WIDGETS_SCROLLBAR_KNOB_SNAP                                 NO

#undef   TWEAK_WIDGETS_UP_DOWN_MOVES_TO_START_END
#define  TWEAK_WIDGETS_UP_DOWN_MOVES_TO_START_END                          YES

#undef   TWEAK_DISPLAY_PIXEL_SCALE_RENDERING
#define  TWEAK_DISPLAY_PIXEL_SCALE_RENDERING                               YES

#undef   TWEAK_VEGA_TEXCORD_BIAS_FOR_REPEAT
#define  TWEAK_VEGA_TEXCORD_BIAS_FOR_REPEAT                                YES

#undef   TWEAK_SKIN_HIGH_RESOLUTION_IMAGE
#define  TWEAK_SKIN_HIGH_RESOLUTION_IMAGE                                  YES
#if TWEAK_SKIN_HIGH_RESOLUTION_IMAGE == YES
#undef  HIGH_RESOLUTION_IMAGE_SUFFIX
#define HIGH_RESOLUTION_IMAGE_SUFFIX                                       UNI_L("@2x")
#endif // TWEAK_SKIN_HIGH_RESOLUTION_IMAGE

#undef TWEAK_AUTOUPDATECHECKER_PLATFORM_HEADER
#define TWEAK_AUTOUPDATECHECKER_PLATFORM_HEADER	                           YES
#define OAUC_PLATFORM_INCLUDES "adjunct/autoupdate/autoupdate_checker/platforms/mac/includes/platform.h"

// This include MUST be last in the file
#include "adjunct/quick/custombuild-tweaks.h"

#endif// MAC_TWEAKS_H

