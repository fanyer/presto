/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Morten Stenshorne
*/

#include "core/pch.h"

#ifdef TEXTSHAPER_SUPPORT

#include "modules/display/textshaper.h"
#include "modules/util/tempbuf.h"

// opentype processes these languages, so we shouldn't process them here
#ifdef MDF_OPENTYPE_SUPPORT
# undef SHAPE_DEVANAGARI
// add other languages processed by opentype here
#endif // MDF_OPENTYPE_SUPPORT

#define IS_DEVANAGARI(ch) ((ch) >= 0x0900 && (ch) <= 0x097f)
#define IS_MALAYALAM(ch)  ((ch) >= 0x0d23 && (ch) <= 0x0d33)
#define IS_ARABIC(ch) ((ch) >= 0x0620 && (ch) < 0x0750)
#define IS_JOINER(ch) ((ch) == 0x200c || (ch) == 0x200d)

// Include auto-generated tables
#include "modules/display/text_shaper.inl"

static const uni_char ligaturetable[] = {
#ifdef SHAPE_ARABIC
	// arabic ligatures
	
	0xfedf, // ALEF WITH MADDA ABOVE(r) + LAM(l), component 1
	0xfe82, // ALEF WITH MADDA ABOVE(r) + LAM(l), component 2
	0, // ALEF WITH MADDA ABOVE(r) + LAM(l), component 3
	0xfef5, // ALEF WITH MADDA ABOVE(r) + LAM(l), ligature

	0xfee0, // ALEF WITH MADDA ABOVE(r) + LAM(m), component 1
	0xfe82, // ALEF WITH MADDA ABOVE(r) + LAM(m), component 2
	0, // ALEF WITH MADDA ABOVE(r) + LAM(m), component 3
	0xfef6, // ALEF WITH MADDA ABOVE(r) + LAM(m), ligature

	0xfedf, // ALEF WITH HAMZA ABOVE(r) + LAM(l), component 1
	0xfe84, // ALEF WITH HAMZA ABOVE(r) + LAM(l), component 2
	0, // ALEF WITH HAMZA ABOVE(r) + LAM(l), component 3
	0xfef7, // ALEF WITH HAMZA ABOVE(r) + LAM(l), ligature

	0xfee0, // ALEF WITH HAMZA ABOVE(r) + LAM(m), component 1
	0xfe84, // ALEF WITH HAMZA ABOVE(r) + LAM(m), component 2
	0, // ALEF WITH HAMZA ABOVE(r) + LAM(m), component 3
	0xfef8, // ALEF WITH HAMZA ABOVE(r) + LAM(m), ligature

	0xfedf, // ALEF WITH HAMZA BELOW(r) + LAM(l), component 1
	0xfe88, // ALEF WITH HAMZA BELOW(r) + LAM(l), component 2
	0, // ALEF WITH HAMZA BELOW(r) + LAM(l), component 3
	0xfef9, // ALEF WITH HAMZA BELOW(r) + LAM(l), ligature

	0xfee0, // ALEF WITH HAMZA BELOW(r) + LAM(m), component 1
	0xfe88, // ALEF WITH HAMZA BELOW(r) + LAM(m), component 2
	0, // ALEF WITH HAMZA BELOW(r) + LAM(m), component 3
	0xfefa, // ALEF WITH HAMZA BELOW(r) + LAM(m), ligature

	0xfedf, // ALEF(r) + LAM(l), component 1
	0xfe8e, // ALEF(r) + LAM(l), component 2
	0, // ALEF(r) + LAM(l), component 3
	0xfefb, // ALEF(r) + LAM(l), ligature

	0xfee0, // ALEF(r) + LAM(m), component 1
	0xfe8e, // ALEF(r) + LAM(m), component 2
	0, // ALEF(r) + LAM(m), component 3
	0xfefc, // ALEF(r) + LAM(m), ligature
#endif // SHAPE_ARABIC

#ifdef SHAPE_MALAYALAM
	// malayalam cillacs.aram - currently (2007-08-09) the ligatures are
	// in the unicode pipeline table, but they are present in (at least)
	// the AnjaliOldLipi.ttf font. see bug #259790.

	// malayalam ligature chillu nn
	0x0d23, // nna
	0x0d4d, // virama
	0x200d, // zero-width joiner
	0x0d7a, // chillu nn

	// malayalam ligature chillu n
	0x0d28, // na
	0x0d4d, // virama
	0x200d, // zero-width joiner
	0x0d7b, // chillu n

	// malayalam ligature chillu rr
	0x0d30, // ra
	0x0d4d, // virama
	0x200d, // zero-width joiner
	0x0d7c, // chillu rr

	// malayalam ligature chillu l
	0x0d32, // la
	0x0d4d, // virama
	0x200d, // zero-width joiner
	0x0d7d, // chillu l

	// malayalam ligature chillu ll
	0x0d33, // lla
	0x0d4d, // virama
	0x200d, // zero-width joiner
	0x0d7e, // chillu ll
#endif // SHAPE_MALAYALAM

	0 // terminator
};

/*int TextShaper::ComposeString(uni_char *dst, const uni_char *src, int len)
{
	uni_char *start = dst;
	while (len--)
	{
		uni_char ch = GetIsolatedForm(*src ++);
		if (ch)
			*dst++ = ch;
	}
	return dst - start;
}*/

UnicodePoint TextShaper::GetJoinedForm(UnicodePoint ch, int num)
{
	if (IS_JOINER(ch)) // zero-width non-joiner and joiner
		return 0;
	else if (ch < 0x0620 || ch > 0x06d3)
		return ch;
	int index = ch-0x0620;
	uni_char joined = prestable_start[index];
	if (joined == 0x0000)
		return ch;
	if (prestable_count[index] > num)
		return joined + num;
	else
		return joined;
}

TextShaper::JoiningClass TextShaper::GetJoiningClass(UnicodePoint ch)
{
	if (ch < 0x0620 || ch > 0x074a)
	{
		if (ch == 0x200d) // zero-width joiner
			return TextShaper::DUAL_JOINING;
		/* The rest (including 0x200c - zero-width non-joiner) are treated as
		   non-joining. */
		return TextShaper::NON_JOINING;
	}
	int index = ch-0x0620;

	const JoiningClass jc = static_cast<JoiningClass>(joinclasstable[index]);
	// apparently JOIN_CAUSING can be treated as DUAL_JOINING as long
	// as GetJoinedForm returns passed character when no
	// presentational forms exist
	return jc == JOIN_CAUSING ? DUAL_JOINING : jc;
}

void TextShaper::ResetState() { g_textshaper_state = NOT_JOINING; }

BOOL JoinsLeft(TextShaper::JoiningState s)
{
	return s == TextShaper::JOINING_DUAL || s == TextShaper::JOINING_LEFT;
}
BOOL JoinsLeft(TextShaper::JoiningClass c)
{
	return c == TextShaper::DUAL_JOINING || c == TextShaper::LEFT_JOINING;
}
BOOL JoinsRight(TextShaper::JoiningClass c)
{
	return c == TextShaper::DUAL_JOINING || c == TextShaper::RIGHT_JOINING;
}
UnicodePoint TextShaper::GetJoinedChar(UnicodePoint ch, JoiningClass next_type)
{
	JoiningClass this_type = GetJoiningClass(ch);
	BOOL joinRight = JoinsLeft(g_textshaper_state) && JoinsRight(this_type);
	BOOL joinLeft = JoinsLeft(this_type) && JoinsRight(next_type);
	if (this_type == TRANSPARENT_JOINING)
		return ch;//JoinsLeft(g_textshaper_state) ? GetMedialForm(ch) : GetIsolatedForm(ch);
	uni_char result = 0;
	if (joinLeft)
	{
		g_textshaper_state = JOINING_LEFT;
		if (joinRight)
			result = GetMedialForm(ch);
		else
			result = GetInitialForm(ch);
	}
	else
	{
		g_textshaper_state = NOT_JOINING;
		if (joinRight)
			result = GetFinalForm(ch);
		else
			result = GetIsolatedForm(ch);
	}
	return result ? result : ch;
}

int TextShaper::ConsumeJoiners(const uni_char* str, int len)
{
	int consumed = 0;
	while (consumed < len && IS_JOINER(*str))
	{
		g_textshaper_state = *str == 0x200c ? NOT_JOINING : JOINING_LEFT;
		++str;
		++consumed;
	}
	return consumed;
}
TextShaper::JoiningClass TextShaper::GetNextJoiningClass(const uni_char* str, int len)
{
	int charlen = 0;
	while (len > 0 && GetJoiningClass(Unicode::GetUnicodePoint(str, len, charlen)) == TRANSPARENT_JOINING)
	{
		len -= charlen;
		str += charlen;
	}
	return len > 0 ? GetJoiningClass(Unicode::GetUnicodePoint(str, len, charlen)) : NON_JOINING;
}
UnicodePoint TextShaper::GetShapedChar(const uni_char *str, int len, int& chars_read)
{
	chars_read = 0;

	int consumed = ConsumeJoiners(str, len);
	len -= consumed;

	// a string of only joiners received, what to do?
	// atm, treating it as a regular character, so it'll be drawn as-is.
	if (!len)
	{
		chars_read = 1;
		return *str;
	}

	str += consumed;
	chars_read += consumed;

	int maxlen = len;
	const uni_char *ptr = str;

	int transpcount = 0;
	int buflen = 0;
	UnicodePoint buf[3]; /* ARRAY OK 2011-10-27 peter */
	BOOL match;
	OP_ASSERT(maxlen > 0);
	int charlen = 0;
	UnicodePoint ch = Unicode::GetUnicodePoint(ptr, maxlen, charlen);
	UnicodePoint result = GetJoinedChar(ch, GetNextJoiningClass(ptr+charlen, maxlen-charlen));
	JoiningState s = g_textshaper_state;
	ptr += charlen; chars_read += charlen; maxlen -= charlen;
	ch = result ? result : ch;
	do
	{
		match = FALSE;

		if (GetJoiningClass(ch) == TRANSPARENT_JOINING)
		{
			transpcount ++;
		}
		else
		{
			if (buflen < 3)
			{
				for (int i=0; ligaturetable[i]; i+=4)
				{
					int j;
					for (j=0; j<buflen; j++)
					{
						if (ligaturetable[i+j] != buf[j])
							break;
					}
					if (j == buflen && ligaturetable[i+j] == ch)
					{
						match = TRUE;
						buf[buflen++] = ch;
						if (buflen == 3 || ligaturetable[i+buflen] == 0)
						{
							// We have a complete match
							chars_read = transpcount + ptr - str;
							if (GetJoiningClass(ligaturetable[i+3]) != TRANSPARENT_JOINING)
								g_textshaper_state = (JoinsLeft(GetJoiningClass(ligaturetable[i+3])) &&
									JoinsRight(GetNextJoiningClass(ptr, maxlen))) ?
										JOINING_LEFT : NOT_JOINING;
							return ligaturetable[i+3];
						}
					}
				}
			}
		}

		if (maxlen <= 0 || !match)
			break;

		OP_ASSERT(maxlen > 0);
		ch = GetJoinedChar(Unicode::GetUnicodePoint(ptr, maxlen, charlen), GetNextJoiningClass(ptr+charlen, maxlen-charlen));
		ptr += charlen; maxlen -= charlen;
	} while (TRUE);

	// might have consumed more characters because of a partial ligature match,
	// so we need to restore
	ptr = str+1; maxlen = len-1;
	g_textshaper_state = s;

	consumed = ConsumeJoiners(ptr, maxlen);
	chars_read += consumed;
	return result;
}

OP_STATUS TextShaper::Prepare(const uni_char *src, int slen,
							   uni_char *&dest, int &dlen)
{
	ResetState();

	dlen = 0;
	dest = 0;

	/* NOTE: The size calculation of tempbuf assumes that dest will contain
	   the same number of or less characters than src. With the current
	   implementation, that is OK. */

	TempBuffer *tmpbuf = g_opera->display_module.GetTextShaperBuf();

	// One extra byte for the null terminator
	RETURN_IF_ERROR(tmpbuf->Expand(MAX(slen + 1, 30)));
	dest = tmpbuf->GetStorage();

	// Source and destination indices
	int si = 0, di = 0;

	while(si < slen)
	{
#ifdef SHAPE_DEVANAGARI
		// devanagari (dependent) vowel sign i (U+093f) appears to left of
		// the syllable it changes, despite all other dependent vowel signs
		// appearing to the right. so they are swapped.
		// see pp307-308 in "unicode demystified" and the devanagari
		// codeblock (U+0900-U+097F)
		if (IS_DEVANAGARI(src[si]) && // devanagari range
			si+1 < slen &&
			src[si+1] == 0x093f) // devanagari vowel sign i
		{
			dest[di++] = src[si+1];
			dest[di++] = src[si];
			si += 2;
			continue;
		}
#endif // SHAPE_DEVANAGARI

		int num_read_chars;

		UnicodePoint shaped = GetShapedChar(&src[si], slen-si, num_read_chars);
		di += Unicode::WriteUnicodePoint(&dest[di], shaped);
		si += num_read_chars;

		OP_ASSERT(num_read_chars > 0); // Or we're stuck in an infinite loop
	}
	// Null-terminate
	dest[di] = 0;

	dlen = di;

	return OpStatus::OK;
}

BOOL TextShaper::NeedsTransformation(UnicodePoint up)
{
	// FIXME: if text shaping is to be performed for non-BMP unicode
	// points above code has to be fixed
	if (up > 0xffff)
		return FALSE;

	// "Remember, remember the fifth of November,
	// The gunpowder, treason and plot,
	// I know of no reason why the gunpowder treason
	// Should ever be forgot."
	//
	// it is assumed (in VisualDevice::TransformText) that the need for text
	// shaping can be determined using only the first character in a sequence.
	// this should probably be changed when (if?) we ever get more bits
	// available in the WordInfo struct, so we can tag a string for shaping
	// during layout.

	if (
		IS_JOINER(up)
#ifdef SHAPE_ARABIC
		|| IS_ARABIC(up)
#endif // SHAPE_ARABIC
#ifdef SHAPE_MALAYALAM
			// better test would be for 0x0d4d - virama, see comment above
		|| IS_MALAYALAM(up)
#endif // SHAPE_MALAYALAM
#ifdef SHAPE_DEVANAGARI
		|| IS_DEVANAGARI(up)
#endif // SHAPE_DEVANAGARI
		)
		return TRUE;
	return FALSE;
}
BOOL TextShaper::NeedsTransformation(const uni_char *str, int len)
{
	while (len > 0)
	{
		int consumed;
		UnicodePoint up = Unicode::GetUnicodePoint(str, len, consumed);
		str += consumed;
		len -= consumed;
		OP_ASSERT(len >= 0);
		if (NeedsTransformation(up))
			return TRUE;
	}
	OP_ASSERT(!len);
	return FALSE;
}

#endif // TEXTSHAPER_SUPPORT
