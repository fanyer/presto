/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2002-2006
 *
 * ECMAScript regular expression matcher -- matcher engine
 * Lars T Hansen
 *
 * See regexp_advanced_api.cpp for overall documentation.
 */

#include "core/pch.h"

#ifndef REGEXP_SUBSET

#include "modules/regexp/include/regexp_advanced_api.h"
#include "modules/regexp/src/regexp_private.h"

const uni_char regexp_space_values[] =
{
	CHAR_HT,
	CHAR_VT,
	CHAR_FF,
	' ',
	CHAR_LF,
	CHAR_CR,
	CHAR_NBSP,
	CHAR_USP1,
	CHAR_USP2,
	CHAR_USP3,
	CHAR_USP4,
	CHAR_USP5,
	CHAR_USP6,
	CHAR_USP7,
	CHAR_USP8,
	CHAR_USP9,
	CHAR_USP10,
	CHAR_USP11,
	CHAR_USP12,
	CHAR_USP13,
	CHAR_USP14,
	CHAR_USP15,
	CHAR_LS,
	CHAR_PS,
	0
};

const uni_char regexp_formatting_values[] =
{
	CHAR_SHYP,
	CHAR_ZWNJ,
	CHAR_ZWJ,
	CHAR_LTR,
	CHAR_RTL,
	CHAR_BIDI1,
	CHAR_BIDI2,
	CHAR_BIDI3,
	CHAR_BIDI4,
	CHAR_BIDI5,
	CHAR_MATH1,
	CHAR_MATH2,
	CHAR_MATH3,
	CHAR_DEPR1,
	CHAR_DEPR2,
	CHAR_DEPR3,
	CHAR_DEPR4,
	CHAR_DEPR5,
	CHAR_DEPR6,
	0
};

#endif // !SUBSET_REGEXP
