/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_VALUE_TYPES_H
#define CSS_VALUE_TYPES_H

#include "modules/style/src/css_values.h"

enum CSSValueType {
	CSS_IDENT					= 0x0800,
	CSS_STRING_LITERAL			= 0x0802,
	CSS_ESCAPED_STRING_LITERAL	= 0x0803,
	CSS_HASH					= 0x0806,
	CSS_FUNCTION_RGB			= 0x0808,
	CSS_FUNCTION_RGBA			= 0x0809,
	CSS_FUNCTION_URL			= 0x080a,
	CSS_FUNCTION_ATTR			= 0x080c,
	CSS_FUNCTION_COUNTER		= 0x080d,
	CSS_FUNCTION_COUNTERS		= 0x080e,
	CSS_FUNCTION				= 0x080f,
	CSS_CLASS					= 0x0810,
	CSS_FUNCTION_INTEGER		= 0x0811,
	CSS_SLASH					= '/',
	CSS_COMMA					= ',',
	CSS_COLON					= ':',
	CSS_HASH_CHAR				= '#',
	CSS_ASTERISK_CHAR			= '*',
	CSS_FUNCTION_RECT			= 0x0813,
	CSS_FUNCTION_SKIN			= 0x0814,
	CSS_GENERIC_VOICE_FAMILY	= 0x0815,
	CSS_FUNCTION_DASHBOARD_REGION	= 0x0816,
	CSS_RATIO					= 0x0817,
	CSS_FUNCTION_LANGUAGE_STRING = 0x0818,
	CSS_FUNCTION_LOCAL			= 0x0819,
	CSS_FUNCTION_FORMAT			= 0x081a,
	CSS_FUNCTION_CUBIC_BEZ		= 0x081b,
	CSS_FUNCTION_LOCAL_START	= 0x081c,
	CSS_FUNCTION_END			= 0x081d,
	CSS_FUNCTION_LINEAR_GRADIENT = 0x0820,
	CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT = 0x0821,
	CSS_FUNCTION_O_LINEAR_GRADIENT = 0x0822,
	CSS_FUNCTION_REPEATING_LINEAR_GRADIENT = 0x0823,
	CSS_FUNCTION_RADIAL_GRADIENT = 0x0824,
	CSS_FUNCTION_REPEATING_RADIAL_GRADIENT = 0x0825,
	CSS_FUNCTION_DOUBLE_RAINBOW = 0x0826,
	CSS_FUNCTION_STEPS			= 0x0830,
	CSS_NUMBER					= 0x0100,
	CSS_PERCENTAGE				= 0x0101,
	CSS_SECOND					= 0x0102,
	CSS_EM						= 0x0103,
	CSS_EX						= 0x0104,
	CSS_REM						= 0x0105,
	CSS_PX						= 0x0106,
	CSS_CM						= 0x0107,
	CSS_MM						= 0x0108,
	CSS_IN						= 0x0109,
	CSS_PT						= 0x010a,
	CSS_PC						= 0x010b,
	CSS_MS						= 0x010c,
	CSS_HZ						= 0x010d,
	CSS_DEG						= 0x010e,
	CSS_RAD						= 0x010f,
	CSS_KHZ						= 0x0110,
	CSS_GRAD					= 0x0111,
	CSS_DIMEN					= 0x0112,
	CSS_DIM_TYPE				= 0x0115, // same as pixels but defined by keyword
	CSS_DPI						= 0x0116,
	CSS_DPCM					= 0x0117,
	CSS_DPPX					= 0x0118,
	CSS_INT_NUMBER				= 0x0120,
	CSS_OCT_NUMBER				= 0x0140,
	CSS_HEX_NUMBER				= 0x0160,
	CSS_FLOAT_NUMBER			= 0x0180
};

#define CSS_get_number_ext(v) ((v) & 0x011f)
#define CSS_is_length_number_ext(v) ((v) >= CSS_EM && (v) <= CSS_PC)

#define CSS_is_number(v) ((v) & 0x0100)
#define CSS_get_number(v) ((v) & 0x01e0)

#define CSS_is_gradient(v) ((v) >= CSS_FUNCTION_LINEAR_GRADIENT && (v) <= CSS_FUNCTION_DOUBLE_RAINBOW)

#define CSS_is_resolution_ext(v) ((v) == CSS_DPI || (v) == CSS_DPCM || (v) == CSS_DPPX)

#endif // CSS_VALUE_TYPES_H
