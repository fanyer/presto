/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#if defined(VISITED_PAGES_SEARCH) || defined(USE_SEARCH_ENGINE_WORDHIGHLIGHTER)

#include "modules/search_engine/WordSegmenter.h"

WordSegmenter::WordSegmenter(unsigned flags)
#ifdef USE_UNICODE_SEGMENTATION
	: m_boundary_finder(UnicodeSegmenter::Word)
#endif
{
	m_original_string = NULL;
	m_word_break = NULL;
	m_original_string_end = NULL;
	m_flags = flags;
}

WordSegmenter::~WordSegmenter()
{
	if ((m_flags & DontCopyInputString) == 0)
		OP_DELETEA(const_cast<uni_char*>(m_original_string));
}

OP_STATUS WordSegmenter::Set(const uni_char *string)
{
	if (m_original_string != NULL && (m_flags & DontCopyInputString) == 0)
	{
		OP_DELETEA(const_cast<uni_char*>(m_original_string));
		m_original_string = NULL;
	}

	if (string == NULL)
	{
		m_original_string_end = NULL;
		m_word_break = NULL;

		return OpStatus::OK;
	}

	if ((m_flags & DontCopyInputString) == 0)
		RETURN_OOM_IF_NULL(m_original_string = PreprocessDup(string));
	else
		m_original_string = string;

	m_original_string_end = m_original_string + uni_strlen(m_original_string);

	m_word_break = m_original_string;

	return OpStatus::OK;
}

/* special character ranges:
 0E00 .. 0E7F Thai
 0E80 .. 0EFF Lao
 0F00 .. 0FFF Tibetan
 1000 .. 109F Myanmar (Burma)
 1100 .. 11FF Hangul (Korea)

 1780 .. 17FF Khmer (Cambodia)

 19E0 .. 19FF Khmer Symbols

 2E80 .. 2FFF CJK radicals
 3000 .. 303F CJK symbols and punctuation
 3040 .. 309F Hiragana (Japan)
 30A0 .. 30FF Katakana (Japan)

 3100 .. 312F Bopomofo (Taiwan, not special, has spaces!)

 3130 .. 318F Hangul (Korea)
 3190 .. 319F Kanbun (Japan)

 31A0 .. 31BF Bopomofo (not special, has spaces!)

 31C0 .. 31EF CJK strokes
 31F0 .. 31FF Katakana
 3200 .. 9FBF CJK
/9FC0 .. 9FFF nothing
 A000 .. A4CF Yi (China)
/A4D0 .. A6FF nothing
 A700 .. A71F Chinese tone modifiers
/A720 .. A7FF nothing
 AC00 .. D7AF Hangul
/D7B0 .. F8FF nothing
 F900 .. FAFF CJK

 FF65 .. FF9F Katakana/2
 FFA0 .. FFDF Hangul/2
*/

#define between(i1, c, i2) ((c) >= i1 && (c) <= i2)

#define uni_isthai(c)     (between(0x0E00, c, 0x0EFF) || between(0x1000, c, 0x109F) || between(0x1780, c, 0x17FF) || between(0x19E0, c, 0x19FF))
#define uni_isCJK(c)      (between(0x2E80, c, 0x303F) || between(0x31C0, c, 0x31EF) || between(0x3200, c, 0xA7FF) || between(0xF900, c, 0xFAFF))
#define uni_ishiragana(c)  between(0x3040, c, 0x309F)
#define uni_ishangul(c)   (between(0xAC00, c, 0xD7AF) || between(0xFFA0, c, 0xFFDF))
#define uni_iskatakana(c) (between(0x30A0, c, 0x30FF) || between(0x31F0, c, 0x31FF) || between(0xFF65, c, 0xFF9F))

#define uni_nospacelng(c) (between(0x0E00, c, 0x11FF) || between(0x1780, c, 0x17FF) || between(0x19E0, c, 0x19FF) ||\
	                       between(0x2E80, c, 0x30FF) || between(0x3130, c, 0x319F) || between(0x31C0, c, 0xFAFF) || between(0xFF65, c, 0xFFDF))


#ifndef USE_UNICODE_SEGMENTATION

int WordSegmenter::GetCharFlags(UnicodePoint c)
{
	int prop;
	int rv = 0;

	if (c == 0)
		return BreakBefore | BreakAfter;

	prop = Unicode::GetLinebreakProperties(c);

	if ((prop & LINEBREAK_ALLOW_BEFORE) != 0)
		rv |= BreakBefore;
	if ((prop & LINEBREAK_ALLOW_AFTER) != 0)
		rv |= BreakAfter;

	if ((prop & LINEBREAK_PROHIBIT_BEFORE) != 0)
		rv |= NoBreakBefore;
	if ((prop & LINEBREAK_PROHIBIT_AFTER) != 0)
		rv |= NoBreakAfter;

	if (c <= ' ')
	{
		rv |= BlockLimit;
		return rv;
	}

	if (uni_isalnum(c))
		rv |= AlNum;

	if (c <= 127)
	{
		if (CharSearch(block_limits, (unsigned char)c))
			rv |= BlockLimit;

		if (CharSearch(no_block_end, (unsigned char)c))
			rv |= NoBlockEnd;

		return rv;
	}

	rv |= BlockLimit;

	if (c < 0x0E00)
		return rv;

	if (c <= 0x19FF)
	{
		if (c <= 0x0EFF || between(0x1000, c, 0x109F) || between(0x1780, c, 0x17FF) || c >= 0x19E0)
		{
			if (rv & AlNum)
				rv &= ~0xF;

			rv |= Thai;
		}

		return rv;
	}

	if (uni_isCJK(c))
		rv |= CJK;

	if (uni_ishiragana(c))
	{
		if (rv & AlNum)
			rv &= ~0xF;

		rv |= Hiragana;
	}

	if (uni_ishangul(c))
	{
		if (rv & AlNum)
			rv &= ~0xF;

		rv |= Hangul;
	}

	if (uni_iskatakana(c))
	{
		if (rv & AlNum)
			rv &= ~0xF;

		rv |= Katakana;
	}

	return rv;
}

#else  // USE_UNICODE_SEGMENTATION

#define SET_ALNUM(x, char_class) switch (char_class) \
	{ \
	case CC_Nd: \
	case CC_Nl: \
	case CC_No: \
	case CC_Ll: \
	case CC_Lm: \
	case CC_Lo: \
	case CC_Lt: \
	case CC_Lu: \
		x |= AlNum; \
	}

int WordSegmenter::GetCharFlags(UnicodePoint c)
{
	int rv = 0;

	if (c == 0)
		return BreakBefore | BreakAfter;

	if (c <= 127)
		return rv;

	if (c < 0x0E00)
		return rv;

	if (c <= 0x19FF)
	{
		if (c <= 0x0EFF || between(0x1000, c, 0x109F) || between(0x1780, c, 0x17FF) || c >= 0x19E0)
			rv |= Thai;

		return rv;
	}

	if (uni_isCJK(c))
		rv |= CJK;

	if (uni_ishiragana(c))
		rv |= Hiragana;

	if (uni_ishangul(c))
		rv |= Hangul;

	if (uni_iskatakana(c))
		rv |= Katakana;

	return rv;
}
#endif  // USE_UNICODE_SEGMENTATION


/*
 what makes a word:
 * single CJK (chinese, hanja, kanji, yi) character
 * tibetan syllable
 * couples of katakana characters (or a single self-standing character)
 * couples of hangul characters (or a single self-standing character)
 * triplets of hiragana characters (singles and couples are discarded)
 * triplets of thai/khmer/lao/myanmar characters (or less self-standing characters)
 * other alphanum characters

  what makes a block:
  * Block is a sequence of ascii characters beginning with a word.
  * Block contains at least two words separated by not-block-breaking characters.
  * Block can contain "=.,:;&\?!-@", but doesn't end with them.
    (aargh!@#$5 is a valid block)
*/
void WordSegmenter::GetNextToken(Word &token)
{
	const uni_char *pos, *next_pos;
	int c_prop, next_prop;
	BOOL word_end;
	UnicodePoint uc, next_uc;
	int uc_width, next_uc_width;

	if (m_original_string == NULL)
	{
		token.Empty();
		return;
	}

	if (m_original_string_end - m_word_break)
		uc = Unicode::GetUnicodePoint(m_word_break, (int)(m_original_string_end - m_word_break), uc_width);
	else
	{
		uc = 0;
		uc_width = 0;
	}
#ifdef USE_UNICODE_SEGMENTATION
	m_boundary_finder.Reset();

	if (m_boundary_finder.FindBoundary(m_word_break, uc_width) == 0)
		m_boundary_finder.FindBoundary(m_word_break, uc_width);

	c_prop = GetCharFlags(uc);
	SET_ALNUM(c_prop, m_boundary_finder.LastClass())
#else
	c_prop = GetCharFlags(uc);
#endif

	for (pos = m_word_break; pos < m_original_string_end;
		 pos = next_pos, uc = next_uc, uc_width = next_uc_width, c_prop = next_prop)
	{
		next_pos = pos + uc_width;
		if (next_pos < m_original_string_end)
			next_uc = Unicode::GetUnicodePoint(next_pos, (int)(m_original_string_end - next_pos), next_uc_width);
		else
		{
			next_uc_width = 1;
			next_uc = 0;
		}

		next_prop = GetCharFlags(next_uc);
#ifdef USE_UNICODE_SEGMENTATION
		if ((c_prop & NoSpaceLng) != (next_prop & NoSpaceLng))
		{
			word_end = TRUE;
			if (uni_isalnum(next_uc))
				next_prop |= AlNum;

			if (m_boundary_finder.FindBoundary(next_pos, next_uc_width) == 0)
				m_boundary_finder.FindBoundary(next_pos, next_uc_width);
		}
		else {
			word_end = (m_boundary_finder.FindBoundary(next_pos, next_uc_width) == 0);
			if (word_end)
				m_boundary_finder.FindBoundary(next_pos, next_uc_width);

			// workaround for unicode always breaking on hiragana characters
			if ((c_prop & Hiragana) != 0 && (next_prop & Hiragana) != 0)
				word_end = FALSE;
			// workaround for unicode always breaking on thai characters
			else if ((c_prop & Thai) != 0 && (next_prop & Thai) != 0)
				word_end = FALSE;

			SET_ALNUM(next_prop, m_boundary_finder.LastClass())
		}

		// Additional word breaks for fine-grained word-segmenting
		if ((m_flags & FineSegmenting) != 0)
		{
			WordBreakType wb1 = Unicode::GetWordBreakType(uc);
			WordBreakType wb2 = Unicode::GetWordBreakType(next_uc);
			if (((wb1 == WB_ALetter) != (wb2 == WB_ALetter)) ||
				((wb1 == WB_Numeric) != (wb2 == WB_Numeric)) ||
				(Unicode::IsLower(uc) && Unicode::IsUpper(next_uc)))
				word_end = TRUE;
		}
#else
		word_end = ((c_prop & BreakAfter) != 0 && (next_prop & NoBreakBefore) == 0) ||
			((next_prop & BreakBefore) != 0 && (c_prop & NoBreakAfter) == 0) ||
			(c_prop & NoSpaceLng) != (next_prop & NoSpaceLng);
#endif

		// skip leading spaces/punctuation
		if ((c_prop & AlNum) == 0 && pos == m_word_break)
		{
			m_word_break = next_pos;
			continue;
		}

		// word end
		if (word_end)  // change of script
		{
			token.Set(m_word_break, (int)(next_pos - m_word_break));

			m_word_break = next_pos;
			return;
		}

		// japanese/korean, thai/lao/khmer/myanmar
		if ((m_flags & DisableNGrams) == 0 &&
			((pos - m_word_break >= 1 && (c_prop & (Katakana | Hangul)) != 0) ||
			(pos - m_word_break >= 2 && (c_prop & (Thai | Hiragana)) != 0)))
		{
			token.Set(m_word_break, (int)(next_pos - m_word_break));
			int width;
			Unicode::GetUnicodePoint(m_word_break, (int)(m_original_string_end - m_word_break), width);
			m_word_break += width;
			return;
		}
	}

	token.Empty();
}

OP_BOOLEAN WordSegmenter::GetNextToken(OpString &token)
{
	Word tmp;
	GetNextToken(tmp);
	RETURN_IF_ERROR(token.Set(tmp.ptr, tmp.len));
	return tmp.IsEmpty() ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
}

TVector<WordSegmenter::Word> *WordSegmenter::GetTokens(void)
{
	OpAutoPtr< TVector<Word> > rv;
	Word token;

	rv.reset(OP_NEW(TVector<Word>, ()));
	if (rv.get() == NULL)
		return NULL;

	GetNextToken(token);
	while (!token.IsEmpty())
	{
		RETURN_VALUE_IF_ERROR(rv->Add(token), NULL);
		GetNextToken(token);
	}

	return rv.release();
}

TVector<uni_char *> *WordSegmenter::Parse(BOOL *last_is_prefix)
{
	OpAutoPtr< TVector<uni_char *> > rv;
	Word token;
	uni_char *str;

	if (last_is_prefix != NULL)
		*last_is_prefix = FALSE;

	rv.reset(OP_NEW(TVector<uni_char *>, (&UniStrCompare, &PtrDescriptor<uni_char>::DestructArray)));
	if (rv.get() == NULL)
		return NULL;

	GetNextToken(token);
	while (!token.IsEmpty())
	{
    	if ((str = token.Extract()) == NULL)
    		return NULL;
		RETURN_VALUE_IF_ERROR(rv->Add(str), NULL);
		if (last_is_prefix != NULL)
			*last_is_prefix = (*(token.ptr+token.len) == '\0'); // Prefix only if the last word ends the string
		GetNextToken(token);
	}

	return rv.release();
}

#ifdef USE_UNICODE_SEGMENTATION
BOOL WordSegmenter::WordBreak(const uni_char *buf, const uni_char *s, BOOL fine_segmenting)
{
	const uni_char *gc_boundary;
	int boundary;
	UnicodeSegmenter segmenter(UnicodeSegmenter::Word);

	if (s <= buf)
		return TRUE;

	if (!UnicodeSegmenter::IsGraphemeClusterBoundary(s[-1], s[0]))
		return FALSE;

	WordBreakType wb1 = Unicode::GetWordBreakType(s[-1]);
	WordBreakType wb2 = Unicode::GetWordBreakType(s[0]);

	// Override rule WB13 in UnicodeSegmenter
	if (wb1 == WB_Katakana || wb2 == WB_Katakana)
		return TRUE;

	// Additional word breaks for fine-grained word-segmenting
	if (fine_segmenting)
	{
		if (((wb1 == WB_ALetter) != (wb2 == WB_ALetter)) ||
			((wb1 == WB_Numeric) != (wb2 == WB_Numeric)) ||
			(Unicode::IsLower(s[-1]) && Unicode::IsUpper(s[0])))
			return TRUE;
	}

	gc_boundary = s - 1;
	while (gc_boundary > buf && !UnicodeSegmenter::IsGraphemeClusterBoundary(gc_boundary[-1], gc_boundary[0]))
		--gc_boundary;

	if ((boundary = segmenter.FindBoundary(gc_boundary, (int)(s - gc_boundary + 1))) == 0)
		boundary = segmenter.FindBoundary(gc_boundary, (int)(s - gc_boundary + 1));
	return boundary == s - gc_boundary;
}
#else
BOOL WordSegmenter::WordBreak(const uni_char *buf, const uni_char *s)
{
	int f1, f2;

	if (s > buf)
		f1 = GetCharFlags(s[-1]);
	else
		f1 = GetCharFlags(0);
	f2 = GetCharFlags(*s);

	return (f1 & AlNum) == 0 || (f2 & AlNum) == 0 ||
		((f1 & BreakAfter) != 0 && (f2 & NoBreakBefore) == 0) || ((f2 & BreakBefore) != 0 && (f1 & NoBreakAfter) == 0);
}
#endif

BOOL WordSegmenter::IsInvisibleWordCharacter(const uni_char ch)
{
	return Unicode::GetWordBreakType((UnicodePoint)ch) == WB_Format;
}

uni_char *WordSegmenter::PreprocessDup(const uni_char *src)
{
	if (!src)
		return NULL;

	uni_char *dst = OP_NEWA(uni_char, uni_strlen(src)+1);
	if (!dst)
		return NULL;

	uni_char *p = dst;
	uni_char ch;
	while ((ch = *src++) != 0)
		if (!IsInvisibleWordCharacter(ch))
			*p++ = ch;
	*p = 0;

	return dst;
}

BOOL WordSegmenter::UniStrCompare(const void *left, const void *right)
{
	return uni_strcmp(*(uni_char**)left, *(uni_char**)right) < 0;
}

#endif  // VISITED_PAGES_SEARCH

