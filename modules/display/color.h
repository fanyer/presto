/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _COLOR_MANAGER_
#define _COLOR_MANAGER_

/**
 * This color value has the special meaning as "Not set". It's defined
 * to be 32 set bits of the type UINT32.
 */
#define USE_DEFAULT_COLOR 0xFFFFFFFFU

/**
 * For platforms that can't handle alpha correctly, this color will be turned transparent in gadgets in some situations.
 */
#define REALLY_UGLY_COLOR 0xFEFF00FF

// Define if platform use gadgets, and can't handle drawing of alpha channel.
// The ugly color will be used to mask away transparent pixels. (Ugly color will always be transparent!)
#ifdef MSWIN
#define REALLY_UGLY_COLOR_HACK
#endif

/**
 * Check that color contrast between given font color and background color is better than minimum value.
 *  If not, return a new font_color.
 */
COLORREF CheckColorContrast(COLORREF font_color, COLORREF bg_color, int minimum_contrast, COLORREF light_font_color, COLORREF dark_font_color);

/**
 * Return the RGBA COLORREF for a COLORREF if it can be found without currentColor.
 */
COLORREF GetRGBA(COLORREF col_ref);

/**
 * Interpolate linearly between from and to colors in pre-multiplied space to
 * get a color at a certain point between the two colors given by the percentage
 * parameter.
 *
 * @param from The start color of the interpolation.
 * @param to The end color of the interpolation.
 * @param point A value between 0 and 1 where 0 is from and 1 is to.
 * @return The interpolated color at the given percentage.
 */
COLORREF InterpolatePremultipliedRGBA(COLORREF from, COLORREF to, double point);

#endif /* _COLOR_MANAGER_ */
