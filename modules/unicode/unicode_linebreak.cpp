/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef USE_UNICODE_LINEBREAK
#include "modules/unicode/unicode.h"
#include "modules/unicode/unicode_segmenter.h"

#ifdef UNICODE_FALLBACK_THAI_LINEBREAK
#include "modules/unicode/unicode_linebreak_thai_fallback.h"
#endif

LinebreakOpportunity Unicode::IsLinebreakOpportunity(UnicodePoint first, UnicodePoint second, BOOL space_separated)
{
//	OP_ASSERT(!IsSurrogate(first) && !IsSurrogate(second) || !"Invalid call to Unicode::IsLinebreakOpportunity()");
	
	// LB2 Never break at the start of text.
	// sot x
	if (!first)
		return LB_NO;
	// LB3 Always break at the end of text.
	// ! eot
	if (!second)
		return LB_YES;

	// LB1 Assign a line breaking class to each code point of the input.
	// Resolve AI, CB, SA, SG and XX into other line breaking classes
	// depending on criteria outside the scope of this algorithm.
	LinebreakClass clfirst = LineBreakRemap(GetLineBreakClass(first));

	// The caller need to perform combining mark and space compression,
	// so any characters in the SP or CM classes seen here are errors.
	if (clfirst == LB_SP || clfirst == LB_CM)
	{
		// This is incorrect; see bug #337981.
		return LB_MAYBE;
	}
	// LB4 Always break after hard line breaks (but never between CR and LF)
	// BK !
	if (clfirst == LB_BK)
		return LB_YES;

	// no need to check clsecond if the clfirst is hit
	LinebreakClass clsecond = LineBreakRemap(GetLineBreakClass(second));
	if (clsecond == LB_SP || clsecond == LB_CM)
	{
		// This is incorrect; see bug #337981.
		return LB_MAYBE;
	}

	// LB5 Treat CR followed by LF, as well as CR, LF, and NL as hard line
	// breaks.
	// CR x LF
	// CR !
	if (clfirst == LB_CR)
		return (clsecond == LB_LF && !space_separated) ? LB_NO : LB_YES;
	// LF !
	// NL !
	if (clfirst == LB_LF || clfirst == LB_NL)
		return LB_YES;

	// LB6 Do not break before hard line breaks.
	// x (BK | CR | LF | NL)
	if (clsecond == LB_BK || clsecond == LB_CR || clsecond == LB_LF || clsecond == LB_NL)
		return LB_NO;

	// LB9 Do not break a combining character sequence.
	// LB10 Treat any remaining combining marks as AL.
	// Handled through API setup

	// LB7 -- LB8, LB11 -- LB31
	// Handled through pair table lookup
	// (UAX#14, X7.3)

	enum LBFlag
	{
		N,	// "^" Prohibited break
		I,	// "%" Indirect break opportunity
		Y,	// "_" Direct break opportunity
		U	// Undetermined (should be handled by other rules!)
	};

	// Remove some classes from the table to reduce size.
	static const LinebreakClass lb_first_table = LB_WJ;
	OP_ASSERT(clfirst >= lb_first_table || !"Internal error");
	OP_ASSERT(clsecond>= lb_first_table || !"Internal error");

	// Tailored rules:
	// CORE-5163: LB30: AL + OP
	// CORE-100: LB13: (AL | IS) % IS
	// CORE-32455: PR % PR
	static const unsigned char pair_table[LB_Last - lb_first_table + 1][LB_Last - lb_first_table + 1] =
	{
		       /*WJ ZW GL SP B2 BA BB HY CB CL CP EX IN NS OP QU IS NU PO PR SY AI AL CJ H2 H3 HL ID JL JV JT SA*/
		/*WJ*/ { N, N, N, N, N, I, N, I, N, N, N, N, N, I, N, I, N, N, N, N, N, U, N, I, N, N, N, I, N, N, N, U   },
		/*ZW*/ { Y, N, Y, N, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, Y, U, Y, Y, Y, Y, Y, Y, Y, Y, Y, U   },
		/*GL*/ { N, N, I, N, I, I, I, I, I, N, N, N, I, I, I, I, N, I, I, I, N, U, I, I, I, I, I, I, I, I, I, U   },
		/*SP*/ { N, N, Y, N, N, I, N, I, N, N, N, N, N, I, N, I, N, N, N, N, N, U, N, I, N, N, N, I, N, N, N, U   },
		/*B2*/ { N, N, I, N, N, I, Y, I, Y, N, N, N, Y, I, Y, I, N, Y, Y, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   },
		/*BA*/ { N, N, Y, N, Y, I, Y, I, Y, N, N, N, Y, I, Y, I, N, Y, Y, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   },
		/*BB*/ { N, N, I, N, I, I, I, I, Y, N, N, N, I, I, I, I, N, I, I, I, N, U, I, I, I, I, I, I, I, I, I, U   }, /* F */
		/*HY*/ { N, N, Y, N, Y, I, Y, I, Y, N, N, N, Y, I, Y, I, N, I, Y, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   }, /* i */
		/*CB*/ { N, N, I, N, Y, Y, Y, Y, Y, N, N, N, Y, Y, Y, I, N, Y, Y, Y, N, U, Y, Y, Y, Y, Y, Y, Y, Y, Y, U   }, /* r */
		/*CL*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, N, Y, I, N, Y, I, I, N, U, Y, N, Y, Y, Y, Y, Y, Y, Y, U   }, /* s */
		/*CP*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, N, Y, I, N, I, I, I, N, U, I, N, Y, Y, I, Y, Y, Y, Y, U   }, /* t */
		/*EX*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, I, Y, I, N, Y, Y, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   },
		/*IN*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, Y, I, N, Y, Y, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   },
		/*NS*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, I, Y, I, N, Y, Y, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   },
		/*OP*/ { N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, U, N, N, N, N, N, N, N, N, N, U   }, // <- hard N
		/*QU*/ { N, N, I, N, I, I, I, I, I, N, N, N, I, I, N, I, N, I, I, I, N, U, I, I, I, I, I, I, I, I, I, U   },
		/*IS*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, I, Y, I, I, I, Y, Y, N, U, I, I, Y, Y, I, Y, Y, Y, Y, U   },
		/*NU*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, I, I, N, I, I, I, N, U, I, I, Y, Y, I, Y, Y, Y, Y, U   },
		/*PO*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, I, I, I, N, I, Y, Y, N, U, I, I, Y, Y, I, Y, Y, Y, Y, U   },
		/*PR*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, I, I, I, N, I, Y, I, N, U, I, I, I, I, I, I, I, I, I, U   },
		/*SY*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, I, Y, I, N, I, Y, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   },
		/*AI*/ { U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U   },
		/*AL*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, Y, I, I, I, Y, Y, N, U, I, I, Y, Y, I, Y, Y, Y, Y, U   },
		/*CJ*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, Y, I, Y, I, N, Y, Y, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   },
		/*H2*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, Y, I, N, I, I, Y, N, U, Y, I, Y, Y, Y, Y, Y, I, I, U   },
		/*H3*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, Y, I, N, Y, I, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, I, U   },
		/*HL*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, I, I, N, I, Y, Y, N, U, I, I, Y, Y, I, Y, Y, Y, Y, U   },
		/*ID*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, Y, I, N, Y, I, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, Y, U   },
		/*JL*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, Y, I, N, Y, I, Y, N, U, Y, I, I, I, Y, Y, I, I, Y, U   },
		/*JV*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, Y, I, N, Y, I, Y, N, U, Y, I, Y, Y, Y, Y, Y, I, I, U   },
		/*JT*/ { N, N, I, N, Y, I, Y, I, Y, N, N, N, I, I, Y, I, N, Y, I, Y, N, U, Y, I, Y, Y, Y, Y, Y, Y, I, U   },
		/*SA*/ { U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U, U   },
															/* Second */
	};

	if (clfirst < lb_first_table || clsecond < lb_first_table ||
		clfirst > LB_Last || clsecond > LB_Last)
	{
		OP_ASSERT(!"Should have been caught by base rules");
		return LB_NO;
	}

	switch (static_cast<LBFlag>(pair_table[clfirst - lb_first_table][clsecond - lb_first_table]))
	{
	default:
		OP_ASSERT(!"Invalid data in table");
		/* Fall through. */
	case U:
		OP_ASSERT(!"Should have been caught by base rules");
		/* Fall through. */
	case I:
		if (space_separated)
		{
		/* Fall through. */
	case Y:
			return LB_YES;
		}
		else
		{
		/* Fall through. */
	case N:
			return LB_NO;
		}
	}
}

LinebreakOpportunity Unicode::Is_SA_LinebreakOpportunity(UnicodePoint prev, const uni_char *buf, int len)
{
	// Do line breaking of Complex Context Dependent (South East Asian) text.
	// Delegate to sub-functions where we have the knowledge.
	OP_ASSERT(buf && len > 0);

	int w;
	(void)w;

	OP_ASSERT(GetLineBreakClass(Unicode::GetUnicodePoint(buf, len, w)) == LB_SA || GetLineBreakClass(prev) == LB_SA);

	UnicodePoint cur = Unicode::GetUnicodePoint(buf, len, w);
	// Delegate to sub-function if appropriate
#ifdef UNICODE_FALLBACK_THAI_LINEBREAK
	if (FallbackThaiUnicodeLinebreaker::IsValidChar(cur))
		return FallbackThaiUnicodeLinebreaker::IsLinebreakOpportunity(prev, buf, len);
	else if (FallbackThaiUnicodeLinebreaker::IsValidChar(prev))
		return LB_YES;
#endif

	// As long as we don't know how to linebreak Khmer, Mayanmar and Lao, we must fall back to the regular
	// linebreak check. Always assuming there is a linebreaking opportunity (or any kind of guessing)
	// will break text shaping badly. We must never break between any glyphs that should be shaped.
	// 
	return IsLinebreakOpportunity(prev, cur, FALSE);
}

LinebreakOpportunity Unicode::IsLinebreakOpportunity(UnicodePoint prev, const uni_char *buf, int len, BOOL space_separated)
{
	// LB2 Never break at the start of text.
	// sot x
	if (!prev)
		return LB_NO;
	// LB3 Always break at the end of text.
	// ! eot
	if (!buf || !len)
		return LB_YES;

	int w;
	UnicodePoint cur = GetUnicodePoint(buf, len, w);
	OP_ASSERT(w > 0);

	if (!space_separated && (GetLineBreakClass(cur) == LB_SA || GetLineBreakClass(prev) == LB_SA))
		return Is_SA_LinebreakOpportunity(prev, buf, len);

	return IsLinebreakOpportunity(prev, cur, space_separated);
}

#endif // USE_UNICODE_LINEBREAK
