// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#include "WindowsComplexScript.h"

#ifdef SUPPORT_TEXT_DIRECTION

#include <usp10.h>


static BOOL3 needs_rtl_extra_spacing_fix = MAYBE;

void WIN32ComplexScriptSupport::GetTextExtent(HDC hdc, const uni_char* str, int len, SIZE* s, INT32 extra_char_spacing)
{
	BidiCategory category = Unicode::GetBidiCategory(*str);
	BOOL rtl = category == BIDI_AL || category == BIDI_R;

	if (rtl)
	{
		SCRIPT_STRING_ANALYSIS ssa;
		HRESULT hr = ScriptStringAnalyse(hdc, str, len, len*3/2+1, -1, SSA_GLYPHS | SSA_FALLBACK | SSA_RTL,
												0, NULL, NULL, NULL, NULL, NULL, &ssa);

		if (SUCCEEDED(hr))
		{
			const SIZE* size = ScriptString_pSize(ssa);
			*s = *size;

			ScriptStringFree(&ssa);
		}

		s->cx += len * extra_char_spacing;

		return;
	}

	if (extra_char_spacing == 0)
		GetTextExtentPoint32(hdc, str, len, s);
	else
	{
		//set the extra character spacing in the hdc, so GetTextExtentPoint takes it into account			
		int old_extra_spacing = GetTextCharacterExtra(hdc);
		SetTextCharacterExtra(hdc, extra_char_spacing);
		GetTextExtentPoint32(hdc, str, len, s);
		SetTextCharacterExtra(hdc, old_extra_spacing);

		if (rtl)
		{
			if (needs_rtl_extra_spacing_fix == MAYBE)
			{
				SetTextCharacterExtra(hdc, 0);
				SIZE s_0;
				GetTextExtentPoint32(hdc, str, len, &s_0);
				if (s_0.cx == s->cx)
				{
					needs_rtl_extra_spacing_fix = YES;
				}
				else
				{
					needs_rtl_extra_spacing_fix = NO;
				}
				SetTextCharacterExtra(hdc, old_extra_spacing);
			}

			if (needs_rtl_extra_spacing_fix == YES)
			{
				// We need to calculate the size of rtl characters but there is no uniscribe library available.
				// GetTextExtentPoint32 might ignore extra spacing on win9x: calculate it manually
				// That's at least my guess why this code is here (huibk, 2006)
				s->cx += len * extra_char_spacing;
			}
		}
	}
}

void WIN32ComplexScriptSupport::TextOut(HDC hdc, int x, int y, const uni_char* text, int len)
{
	BidiCategory category = Unicode::GetBidiCategory(*text);

	if (category == BIDI_AL || category == BIDI_R)
	{
		SCRIPT_STRING_ANALYSIS ssa;
		HRESULT hr = ScriptStringAnalyse(hdc, text, len, len*3/2+1, -1, SSA_GLYPHS | SSA_FALLBACK | SSA_RTL,
												0, NULL, NULL, NULL, NULL, NULL, &ssa);

		if (SUCCEEDED(hr))
		{
			hr = ScriptStringOut(ssa, x, y, ETO_OPAQUE, NULL, 0, 0, FALSE);

			ScriptStringFree(&ssa);
		}

		return;
	}

	::TextOut(hdc, x, y, text, len);
}

#endif SUPPORT_TEXT_DIRECTION
