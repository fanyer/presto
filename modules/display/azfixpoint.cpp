#include "core/pch.h"

#ifdef ADAPTIVE_ZOOM_SUPPORT

#include "modules/display/azfixpoint.h"

#ifdef _DEBUG
AZLevel AZ_INTTOFIX(int v)
{
	OP_ASSERT((unsigned int)op_abs(v) < (1 << (8*sizeof(AZLevel) - AZ_FIX_DECBITS)));
	return ((AZLevel)((v)<<(AZ_FIX_DECBITS)));
}
#endif // _DEBUG


// taken from vegafixpoint.cpp
AZLevel AZMul(AZLevel x, AZLevel y)
{
	/* multiply two 32-bit numbers to a 64-bit number */

	BOOL positive = TRUE;
	UINT32 lores, hires, tmpres, tmpres2;
	UINT32 a, b, c, d;

	if (x < 0)
	{
		x = -x;
		positive = FALSE;
	}
	if (y < 0)
	{
		y = -y;
		positive = !positive;
	}

	a = x>>16;
	b = x&0xffff;
	c = y>>16;
	d = y&0xffff;

	lores = b*d;
	hires = a*c;

	// a and c cannot have the maximum value, so this will always fit
	tmpres = a*d + b*c;
	hires += (tmpres >> 16);
	tmpres <<= 16;
	tmpres2 = lores + tmpres;
	// if the high bit of both number being added is set, we have a carry. 
	// if one of the high bits is set and the high bit of the result is not set, we have a carry
	const UINT32 topbit = 1u << 31;
	if ((lores&topbit && tmpres&topbit) ||
		((lores&topbit)^(tmpres&topbit) && !(tmpres2&topbit)))
		++hires;
	lores = tmpres2;

	if (!positive)
	{
		hires = ~hires;
		lores = ~lores;
		if (lores == ~0U)
			++hires;
		++lores;
	}

	return (AZLevel)((hires<<(32-AZ_FIX_DECBITS))|(lores>>AZ_FIX_DECBITS));
}

// taken from vegafixpoint.cpp
AZLevel AZDiv(AZLevel x, AZLevel y)
{
	BOOL positive = TRUE;
	UINT32 res, tmpres, shift;
	UINT32 a, b;

	if (x < 0)
	{
		x = -x;
		positive = FALSE;
	}
	if (y < 0)
	{
		y = -y;
		positive = !positive;
	}

	shift = 0;
	a = x;
	b = y;

	res = 0;
	// we need to divide and shift 16 bits left. When this shift occurs we might be able to add more decimal numbers
	while (a && shift < AZ_FIX_DECBITS)
	{
		while (b > a && shift < AZ_FIX_DECBITS)
		{
			if (a&(1u<<31))
			{
				b >>= 1;
			}
			else
			{
				a <<= 1;
			}
			res <<= 1;
			++shift;
		}

		tmpres = a/b;
		res += tmpres;
		a = a - b*tmpres;
	}
	if (shift < AZ_FIX_DECBITS)
	{
		res <<= (AZ_FIX_DECBITS-shift);
	}

	if (!positive)
	{
		res = ~res;
		++res;
	}

	return (AZLevel)res;
}

#endif // ADAPTIVE_ZOOM_SUPPORT
