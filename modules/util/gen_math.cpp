/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/util/gen_math.h"

#ifdef UTIL_ADVANCED_ROUND

//	ceil towards zero
static double CeilZero ( double num)
{
	if (num>=0)
		return op_ceil(num);
	else
		return op_floor(num);
}

//	floor towards zero
static double FloorZero( double num)
{
	if (num>=0)
		return op_floor(num);
	else
		return op_ceil(num);
}

double Round(double num, int nDigitsAfterComma, ROUND_KIND roundKind)
{
	double result;
	double quotient = 0;
	double fraction = op_modf( num, &quotient);
	
	if (fraction == 0.0)
		result = quotient;
	else
	{
		double pow10 = op_pow(10.0,nDigitsAfterComma);
		double newFraction = fraction * pow10;

		switch (roundKind)
		{
			case ROUND_DOWN:
			{
				newFraction = FloorZero( newFraction);
				break;
			}
			case ROUND_NORMAL:
			{
				double ffloor = FloorZero( newFraction);
				newFraction = (op_fabs(newFraction-ffloor)<0.5) ? ffloor : CeilZero(newFraction);
				break;
			}
			case ROUND_UP:
			{
				newFraction = CeilZero(newFraction);
				break;
			}
		}
		newFraction /= pow10;
		result = quotient + newFraction;
	}
	return result;
}

#endif // UTIL_ADVANCED_ROUND

int
OpRound(double num)
{
	double fl = op_floor(num);
	double ce = op_ceil(num);
	if (fl == ce)
		return (int)num;
	else if (num - fl < ce - num)
		return (int)fl;
	else
		return (int)ce;
}
