/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _CSS_MODE_H_
#define _CSS_MODE_H_

/**
 * *shrug*
 */
enum CSSMODE
{
    CSS_NONE = 0,
    CSS_FULL,
	// end of list

	NUMBER_OF_CSSMODES
};

/**
 * Different ways to parse CSS.
 */
enum
{
	CSS_COMPAT_MODE_AUTO = 0,
	CSS_COMPAT_MODE_QUIRKS,
	CSS_COMPAT_MODE_STRICT
};

/**
 * Different layout modes. Ask the layout guys if you want to know
 * what they are, though I think we might have information on our web
 * as well.
 */
typedef enum {
	LAYOUT_NORMAL = 0,
	LAYOUT_SSR,
	LAYOUT_CSSR,
	LAYOUT_AMSR,
	LAYOUT_MSR
#ifdef TV_RENDERING
	, LAYOUT_TVR
#endif // TV_RENDERING 
} LayoutMode;

#endif // _CSS_MODE_H_
