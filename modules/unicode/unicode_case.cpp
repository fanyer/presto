/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/unicode/unicode.h"
#include "modules/util/handy.h"

struct NonBMPCase
{
	UnicodePoint lower, upper;
};

#include "modules/unicode/tables/caseinfo.inl"

/**
 * Look up the value from the case_info table for a specific character.
 *
 * @param c Character to look up.
 * @return The corresponding value from the case_info table.
 */
unsigned short Unicode::GetCaseInfo(UnicodePoint c)
{
	// Binary search for the code point in the data table
	size_t low = 0;
	size_t high = c < 256 ? CI_MAX_256 : ARRAY_SIZE(case_info_char);
	size_t middle;
	if (c >= 0xff5b)
	{
		// A few non-BMP code points have case mappings, but the vast
		// majority do not. Only flag the ones that have them here.
		if (c >= NONBMP_CASE_LOWEST && c <= NONBMP_CASE_HIGHEST)
		{
			return NONBMP << CASE_INFO_BITS;
		}
		return 0;
	}
	do
	{
		middle = (high + low) >> 1;
		if (case_info_char[middle] <= c)
		{
			if (case_info_char[middle + 1] > c)
			{
				// Codepoint found
				// If value is 0, this character has unspecified case,
				// so do not do any further processing
				if (!case_info_ind[middle])
				{
					return 0;
				}

				// This codepoint contains case information, so look
				// up the data from the second table. This data point
				// contains both the type of character and how to
				// convert it. The lower CASE_INFO_BITS contains an
				// index into the second data table and the following
				// two bits contains information on how to use
				// that value.
				return case_info[case_info_ind[middle] - 1];	// Range hit
			}
			low = middle;
		}
		else
		{
			high = middle;
		}
	}
	while (1);
}

uni_char *Unicode::Lower(uni_char * data, BOOL in_place)
{
	// Possibly allocate a new string
	if (!in_place)
	{
		data = uni_strdup(data);
		if (!data)
		{
			return NULL;
		}
	}

	// Convert every character to lower case
	for (register uni_char * p = data; *p; p++)
	{
		// Hard-code behaviour for ASCII characters
		if (*p < 128)
		{
			if (*p >= 'A' && *p <= 'Z')
				*p |= 0x20;
		}
		else
		{
			// Handle surrogate pairs specifically, there are not many
			// characters beyond BMP that have case mappings anyway.
			if (IsSurrogate(*p) && IsHighSurrogate(*p) && IsLowSurrogate(*(p + 1)))
			{
				UnicodePoint up = DecodeSurrogate(*p, *(p + 1));
				ToLowerInternal(&up);
				MakeSurrogate(up, *p, *(p + 1));
				p ++;
			}
			else
			{
				UnicodePoint up = static_cast<UnicodePoint>(*p);
				ToLowerInternal(&up);
				*p = up;
			}
		}
	}

	return data;
}

/** Look-up function for characters >= 0x80, modifying *p only if needed */
void Unicode::ToLowerInternal(UnicodePoint *p)
{
	// Look up case information in the data table
	unsigned short info;
	if ((info = GetCaseInfo(*p)) != 0)
	{
		// Fetch the index into the third table from the low
		// CASE_INFO_BITS of the value
		short d = case_info_data_table[info & ((1 << CASE_INFO_BITS) - 1)];

		// Fetch how to use the data from the following two bits
		switch (info >> CASE_INFO_BITS)
		{
//		case LOWERDELTA:	// Contains delta to convert from lowercase
//			return c;		// -> do nothing.
		case UPPERDELTA:	// Contains delta to convert to lowercase
		case BOTHDELTA:		// Contains delta to convert to both cases
			*p += d;		// -> add value.
			break;
		case CASEBIT:		// Contains bit mask
			*p |= d;		// -> set bit
			break;
		case BITOFFSET:		// Contains bit mask offset by one codepoint
			*p = ((*p - d) | d) + d;	// -> set bit on offset
			break;
		case LARGEUPPER:	// Contains large delta to convert from lowercase
			*p += d * 2;	// -> add modified value.
			break;
		case NONBMP:		// Linear lookup in non-BMP table.
			for (size_t i = 0; i < ARRAY_SIZE(nonbmp_case); ++ i)
				if (nonbmp_case[i].upper == *p)
				{
					*p = nonbmp_case[i].lower;
					break;
				}
			break;
		}
		/* UNREACHED: The cases above are all there is.
		 * Note that adding a 'default:' causes significant slowdown.
		 */
	}
}

uni_char *Unicode::Upper(uni_char * data, BOOL in_place)
{
	OP_ASSERT(data || !"Unicode::Upper() does not allow NULL in input!");

	// Possibly allocate a new string
	if (!in_place)
	{
		data = uni_strdup(data);
		if (!data)
		{
			return NULL;
		}
	}

	// Convert every character to upper case
	for (register uni_char * p = data; *p; p++)
	{
		// Hard-code behaviour for ASCII characters
		if (*p < 128)
		{
			if (*p >= 'a' && *p <= 'z')
				*p &= ~0x20;
		}
		else
		{
			// Handle surrogate pairs specifically, there are not many
			// characters beyond BMP that have case mappings anyway.
			if (IsSurrogate(*p) && IsHighSurrogate(*p) && IsLowSurrogate(*(p + 1)))
			{
				UnicodePoint up = DecodeSurrogate(*p, *(p + 1));
				ToUpperInternal(&up);
				MakeSurrogate(up, *p, *(p + 1));
				p ++;
			}
			else
			{
				UnicodePoint up = static_cast<UnicodePoint>(*p);
				ToUpperInternal(&up);
				*p = up;
			}
		}
	}

	return data;
}

/** Look-up function for characters >= 0x80, modifying *p only if needed */
void Unicode::ToUpperInternal(UnicodePoint *p)	
{
	// Look up case information in the data table
	unsigned short info;
	if ((info = GetCaseInfo(*p)) != 0)
	{
		// Fetch the index into the second table from the low
		// CASE_INFO_BITS of the value
		short d = case_info_data_table[info & ((1<<CASE_INFO_BITS)-1)];

		// Fetch how to use the data from the following two bits
		switch (info >> CASE_INFO_BITS)
		{
		case LOWERDELTA:	// Contains delta to convert to uppercase
		case BOTHDELTA:		// Contains delta to convert to both cases
			*p -= d;		// -> subtract value.
			break;
//		case UPPERDELTA:	// Contains delta to convert from uppercase
//			return c;		// -> do nothing.
		case CASEBIT:		// Contains bit mask
			*p &= ~d;		// -> unset bit
			break;
		case BITOFFSET:		// Contains bit mask offset by one codepoint
			*p = ((*p - d) & ~d) + d;	// -> unset bit on offset
			break;
		case LARGELOWER:	// Contains large delta to convert from lowercase
			*p -= d * 2;	// -> add modified value.
			break;
		case NONBMP:		// Binary search in non-BMP table.
			{
				size_t low = 0;
				size_t high = ARRAY_SIZE(nonbmp_case);
				size_t middle;
				while (1)
				{
					middle = (high + low) >> 1;
					if (*p == nonbmp_case[middle].lower)
					{
						// Codepoint found
						*p = nonbmp_case[middle].upper;
						break;
					}
					else if (nonbmp_case[middle].lower < *p)
					{
						if (low == middle)
							break;
						low = middle;
					}
					else if (nonbmp_case[middle].lower > *p)
					{
						if (high == middle)
							break;
						high = middle;
					}
				};
			}
			break;
		}
		/* UNREACHED: The cases above are all there is.
		 * Note that adding a 'default:' causes significant slowdown with gcc 3.2.3.
		 */
	}
}

struct CaseFoldSimpleMappingBMP	// 5 bytes
{
	unsigned int code:16;
	int delta:20;
	unsigned int compression:4;
};

struct CaseFoldSimpleMappingNonBMP	// 7 bytes
{
	UnicodePoint code;
	short delta;
	unsigned char compression;
};

struct CaseFoldFullMapping			// 8 bytes
{
	uni_char code;
	uni_char mapping[3];
};

template<class Mapping>
const Mapping *FindMapping(UnicodePoint c, const Mapping *mappingArray, int arraySize, BOOL rangeMode)
{
	OP_ASSERT(mappingArray && arraySize >= 0 || !"Invalid call to FindMapping()");
	// Binary search for the code point in the table
	int low = 0;
	int high = arraySize - 1;
	int middle;
	while (low <= high)
	{
		middle = (high + low) >> 1;
		if (mappingArray[middle].code == c ||
			rangeMode && mappingArray[middle].code < c && mappingArray[middle+1].code > c)
			return &mappingArray[middle];

		if (mappingArray[middle].code < c)
			low = middle + 1;
		else
			high = middle - 1;
	}
	return NULL;
}

#include "modules/unicode/tables/casefold.inl"

UnicodePoint DecompressMapping(UnicodePoint code, int delta, CaseFoldSimpleCompressionType compression)
{
	switch (compression)
	{
	case CFC_SET_BIT0:
		return code | 0x00000001;
	case CFC_CLEAR_BIT0:
		return code & 0xfffffffe;
	case CFC_DELTA_FOR_EVEN:
		return code + (((code + 1) % 2) ? delta : 0);
	case CFC_DELTA_FOR_ODD:
		return code + ((code % 2) ? delta : 0);
	case CFC_NO_COMPRESSION:
	case CFC_DELTA:
	default:
		return code + delta;
	}
}

UnicodePoint Unicode::ToCaseFoldSimple(UnicodePoint c)
{
	if (c > 0xffff)
	{
		const CaseFoldSimpleMappingNonBMP *mapping = FindMapping<CaseFoldSimpleMappingNonBMP>(c, casefold_simple_data_nonbmp, ARRAY_SIZE(casefold_simple_data_nonbmp), TRUE);
		OP_ASSERT(mapping);
		return DecompressMapping(c, mapping->delta, (CaseFoldSimpleCompressionType)mapping->compression);
	}
	else
	{
		const CaseFoldSimpleMappingBMP *mapping = FindMapping<CaseFoldSimpleMappingBMP>(c, casefold_simple_data_bmp, ARRAY_SIZE(casefold_simple_data_bmp), TRUE);
		OP_ASSERT(mapping);
		return DecompressMapping(c, mapping->delta, (CaseFoldSimpleCompressionType)mapping->compression);
	}
}

int Unicode::ToCaseFoldFull(UnicodePoint c, UnicodePoint *output)
{
	const CaseFoldFullMapping *full_mapping = FindMapping<CaseFoldFullMapping>(c, casefold_full_data, ARRAY_SIZE(casefold_full_data), FALSE);
	if (full_mapping)
	{
		int i;
		for (i = 0; i < 3 && full_mapping->mapping[i]; ++i)
			*(output++) = full_mapping->mapping[i];
		return i; 
	}
	*output = ToCaseFoldSimple(c);
	return 1;
}

uni_char *Unicode::ToCaseFoldFull(const uni_char *s)
{
	OP_ASSERT(s || !"Invalid call to Unicode::ToCaseFoldFull()");
	size_t len = uni_strlen(s);
	uni_char *data = OP_NEWA(uni_char, len * 3 + 1); // to be on the safe side we must assume that the length will be tripled
	if (!data)
		return NULL;
	
	const uni_char *end = s + len;
	uni_char *writing_pos = data;
	while (s<end)
	{
		int consumed;
		UnicodePoint cp = GetUnicodePoint(s, len, consumed);
		s+=consumed; 
		len-=consumed;
		
		const CaseFoldFullMapping *full_mapping = FindMapping<CaseFoldFullMapping>(cp, casefold_full_data, ARRAY_SIZE(casefold_full_data), FALSE);
		if (full_mapping)
			for (int i = 0; i < 3 && full_mapping->mapping[i]; ++i)
				writing_pos += WriteUnicodePoint(writing_pos, full_mapping->mapping[i]);
		else
		{
			cp = ToCaseFoldSimple(cp);
			writing_pos += WriteUnicodePoint(writing_pos, cp);
		}
	}
	*writing_pos = 0;

	return data;
}
