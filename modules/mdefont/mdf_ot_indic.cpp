/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** wonko
*/

#include "core/pch.h"

#ifdef MDEFONT_MODULE
#ifdef MDF_OPENTYPE_SUPPORT

#include "modules/mdefont/mdf_ot_indic.h"
#include "modules/unicode/unicode.h"

// controlls behavior of indic_reorder_syllable
#define MOVE_INITIAL_DEAD_RA
#define REORDER_REORDRANTS
#define MOVE_NUKTA

#define ZWNJ     0x200c
#define ZWJ      0x200d

BOOL IsReordrant(const uni_char ch)
{
	// NOTE: keep this list ordered, or the early bail might fail
	const uni_char Reordrants[] = {
		0x093f, // devanagari vowel sign i
		0x09bf, // bengali vowel sign i
		0x09c7, // bengali vowel sign e
		0x09c8, // bengali vowel sign ai
		0x0a3f, // gurmukhi vowel sign i
		0x0abf, // gujarati vowel sign i
		0x0b47, // oriya vowel sign e
		0x0bc6, // tamil vowel sign e
		0x0bc7, // tamil vowel sign ee
		0x0bc8, // tamil vowel sign ai
		0x0d46, // malayalam vowel sign e
		0x0d47, // malayalam vowel sign ee
		0x0d48, // malayalam vowel sign ai
		0x0dd9, // sinhala vowel sign kombuva
		0x0dda, // sinhala vowel sign diga kombuva
		0x0ddb, // sinhala vowel sign kombu deka
		0x1031, // myanmar vowel sign e
		0x17be, // khmer vowel sign oe
		0x17c1, // khmer vowel sign e
		0x17c2, // khmer vowel sign ae
		0x17c3, // khmer vowel sign ai
	};
	UINT32 i = 0;
	const UINT32 TabSize = sizeof(Reordrants)/sizeof(*Reordrants);
	if (ch > Reordrants[TabSize-1])
		return FALSE;
	while (i < TabSize && ch > Reordrants[i])
		++i;
	return i < TabSize && ch == Reordrants[i];
}

// stupid macros for adding ranges
#define BEGIN_INDIC_RANGES(r, c) {										\
	OP_ASSERT(!r.ranges);												\
	r.ranges = OP_NEWA(IndicScriptRange::Ranges::RangeInfo, c);			\
	if (!r.ranges) return OpStatus::ERR_NO_MEMORY;						\
	r.count = c; IndicScriptRange::Ranges::RangeInfo* rp = ranges.ranges; int i = 0;
#define SET_INDIC_RANGE(s, e, f) rp[i].start = s; rp[i].end = e; rp[i].flag = f; ++i;
#define END_INDIC_RANGES(r) OP_ASSERT(i == r.count); }
OP_STATUS init_devanagari_ranges(IndicScriptRange::Ranges& ranges)
{
	BEGIN_INDIC_RANGES(ranges, 5)
	SET_INDIC_RANGE(0x0904, 0x0914, IndicScriptRange::INDEP_VOWEL)
	SET_INDIC_RANGE(0x093e, 0x094c, IndicScriptRange::DEP_VOWEL)
	SET_INDIC_RANGE(0x0915, 0x0939, IndicScriptRange::CONSONANT)
  	SET_INDIC_RANGE(0x0958, 0x095f, IndicScriptRange::CONSONANT)
  	SET_INDIC_RANGE(0x0966, 0x096f, IndicScriptRange::DIGIT)
	END_INDIC_RANGES(ranges)
	return OpStatus::OK;
}
OP_STATUS init_kannada_ranges(IndicScriptRange::Ranges& ranges)
{
	BEGIN_INDIC_RANGES(ranges, 5)
	SET_INDIC_RANGE(0x0c85, 0x0c94, IndicScriptRange::INDEP_VOWEL)
	SET_INDIC_RANGE(0x0cbe, 0x0ccc, IndicScriptRange::DEP_VOWEL)
	SET_INDIC_RANGE(0x0c95, 0x0cb9, IndicScriptRange::CONSONANT)
	SET_INDIC_RANGE(0x0c80, 0x0cff, IndicScriptRange::BELOW_BASE)
	SET_INDIC_RANGE(0x0ce6, 0x0cef, IndicScriptRange::DIGIT)
	END_INDIC_RANGES(ranges)
	return OpStatus::OK;
}
OP_STATUS init_gujarati_ranges(IndicScriptRange::Ranges& ranges)
{
	BEGIN_INDIC_RANGES(ranges, 4)
	SET_INDIC_RANGE(0x0a85, 0x0a94, IndicScriptRange::INDEP_VOWEL)
	SET_INDIC_RANGE(0x0abe, 0x0acc, IndicScriptRange::DEP_VOWEL)
	SET_INDIC_RANGE(0x0a95, 0x0ab9, IndicScriptRange::CONSONANT)
	SET_INDIC_RANGE(0x0ae6, 0x0aef, IndicScriptRange::DIGIT)
	END_INDIC_RANGES(ranges)
	return OpStatus::OK;
}
OP_STATUS CreateScriptRange(const IndicGlyphClass::Script s,
							IndicScriptRange*& script, IndicScriptRange::Ranges& ranges)
{
	switch(s)
	{
	case IndicGlyphClass::DEVANAGARI:
		RETURN_IF_ERROR( init_devanagari_ranges(ranges) );
		script = OP_NEW(IndicScriptRange, (0x0930, 0x093c, 0x094d,
									  &ranges,
									  0x0901, 0x097f));
		return script ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		break;
	case IndicGlyphClass::KANNADA:
		RETURN_IF_ERROR( init_kannada_ranges(ranges) );
		script = OP_NEW(IndicScriptRange, (0x0cb0, 0x0cbc, 0x0ccd,
									  &ranges,
									  0x0c80, 0x0cff));
		return script ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		break;
	case IndicGlyphClass::GUJARATI:
		RETURN_IF_ERROR( init_gujarati_ranges(ranges) );
		script = OP_NEW(IndicScriptRange, (0x0ab0, 0x0abc, 0x0acd,
									  &ranges,
									  0x0a80, 0x0aff));
		return script ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		break;
	}
	OP_ASSERT(!"FIXME: script not implemented");
	script = 0;
	ranges.ranges = 0;
	ranges.count = 0;
	return OpStatus::ERR;
}


IndicScriptRange::IndicScriptRange(const uni_char ra, const uni_char nukta, const uni_char virama,
								   const IndicScriptRange::Ranges* const ranges,
								   const uni_char tab_start, const uni_char tab_end)
	: m_ra(ra), m_nukta(nukta), m_virama(virama),
	  m_ranges(ranges),
	  m_tab_start(tab_start), m_tab_end(tab_end)
{}
BOOL IndicScriptRange::InRange(const uni_char ch)
{
	return ch >= m_tab_start && ch <= m_tab_end;
}
IndicScriptRange::Flags IndicScriptRange::GetFlags(const uni_char ch)
{
	Flags flags = m_ranges->GetFlags(ch);
	if (IsReordrant(ch))
		flags = (Flags)(flags | REORDRANT);
	return flags;
}

IndicGlyphClass::IndicGlyphClass()
{
	for (int i = 0; i < (int)UNKNOWN; ++i)
	{
		m_scripts[i] = 0;
		m_script_ranges[i].ranges = 0;
		m_script_ranges[i].count = 0;
	}
}
IndicGlyphClass::~IndicGlyphClass()
{
	for (int i = 0; i < (int)UNKNOWN; ++i)
	{
		OP_DELETEA(m_script_ranges[i].ranges);
		m_script_ranges[i].ranges = 0;
		m_script_ranges[i].count = 0;
		OP_DELETE(m_scripts[i]);
		m_scripts[i] = 0;
	}
}
OP_STATUS IndicGlyphClass::Initialize()
{
	for (int i = 0; i < (int)UNKNOWN; ++i)
		RETURN_IF_ERROR(CreateScriptRange((Script)i, m_scripts[i], m_script_ranges[i]));
	return OpStatus::OK;
}

BOOL IndicGlyphClass::IsIndic(const uni_char ch)
{
	return is_indic(ch);
}
IndicGlyphClass::Script IndicGlyphClass::GetScript(const uni_char ch)
{
	for (int i = 0; i < (int)UNKNOWN; ++i)
		if (m_scripts[i]->InRange(ch))
			return (Script)i;
	return UNKNOWN;
}
const IndicScriptRange& IndicGlyphClass::GetScriptRange(const IndicGlyphClass::Script s)
{
	OP_ASSERT(s < UNKNOWN);
	return *m_scripts[s];
}
IndicScriptRange::Flags IndicGlyphClass::GetFlags(const uni_char ch,
												  IndicGlyphClass::Script s/* = UNKNOWN*/)
{
	// if not told, find out what script this is
	if (s == UNKNOWN)
	{
		s = GetScript(ch);
		if (s == UNKNOWN)
			return (IndicScriptRange::Flags)0;
	}

	return m_scripts[s]->GetFlags(ch);
}

BOOL IndicGlyphClass::IsGraphemeClusterBoundary(const uni_char ch1, const uni_char ch2)
{
	IndicGlyphClass::Script s = GetScript(ch1);

	// unknown or different scripts
	if (s == UNKNOWN || !m_scripts[s]->InRange(ch2))
		return TRUE;

	// not perfect (need string for that), but hopefully good enough
	// to suppress asserts in Unicode::IsLinebreakOpportunity
	return (m_scripts[s]->GetFlags(ch2) & (IndicScriptRange::CONSONANT | IndicScriptRange::INDEP_VOWEL | IndicScriptRange::DIGIT)) != 0;
}



BOOL indic_find_base(const IndicGlyphClass::Script script,
					 const uni_char* sstr, const UINT32 slen, UINT32& base)
{
	IndicScriptRange sr = g_indic_scripts->GetScriptRange(script);

	if (!sstr || !slen)
		return FALSE;

	IndicScriptRange::Flags flags;
    base = slen-1;
    while (!((flags = g_indic_scripts->GetFlags(sstr[base], script)) & IndicScriptRange::CONSONANT) ||
		   flags & IndicScriptRange::POST_BASE || flags & IndicScriptRange::BELOW_BASE)
    {
		if (base == 0)
			// accept initial independent vowel as base
			return (flags & (IndicScriptRange::INDEP_VOWEL | IndicScriptRange::CONSONANT)) != 0;
		--base;
    }
    // below-base
    // if we've found a vattu sequence, move to the real base (since the ra is but part of the vattu)
	if (base >= 2 && sstr[base] == sr.m_ra && sstr[base-1] == sr.m_virama)
	{
		int p = base-2;
		if (base >= 3 && sstr[p] == sr.m_nukta) // might be a nukta after the consonant
			--p;
		if (sstr[p] != sr.m_ra && (sr.GetFlags(sstr[p]) & IndicScriptRange::CONSONANT))
			base = p;
	}
    // FIXME: are there other below-base consonants than vattu?

    return TRUE;
}

void indic_reorder_syllable(IndicGlyphClass::Script script,
							uni_char* sstr, const UINT32 slen, UINT32& base, BOOL& hasReph)
{
	IndicScriptRange sr = g_indic_scripts->GetScriptRange(script);

	UINT32 p = 0;

	if (!sstr || !slen)
		return;

	// if base is a vowel we don't do this
	if (!(base == 0 && (sr.GetFlags(sstr[base]) & IndicScriptRange::INDEP_VOWEL)))
	{
		// FIXME:OPTIMIZE: already done in find_base, shouldn't have to be done again
		UINT32 last = slen-1;
		while (!(sr.GetFlags(sstr[last]) & IndicScriptRange::CONSONANT))
		{
			OP_ASSERT(last > 0);
			--last;
		}

		// move base virama to last consonant
		if (last != base)
		{
			UINT32 hpos = base+1;
			OP_ASSERT(last > base);
			OP_ASSERT(hpos < slen);

#ifdef MOVE_NUKTA
			unsigned char base_has_nukta = 0;
			if (sstr[hpos] == sr.m_nukta)
				base_has_nukta = 1;
			OP_ASSERT(sstr[hpos+base_has_nukta] == sr.m_virama);
#else
			OP_ASSERT(sstr[hpos] == sr.m_virama);
#endif // MOVE_NUKTA

			op_memmove(sstr+hpos, sstr+hpos+1, (last-hpos)*sizeof(uni_char));

#ifdef MOVE_NUKTA
			// if base has nukta it should fall after the moved virama
			sstr[last] = sr.m_nukta;
			sstr[last-base_has_nukta] = sr.m_virama; // overwrite the nukta we just wrote if there isn't one
#else
			sstr[last] = sr.m_virama;
#endif // MOVE_NUKTA
		}
	}

#ifdef MOVE_INITIAL_DEAD_RA
    // move dead RA
    if (base >= 2 && slen > 2 && sstr[0] == sr.m_ra && sstr[1] == sr.m_virama)
    {
		UINT32 newpos = base+1;
		// if base is followed by vattu, the reph falls after the vattu
		if (newpos+1 < slen && sstr[newpos] == sr.m_ra && sstr[newpos+1] == sr.m_virama)
			newpos += 2;
		while (newpos < slen && (sr.GetFlags(sstr[newpos]) & IndicScriptRange::DEP_VOWEL))
			++newpos;
		newpos -= 2; // we're moving two steps
		OP_ASSERT(newpos <= slen-2); // sanity check
		op_memmove(sstr, sstr+2, newpos*sizeof(uni_char));
		sstr[newpos] = sr.m_ra;
		sstr[newpos+1] = sr.m_virama;
		OP_ASSERT(base >= 2);
		base -= 2;

		hasReph = TRUE;
    }
	else
		hasReph = FALSE;
#endif

    p = 0;
    // reorder vattu:s (any vattu on the base is already taken care of)
    while (p+2 < base)
    {
		if (sstr[p] != sr.m_ra && (sr.GetFlags(sstr[p]) & IndicScriptRange::CONSONANT))
		{
			unsigned int q = p+1;
			// might be a nukta after the consonant in a vattu sequence
			if (p+3 < base && sstr[q] == sr.m_nukta)
				++q;
			if (sstr[q] == sr.m_virama && sstr[q+1] == sr.m_ra)
			{
				sstr[p+1] = sr.m_ra;
				sstr[p+2] = sr.m_virama;
				if (p == q+2)
				{
					sstr[p+3] = sr.m_nukta;
					++p;
				}
				p += 2; // remember that if increases one
			}
		}
		++p;
    }

#ifdef REORDER_REORDRANTS
    // reorder reordrant vowels
    p = base+1;
    while (p < slen)
    {
		if (IsReordrant(sstr[p]))
		{
// WONKO: seems like reordrants should be put first, i.e. before any dead consonants
// selftest with sequence [938 94d 925 93f 930] tests this
#if 0
			// move to left of base
			uni_char c = sstr[p];
			op_memmove(sstr+base+1, sstr+base, (p-base)*sizeof(uni_char));
			sstr[base] = c;
			++base;
			++p;
#else
			// move to left of base
			uni_char c = sstr[p];
			op_memmove(sstr+1, sstr, p*sizeof(uni_char));
			sstr[0] = c;
			++base;
			++p;
#endif
		}
		else
			++p;
    }
#endif

	// place nuktas last (locally - i.e. before the next consonant, not last in syllable)
	// so they don't interfere with ligature lookup
	for (p = 0; p < slen-1; ++p)
	{
		if (sstr[p] == sr.m_nukta)
		{
			unsigned int q = p+1;
			while (q < slen && !(sr.GetFlags(sstr[q]) & IndicScriptRange::CONSONANT))
				++q;
			--q;
			if (p < q && q < slen)
			{
				op_memmove(sstr+p, sstr+p+1, q-p);
				sstr[q] = sr.m_nukta;
			}

		}
	}
}

BOOL indic_find_syllable(IndicGlyphClass::Script script,
						 const uni_char* str, const UINT32 len, UINT32& slen)
{
/*
  C  - consonant
  N  - nukta
  H  - virama (halant)
  VO - independent vowel
  M  - matra (dependent vowel)
  VM - vowel modifier (eg canabrindu, anusvara (?), visagara)
  SM - stress or tone mark (eg udatta, anudatta, acute, grave)

  dead consonant ->
    <C [N] H>
  syllable ->
    <dead consonant>* C [M] [VM] [SM] |
    <dead consonant>* C H |
    VO [VM] [SM]
*/

	IndicScriptRange sr = g_indic_scripts->GetScriptRange(script);

    UINT32 i = 0;

    if (!str || !len)
		return FALSE;

    // sanity check: we should always get a consonant or an independent vowel to start with
    if (!(sr.GetFlags(str[i]) & (IndicScriptRange::CONSONANT | IndicScriptRange::INDEP_VOWEL)))
    {
		// If len == 1 we might get here because of truncated input but if len > 1 and we get
		// here it probably wasn't indic text after all. Might be mixed indic and non-indic or
		// just random characters that were mislabeled as an indic script.
		return FALSE;
    }

    // skip dead consonants
    while (i+1 < len && (sr.GetFlags(str[i]) & IndicScriptRange::CONSONANT))
    {
		// vattu
		if (str[i] != sr.m_ra && i+2 < len && str[i+1] == sr.m_virama && str[i+2] == sr.m_ra)
		{
			i += 3;
		}
		else
		{
			UINT32 j = 1;

			if (str[i+j] == sr.m_nukta)
				++j;

			// end of string, can't be a dead consonant
			if (i+j == len)
				break;

			if (str[i+j] == sr.m_virama)
			{
				i += j+1;
			}
			else
				break;
		}
    }

    // sanity check
    OP_ASSERT(i <= len);

    // rule 2, either a vowel will follow or we're at the end of the string
    if (i > 0 && (sr.GetFlags(str[i]) & IndicScriptRange::INDEP_VOWEL) || (i == len || !sr.InRange(str[i])))
    {
		slen = i;
		return TRUE;
    }

    // rule 1 and three, we're at the last consonant in the syllable, and need to continue until
	// we find the next one or an independent vowel
    ++i;
    while (i < len && sr.InRange(str[i]) &&
		   !(sr.GetFlags(str[i]) & (IndicScriptRange::CONSONANT | IndicScriptRange::INDEP_VOWEL)))
		++i;
    slen = i;
    return TRUE;
}

#endif // MDF_OPENTYPE_SUPPORT
#endif // MDEFONT_MODULE
