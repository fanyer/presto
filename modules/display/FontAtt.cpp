/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/FontAtt.h"

#include "modules/display/styl_man.h"
#include "modules/util/handy.h"
#include "modules/util/gen_str.h"

#ifdef OPFONT_GLYPH_PROPS
#include "modules/display/fontcache.h"
#include "modules/pi/OpFont.h"
#endif // OPFONT_GLYPH_PROPS

const uni_char*	FontAtt::GetFaceName() const
{
	return styleManager->GetFontFace(m_fontnumber);
}

void FontAtt::SetFaceName(const uni_char* faceName)
{
	const short num = styleManager->GetFontNumber(faceName);
	OP_ASSERT(num >= 0);
	SetFontNumber(num);
}

#ifdef PREFS_READ
BOOL FontAtt::Unserialize(const uni_char *str)
{
	uni_char serializebuf[256]; /* ARRAY OK 2007-11-21 emil */
	uni_strlcpy(serializebuf, str, ARRAY_SIZE(serializebuf));
	uni_char *tokens[8];
	if (7 == GetStrTokens(serializebuf, UNI_L(","), UNI_L(""), tokens, 7, FALSE))
	{
		short fontnumber = styleManager->GetFontNumber(tokens[6]);
		// Only use the font if it is recognized
		if (fontnumber != -1)
		{
			SetFontNumber(fontnumber);
		}

		// Always use the rest of the data
		SetSize(      uni_strtol(tokens[0], NULL, 10, NULL));
		SetWeight(    uni_strtol(tokens[1], NULL, 10, NULL));
		SetItalic(    uni_strtol(tokens[2], NULL, 10, NULL) != 0);
		SetUnderline( uni_strtol(tokens[3], NULL, 10, NULL) != 0);
		SetOverline(  uni_strtol(tokens[4], NULL, 10, NULL) != 0);
		SetStrikeOut( uni_strtol(tokens[5], NULL, 10, NULL) != 0);
		return TRUE;
	}

	// Couldn't parse data
	return FALSE;
}
#endif // PREFS_READ

#if defined PREFS_WRITE || defined PREFS_HAVE_STRING_API
OP_STATUS FontAtt::Serialize(OpString* target) const
{
	uni_char serializebuf[256]; /* ARRAY OK 2007-11-21 emil */
	uni_snprintf(serializebuf, ARRAY_SIZE(serializebuf),
	             UNI_L("%d,%d,%d,%d,%d,%d,%s"),
	             GetHeight(), GetWeight(), GetItalic(),
				 GetUnderline(), GetOverline(), GetStrikeOut(),
				 GetFaceName());
	return target->Set(serializebuf);
}
#endif // PREFS_WRITE

INT32 FontAtt::GetExHeight()
{
#ifdef OPFONT_GLYPH_PROPS
	OpFont* tmpFont = g_font_cache->GetFont(*this, 100);
	if (tmpFont)
	{
		OpFont::GlyphProps glyph_props;
		OP_STATUS status = tmpFont->GetGlyphProps('x', &glyph_props);
		g_font_cache->ReleaseFont(tmpFont);

		if (OpStatus::IsSuccess(status))
			return glyph_props.top;
	}
#endif // OPFONT_GLYPH_PROPS

	return m_fontHeight / 2;
}
