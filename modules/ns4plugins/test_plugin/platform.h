/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Opera test plugin.
 *
 * Platform defines.
 *
 * Copyright (C) 2011 Opera Software ASA.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "common.h"

#if defined(XP_UNIX) || defined(XP_GOGI)
# ifdef __linux__
#include <alloca.h>
# else
#include <stdlib.h>
# endif // __linux__
#endif // XP_UNIX || XP_GOGI

#ifdef XP_UNIX
#include <stdarg.h>
#include <X11/Intrinsic.h>
#include <gtk/gtk.h>
#ifndef MOZ_X11
#define MOZ_X11
#endif // MOZ_X11
#endif // XP_UNIX

#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN
#ifndef WM_MOUSEHWHEEL
#define WM_MOUSEHWHEEL 0x020E
#endif // !WM_MOUSEHWHEEL
#define STRCASECMP _stricmp
#define SNPRINTF(target, length, ...) sprintf_s<length>(target, __VA_ARGS__)
/* Mapping of Windows RECT to NPRect convention. */
typedef struct {
  unsigned int left;
  unsigned int top;
  unsigned int right;
  unsigned int bottom;
} NPRect32;
#endif // _WIN32

#ifndef STRCASECMP
#define STRCASECMP strcasecmp
#endif // STRCASECMP

#ifndef SNPRINTF
#define SNPRINTF snprintf
#endif // SNPRINTF

enum
{
#ifndef XP_UNIX
	/* X11 defines these and they're handy. */
	KeyPress,
	KeyRelease,
	ButtonPress,
	ButtonRelease,
#endif // !XP_UNIX
	/* Additional defines for all platforms. */
	KeyModifierChange,
	TextInput,
	MouseMove,
	MouseWheel,
	MouseOver,
	MouseOut,
	MouseDrag
};

#endif // PLATFORM_H
