/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_GEN_MATH_H
#define MODULES_UTIL_GEN_MATH_H

#ifdef UTIL_ADVANCED_ROUND

typedef enum		// "ROUNDKIND" moved to ending brace.
{
	ROUND_DOWN,		//	round towards zero
	ROUND_NORMAL,	//	round to nearest
	ROUND_UP		//	round away from zero
} ROUND_KIND;

double Round(double num, int nDigitsAfterComma, ROUND_KIND roundKind);

#endif // UTIL_ADVANCED_ROUND

int OpRound(double num);

#endif // !MODULES_UTIL_GEN_MATH_H
