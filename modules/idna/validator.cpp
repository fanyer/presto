/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/idna/idna.h"
#include "modules/unicode/unicode_stringiterator.h"

// Exception table
struct Exception_st
{
	UnicodePoint codepoint;
	IDNALabelValidator::IDNAProperty value;
};

#include "modules/idna/tables/exceptions.inl"

BOOL IDNALabelValidator::IsInExceptions(UnicodePoint codepoint, IDNAProperty &exception_value)
{
	// Binary search the code point in the exceptions_table
	int low = 0;
	int high = ARRAY_SIZE(exceptions_table)-1;
	int middle;
	while (low<=high)
	{
		middle = (high+low) >> 1;
		if (exceptions_table[middle].codepoint == codepoint)
		{
			exception_value = exceptions_table[middle].value;
			return TRUE;
		}

		if (exceptions_table[middle].codepoint < codepoint)
			low = middle+1;
		else
			high = middle-1;
	}
	return FALSE;
}

BOOL IDNALabelValidator::IsInBackwardCompatible(UnicodePoint codepoint, IDNAProperty &exception_value)
{
	/**
	 * This category includes the code points that property values in
	 * versions of Unicode after 5.2 have changed in such a way that the
	 * derived property value would no longer be PVALID or DISALLOWED.  If
	 * changes are made to future versions of Unicode so that code points
	 * might change the property value from PVALID or DISALLOWED, then this
	 * table can be updated and keep special exception values so that the
	 * property values for code points stay stable. This set that is at release of
	 * rfc5892 document is empty.
	 */
	return FALSE;
}

BOOL IDNALabelValidator::IsUnassigned(UnicodePoint codepoint)
{
	BOOL non_character = codepoint >= 0xFDD0 && codepoint <= 0xFDEF || ((codepoint & 0xFFFE) == 0xFFFE);
	return !non_character && Unicode::GetCharacterClass(codepoint) == CC_Unknown;
}

BOOL IDNALabelValidator::IsUnstable_L(UnicodePoint codepoint)
{
#ifdef SUPPORT_UNICODE_NORMALIZE
	if (codepoint == 0x0000)
		return FALSE;
	uni_char s[2]; /* ARRAY OK 2012-02-07 peter */
	uni_char *normalized =
		Unicode::Normalize(s, Unicode::WriteUnicodePoint(s, codepoint), TRUE, TRUE);
	if (!normalized)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	uni_char *casefolded = Unicode::ToCaseFoldFull(normalized);
	OP_DELETEA(normalized);
	if (!casefolded)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	uni_char *renormalized = Unicode::Normalize(casefolded, uni_strlen(casefolded), TRUE, TRUE);\
	OP_DELETEA(casefolded);
	if (!renormalized)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	BOOL result = Unicode::GetUnicodePoint(renormalized, uni_strlen(renormalized)) != codepoint;
	OP_DELETEA(renormalized);

	return result;
#else
	return FALSE;
#endif
}

BOOL IDNALabelValidator::IsLDH(UnicodePoint codepoint)
{
	return codepoint==0x002D || codepoint>=0x0030 && codepoint<=0x0039 || codepoint>=0x0061 && codepoint<=0x007A;
}

BOOL IDNALabelValidator::IsIgnorableProperties(UnicodePoint codepoint)
{
	BOOL non_character = codepoint >= 0xFDD0 && codepoint <= 0xFDEF || ((codepoint & 0xFFFE) == 0xFFFE);
	return non_character
		|| Unicode::CheckPropertyValue(codepoint, PROP_White_Space)
		|| Unicode::CheckDerivedPropertyValue(codepoint, DERPROP_Default_Ignorable_Code_Point);
}

BOOL IDNALabelValidator::IsIgnorableBlocks(UnicodePoint codepoint)
{
	UnicodeBlockType blockType = Unicode::GetUnicodeBlockType(codepoint);
	return blockType==BLK_CombiningDiacriticalMarksforSymbols || blockType==BLK_MusicalSymbols || blockType==BLK_AncientGreekMusicalNotation;
}

BOOL IDNALabelValidator::IsOldHangulJamo(UnicodePoint codepoint)
{
	HangulSyllableType type = Unicode::GetHangulSyllableType(codepoint);
	return type==Hangul_L || type==Hangul_V || type==Hangul_T;
}

BOOL IDNALabelValidator::IsLetterDigit(UnicodePoint codepoint)
{
	CharacterClass cc = Unicode::GetCharacterClass(codepoint);
	return cc==CC_Ll || cc==CC_Lu || cc==CC_Lo || cc==CC_Nd || cc==CC_Lm || cc==CC_Mn || cc==CC_Mc;
}

IDNALabelValidator::IDNAProperty IDNALabelValidator::GetIDNAProperty_L(UnicodePoint codepoint)
{
	IDNAProperty result;
	if (IsInExceptions(codepoint, result))
		return result;
	else if (IsInBackwardCompatible(codepoint, result))
		return result;
	else if (IsUnassigned(codepoint))
		return IDNA_UNASSIGNED;
	else if (IsLDH(codepoint))
		return IDNA_PVALID;
	else if (Unicode::CheckPropertyValue(codepoint, PROP_Join_Control) )
		return IDNA_CONTEXTJ;
	else if (IsUnstable_L(codepoint) || IsIgnorableProperties(codepoint) || IsIgnorableBlocks(codepoint) || IsOldHangulJamo(codepoint))
		return IDNA_DISALLOWED;
	else if (IsLetterDigit(codepoint))
		return IDNA_PVALID;
	return IDNA_DISALLOWED;
}

BOOL IDNALabelValidator::CheckContextualRules(const UnicodeStringIterator &it)
{
	if (*it >= 0x0660 && *it <= 0x0669) // ARABIC-INDIC DIGITS
	{
		for (UnicodeStringIterator it2 = it.FromBeginning(); !it2.IsAtEnd(); ++it2)
			if (*it2 >= 0x06f0 && *it2 <= 0x06f9)
				return FALSE;
		return TRUE;
	}

	if (*it >= 0x06f0 && *it <= 0x06f9) // EXTENDED ARABIC-INDIC DIGITS
	{
		for (UnicodeStringIterator it2 = it.FromBeginning(); !it2.IsAtEnd(); ++it2)
			if (*it2 >= 0x0660 && *it2 <= 0x0669)
				return FALSE;
		return TRUE;
	}

	UnicodeStringIterator before = it, after = it;
	++after;
	bool isAfter = !after.IsAtEnd();
	bool isBefore = !before.IsAtBeginning();
	if (isBefore)
		--before;

	//implements contextual rules defined by document rfc5892 for particular codepoints
	switch (*it)
	{
		case 0x200c: // ZERO WIDTH NON-JOINER
			{
#ifdef SUPPORT_UNICODE_NORMALIZE
				if (isBefore && Unicode::GetCombiningClassFromCodePoint(*before)==9) // "Virama"
					return TRUE;
#endif // SUPPORT_UNICODE_NORMALIZE
				if (!isBefore || !isAfter)
					return FALSE;

				UnicodeStringIterator it2 = before;
				while (!it2.IsAtBeginning() && Unicode::GetUnicodeJoiningType(*it2) == JT_T)
					--it2;
				if (Unicode::GetUnicodeJoiningType(*it2) != JT_L && Unicode::GetUnicodeJoiningType(*it2) != JT_D)
					return FALSE;

				it2 = after;
				while (!it2.IsAtEnd() && Unicode::GetUnicodeJoiningType(*it2) == JT_T)
					++it2;
				if (it2.IsAtEnd() || Unicode::GetUnicodeJoiningType(*it2) != JT_R && Unicode::GetUnicodeJoiningType(*it2) != JT_D)
					return FALSE;
				return TRUE;
			}
		case 0x200d: // ZERO WIDTH JOINER
#ifdef SUPPORT_UNICODE_NORMALIZE
			if (isBefore && Unicode::GetCombiningClassFromCodePoint(*before)==9) // "Virama"
				return TRUE;
#else
			return FALSE;
#endif
		case 0x00b7: // MIDDLE DOT
			return isBefore && isAfter && *before == 0x006c && *after == 0x006c;
#ifdef USE_UNICODE_SCRIPT
		case 0x0375: // GREEK LOWER NUMERAL SIGN (KERAIA)
			return isAfter && Unicode::GetScriptType(*after) == SC_Greek;
		case 0x05f3: // HEBREW PUNCTUATION GERESH
		case 0x05f4: // HEBREW PUNCTUATION GERSHAYIM
			return isBefore && Unicode::GetScriptType(*before) == SC_Hebrew;
		case 0x30fb: // KATAKANA MIDDLE DOT
			{
				for (UnicodeStringIterator it2 = it.FromBeginning(); !it2.IsAtEnd(); ++it2)
				{
					ScriptType st = Unicode::GetScriptType(*it2);
					if (st == SC_Hiragana || st == SC_Katakana || st == SC_Han)
						return TRUE;
				}
				return FALSE;
			}
#endif // USE_UNICODE_SCRIPT
		default:
			return FALSE;
	}
}

BOOL IDNALabelValidator::IsCodePointIDNAValid_L(const UnicodeStringIterator &it)
{
	IDNAProperty idnaproperty = GetIDNAProperty_L(*it);
	switch (idnaproperty)
	{
	case IDNA_PVALID:
		return TRUE;
	case IDNA_DISALLOWED:
	case IDNA_UNASSIGNED:
		return FALSE;
	case IDNA_CONTEXTJ:
	case IDNA_CONTEXTO:
		return CheckContextualRules(it);
	}
	return FALSE;
}

#ifdef SUPPORT_TEXT_DIRECTION
BOOL IDNALabelValidator::CheckBidiRule(const uni_char *label, int len)
{
	// implementation of rfc5893
	UnicodeStringIterator first(label, 0, len);
	UnicodeStringIterator last = first;
	while (!last.IsAtEnd())
		++last;
	--last;

	BidiCategory bidi = Unicode::GetBidiCategory(*first);
	if (bidi != BIDI_L && bidi != BIDI_R && bidi != BIDI_AL)
		return FALSE;

	BOOL RTL = bidi != BIDI_L;

	if (RTL)
	{
		BOOL an_present = FALSE, en_present = FALSE;
		for (UnicodeStringIterator it=first; !it.IsAtEnd(); ++it)
		{
			bidi = Unicode::GetBidiCategory(*it);
			if (bidi != BIDI_R && bidi != BIDI_AL && bidi != BIDI_AN && bidi != BIDI_EN && bidi != BIDI_ES &&
				bidi != BIDI_CS && bidi != BIDI_ET && bidi != BIDI_ON && bidi != BIDI_BN && bidi != BIDI_NSM)
				return FALSE;

			an_present = an_present || bidi == BIDI_AN;
			en_present = en_present || bidi == BIDI_EN;
			if (an_present && en_present)
				return FALSE;
		}

		BOOL wasFirst = FALSE;
		for (UnicodeStringIterator it = last; !wasFirst; --it)
		{
			bidi = Unicode::GetBidiCategory(*it);
			if (bidi == BIDI_R || bidi == BIDI_AL || bidi == BIDI_EN || bidi == BIDI_AN)
				break;
			if (bidi != BIDI_NSM)
				return FALSE;
			wasFirst = it.IsAtBeginning();
		}
	}
	else
	{
		for (UnicodeStringIterator it=first; !it.IsAtEnd(); ++it)
		{
			bidi = Unicode::GetBidiCategory(*it);
			if (bidi != BIDI_L && bidi != BIDI_EN && bidi != BIDI_ES && bidi != BIDI_CS && bidi != BIDI_ET &&
				bidi != BIDI_ET && bidi != BIDI_ON && bidi != BIDI_BN && bidi != BIDI_NSM)
				return FALSE;
		}

		BOOL wasFirst = FALSE;
		for (UnicodeStringIterator it = last; wasFirst; --it)
		{
			bidi = Unicode::GetBidiCategory(*it);
			if (bidi == BIDI_L || bidi == BIDI_EN)
				break;
			if (bidi != BIDI_NSM)
				return FALSE;
			wasFirst = it.IsAtBeginning();
		}
	}
	return TRUE;
}
#endif // SUPPORT_TEXT_DIRECTION

BOOL IDNALabelValidator::IsBlockedAdditionally(UnicodePoint cp)
{
	if (
		cp>=0x0250 && cp<=0x02AF || //IPA Extensions
		cp>=0x1680 && cp<=0x169f || //Ogham
		cp>=0x16a0 && cp<=0x16ff    //Runic
	)
		return TRUE;
	else
		return FALSE;
}

BOOL IDNALabelValidator::IsDomainNameIDNAValid_L(const uni_char *domainname, BOOL additionalBlocking)
{
#ifdef SUPPORT_TEXT_DIRECTION
	size_t domainname_len = uni_strlen(domainname);
	BOOL bidiDomain = FALSE;
	for (UnicodeStringIterator it(domainname, 0, domainname_len); !it.IsAtEnd(); ++it)
	{
		BidiCategory bc = Unicode::GetBidiCategory(*it);
		bidiDomain |= bc == BIDI_R || bc == BIDI_AL || bc == BIDI_AN;
	}
#endif // SUPPORT_TEXT_DIRECTION
	
	const uni_char *comp_end;
	do{
		comp_end = uni_strchr(domainname, '.');
		int label_len = comp_end ? (comp_end - domainname) : uni_strlen(domainname);

		if (label_len)
		{
			if (!IsLabelIDNAValid_L(domainname, label_len, additionalBlocking))
				return FALSE;

#ifdef SUPPORT_TEXT_DIRECTION
			if (label_len && bidiDomain && !CheckBidiRule(domainname, label_len))
				return FALSE;
#endif // SUPPORT_TEXT_DIRECTION
		}

		if (comp_end)
			domainname = comp_end + 1;
	} while (comp_end);

	return TRUE;
}

BOOL IDNALabelValidator::IsLabelIDNAValid_L(const uni_char *label, int len, BOOL additionalBlocking)
{
	UnicodeStringIterator it(label, 0, len);
	while (!it.IsAtEnd() && (IsLDH(*it) || *it == '_')) // Microsoft Windows systems often use underscores in hostnames so we must allow it ascii labels
		++it;
	if (it.IsAtEnd())
		return TRUE;

	for (; !it.IsAtEnd(); ++it)
		if (!IDNALabelValidator::IsCodePointIDNAValid_L(it)
			|| additionalBlocking && IsBlockedAdditionally(*it))
			return FALSE;

	return TRUE;
}
